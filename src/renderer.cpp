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

const f32 quad_vertices[8] = {
	-1.0f, -1.0f,
	1.0f, -1.0f,
	-1.0f, 1.0f,
	1.0f, 1.0f
};

const f32 tex_coords_vertices[8] = {
	0.0f, 1.0f,
	1.0f, 1.0f,
	0.0f, 0.0f,
	1.0f, 0.0f
};

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

static u32 shaders_compile(char *vertex_code, char *fragment_code) {
	GLuint vertex_shader = shader_load(vertex_code, GL_VERTEX_SHADER);
	GLuint fragment_shader = shader_load(fragment_code, GL_FRAGMENT_SHADER);

	GLuint program = glCreateProgram();
	glAttachShader(program, vertex_shader);
	glAttachShader(program, fragment_shader);
	glLinkProgram(program);

	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);

	glUseProgram(program);

	return program;
}

static void shaders_init(HardwareRenderer *renderer) {
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

	renderer->program = shaders_compile(vertex_code, fragment_code);

	renderer->shader_glyph_map_slot = glGetUniformLocation(renderer->program, "glyph_map");
	renderer->shader_cell_size_slot = glGetUniformLocation(renderer->program, "cell_size");
	renderer->shader_grid_size_slot = glGetUniformLocation(renderer->program, "grid_size");
	renderer->shader_win_size_slot = glGetUniformLocation(renderer->program, "win_size");
	renderer->shader_time_slot = glGetUniformLocation(renderer->program, "time");
	renderer->shader_background_color_slot = glGetUniformLocation(renderer->program, "background_color");
}

static u32 create_texture(GLint uniform_slot) {
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

	return tex;
}

static u32 create_cells_ssbo() {
	GLuint ssbo;

	glGenBuffers(1, &ssbo);
#ifdef _WIN32
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, ssbo);
#endif

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

static void renderer_init_glyph_map(HardwareRenderer *renderer) {
	renderer->glyph_texture = create_texture(renderer->shader_glyph_map_slot);
	renderer->shader_cells_ssbo = create_cells_ssbo();

	glyph_map_update_texture(renderer->glyph_map);
}

void Renderer::init(GLFWwindow *window, DrawBuffer *draw_buffer, GlyphMap *glyph_map) {
	this->window = window;
	this->buffer = draw_buffer;
	this->glyph_map = glyph_map;
}

void HardwareRenderer::reinit(s32 width, s32 height) {
	shaders_init(this);

	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);

	glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 2, GL_FLOAT, 0, 0, 0);
	glEnableVertexAttribArray(0);
	
	renderer_init_glyph_map(this);
	resize(width, height);
}

void HardwareRenderer::deinit() {
	glDeleteVertexArrays(1, &vao);
	glDeleteBuffers(1, &vbo);
	glDeleteBuffers(1, &shader_cells_ssbo);
	glDeleteTextures(1, &glyph_texture);
	glDeleteProgram(program);
}

void HardwareRenderer::resize(s32 width, s32 height) {
	FontMetrics metrics = glyph_map->metrics;

	glUniform2ui(shader_cell_size_slot, metrics.glyph_width, metrics.glyph_height);
	glUniform2ui(shader_grid_size_slot, buffer->columns, buffer->rows);
	glUniform2ui(shader_win_size_slot, width, height);
}

void HardwareRenderer::end() {
	glClear(GL_COLOR_BUFFER_BIT);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void HardwareRenderer::query_cell_data() {
#ifdef _WIN32
	glBufferData(GL_SHADER_STORAGE_BUFFER, buffer->cells_size, buffer->cells, GL_DYNAMIC_DRAW);
#endif
}

void HardwareRenderer::query_settings() {
	glUniform1ui(shader_background_color_slot, color_hex_from_rgb(settings.bg_temp));
	
	glfwSwapInterval(settings.vsync ? 1 : 0);
	glfwSetWindowOpacity(window, settings.opacity);
}

void HardwareRenderer::update_time(f64 time) {
    glUniform1f(shader_time_slot, time);
}





const char* software_renderer_vertex_shader = R"(
    #version 410 core
    layout (location = 0) in vec2 in_pos;
    layout (location = 1) in vec2 in_tex_coords;
    out vec2 out_tex_coords;
    void main()
    {
        gl_Position = vec4(in_pos.x, in_pos.y, 0.0, 1.0);
        out_tex_coords = in_tex_coords;
    }
)";

