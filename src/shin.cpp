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

#include "../extern/imgui/imgui.h"
#include "../extern/imgui/imgui_impl_glfw.h"
#include "../extern/imgui/imgui_impl_opengl3.h"

#define MAX_LINE_LENGTH 256

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

bool check_opengl_error() {
    bool found_error = false;
    int glErr = glGetError();
    while (glErr != GL_NO_ERROR) {
        printf("glError: %d\n", glErr);
		found_error = true;
        glErr = glGetError();
    }
    return found_error;
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



	InputEvent input_event;
	input_event.type = INPUT_EVENT_PRESSED;
	input_event.key_comb = kcomb;
	input_event.ch = (char) key;

	Editor *ed = (Editor *) glfwGetWindowUserPointer(window);
	ed->last_input_event = input_event;

	keymap_dispatch_event(ed);
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

	InputEvent input_event;
	input_event.type = INPUT_EVENT_PRESSED;
	input_event.key_comb = kcomb;
	input_event.ch = (char) key;

	Editor *ed = (Editor *) glfwGetWindowUserPointer(window);
	ed->last_input_event = input_event;

	keymap_dispatch_event(ed);
}

void draw_buffer_resize(Editor *ed);
void window_resize(GLFWwindow *window, s32 width, s32 height) {
	Editor *ed = (Editor *) glfwGetWindowUserPointer(window);
	draw_buffer_resize(ed);
}

GLFWwindow *window_create(Editor *ed, u32 width, u32 height) {
	glfwInit();

	glfwDefaultWindowHints();

	glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
#else
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
#endif

	GLFWwindow *window = glfwCreateWindow(width, height, "Shin", 0, 0);

	glfwSetWindowUserPointer(window, ed);

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
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
	const char* glsl_version = "#version 410 core";
    ImGui_ImplOpenGL3_Init(glsl_version);

	return window;
}

void draw_buffer_resize(Editor *ed) {
	Renderer *renderer = ed->renderer;
	GLFWwindow *window = renderer->window;
	GlyphMap *glyph_map = renderer->glyph_map;
	DrawBuffer *buffer = renderer->buffer;

	s32 width, height;
	glfwGetFramebufferSize(window, &width, &height);

	FontMetrics metrics = glyph_map->metrics;

	buffer->columns = floor((f32)width / metrics.glyph_width);
	buffer->rows = floor((f32)height / metrics.glyph_height);
	buffer->cells_size = sizeof(Cell) * buffer->columns * buffer->rows;
	buffer->cells = (Cell *) realloc(buffer->cells, buffer->cells_size);

	Bounds *bounds = &ed->pane_pool[ed->active_pane_index].bounds;
	bounds->left = 0;
	bounds->top = 0;
	bounds->width = buffer->columns;
	bounds->height = buffer->rows - 1;
}

void draw_buffer_init(DrawBuffer *buffer) {
	buffer->cells = (Cell *) malloc(buffer->cells_size);
	memset(buffer->cells, 0, buffer->cells_size);
}

