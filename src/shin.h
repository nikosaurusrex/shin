#include "common.h"

#define MAX_NUMBER_LENGTH 12
#define MAX_STATUS_LENGTH 256
#define MAX_PANES 12
#define MAX_KEY_COMBINATIONS (1 << (8 + 3))

#define CTRL  (1 << 8)
#define ALT   (1 << 9)
#define SHIFT (1 << 10)

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

typedef void (*ShortcutFunction)();
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
	bool vsync;
	u32 tab_width;
    f32 opacity;

	bool show;

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

// common functions
void read_file_to_buffer(Buffer *buffer);
void write_buffer_to_file(Buffer *buffer);

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
Pane *pane_create(Bounds bounds, Buffer *buffer);
void pane_update_scroll(Pane *pane);
void pane_split_vertically();
void pane_split_horizontally();

// keymap functions
Keymap *keymap_create_empty();
void keymap_dispatch_event(Keymap *keymap, InputEvent event);
void create_default_keymaps();

// commands functions
void begin_command();
void command_confirm();
void command_exit();
void command_insert_char();
void command_delete();

u32 command_get_cursor();
char command_buffer_get(u32 i);

// highlighting
void highlighting_parse(Pane *pane);

/* TODO: Clean up globals */
extern Buffer *current_buffer;
extern InputEvent last_input_event;

extern Pane pane_pool[MAX_PANES];
extern u32 pane_count;
extern u32 active_pane_index;

extern Keymap *keymaps[MODES_COUNT];

extern Settings settings;
extern bool global_running;
