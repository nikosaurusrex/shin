#include "shin.h"
#include "buffer.cpp"
#include "commands.cpp"

#define GLEW_STATIC
#include <glew/glew.h>
#include <glfw/glfw3.h>
#include <freetype/freetype.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define GLYPH_MAP_COUNT_X 32
#define GLYPH_MAP_COUNT_Y 16

#define GLYPH_MODE_NORMAL 0
#define GLYPH_MODE_INVERT 1

#define MAX_LINE_LENGTH 256

struct FontMetrics {
	s32 descender;
	s32 glyph_width;
	s32 glyph_height;
}; 

struct GlyphMap {
	FontMetrics metrics;
	u8 *data;
	u32 width;
	u32 height;
};

struct Cell {
	u32 glyph_index;
	u32 glyph_mode;
	u32 foreground;
	u32 background;
};

struct DrawBuffer {
	Cell *cells;
	u64 cells_size;
	u32 rows;
	u32 columns;
};

struct Renderer {
	GlyphMap *glyph_map;
	GLFWwindow *window;
	DrawBuffer buffer;
};

void read_file_to_buffer(Buffer *buffer) {
	FILE *file = fopen(buffer->file_path, "rb");
	if (!file) return;

	fseek(file, 0, SEEK_END);
	u64 file_size = ftell(file);
	fseek(file, 0, SEEK_SET);

	buffer_clear(buffer);
	buffer_grow_if_needed(buffer, file_size);

	u64 size_read = fread(buffer->data, 1, file_size, file);

	fclose(file);

	buffer->gap_start = size_read;
}

void write_buffer_to_file(Buffer *buffer) {
	FILE *file = fopen(buffer->file_path, "w");

	if (!file) return;

	fwrite(buffer->data, 1, buffer->gap_start, file);
	fwrite(buffer->data + buffer->gap_end, 1, buffer->size - buffer->gap_end, file);

	fclose(file);
}

void shin_exit() {
	/* TODO: exit */
	// PostQuitMessage(0);
}

void create_default_keymaps() {
	// insert keymap
	Keymap *keymap = keymap_create_empty();

	for (char ch = ' '; ch <= '~'; ++ch) {
		keymap->commands[ch] = command_insert_char;
	}
	keymap->commands[GLFW_KEY_ENTER] = command_insert_new_line;
	keymap->commands[GLFW_KEY_DELETE] = command_delete_forwards;
	keymap->commands[GLFW_KEY_BACKSPACE] = command_delete_backwards;
	keymap->commands[GLFW_KEY_LEFT] = command_cursor_back;
	keymap->commands[GLFW_KEY_RIGHT] = command_cursor_next;
	keymap->commands[GLFW_KEY_F4 | ALT] = command_quit;
	keymap->commands['S' | CTRL] = command_save_buffer;
	keymap->commands['R' | CTRL] = command_reload_buffer;
	keymap->commands[GLFW_KEY_ESCAPE] = command_normal_mode;

	insert_keymap = keymap;
	
// normal keymap
	keymap = keymap_create_empty();
	keymap->commands['X'] = command_delete_forwards;
	keymap->commands['H'] = command_cursor_back;
	keymap->commands['L'] = command_cursor_next;
	keymap->commands['J'] = command_next_line;
	keymap->commands['K'] = command_prev_line;
	keymap->commands['I' | SHIFT] = command_goto_beginning_of_line;
	keymap->commands['A' | SHIFT] = command_goto_end_of_line;
	keymap->commands[GLFW_KEY_F4 | ALT] = command_quit;
	keymap->commands['S' | CTRL] = command_save_buffer;
	keymap->commands['R' | CTRL] = command_reload_buffer;
	keymap->commands['I'] = command_insert_mode;
	keymap->commands['A'] = command_insert_mode_next;
	keymap->commands['O'] = command_new_line_after;
	keymap->commands['O' | SHIFT] = command_new_line_before;
	// TODO: fix wrong binding
	keymap->commands['G'] = command_goto_buffer_begin;
	keymap->commands['G' | SHIFT] = command_goto_buffer_end;
	keymap->commands['H' | CTRL] = command_prev_pane;
	keymap->commands['L' | CTRL] = command_next_pane;
	normal_keymap = keymap;
}

