#include "shin.h"
#include "buffer.cpp"
#include "commands.cpp"
#include "shortcuts.cpp"
#include "highlighting.cpp"

#ifdef _WIN32
#define GLEW_STATIC
#include <glew/glew.h>
#endif

#ifdef __APPLE__
#include <OpenGL/gl3.h>
#include <OpenGL/gl3ext.h>
#endif

#include <glfw/glfw3.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include "extern/imgui/imgui.h"
#include "extern/imgui/imgui_impl_glfw.h"
#include "extern/imgui/imgui_impl_opengl3.h"

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

	GLint shader_glyph_map_slot;
	GLint shader_cells_slot;
	GLint shader_cell_size_slot;
	GLint shader_win_size_slot;
	GLint shader_time_slot;
};

void read_file_to_buffer(Buffer *buffer) {
	if (buffer->file_path == 0) return;

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
	if (buffer->file_path == 0) return;
	
	FILE *file = fopen(buffer->file_path, "w");

	if (!file) return;

	fwrite(buffer->data, 1, buffer->gap_start, file);
	fwrite(buffer->data + buffer->gap_end, 1, buffer->size - buffer->gap_end, file);

	fclose(file);
}

u32 color_hex_from_rgb(f32 rgb[3]) {
	u32 r = (u32)(rgb[0] * 255.0f) << 16;
	u32 g = (u32)(rgb[1] * 255.0f) << 8;
	u32 b = (u32)(rgb[2] * 255.0f);
	return r | g | b;
}

