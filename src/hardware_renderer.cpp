#include "shin.h"

#ifdef _WIN32
#define GLEW_STATIC
#include <glew/glew.h>
#endif

#ifdef __APPLE__
#include <OpenGL/gl3.h>
#include <OpenGL/gl3ext.h>
#endif

#include <glfw/glfw3.h>

static GLuint shader_load(char *code, GLenum type) {
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &code, 0);
	glCompileShader(shader);

	GLint compiled;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

	if (compiled == GL_FALSE) {
		GLint length;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
		char *log = (char *) malloc(length);
		glGetShaderInfoLog(shader, length, 0, log);
		printf("%s\n", log);
	}

	return shader;
}

static void shaders_init(Renderer *renderer) {
	char *vertex_code = read_entire_file("resources/vert.glsl");
	char *fragment_code = read_entire_file("resources/frag.glsl");

	if (!vertex_code) {
		puts("Failed to read resources/vert.glsl!");
		exit(1);
	}
	
	if (!fragment_code) {
		puts("Failed to read resources/frag.glsl!");
		exit(1);
	}

	GLuint vertex_shader = shader_load(vertex_code, GL_VERTEX_SHADER);
	GLuint fragment_shader = shader_load(fragment_code, GL_FRAGMENT_SHADER);

	GLuint program = glCreateProgram();
	glAttachShader(program, vertex_shader);
	glAttachShader(program, fragment_shader);
	glLinkProgram(program);

	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);

	glUseProgram(program);

	renderer->shader_glyph_map_slot = glGetUniformLocation(program, "glyph_map");
	renderer->shader_cell_size_slot = glGetUniformLocation(program, "cell_size");
	renderer->shader_grid_size_slot = glGetUniformLocation(program, "grid_size");
	renderer->shader_win_size_slot = glGetUniformLocation(program, "win_size");
	renderer->shader_time_slot = glGetUniformLocation(program, "time");
	renderer->shader_background_color_slot = glGetUniformLocation(program, "background_color");
}

static void create_glyph_texture(GLint uniform_slot) {
	GLuint tex;

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	glGenTextures(1, &tex);
	glUniform1i(uniform_slot, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

static u32 create_cells_ssbo() {
	GLuint ssbo;

	glGenBuffers(1, &ssbo);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, ssbo);

	return ssbo;
}

static void glyph_map_update_texture(GlyphMap *glyph_map) {
	glActiveTexture(GL_TEXTURE0);

	glTexImage2D(
		GL_TEXTURE_2D,
		0,
		GL_RED,
		glyph_map->width,
		glyph_map->height,
		0,
		GL_RED,
		GL_UNSIGNED_BYTE,
		glyph_map->data	
	);
}

static bool checkOpenGLError() {
    bool found_error = false;
    int glErr = glGetError();
    while (glErr != GL_NO_ERROR) {
        printf("glError: %d\n", glErr);
        found_error = true;
        glErr = glGetError();
    }
    return found_error;
}

static void renderer_init_glyph_map(Renderer *renderer) {
	renderer->glyph_map = glyph_map_create("resources/consolas.ttf", 20);

	create_glyph_texture(renderer->shader_glyph_map_slot);
	renderer->shader_cells_ssbo = create_cells_ssbo();

	glyph_map_update_texture(renderer->glyph_map);
}

void renderer_init(Renderer *renderer, GLFWwindow *window) {
	renderer->window = window;

	shaders_init(renderer);

	f32 vertices[12] = {
		-1.0, -1.0,
		-1.0, 1.0,
		1.0, -1.0,
		-1.0, 1.0,
		1.0, 1.0,
		1.0, -1.0
	};

	GLuint vao;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	GLuint vbo;
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 2, GL_FLOAT, 0, 0, 0);
	glEnableVertexAttribArray(0);
	
	renderer_init_glyph_map(renderer);
}

void renderer_query_cell_data(Renderer *renderer) {
	DrawBuffer *buffer = &renderer->buffer;

	checkOpenGLError();
	glBufferData(GL_SHADER_STORAGE_BUFFER, buffer->cells_size, buffer->cells, GL_DYNAMIC_DRAW);
}

void renderer_query_settings(Renderer *renderer, s32 width, s32 height) {
    DrawBuffer *buffer = &renderer->buffer;
	FontMetrics metrics = renderer->glyph_map->metrics;

	glUniform2ui(renderer->shader_cell_size_slot, metrics.glyph_width, metrics.glyph_height);
	glUniform2ui(renderer->shader_grid_size_slot, buffer->columns, buffer->rows);
	glUniform2ui(renderer->shader_win_size_slot, width, height);
	glUniform1ui(renderer->shader_background_color_slot, color_hex_from_rgb(settings.bg_temp));
}

void renderer_update_time(Renderer *renderer, f64 time) {
    glUniform1f(renderer->shader_time_slot, time);
}

void renderer_end(Renderer *renderer) {
	glClear(GL_COLOR_BUFFER_BIT);

	glDrawArrays(GL_TRIANGLES, 0, 6);
}
