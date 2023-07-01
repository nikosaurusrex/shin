#include "common.h"

#define MAX_NUMBER_LENGTH 12
#define MAX_STATUS_LENGTH 256
#define MAX_PANES 12
#define MAX_KEY_COMBINATIONS (1 << (8 + 3))

#define CTRL  (1 << 8)
#define ALT   (1 << 9)
#define SHIFT (1 << 10)

#define GLYPH_MAP_COUNT_X 32
#define GLYPH_MAP_COUNT_Y 16

#define GLYPH_INVERT 0x1
#define GLYPH_BLINK 0x2

enum Mode {
	MODE_INSERT = 0,
	MODE_NORMAL,
	MODE_COMMAND,
    MODE_VISUAL,
	MODES_COUNT
};

struct Buffer {
	Mode mode;
	char *data;
	char *file_path;

	u32 size;
	u32 gap_start;
	u32 gap_end;
    
	u32 cursor;
    s32 cursor_width;
};

enum InputEventType {
	INPUT_EVENT_PRESSED,
	INPUT_EVENT_RELEASED
};

struct Editor;
typedef void (*ShortcutFunction)(Editor *ed);
struct Shortcut {
	const char *name;
	ShortcutFunction function;
};

struct Keymap {
	Shortcut shortcuts[MAX_KEY_COMBINATIONS];
};

struct InputEvent {
	InputEventType type;
	u16 key_comb;
	char ch;
};

struct Bounds {
	u32 left;
	u32 top;
	u32 width;
	u32 height;
};

struct Highlight {
    u32 start;
    u32 end;
    u32 color_index;
};

struct Pane;
struct Pane {
    char status[MAX_STATUS_LENGTH];
	Array<Highlight> highlights;

	Bounds bounds;
	Buffer *buffer;

	u32 start;
	u32 end;
    u32 line_start;
};

enum ColorPalette : u32 {
	COLOR_BG = 0,	
	COLOR_FG,
	COLOR_KEYWORD,
	COLOR_DIRECTIVE,
	COLOR_NUMBER,
	COLOR_STRING,
	COLOR_TYPE,
	COLOR_COMMENT,
    COLOR_SELECTION,
	COLOR_COUNT
};

struct Settings {
	u32 colors[COLOR_COUNT];
	u32 tab_width;
	u32 font_size;
    f32 opacity;
	bool vsync;
	bool hardware_rendering;

	bool show;
	bool last_hardware_rendering;

	/* TODO: maybe rework this later */
	f32 bg_temp[3];
	f32 fg_temp[3];
	f32 keyword_temp[3];
	f32 directive_temp[3];
	f32 number_temp[3];
	f32 string_temp[3];
	f32 type_temp[3];
	f32 comment_temp[3];
	f32 selection_temp[3];
};

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

struct GLFWwindow;
struct Renderer {
	GLFWwindow *window;
	DrawBuffer *buffer;
	GlyphMap *glyph_map;

	void init(GLFWwindow *window, DrawBuffer *draw_buffer, GlyphMap *gylph_map);
	virtual void reinit(s32 width, s32 height) = 0;
	virtual void deinit() = 0;
	virtual void resize(s32 width, s32 height) = 0;
	virtual void end() = 0;
	virtual void query_cell_data() = 0;
	virtual void query_settings(Settings *settings) = 0;
	virtual void update_time(f64 time) = 0;
};

struct HardwareRenderer : Renderer {
	u32 vao;
	u32 vbo;
	u32 program;
	u32 glyph_texture;

	s32 shader_glyph_map_slot;
	s32 shader_cells_slot;
	u32 shader_cells_ssbo;
	s32 shader_cell_size_slot;
	s32 shader_grid_size_slot;
	s32 shader_win_size_slot;
	s32 shader_time_slot;
	s32 shader_background_color_slot;

	void reinit(s32 width, s32 height);
	void deinit();
	void resize(s32 width, s32 height);
	void end();
	void query_cell_data();
	void query_settings(Settings *settings);
	void update_time(f64 time);
};