void create_default_keymaps() {
	// insert keymap
	Keymap *keymap = keymap_create_empty();

	for (char ch = ' '; ch <= '~'; ++ch) {
		keymap->shortcuts[ch] = shortcut_insert_char;
		keymap->shortcuts[ch | SHIFT] = shortcut_insert_char;
	}
	keymap->shortcuts[GLFW_KEY_ENTER] = shortcut_insert_new_line;
	keymap->shortcuts[GLFW_KEY_TAB] = shortcut_insert_tab;
	keymap->shortcuts[GLFW_KEY_DELETE] = shortcut_delete_forwards;
	keymap->shortcuts[GLFW_KEY_BACKSPACE] = shortcut_delete_backwards;
	keymap->shortcuts[GLFW_KEY_LEFT] = shortcut_cursor_back;
	keymap->shortcuts[GLFW_KEY_RIGHT] = shortcut_cursor_next;
	keymap->shortcuts[GLFW_KEY_F4 | ALT] = shortcut_quit;
	keymap->shortcuts[GLFW_KEY_ESCAPE] = shortcut_normal_mode;

	keymaps[MODE_INSERT] = keymap;
	
	// normal keymap
	keymap = keymap_create_empty();
	keymap->shortcuts['X'] = shortcut_delete_forwards;
	keymap->shortcuts['H'] = shortcut_cursor_back;
	keymap->shortcuts['L'] = shortcut_cursor_next;
	keymap->shortcuts['J'] = shortcut_next_line;
	keymap->shortcuts['K'] = shortcut_prev_line;
	keymap->shortcuts['W'] = shortcut_go_word_next;
	keymap->shortcuts['E'] = shortcut_go_word_end;
	keymap->shortcuts['B'] = shortcut_go_word_prev;
	keymap->shortcuts['I' | SHIFT] = shortcut_goto_beginning_of_line;
	keymap->shortcuts['A' | SHIFT] = shortcut_goto_end_of_line;
	keymap->shortcuts[GLFW_KEY_F4 | ALT] = shortcut_quit;
	keymap->shortcuts['I'] = shortcut_insert_mode;
	keymap->shortcuts['A'] = shortcut_insert_mode_next;
	keymap->shortcuts['O'] = shortcut_new_line_after;
	keymap->shortcuts['O' | SHIFT] = shortcut_new_line_before;
	keymap->shortcuts['G' | SHIFT] = shortcut_goto_buffer_end;
	keymap->shortcuts['W' | CTRL] = shortcut_window_operation;
	keymap->shortcuts[GLFW_KEY_F3] = shortcut_show_settings;
	keymap->shortcuts[':' | SHIFT] = shortcut_begin_command;
	// TODO: fix wrong binding
	keymap->shortcuts['G'] = shortcut_goto_buffer_begin;

	keymaps[MODE_NORMAL] = keymap;
	
	// window operation
	keymap = keymap_create_empty();
	keymap->shortcuts['H' | CTRL] = shortcut_prev_pane;
	keymap->shortcuts['H'] = shortcut_prev_pane;
	keymap->shortcuts['L' | CTRL] = shortcut_next_pane;
	keymap->shortcuts['L'] = shortcut_next_pane;
	keymap->shortcuts['V' | CTRL] = shortcut_split_vertically;
	keymap->shortcuts['V'] = shortcut_split_vertically;
	keymap->shortcuts['S' | CTRL] = shortcut_split_horizontally;
	keymap->shortcuts['S'] = shortcut_split_horizontally;
	keymaps[MODE_WINDOW_OPERATION] = keymap;
	
	// command keymap
	keymap = keymap_create_empty();

	for (char ch = ' '; ch <= '~'; ++ch) {
		keymap->shortcuts[ch] = shortcut_insert_char;
		keymap->shortcuts[ch | SHIFT] = shortcut_insert_char;
	}
	keymap->shortcuts[GLFW_KEY_ENTER] = shortcut_insert_new_line;
	keymap->shortcuts[GLFW_KEY_DELETE] = shortcut_delete_forwards;
	keymap->shortcuts[GLFW_KEY_BACKSPACE] = shortcut_delete_backwards;
	keymap->shortcuts[GLFW_KEY_LEFT] = shortcut_cursor_back;
	keymap->shortcuts[GLFW_KEY_RIGHT] = shortcut_cursor_next;
	keymap->shortcuts[GLFW_KEY_ENTER] = shortcut_command_confirm;
	keymap->shortcuts[GLFW_KEY_ESCAPE] = shortcut_normal_mode;

	keymaps[MODE_COMMAND] = keymap;
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

	if (!(mods & GLFW_MOD_CONTROL) && !(mods & GLFW_MOD_ALT)) {
		if (GLFW_KEY_SPACE <= key && key <= GLFW_KEY_GRAVE_ACCENT) {
			return;
		}
	}

	u16 kcomb = key;
	if (mods & GLFW_MOD_CONTROL) {
		kcomb |= CTRL;
	}

	if (mods & GLFW_MOD_SHIFT) {
		kcomb |= SHIFT;
	}

	if (mods & GLFW_MOD_ALT) {
		kcomb |= ALT;
	}

	last_input_event.type = INPUT_EVENT_PRESSED;
	last_input_event.key_comb = kcomb;
	last_input_event.ch = (char) key;

	Keymap *keymap = keymaps[current_buffer->mode];

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

	last_input_event.type = INPUT_EVENT_PRESSED;
	last_input_event.key_comb = kcomb;
	last_input_event.ch = (char) key;

	Keymap *keymap = keymaps[current_buffer->mode];

	keymap_dispatch_event(keymap, last_input_event);
}