void render_pane(Editor *ed, DrawBuffer *draw_buffer, Pane *pane, bool is_active_pane) {
	Buffer *buffer = pane->buffer;
	Bounds bounds = pane->bounds;
	Settings *settings = &ed->settings;

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
	snprintf(line_number_buffer, sizeof(line_number_buffer), "%d", pane->line_start + bounds.height - 1);
	u32 max_line_number_length = strlen(line_number_buffer);

	for (pos = start; pos < buffer_length(buffer) && lines_drawn < bounds.height - 1; pos = cursor_next(buffer, pos)) {
		u32 prev_pos = pos;

		u32 line_length = buffer_get_line(buffer, line, sizeof(line) - 1, &pos);
		line[line_length] = 0;

		u32 render_x = bounds.left + max_line_number_length + 1;
		u32 render_y = bounds.top + lines_drawn;

		// display line number 
		snprintf(line_number_buffer, sizeof(line_number_buffer), "%d", pane->line_start + lines_drawn + 1);
		u32 line_number_length = strlen(line_number_buffer);

		u32 line_number_offset = max_line_number_length - line_number_length;
		for (u32 j = 0; j < line_number_length; ++j) {
			Cell *cell = &draw_buffer->cells[line_number_offset + j + render_y * draw_buffer->columns];
			cell->glyph_index = line_number_buffer[j] - 32;
			cell->background = settings->colors[COLOR_BG];
			cell->foreground = settings->colors[COLOR_FG];
			cell->glyph_flags = 0;
		}

		// buffer rendering

		u32 draw_line_length = MIN(line_length, MIN(MAX_LINE_LENGTH, MIN(bounds.width, draw_buffer->columns)));

		for (u32 i = 0; i < draw_line_length; ++i) {
			if (i + max_line_number_length + 1 >= bounds.width) {
				break;
			}

			char ch = line[i];
			Cell *cell = &draw_buffer->cells[render_x + render_y * draw_buffer->columns];

			cell->glyph_index = ch - 32;
			cell->background = settings->colors[COLOR_BG];
			cell->foreground = settings->colors[COLOR_FG];
			cell->glyph_flags = 0;
			
			// highlights
			if (has_highlights) {
				highlight = pane->highlights[highlight_index];
				bool in_highlight = highlight.start <= render_cursor && render_cursor <= highlight.end;
				if (in_highlight) {
					cell->foreground = settings->colors[highlight.color_index];
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

				if (buffer->mode == MODE_VISUAL) {
					cell->background = settings->colors[COLOR_SELECTION];
				} else {
					cell->glyph_flags |= GLYPH_INVERT;
				}
 				has_drawn_cursor = true;
			}

			// render tab character as tab_width wide
			if (ch == '\t') {
				for (u32 k = 1; k < settings->tab_width; ++k) {
					draw_buffer->cells[render_x + k + render_y * draw_buffer->columns].background = cell->background;
				}
				render_x += settings->tab_width;
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
		cell->background = settings->colors[COLOR_BG];
		cell->foreground = settings->colors[COLOR_FG];
		cell->glyph_flags = GLYPH_INVERT;
	}
}

void render(Editor *ed, DrawBuffer *draw_buffer) {

	memset(draw_buffer->cells, 0, draw_buffer->cells_size);

	Pane *active_pane = &ed->pane_pool[ed->active_pane_index];
	pane_update_scroll(active_pane);
	highlighting_parse(active_pane);

	for (u32 i = 0; i < ed->pane_count; ++i) {
		Pane *pane = &ed->pane_pool[i];
		render_pane(ed, draw_buffer, pane, i == ed->active_pane_index);
	}

	// command textbox
	if (ed->current_buffer->mode == MODE_COMMAND) {
		u32 start = (draw_buffer->rows - 1) * draw_buffer->columns;
		for (u32 i = 0; i < MIN(command_get_cursor(), draw_buffer->columns); ++i) {
			Cell *cell = &draw_buffer->cells[start + i];

			cell->glyph_index = command_buffer_get(i) - 32;
			cell->background = ed->settings.colors[COLOR_BG];
			cell->foreground = ed->settings.colors[COLOR_FG];
			cell->glyph_flags = 0;
		}
		draw_buffer->cells[start + command_get_cursor()].glyph_flags |= GLYPH_INVERT;
	}
}

void render_settings_window(Editor *ed, HardwareRenderer *hwr, SoftwareRenderer *swr) {
	Settings *settings = &ed->settings;
	GLFWwindow *window = ed->renderer->window;

	ImGui::Begin("Settings");

	ImGui::ColorEdit3("Background color", settings->bg_temp, ImGuiColorEditFlags_NoInputs);
	ImGui::ColorEdit3("Foreground color", settings->fg_temp, ImGuiColorEditFlags_NoInputs);
	ImGui::ColorEdit3("Keyword color", settings->keyword_temp, ImGuiColorEditFlags_NoInputs);
	ImGui::ColorEdit3("Directive color", settings->directive_temp, ImGuiColorEditFlags_NoInputs);
	ImGui::ColorEdit3("Number color", settings->number_temp, ImGuiColorEditFlags_NoInputs);
	ImGui::ColorEdit3("String color", settings->string_temp, ImGuiColorEditFlags_NoInputs);
	ImGui::ColorEdit3("Type color", settings->type_temp, ImGuiColorEditFlags_NoInputs);
	ImGui::ColorEdit3("Comment color", settings->comment_temp, ImGuiColorEditFlags_NoInputs);
	ImGui::ColorEdit3("Selection color", settings->selection_temp, ImGuiColorEditFlags_NoInputs);
	ImGui::DragFloat("Opacity", &settings->opacity, 0.05f, 0.1f, 1.0f);
	ImGui::DragInt("Tab width", (s32 *) &settings->tab_width, 1, 1, 16);
	ImGui::DragInt("Font size", (s32 *) &settings->font_size, 1, 1, 60);
	ImGui::Checkbox("Vsync", &settings->vsync);
	ImGui::Checkbox("Hardware Rendering", &settings->hardware_rendering);

	settings->colors[COLOR_BG] = color_hex_from_rgb(settings->bg_temp);
	settings->colors[COLOR_FG] = color_hex_from_rgb(settings->fg_temp);
	settings->colors[COLOR_KEYWORD] = color_hex_from_rgb(settings->keyword_temp);
	settings->colors[COLOR_DIRECTIVE] = color_hex_from_rgb(settings->directive_temp);
	settings->colors[COLOR_NUMBER] = color_hex_from_rgb(settings->number_temp);
	settings->colors[COLOR_STRING] = color_hex_from_rgb(settings->string_temp);
	settings->colors[COLOR_TYPE] = color_hex_from_rgb(settings->type_temp);
	settings->colors[COLOR_COMMENT] = color_hex_from_rgb(settings->comment_temp);
	settings->colors[COLOR_SELECTION] = color_hex_from_rgb(settings->selection_temp);

	glfwSwapInterval(settings->vsync ? 1 : 0);
	glfwSetWindowOpacity(window, settings->opacity);

	#ifdef __APPLE__
		settings.hardware_rendering = false;
	#endif

	if (settings->hardware_rendering != settings->last_hardware_rendering) {
		s32 width, height;
		glfwGetFramebufferSize(window, &width, &height);

		if (settings->hardware_rendering) {
			swr->deinit();
			hwr->reinit(width, height);
			ed->renderer = hwr;
		} else {
			hwr->deinit();
			swr->reinit(width, height);
			ed->renderer = swr;
		}

		settings->last_hardware_rendering = settings->hardware_rendering;
	}

	ed->renderer->query_settings(settings);

	ImGui::End();
}

void set_default_settings(Settings *settings) {
	settings->colors[COLOR_BG] = 0x2A282A;
	settings->colors[COLOR_FG] = 0xd6b48b;
	settings->colors[COLOR_KEYWORD] = 0xffffff;
	settings->colors[COLOR_DIRECTIVE] = 0xffffff;
	settings->colors[COLOR_NUMBER] = 0x3bc4b9;
	settings->colors[COLOR_STRING] = 0xC0B8B7;
	settings->colors[COLOR_TYPE] = 0x8AC887;
	settings->colors[COLOR_COMMENT] = 0xE6E249;
	settings->colors[COLOR_SELECTION] = 0xf07a8e;

	settings->tab_width = 4;
	settings->font_size = 20;
	settings->opacity = 1.0f;
	
	settings->vsync = true;
	
#ifdef __APPLE__
	settings->hardware_rendering = false;
#else
	settings->hardware_rendering	= true;
#endif
}

void load_settings_file_or_set_default(Settings *settings) {
	FILE *f = fopen("config", "rb");
	if (f) {
		fread(settings->colors, sizeof(u32), COLOR_COUNT, f);
		fread(&settings->tab_width, sizeof(u32), 1, f);
		fread(&settings->font_size, sizeof(u32), 1, f);
		fread(&settings->opacity, sizeof(f32), 1, f);
		fread(&settings->vsync, sizeof(bool), 1, f);
		fread(&settings->hardware_rendering, sizeof(bool), 1, f);

		fclose(f);
	} else {
		set_default_settings(settings);
	}

	color_set_rgb_from_hex(settings->bg_temp, settings->colors[COLOR_BG]);
	color_set_rgb_from_hex(settings->fg_temp, settings->colors[COLOR_FG]);
	color_set_rgb_from_hex(settings->keyword_temp, settings->colors[COLOR_KEYWORD]);
	color_set_rgb_from_hex(settings->directive_temp, settings->colors[COLOR_DIRECTIVE]);
	color_set_rgb_from_hex(settings->number_temp, settings->colors[COLOR_NUMBER]);
	color_set_rgb_from_hex(settings->string_temp, settings->colors[COLOR_STRING]);
	color_set_rgb_from_hex(settings->type_temp, settings->colors[COLOR_TYPE]);
	color_set_rgb_from_hex(settings->comment_temp, settings->colors[COLOR_COMMENT]);
	color_set_rgb_from_hex(settings->selection_temp, settings->colors[COLOR_SELECTION]);

	settings->last_hardware_rendering = settings->hardware_rendering;
}

void save_settings(Settings *settings) {
	FILE *f = fopen("config", "wb");
	if (!f) {
		return;
	}

	fwrite(settings->colors, sizeof(u32), COLOR_COUNT, f);
	fwrite(&settings->tab_width, sizeof(u32), 1, f);
	fwrite(&settings->font_size, sizeof(u32), 1, f);
	fwrite(&settings->opacity, sizeof(f32), 1, f);
	fwrite(&settings->vsync, sizeof(bool), 1, f);
	fwrite(&settings->hardware_rendering, sizeof(bool), 1, f);

	fclose(f);
}

int main() {
	Editor editor =  {0};
	editor.active_pane_index = 0;
	editor.running = true;

	// load settings
	Settings *settings = &editor.settings;
	create_default_keymaps(&editor);
	load_settings_file_or_set_default(settings);

	// setup default pane and buffer
	pane_create(&editor, {0, 0, 30, 20});
	
	// create window
	GLFWwindow *window = window_create(&editor, 1280, 720);

#ifdef _WIN32
	glewInit();
#endif
	
	// setup glyphmap, draw_buffer and renderer
	GlyphMap *glyph_map;
	DrawBuffer draw_buffer;

	HardwareRenderer hardware_renderer;
	SoftwareRenderer software_renderer;

	if (settings->hardware_rendering) {
		editor.renderer = &hardware_renderer;
	} else {
		editor.renderer = &software_renderer;
	}

	glyph_map_init();
	glyph_map = glyph_map_create("resources/consolas.ttf", settings->font_size);
	draw_buffer_init(&draw_buffer);

	hardware_renderer.init(window, &draw_buffer, glyph_map);
	software_renderer.init(window, &draw_buffer, glyph_map);

	draw_buffer_resize(&editor);
	
	s32 width, height;
	glfwGetFramebufferSize(window, &width, &height);
	editor.renderer->reinit(width, height);
	
	editor.renderer->query_settings(settings);

	// main loop

	glClearColor(0.0, 0.0, 0.0, 1.0);

	f64 prev_time = glfwGetTime();
	u32 frames = 0;
	while (!glfwWindowShouldClose(window) && editor.running) {
		glfwPollEvents();

		f64 current_time = glfwGetTime();
		f64 delta = current_time - prev_time;

		frames++;
		if (delta >= 1) {
			char title[128];

			const char *render_mode = (settings->hardware_rendering) ? "GPU" : "CPU";
			
			sprintf(title, "Shin [%s]: '%s' %d FPS, %f ms/f\n", render_mode, editor.current_buffer->file_path, frames, (delta * 1000)/frames);
			glfwSetWindowTitle(window, title);

			frames = 0;

			prev_time = current_time;
		}
		
		Renderer *renderer = editor.renderer;
		renderer->update_time(current_time);

		render(&editor, renderer->buffer);

		renderer->query_cell_data();
		renderer->end();
		
		if (settings->show) {
			ImGui_ImplOpenGL3_NewFrame();
        	ImGui_ImplGlfw_NewFrame();
        	ImGui::NewFrame();

			render_settings_window(&editor, &hardware_renderer, &software_renderer);

			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		}

		glfwSwapBuffers(window);
	}

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
	glfwTerminate();

	save_settings(settings);

	return 0;
}
