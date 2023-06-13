#include "shin.h"
#include "buffer.cpp"
#include "shortcuts.cpp"
#include "commands.cpp"
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

#define GLYPH_INVERT 0x1
#define GLYPH_BLINK 0x2

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
	u32 glyph_flags;
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
	GLint shader_cells_ssbo;
	GLint shader_cell_size_slot;
	GLint shader_grid_size_slot;
	GLint shader_win_size_slot;
	GLint shader_time_slot;
	GLint shader_background_color_slot;
};

char *read_entire_file(const char *file_path) {
	FILE *file = fopen(file_path, "rb");
	if (!file) {
		return 0;
	}

	fseek(file, 0, SEEK_END);
	size_t length = ftell(file);
	fseek(file, 0, SEEK_SET);

	char *contents = (char *) malloc(length + 1);
	fread(contents, 1, length, file);
	contents[length] = 0;

	return contents;
}

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

void color_set_rgb_from_hex(f32 rgb[3], u32 hex) {
	u32 r = (hex >> 16) & 0xFF;
	u32 g = (hex >> 8) & 0xFF;
	u32 b = hex & 0xFF;
	rgb[0] = r / 255.0f;
	rgb[1] = g / 255.0f;
	rgb[2] = b / 255.0f;
}

u32 color_invert(u32 c) {
	return 0xFFFFFFFF - c;
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

	u32 chars_count = ('~' - ' ') + 1;

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

void create_default_keymaps() {
	// insert keymap
	Keymap *keymap = keymap_create_empty();

	for (char ch = ' '; ch <= '~'; ++ch) {
		if (ch == ':') continue;
		keymap->shortcuts[ch] = shortcut_insert_char;
		keymap->shortcuts[ch | SHIFT] = shortcut_insert_char;
	}
	keymap->shortcuts[GLFW_KEY_ENTER] = shortcut_insert_new_line;
	keymap->shortcuts[GLFW_KEY_TAB] = shortcut_insert_tab;
	keymap->shortcuts[GLFW_KEY_DELETE] = shortcut_delete_forwards;
	keymap->shortcuts[GLFW_KEY_BACKSPACE] = shortcut_delete_backwards;
	keymap->shortcuts[GLFW_KEY_BACKSPACE | SHIFT] = shortcut_delete_backwards;
	keymap->shortcuts[GLFW_KEY_LEFT] = shortcut_cursor_back;
	keymap->shortcuts[GLFW_KEY_RIGHT] = shortcut_cursor_next;
	keymap->shortcuts[GLFW_KEY_F4 | ALT] = shortcut_quit;
	keymap->shortcuts[GLFW_KEY_ESCAPE] = shortcut_normal_mode;

	keymaps[MODE_INSERT] = keymap;
	
	// normal keymap
	keymap = keymap_create_empty();

	for (char ch = ' '; ch <= '~'; ++ch) {
		keymap->shortcuts[ch] = shortcut_normal_insert;
		keymap->shortcuts[ch | SHIFT] = shortcut_normal_insert;
		keymap->shortcuts[ch | CTRL] = shortcut_normal_insert;
	}
	keymap->shortcuts[GLFW_KEY_F4 | ALT] = shortcut_quit;
	keymap->shortcuts[GLFW_KEY_F3] = shortcut_show_settings;
	keymap->shortcuts[':' | SHIFT] = shortcut_begin_command;
	keymap->shortcuts[GLFW_KEY_ESCAPE] = shortcut_normal_mode_clear;

	keymaps[MODE_NORMAL] = keymap;
	
	// command keymap
	keymap = keymap_create_empty();

	for (char ch = ' '; ch <= '~'; ++ch) {
		keymap->shortcuts[ch] = shortcut_command_insert_char;
		keymap->shortcuts[ch | SHIFT] = shortcut_command_insert_char;
	}
	keymap->shortcuts[GLFW_KEY_BACKSPACE] = shortcut_command_delete;
	keymap->shortcuts[GLFW_KEY_ENTER] = shortcut_command_confirm;
	keymap->shortcuts[GLFW_KEY_ESCAPE] = shortcut_command_exit;

	keymaps[MODE_COMMAND] = keymap;

	// visual keymap
	keymap = keymap_create_empty();

	keymap->shortcuts['L'] = shortcut_visual_next;
	keymap->shortcuts['H'] = shortcut_visual_back;
	keymap->shortcuts['J'] = shortcut_visual_next_line;
	keymap->shortcuts['K'] = shortcut_visual_prev_line;
	keymap->shortcuts['W'] = shortcut_visual_word_next;
	keymap->shortcuts['E'] = shortcut_visual_word_end;
	keymap->shortcuts['B'] = shortcut_visual_word_prev;
	keymap->shortcuts['D'] = shortcut_visual_delete;
	keymap->shortcuts[GLFW_KEY_ESCAPE] = shortcut_normal_mode;

	keymaps[MODE_VISUAL] = keymap;
}


void key_callback(GLFWwindow* window, s32 key, s32 scancode, s32 action, s32 mods) {
	if (action != GLFW_PRESS && action != GLFW_REPEAT) {
		return;
	}

	if (!(mods & GLFW_MOD_CONTROL) || key >= GLFW_KEY_LEFT_BRACKET) {
		if (GLFW_KEY_SPACE <= key && key <= GLFW_KEY_GRAVE_ACCENT) {
			return;
		}
	}
	
	switch (key) {
		case GLFW_KEY_LEFT_CONTROL:
		case GLFW_KEY_RIGHT_CONTROL:
		case GLFW_KEY_LEFT_ALT:
		case GLFW_KEY_RIGHT_ALT:
		case GLFW_KEY_LEFT_SHIFT:
		case GLFW_KEY_RIGHT_SHIFT:
			return;
		default:
			break;
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

void draw_buffer_resize(Renderer *renderer);
void window_resize(GLFWwindow *window, s32 width, s32 height) {
	Renderer *renderer = (Renderer *) glfwGetWindowUserPointer(window);
	draw_buffer_resize(renderer);
}

GLFWwindow *window_create(Renderer *renderer, u32 width, u32 height) {
	glfwInit();

	glfwDefaultWindowHints();

	glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
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

	glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
	const char* glsl_version = "#version 430 core";
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

void create_glyph_texture(GLint uniform_slot) {
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

u32 create_cells_ssbo() {
	GLuint ssbo;

	glGenBuffers(1, &ssbo);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, ssbo);

	return ssbo;
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

bool checkOpenGLError() {
    bool found_error = false;
    int glErr = glGetError();
    while (glErr != GL_NO_ERROR) {
        printf("glError: %d\n", glErr);
        found_error = true;
        glErr = glGetError();
    }
    return found_error;
}

void query_cell_data(Renderer *renderer) {
	DrawBuffer *buffer = &renderer->buffer;

	checkOpenGLError();
	glBufferData(GL_SHADER_STORAGE_BUFFER, buffer->cells_size, buffer->cells, GL_DYNAMIC_DRAW);
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

	Bounds *bounds = &pane_pool[active_pane_index].bounds;
	bounds->left = 0;
	bounds->top = 0;
	bounds->width = buffer->columns;
	bounds->height = buffer->rows - 1;

	glUniform2ui(renderer->shader_cell_size_slot, metrics.glyph_width, metrics.glyph_height);
	glUniform2ui(renderer->shader_grid_size_slot, buffer->columns, buffer->rows);
	glUniform2ui(renderer->shader_win_size_slot, width, height);

	glUniform1ui(renderer->shader_background_color_slot, color_hex_from_rgb(settings.bg_temp));

	glfwSwapInterval(settings.vsync ? 1 : 0);
	glfwSetWindowOpacity(renderer->window, settings.opacity);
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

	create_glyph_texture(renderer->shader_glyph_map_slot);
	renderer->shader_cells_ssbo = create_cells_ssbo();

	glyph_map_update_texture(renderer->glyph_map);
	query_cell_data(renderer);
}

void renderer_render_pane(Renderer *renderer, Pane *pane, bool is_active_pane) {
	DrawBuffer *draw_buffer = &renderer->buffer;
	Buffer *buffer = pane->buffer;
	Bounds bounds = pane->bounds;

	u32 start = pane->start;
	u32 lines_drawn = 0;
	u32 pos;

	char line[MAX_LINE_LENGTH];
	bool has_drawn_cursor = false;

	u32 highlight_index = 0;
	Highlight highlight;
	bool has_highlights = highlight_index < pane->highlights.length;

	u32 render_cursor = pane->start;

	char line_number_buffer[MAX_NUMBER_LENGTH];
	itoa(pane->line_start + bounds.height - 1, line_number_buffer, 10);
	u32 max_line_number_length = strlen(line_number_buffer);

	for (pos = start; pos < buffer_length(buffer) && lines_drawn < bounds.height - 1; pos = cursor_next(buffer, pos)) {
		u32 prev_pos = pos;

		u32 line_length = buffer_get_line(buffer, line, sizeof(line) - 1, &pos);
		line[line_length] = 0;

		u32 draw_line_length = MIN(line_length, MIN(MAX_LINE_LENGTH, MIN(bounds.width, draw_buffer->columns)));

		u32 render_x = bounds.left + max_line_number_length + 1;
		u32 render_y = bounds.top + lines_drawn;

		// display line number 
		itoa(pane->line_start + lines_drawn + 1, line_number_buffer, 10);
		u32 line_number_length = strlen(line_number_buffer);

		u32 line_number_offset = max_line_number_length - line_number_length;
		for (u32 j = 0; j < line_number_length; ++j) {
			Cell *cell = &draw_buffer->cells[line_number_offset + j + render_y * draw_buffer->columns];
			cell->glyph_index = line_number_buffer[j] - 32;
			cell->background = settings.colors[COLOR_BG];
			cell->foreground = settings.colors[COLOR_FG];
			cell->glyph_flags = 0;
		}

		// buffer rendering

		for (u32 i = 0; i < draw_line_length; ++i) {
			char ch = line[i];
			Cell *cell = &draw_buffer->cells[render_x + render_y * draw_buffer->columns];

			cell->glyph_index = ch - 32;
			cell->background = settings.colors[COLOR_BG];
			cell->foreground = settings.colors[COLOR_FG];
			cell->glyph_flags = 0;
			
			// highlights
			if (has_highlights) {
				highlight = pane->highlights[highlight_index];
				bool in_highlight = highlight.start <= render_cursor && render_cursor <= highlight.end;
				if (in_highlight) {
					cell->foreground = settings.colors[highlight.color_index];
				}
				
				if (render_cursor == highlight.end) {
					highlight_index++;
					if (highlight_index < pane->highlights.length) {
						highlight = pane->highlights[highlight_index];
					} else {
						has_highlights = false;
					}
				}
			}

			// cursor / visual mode selection
			u32 render_cursor_start = MIN(buffer->cursor, buffer->cursor + buffer->cursor_width);
			u32 render_cursor_end = MAX(buffer->cursor, buffer->cursor + buffer->cursor_width);

			if (render_cursor >= render_cursor_start &&
				render_cursor <= render_cursor_end &&
				is_active_pane) {

				if (current_buffer->mode == MODE_VISUAL) {
					cell->background = settings.colors[COLOR_SELECTION];
				} else {
					cell->glyph_flags |= GLYPH_INVERT;
				}
 				has_drawn_cursor = true;
			}

			// render tab character as tab_width wide
			if (ch == '\t') {
				for (u32 k = 1; k < settings.tab_width; ++k) {
					draw_buffer->cells[render_x + k + render_y * draw_buffer->columns].background = cell->background;
				}
				render_x += settings.tab_width;
			} else {
				render_x++;
			}

			render_cursor++;
		}
		
		if (!has_drawn_cursor && pos == buffer->cursor && is_active_pane) {
			draw_buffer->cells[render_x + render_y * draw_buffer->columns].glyph_flags = GLYPH_INVERT;
			has_drawn_cursor = true;
		}

		lines_drawn++;
		render_cursor++;
	}

	if (is_active_pane && !has_drawn_cursor) {
		draw_buffer->cells[bounds.left + max_line_number_length + 1 +
						   (bounds.top + lines_drawn) * draw_buffer->columns].glyph_flags = GLYPH_INVERT;
	}

	pane->end = cursor_get_end_of_prev_line(buffer, pos);

	// render status
	/* TODO: maybe not call this! */
	const char *mode_string = "NORMAL";
	if (buffer->mode == MODE_INSERT) {
		mode_string = "INSERT";
	} else if (buffer->mode == MODE_VISUAL) {
		mode_string = "VISUAL";
	}
	snprintf(pane->status, MAX_STATUS_LENGTH, "%s %s", mode_string, buffer->file_path);

	u32 status_start = bounds.left + (bounds.top + bounds.height - 1) * draw_buffer->columns;
	u32 status_length = strlen(pane->status);
	for (u32 i = 0; i < MIN(draw_buffer->columns, bounds.width); ++i) {
		Cell *cell = &draw_buffer->cells[status_start + i];

		if (i < status_length) {
			cell->glyph_index = pane->status[i] - 32;
		}
		cell->background = settings.colors[COLOR_BG];
		cell->foreground = settings.colors[COLOR_FG];
		cell->glyph_flags = GLYPH_INVERT;
	}
}

void renderer_render(Renderer *renderer) {
	DrawBuffer *draw_buffer = &renderer->buffer;

	memset(draw_buffer->cells, 0, draw_buffer->cells_size);

	Pane *active_pane = &pane_pool[active_pane_index];
	pane_update_scroll(active_pane);
	highlighting_parse(active_pane);

	for (u32 i = 0; i < pane_count; ++i) {
		Pane *pane = &pane_pool[i];
		renderer_render_pane(renderer, pane, i == active_pane_index);
	}

	// command textbox
	if (current_buffer->mode == MODE_COMMAND) {
		u32 start = (draw_buffer->rows - 1) * draw_buffer->columns;
		for (u32 i = 0; i < MIN(command_cursor, draw_buffer->columns); ++i) {
			Cell *cell = &draw_buffer->cells[start + i];

			cell->glyph_index = command_buffer[i] - 32;
			cell->background = settings.colors[COLOR_BG];
			cell->foreground = settings.colors[COLOR_FG];
			cell->glyph_flags = 0;
		}
		draw_buffer->cells[start + command_cursor].glyph_flags |= GLYPH_INVERT;
	}
}

void renderer_end(GLFWwindow *window) {
	glClear(GL_COLOR_BUFFER_BIT);

	glDrawArrays(GL_TRIANGLES, 0, 6);
}

void render_settings_window(Renderer *renderer, GLFWwindow *window) {
	ImGui::Begin("Settings");

	ImGui::ColorEdit3("Background color", settings.bg_temp, ImGuiColorEditFlags_NoInputs);
	ImGui::ColorEdit3("Foreground color", settings.fg_temp, ImGuiColorEditFlags_NoInputs);
	ImGui::ColorEdit3("Keyword color", settings.keyword_temp, ImGuiColorEditFlags_NoInputs);
	ImGui::ColorEdit3("Directive color", settings.directive_temp, ImGuiColorEditFlags_NoInputs);
	ImGui::ColorEdit3("Number color", settings.number_temp, ImGuiColorEditFlags_NoInputs);
	ImGui::ColorEdit3("String color", settings.string_temp, ImGuiColorEditFlags_NoInputs);
	ImGui::ColorEdit3("Type color", settings.type_temp, ImGuiColorEditFlags_NoInputs);
	ImGui::ColorEdit3("Comment color", settings.comment_temp, ImGuiColorEditFlags_NoInputs);
	ImGui::ColorEdit3("Selection color", settings.selection_temp, ImGuiColorEditFlags_NoInputs);
	ImGui::Checkbox("Vsync", &settings.vsync);
	ImGui::DragFloat("Opacity", &settings.opacity, 0.05f, 0.1f, 1.0f);

	settings.colors[COLOR_BG] = color_hex_from_rgb(settings.bg_temp);
	settings.colors[COLOR_FG] = color_hex_from_rgb(settings.fg_temp);
	settings.colors[COLOR_KEYWORD] = color_hex_from_rgb(settings.keyword_temp);
	settings.colors[COLOR_DIRECTIVE] = color_hex_from_rgb(settings.directive_temp);
	settings.colors[COLOR_NUMBER] = color_hex_from_rgb(settings.number_temp);
	settings.colors[COLOR_STRING] = color_hex_from_rgb(settings.string_temp);
	settings.colors[COLOR_TYPE] = color_hex_from_rgb(settings.type_temp);
	settings.colors[COLOR_COMMENT] = color_hex_from_rgb(settings.comment_temp);
	settings.colors[COLOR_SELECTION] = color_hex_from_rgb(settings.selection_temp);

	glUniform1ui(renderer->shader_background_color_slot, color_hex_from_rgb(settings.bg_temp));

	glfwSwapInterval(settings.vsync ? 1 : 0);
	glfwSetWindowOpacity(window, settings.opacity);

	ImGui::End();
}

void set_default_settings() {
	settings.colors[COLOR_BG] = 0x2A282A;
	settings.colors[COLOR_FG] = 0xd6b48b;
	settings.colors[COLOR_KEYWORD] = 0xffffff;
	settings.colors[COLOR_DIRECTIVE] = 0xffffff;
	settings.colors[COLOR_NUMBER] = 0x3bc4b9;
	settings.colors[COLOR_STRING] = 0xC0B8B7;
	settings.colors[COLOR_TYPE] = 0x8AC887;
	settings.colors[COLOR_COMMENT] = 0xE6E249;
	settings.colors[COLOR_SELECTION] = 0xf07a8e;

	settings.vsync = true;
	settings.tab_width = 4;
	settings.opacity = 1.0f;
}

void load_settings_file_or_set_default() {
	FILE *f = fopen("config", "rb");
	if (f) {
		fread(settings.colors, sizeof(u32), COLOR_COUNT, f);
		fread(&settings.vsync, sizeof(bool), 1, f);
		fread(&settings.tab_width, sizeof(u32), 1, f);
		fread(&settings.opacity, sizeof(f32), 1, f);

		fclose(f);
	} else {
		set_default_settings();
	}

	color_set_rgb_from_hex(settings.bg_temp, settings.colors[COLOR_BG]);
	color_set_rgb_from_hex(settings.fg_temp, settings.colors[COLOR_FG]);
	color_set_rgb_from_hex(settings.keyword_temp, settings.colors[COLOR_KEYWORD]);
	color_set_rgb_from_hex(settings.directive_temp, settings.colors[COLOR_DIRECTIVE]);
	color_set_rgb_from_hex(settings.number_temp, settings.colors[COLOR_NUMBER]);
	color_set_rgb_from_hex(settings.string_temp, settings.colors[COLOR_STRING]);
	color_set_rgb_from_hex(settings.type_temp, settings.colors[COLOR_TYPE]);
	color_set_rgb_from_hex(settings.comment_temp, settings.colors[COLOR_COMMENT]);
	color_set_rgb_from_hex(settings.selection_temp, settings.colors[COLOR_SELECTION]);
}

void save_settings() {
	FILE *f = fopen("config", "wb");
	if (!f) {
		return;
	}

	fwrite(settings.colors, sizeof(u32), COLOR_COUNT, f);
	fwrite(&settings.vsync, sizeof(bool), 1, f);
	fwrite(&settings.tab_width, sizeof(u32), 1, f);
	fwrite(&settings.opacity, sizeof(f32), 1, f);

	fclose(f);
}

int main() {
	create_default_keymaps();
	load_settings_file_or_set_default();

	current_buffer = buffer_create(32);
	pane_create({0, 0, 30, 20}, current_buffer);

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

			prev_time = current_time;
		}
		
		glUniform1f(renderer.shader_time_slot, current_time);

		/* TODO: think about order of these calls */
		renderer_render(&renderer);
		query_cell_data(&renderer);
		renderer_end(window);
		
		if (settings.show) {
			ImGui_ImplOpenGL3_NewFrame();
        	ImGui_ImplGlfw_NewFrame();
        	ImGui::NewFrame();

			render_settings_window(&renderer, window);

			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		}

		glfwSwapBuffers(window);
	}

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
	glfwTerminate();

	save_settings();

	return 0;
}