char *read_entire_file(const char *file_path) {
	FILE *file = fopen(file_path, "rb");
	if (!file) {
		printf("Failed to open file '%s'!\n", file_path);
		exit(1);
	}

	fseek(file, 0, SEEK_END);
	size_t length = ftell(file);
	fseek(file, 0, SEEK_SET);

	char *contents = (char *) malloc(length + 1);
	fread(contents, 1, length, file);
	contents[length] = 0;

	return contents;
}

FontMetrics font_metrics_get(FT_Face face) {
	FontMetrics metrics = {0};

	FT_Load_Char(face, 'M', FT_RENDER_MODE_NORMAL);
	metrics.descender = -face->size->metrics.descender >> 6;
	metrics.glyph_width = face->glyph->advance.x >> 6;
	metrics.glyph_height = (face->size->metrics.ascender >> 6) + metrics.descender;

	return metrics;
}

GlyphMap *glyph_map_create(FT_Library ft, const char *font, u32 pixel_size) {
	FT_Face face;
	FT_New_Face(ft, font, 0, &face);
	FT_Set_Pixel_Sizes(face, 0, pixel_size);

	FontMetrics metrics = font_metrics_get(face);

	GlyphMap *map = (GlyphMap *) malloc(sizeof(GlyphMap));

	map->metrics = metrics;
	map->width = metrics.glyph_width * GLYPH_MAP_COUNT_X;
	map->height = metrics.glyph_height * GLYPH_MAP_COUNT_Y;

	map->data = (u8 *) malloc(map->width * map->height);
	memset(map->data, 0, map->width * map->height);

	u32 chars_count = '~' - ' ';

	for (u32 i = 0; i < chars_count; ++i) {
		u32 code = ' ' + i;
		FT_UInt glyph_index = FT_Get_Char_Index(face, code);
		FT_GlyphSlot glyph = face->glyph;

		FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER);
		FT_Render_Glyph(glyph, FT_RENDER_MODE_NORMAL);

		FT_Bitmap bitmap = glyph->bitmap;
		s32 left = glyph->bitmap_left;
		s32 top = (metrics.glyph_height - metrics.descender) - glyph->bitmap_top;

		if (top < 0) {
			top = 0;
		}

		s32 tile_x = i % GLYPH_MAP_COUNT_X;
		s32 tile_y = i / GLYPH_MAP_COUNT_X;

		s32 start_x = tile_x * metrics.glyph_width + left;
		s32 start_y = tile_y * metrics.glyph_height + top;

		for (s32 pixel_y = 0; pixel_y < bitmap.rows; ++pixel_y) {
			for (s32 pixel_x = 0; pixel_x < bitmap.width; ++pixel_x) {
				u8 color = bitmap.buffer[pixel_x + pixel_y * bitmap.width];

				s32 x = start_x + pixel_x;
				s32 y = start_y + pixel_y;

				map->data[x + y * map->width] = color;
			}
		}
	}

	return map;
}

void draw_buffer_resize(Renderer *renderer);
void window_resize(GLFWwindow *window, s32 width, s32 height) {
	Renderer *renderer = (Renderer *) glfwGetWindowUserPointer(window);
	draw_buffer_resize(renderer);
}

void key_callback(GLFWwindow* window, s32 key, s32 scancode, s32 action, s32 mods) {
	if (action != GLFW_PRESS) {
		return;
	}

	if (GLFW_KEY_SPACE <= key && key <= GLFW_KEY_GRAVE_ACCENT) {
		return;
	}

	u16 kcomb = key;
	if (mods & GLFW_MOD_SHIFT) {
		kcomb |= SHIFT;
	}

	if (mods & GLFW_MOD_CONTROL) {
		kcomb |= CTRL;
	}

	if (mods & GLFW_MOD_ALT) {
		kcomb |= ALT;
	}

	last_input_event.type = INPUT_EVENT_PRESSED;
	last_input_event.key_comb = kcomb;
	last_input_event.ch = (char) key;

	Keymap *keymap;
	if (current_buffer->mode == MODE_NORMAL) {
		keymap = normal_keymap;
	} else {
		keymap = insert_keymap;
	}

	keymap_dispatch_event(keymap, last_input_event);
}