const char* software_renderer_fragment_shader = R"(
    #version 410 core
    in vec2 out_tex_coords;
    out vec4 out_color;
    uniform sampler2D texture1;
    void main()
    {
        out_color = texture(texture1, out_tex_coords);
    }
)";

void SoftwareRenderer::reinit(s32 width, s32 height) {
    resize(width, height);

	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 2, GL_FLOAT, 0, 0, 0);
	glEnableVertexAttribArray(0);

	glGenBuffers(1, &vto);
	glBindBuffer(GL_ARRAY_BUFFER, vto);
	glBufferData(GL_ARRAY_BUFFER, sizeof(tex_coords_vertices), tex_coords_vertices, GL_STATIC_DRAW);
	glVertexAttribPointer(1, 2, GL_FLOAT, 0, 0, 0);
	glEnableVertexAttribArray(1);

	program = shaders_compile(
		(char *) software_renderer_vertex_shader,
		(char *) software_renderer_fragment_shader
	);

	texture = create_texture(
		glGetUniformLocation(program, "texture1")
	);
}

void SoftwareRenderer::deinit() {
    free(screen);
    screen = 0;
    
	glDeleteVertexArrays(1, &vao);
	glDeleteBuffers(1, &vbo);
	glDeleteBuffers(1, &vto);
	glDeleteProgram(program);
    glDeleteTextures(1, &texture);
}

void SoftwareRenderer::resize(s32 width, s32 height) {
    this->width = width;
    this->height = height;

    if (screen) {
        free(screen);
        screen = 0;
    }

    screen = (u32 *) malloc(width * height * sizeof(u32));
}

void SoftwareRenderer::end() {
    glClear(GL_COLOR_BUFFER_BIT);

	for (u32 i = 0; i < width * height; ++i) {
		screen[i] = (0xFFu << 24) | settings.colors[COLOR_BG];
	}

	for (u32 row = 0; row < buffer->rows; ++row) {
		for (u32 column = 0; column < buffer->columns; ++column) {
			Cell *cell = &buffer->cells[column + row * buffer->columns];

			if (cell->glyph_flags != 0 || cell->glyph_index != 0) {
				render_cell(cell, column, row);
			}
		}
	}

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, screen);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void SoftwareRenderer::query_cell_data() {
}

void SoftwareRenderer::query_settings() {
	glfwSwapInterval(settings.vsync ? 1 : 0);
	glfwSetWindowOpacity(window, settings.opacity);
}

void SoftwareRenderer::update_time(f64 time) {
}

void SoftwareRenderer::render_cell(Cell *cell, u32 column, u32 row) {
	FontMetrics metrics = glyph_map->metrics;

	u32 gw = metrics.glyph_width;
	u32 gh = metrics.glyph_height;

	u32 cell_x_index = cell->glyph_index % GLYPH_MAP_COUNT_X;
	u32 cell_y_index = cell->glyph_index / GLYPH_MAP_COUNT_X;
	u32 cell_x = cell_x_index * gw;
	u32 cell_y = cell_y_index * gh;

	u32 xoff = column * gw;
	u32 yoff = row * gh;

	bool invert = cell->glyph_flags & GLYPH_INVERT;
	u32 fg_hex = cell->foreground;
	u32 bg_hex = cell->background;
	if (invert) {
		fg_hex = color_invert(fg_hex);
		bg_hex = color_invert(bg_hex);
	}

	f32 fg[3];
	color_set_rgb_from_hex(fg, fg_hex);
	f32 temp = fg[0];
	fg[0] = fg[2];
	fg[2] = temp;

	f32 bg[3];
	color_set_rgb_from_hex(bg, bg_hex);

	for (u32 y = 0; y < gh; ++y) {
		for (u32 x = 0; x < gw; ++x) {
			u8 glyph_alpha_int = glyph_map->data[(cell_x + x) + (cell_y + y) * glyph_map->width];
			f32 glyph_alpha = (f32)(glyph_alpha_int) / 255.0f;

			f32 pixel_rgb[3];
			pixel_rgb[0] = (glyph_alpha * fg[0]) + (1.0 - glyph_alpha) * bg[0];
			pixel_rgb[1] = (glyph_alpha * fg[1]) + (1.0 - glyph_alpha) * bg[1];
			pixel_rgb[2] = (glyph_alpha * fg[2]) + (1.0 - glyph_alpha) * bg[2];

			u32 pixel = (0xFFu << 24) | color_hex_from_rgb(pixel_rgb);

			screen[(xoff + x) + (yoff + y) * width] = pixel;
		}
	}
}