struct SoftwareRenderer : Renderer {
	volatile u32 *screen = 0;
	s32 width;
	s32 height;
	u32 bg_color;

	u32 vao;
	u32 vbo;
	u32 vto;
	u32 program;
	u32 texture;

	void reinit(s32 width, s32 height);
	void deinit();
	void resize(s32 width, s32 height);
	void end();
	void query_cell_data();
	void query_settings(Settings *settings);
	void update_time(f64 time);

	void render_cell(Cell *cell, u32 column, u32 row);
};

struct Editor {
	Renderer *renderer;
	Buffer *current_buffer;
	InputEvent last_input_event;

	Pane pane_pool[MAX_PANES];
	u32 pane_count;
	u32 active_pane_index;

	Keymap *keymaps[MODES_COUNT];

	Settings settings;
	bool running;
};

// common functions
void read_file_to_buffer(Buffer *buffer);
void write_buffer_to_file(Buffer *buffer);
char *read_entire_file(const char *file_path);
u32 color_hex_from_rgb(f32 rgb[3]);
void color_set_rgb_from_hex(f32 rgb[3], u32 hex);
u32 color_invert(u32 c);

#define CHECK_OPENGL_ERROR() if (check_opengl_error()) printf("at %s:%d\n", __FILE__, __LINE__);
bool check_opengl_error();

// buffer functions
Buffer *buffer_create(u32 size);
void buffer_delete_forwards(Buffer *buffer, u32 pos);
void buffer_delete_backwards(Buffer *buffer, u32 pos);
void buffer_delete_multiple(Buffer *buffer, u32 pos, u32 count);
void buffer_insert(Buffer *buffer, u32 pos, char ch);
u32 buffer_length(Buffer *buffer);
char buffer_get_char(Buffer *buffer, u32 cursor);
u32 buffer_get_line(Buffer *buffer, char *line, u32 line_size, u32 *cursor);
void buffer_grow_if_needed(Buffer *buffer, u32 size_needed);
void buffer_clear(Buffer *buffer);
u32 buffer_line_length(Buffer *buffer, u32 cursor);
void buffer_set_cursor(Buffer *buffer, u32 cursor);
void buffer_goto_beginning(Buffer *buffer);
void buffer_goto_next_line(Buffer *buffer);

// cursor functions
u32 cursor_next(Buffer *buffer, u32 cursor);
u32 cursor_back(Buffer *buffer, u32 cursor);
u32 cursor_get_beginning_of_line(Buffer *buffer, u32 cursor);
u32 cursor_get_end_of_line(Buffer *buffer, u32 cursor);
u32 cursor_get_beginning_of_next_line(Buffer *buffer, u32 cursor);
u32 cursor_get_beginning_of_prev_line(Buffer *buffer, u32 cursor);
u32 cursor_get_end_of_prev_line(Buffer *buffer, u32 cursor);
u32 cursor_get_end_of_next_line(Buffer *buffer, u32 cursor);
u32 cursor_get_column(Buffer *buffer, u32 cursor);
u32 cursor_get_beginning_of_word(Buffer *buffer, u32 cursor);
u32 cursor_get_end_of_word(Buffer *buffer, u32 cursor);
u32 cursor_get_next_word(Buffer *buffer, u32 cursor);
u32 cursor_get_prev_word(Buffer *buffer, u32 cursor);

// pane functions
Pane *pane_create(Editor *ed, Bounds bounds);
void pane_update_scroll(Pane *pane);
void pane_split_vertically(Editor *ed);
void pane_split_horizontally(Editor *ed);

// keymap functions
Keymap *keymap_create_empty();
void keymap_dispatch_event(Editor *ed);
void create_default_keymaps(Editor *ed);

// commands functions
void command_begin(Editor *ed);
void command_confirm(Editor *ed);
void command_exit(Editor *ed);
void command_insert_char(InputEvent input_event);
void command_delete();
u32 command_get_cursor();
char command_buffer_get(u32 i);

// highlighting functions
void highlighting_parse(Pane *pane);

// glyph map functions
void glyph_map_init();
GlyphMap *glyph_map_create(const char *font, u32 pixel_size);