void character_callback(GLFWwindow* window, u32 key) {
	u32 kcomb = key;
	if ('a' <= kcomb && kcomb <= 'z') {
		kcomb -= 32;
	}

	if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT)) {
		kcomb |= SHIFT;
	}

	if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL)) {
		kcomb |= CTRL;
	}

	if (glfwGetKey(window, GLFW_KEY_LEFT_ALT)) {
		kcomb |= ALT;
	}

	printf("%d %d\n", (SHIFT | 'I'), kcomb);

	last_input_event.type = INPUT_EVENT_PRESSED;
	last_input_event.key_comb = kcomb;
	last_input_event.ch = (char) key;

	Keymap *keymap;
	if (current_buffer->mode == MODE_NORMAL) {
		keymap = normal_keymap;
	} else {
		keymap = insert_keymap;
	}

	keymap_dispatch_event(keymap, last_input_event);
}

GLFWwindow *window_create(Renderer *renderer, u32 width, u32 height) {
	glfwInit();

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

#ifdef _MACH
	glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

	GLFWwindow *window = glfwCreateWindow(width, height, "Shin", 0, 0);

	glfwSetWindowUserPointer(window, renderer);
	glfwSetFramebufferSizeCallback(window, window_resize);
	glfwSetKeyCallback(window, key_callback);
	glfwSetCharCallback(window, character_callback);

	glfwMakeContextCurrent(window);
	glViewport(0, 0, width, height);

	return window;
}

GLuint shader_load(char *code, GLenum type) {
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

void shaders_init() {
	char *vertex_code = read_entire_file("shaders/vert.glsl");
	char *fragment_code = read_entire_file("shaders/frag.glsl");

	GLuint vertex_shader = shader_load(vertex_code, GL_VERTEX_SHADER);
	GLuint fragment_shader = shader_load(fragment_code, GL_FRAGMENT_SHADER);

	GLuint program = glCreateProgram();
	glAttachShader(program, vertex_shader);
	glAttachShader(program, fragment_shader);
	glLinkProgram(program);

	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);

	glUseProgram(program);
}

void opengl_create_texture(int id) {
	GLuint tex;

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	glGenTextures(1, &tex);
	glUniform1i(id + 1, id);
	glActiveTexture(GL_TEXTURE0 + id);
	glBindTexture(GL_TEXTURE_2D, tex);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void glyph_map_update_texture(GlyphMap *glyph_map) {
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

	stbi_write_png("glyph_map.png", glyph_map->width, glyph_map->height, 1, glyph_map->data, glyph_map->width);
}

void query_cell_data(Renderer *renderer) {
	DrawBuffer *buffer = &renderer->buffer;
	glActiveTexture(GL_TEXTURE1);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32UI, buffer->columns, buffer->rows, 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT, buffer->cells);
}

void draw_buffer_resize(Renderer *renderer) {
	s32 width, height;
	glfwGetFramebufferSize(renderer->window, &width, &height);

	DrawBuffer *buffer = &renderer->buffer;
	FontMetrics metrics = renderer->glyph_map->metrics;

	buffer->columns = floor((f32)width / metrics.glyph_width);
	buffer->rows = floor((f32)height / metrics.glyph_height);
	buffer->cells_size = sizeof(Cell) * buffer->columns * buffer->rows;
	buffer->cells = (Cell *) realloc(buffer->cells, buffer->cells_size);

	glUniform2ui(3, metrics.glyph_width, metrics.glyph_height);
	glUniform2ui(4, width, height);

	for (u32 i = 0; i < pane_count; ++i) {
		Bounds bounds = panes[i].bounds;
		f32 height = (f32) (bounds.bottom - bounds.top);
		panes[i].showable_lines = (u32) floorf(height / metrics.glyph_height);
	}
}

