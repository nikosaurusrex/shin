#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MIN(a,b) (a < b ? a : b)
#define MAX(a,b) (a > b ? a : b)

#ifdef _MSC_VER
#define INLINE __forceinline
#else
#define INLINE __attribute__((always_inline))
#endif

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef float f32;
typedef double f64;

typedef void (*CommandFunction)();

enum Mode {
	MODE_INSERT = 0,
	MODE_NORMAL = 1,
	MODE_WINDOW_OPERATION = 2,
	MODES_COUNT
};

struct Buffer {
	Mode mode;
	char *data;
	char *file_path;
	u32 cursor;
	u32 size;
	u32 gap_start;
	u32 gap_end;
};

enum {
	MAX_PANES = 16,
	MAX_KEY_COMBINATIONS = 1 << (8 + 3),
};

enum InputEventType {
	INPUT_EVENT_PRESSED,
	INPUT_EVENT_RELEASED
};

struct Command {
	const char *name;
	CommandFunction function;
};

struct Keymap {
	Command commands[MAX_KEY_COMBINATIONS];
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

struct Pane;
struct Pane {
	Bounds bounds;
	Buffer *buffer;
	u32 start;
	u32 end;

	Pane *parent;
	Pane *child;
};

struct Settings {
	u32 bg;
	u32 fg;
	bool vsync;

	bool show;

	/* TODO: maybe rework this later */
	f32 bg_temp[3];
	f32 fg_temp[3];
};

void read_file_to_buffer(Buffer *buffer);
void write_buffer_to_file(Buffer *buffer);
void shin_exit();

/* TODO: Clean up globals */
static Buffer *current_buffer;
static InputEvent last_input_event;

#define MAX_PANES 12
static Pane pane_pool[MAX_PANES];
static u32 pane_count = 0;
static Pane *active_pane;
static Pane *root_pane;

static Keymap *keymaps[MODES_COUNT];

static Settings settings;