GLFWwindow *window_create(Renderer *renderer, u32 width, u32 height) {
	glfwInit();

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

#ifdef __APPLE__
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

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
	const char* glsl_version = "#version 410 core";
    ImGui_ImplOpenGL3_Init(glsl_version);

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

void shaders_init(Renderer *renderer) {
	char *vertex_code = read_entire_file("resources/vert.glsl");
	char *fragment_code = read_entire_file("resources/frag.glsl");

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
	renderer->shader_cells_slot = glGetUniformLocation(program, "cells");
	renderer->shader_cell_size_slot = glGetUniformLocation(program, "cell_size");
	renderer->shader_win_size_slot = glGetUniformLocation(program, "win_size");
	renderer->shader_time_slot = glGetUniformLocation(program, "time");
}

void opengl_create_texture(int id, GLint uniform_slot) {
	GLuint tex;

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	glGenTextures(1, &tex);
	glUniform1i(uniform_slot, id);
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

	/* TODO: Layout management */

	Bounds *command_bounds = &command_pane->bounds;
	command_bounds->left = 0;
	command_bounds->top = buffer->rows - 1;
	command_bounds->width = buffer->columns;
	command_bounds->height = 1;

	Bounds *bounds = &active_pane->bounds;
	bounds->left = 0;
	bounds->top = 0;
	bounds->width = buffer->columns;
	bounds->height = buffer->rows - 1;

	glUniform2ui(renderer->shader_cell_size_slot, metrics.glyph_width, metrics.glyph_height);
	glUniform2ui(renderer->shader_win_size_slot, width, height);
}

void draw_buffer_init(Renderer *renderer) {
	DrawBuffer *buffer = &renderer->buffer;
	buffer->cells = (Cell *) malloc(buffer->cells_size);
	draw_buffer_resize(renderer);
	memset(buffer->cells, 0, buffer->cells_size);
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
}

void renderer_init_glyph_map(FT_Library ft, Renderer *renderer) {
	renderer->glyph_map = glyph_map_create(ft, "resources/consolas.ttf", 20);

	opengl_create_texture(0, renderer->shader_glyph_map_slot);
	opengl_create_texture(1, renderer->shader_cells_slot);

	glyph_map_update_texture(renderer->glyph_map);
	query_cell_data(renderer);
}

void renderer_render(Renderer *renderer, Pane *pane) {
	DrawBuffer *draw_buffer = &renderer->buffer;
	Buffer *buffer = pane->buffer;
	Bounds bounds = pane->bounds;

	u32 start = pane->start;
	u32 lines_drawn = 0;
	u32 pos;

	char line[MAX_LINE_LENGTH];
	bool should_draw_cursor = false;
	bool has_drawn_cursor = false;
	bool is_active_pane = pane == active_pane;

	u32 highlight_index = 0;
	u32 highlight_cursor = 0;

	for (pos = start; pos < buffer_length(buffer) && lines_drawn < bounds.height; pos = cursor_next(buffer, pos)) {
		u32 prev_pos = pos;

		u32 line_length = buffer_get_line(buffer, line, sizeof(line) - 1, &pos);
		line[line_length] = 0;

		if (is_active_pane && (prev_pos <= buffer->cursor && buffer->cursor <= pos)
		) {
			should_draw_cursor = true;
		}

		u32 draw_line_length = MIN(line_length, MIN(MAX_LINE_LENGTH, MIN(bounds.width, draw_buffer->columns)));

		u32 render_x = bounds.left;
		u32 render_y = bounds.top + lines_drawn;

		bool has_highlights = highlight_index < pane->highlights.length;
		Highlight highlight;
		if (has_highlights) {
			highlight = pane->highlights[highlight_index];
			highlight_cursor = prev_pos;
		}

		for (u32 i = 0; i < draw_line_length; ++i) {
			char ch = line[i];
			Cell *cell = &draw_buffer->cells[render_x + render_y * draw_buffer->columns];

			cell->glyph_index = ch - 32;
			cell->background = settings.colors[COLOR_BG];
			cell->foreground = settings.colors[COLOR_FG];
			cell->glyph_mode = GLYPH_MODE_NORMAL;

			if (has_highlights) {
				bool in_highlight = highlight.start <= highlight_cursor && highlight_cursor <= highlight.end;
				if (highlight_cursor == highlight.end) {
					highlight_index++;
					if (highlight_index < pane->highlights.length) {
						highlight = pane->highlights[highlight_index];
					} else {
						has_highlights = false;
					}
				}
				highlight_cursor++;
				if (in_highlight) {
					cell->foreground = settings.colors[highlight.color_index];
				}
			}

			if (ch == '\t') {
				render_x += settings.tab_width;
			} else {
				render_x++;
			}

			if (should_draw_cursor) {
				u32 cursor_buffer_rel_line_pos = buffer->cursor - prev_pos;
				if (cursor_buffer_rel_line_pos == i) {
					cell->glyph_mode = GLYPH_MODE_INVERT;
					has_drawn_cursor = true;
				}
			}
		}
		
		if (!has_drawn_cursor && pos == buffer->cursor) {
			draw_buffer->cells[render_x + render_y * draw_buffer->columns].glyph_mode = GLYPH_MODE_INVERT;
			has_drawn_cursor = true;
		}

		lines_drawn++;
	}

	if (is_active_pane && !has_drawn_cursor) {
		draw_buffer->cells[bounds.left + (bounds.top + lines_drawn) * draw_buffer->columns].glyph_mode = GLYPH_MODE_INVERT;
	}

	pane->end = cursor_get_end_of_line(buffer, pos);
}

void renderer_render_panes(Renderer *renderer) {
	DrawBuffer *draw_buffer = &renderer->buffer;

	memset(draw_buffer->cells, 0, draw_buffer->cells_size);

	for (u32 i = 0; i < pane_count; ++i) {
		Pane *pane = &pane_pool[i];
		pane_update_scroll(pane);
		renderer_render(renderer, pane);
	}
}

void renderer_end(GLFWwindow *window) {
	glClear(GL_COLOR_BUFFER_BIT);

	glDrawArrays(GL_TRIANGLES, 0, 6);
}

void render_settings_window(GLFWwindow *window) {
	ImGui::Begin("Settings");

	ImGui::ColorEdit3("Background color", settings.bg_temp, ImGuiColorEditFlags_NoInputs);
	ImGui::ColorEdit3("Foreground color", settings.fg_temp, ImGuiColorEditFlags_NoInputs);
	ImGui::Checkbox("Vsync", &settings.vsync);

	settings.colors[COLOR_BG] = color_hex_from_rgb(settings.bg_temp);
	settings.colors[COLOR_FG] = color_hex_from_rgb(settings.fg_temp);

	glfwSwapInterval(settings.vsync ? 1 : 0);

	ImGui::End();
}

int main() {
	create_default_keymaps();

	settings.colors[COLOR_BG] = 0x000000;
	settings.colors[COLOR_FG] = 0xFFFFFF;
	settings.colors[COLOR_KEYWORD] = 0xFF0000;

	settings.fg_temp[0] = settings.fg_temp[1] = settings.fg_temp[2] = 1.0f;
	settings.tab_width = 4;

	Buffer *command_buffer = buffer_create(32);
	command_buffer->mode = MODE_COMMAND;
	command_pane = pane_create(0, {0, 0, 0, 0}, command_buffer);

	current_buffer = buffer_create(32);
	current_buffer->file_path = 0;
	pane_create(0, {0, 0, 30, 20}, current_buffer);
	root_pane = active_pane;

	FT_Library ft;
	FT_Init_FreeType(&ft);

	Renderer renderer = {0};
	
	GLFWwindow *window = window_create(&renderer, 1280, 720);

#ifdef _WIN32
	glewInit();
#endif

	renderer_init(&renderer, window);
	renderer_init_glyph_map(ft, &renderer);
	draw_buffer_init(&renderer);

	glClearColor(0.0, 0.0, 0.0, 1.0);

	f64 prev_time = glfwGetTime();
	u32 frames = 0;
	while (!glfwWindowShouldClose(window) && global_running) {
		glfwPollEvents();

		f64 current_time = glfwGetTime();
		f64 delta = current_time - prev_time;

		frames++;
		if (delta >= 1) {
			char title[128];
			sprintf(title, "Shin: '%s' %d FPS, %f ms/f\n", current_buffer->file_path, frames, (delta * 1000)/frames);
			glfwSetWindowTitle(window, title);

			frames = 0;

			/* TODO: decide time interval, make setting */
			if (active_pane != command_pane) {
				highlighting_parse(active_pane);
			}

			prev_time = current_time;
		}
		
		glUniform1f(renderer.shader_time_slot, current_time);

		/* TODO: think about order of these calls */
		renderer_render_panes(&renderer);
		query_cell_data(&renderer);
		renderer_end(window);
		
		if (settings.show) {
			ImGui_ImplOpenGL3_NewFrame();
        	ImGui_ImplGlfw_NewFrame();
        	ImGui::NewFrame();

			render_settings_window(window);

			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		}

		glfwSwapBuffers(window);
	}

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
	glfwTerminate();
	return 0;
}