void draw_buffer_init(Renderer *renderer) {
	DrawBuffer *buffer = &renderer->buffer;
	buffer->cells = (Cell *) malloc(buffer->cells_size);
	draw_buffer_resize(renderer);
	memset(buffer->cells, 0, buffer->cells_size);
}

void renderer_init(Renderer *renderer, GLFWwindow *window) {
	renderer->window = window;

	shaders_init();

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
}

void renderer_init_glyph_map(FT_Library ft, Renderer *renderer) {
	renderer->glyph_map = glyph_map_create(ft, "Consolas.ttf", 40);

	opengl_create_texture(0);
	opengl_create_texture(1);

	glyph_map_update_texture(renderer->glyph_map);
	query_cell_data(renderer);
}

void window_update(GLFWwindow *window) {
	glClear(GL_COLOR_BUFFER_BIT);

	glDrawArrays(GL_TRIANGLES, 0, 6);
	glfwSwapBuffers(window);
}

void renderer_render(Renderer *renderer) {
	DrawBuffer *draw_buffer = &renderer->buffer;

	// clear screen
	memset(draw_buffer->cells, 0, draw_buffer->cells_size);

	Pane *pane = &panes[active_pane];
	Buffer *buffer = pane->buffer;

	pane_update_scroll(pane);

	u32 start = pane->start;
	u32 lines_drawn = 0;
	u32 pos;

	char line[MAX_LINE_LENGTH];

	for (pos = start; pos < buffer_length(buffer) && lines_drawn < draw_buffer->rows; pos = cursor_next(buffer, pos)) {
		u32 prev_pos = pos;

		u32 line_length = buffer_get_line(buffer, line, sizeof(line) - 1, &pos);
		line[line_length] = 0;

		u32 draw_line_length = MIN(line_length, MIN(MAX_LINE_LENGTH, draw_buffer->columns));
		for (u32 i = 0; i < draw_line_length; ++i) {
			Cell *cell = &draw_buffer->cells[i + lines_drawn * draw_buffer->columns];
			cell->glyph_index = line[i] - 32;
			cell->foreground = 0xFFFFFF;
			cell->background = 0x000000;
			cell->glyph_mode = GLYPH_MODE_NORMAL;
		}

		if (pane == &panes[active_pane] && prev_pos <= buffer->cursor && buffer->cursor <= pos) {
			u32 i = buffer->cursor - prev_pos;

			draw_buffer->cells[i + lines_drawn * draw_buffer->columns].glyph_mode = GLYPH_MODE_INVERT;
		}

		lines_drawn++;
		
		if (lines_drawn >= pane->showable_lines) {
			break;
		}
	}

	pane->end = cursor_get_end_of_line(buffer, pos);
}

int main() {
	create_default_keymaps();
	current_buffer = buffer_create(32);
	current_buffer->file_path = (char *) "test.txt";

	pane_create({0, 500, 0, 500}, current_buffer);

	FT_Library ft;
	FT_Init_FreeType(&ft);

	Renderer renderer = {0};
	
	GLFWwindow *window = window_create(&renderer, 1280, 720);

	glewInit();

	renderer_init(&renderer, window);
	renderer_init_glyph_map(ft, &renderer);
	draw_buffer_init(&renderer);
	
	glClearColor(0.0, 0.0, 0.0, 1.0);

	f64 prev_time = glfwGetTime();
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		f64 current_time = glfwGetTime();
		f64 delta = prev_time - current_time;

		if (current_time - prev_time >= 0.01) {
			glUniform1f(5, current_time);

			draw_buffer_resize(&renderer);

			renderer_render(&renderer);
			query_cell_data(&renderer);
			window_update(window);

			prev_time = current_time;
		}
	}

	glfwTerminate();

	return 0;
}
