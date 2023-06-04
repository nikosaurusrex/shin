#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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

template<typename T>
struct Array {
    T *data = 0;
    s64 length = 0;
    s64 allocated = 0;
    
    const int NEW_MEM_CHUNK_ELEMENT_COUNT =  16;
    
    Array(s64 reserve_amount = 0) {
        reserve(reserve_amount);
    }
    
    ~Array() {
        reset();
    }
    
    void reserve(s64 amount) {
        if (amount <= 0) amount = NEW_MEM_CHUNK_ELEMENT_COUNT;
        if (amount <= allocated) return;
        
        T *new_mem = (T *)malloc(amount * sizeof(T));
        
        if (data) {
            memcpy(new_mem, data, length * sizeof(T));
            free(data);
        }
        
        data = new_mem;
        allocated = amount;
    }
    
    void resize(s64 amount) {
        reserve(amount);
        length = amount;
    }
    
    void add(T element) {
        if (length+1 >= allocated) reserve(allocated * 2);
        
        data[length] = element;
        length += 1;
    }
    
    T unordered_remove(s64 index) {
        assert(index >= 0 && index < length);
        assert(length);
        
        T last = pop();
        if (index < length) {
            (*this)[index] = last;
        }
        
        return last;
    }

    T ordered_remove(s64 index) {
        assert(index >= 0 && index < length);
        assert(length);

        T item = (*this)[index];
        memmove(data + index, data + index + 1, ((length - index) - 1) * sizeof(T));

        length--;
        return item;
    }
    
    T pop() {
        assert(length > 0);
        T result = data[length-1];
        length -= 1;
        return result;
    }
    
    void clear() {
        length = 0;
    }
    
    void reset() {
        length = 0;
        allocated = 0;
        
        if (data) free(data);
        data = 0;
    }
    
    T &operator[] (s64 index) {
        assert(index >= 0 && index < length);
        return data[index];
    }
    
    T *begin() {
        return &data[0];
    }
    
    T *end() {
        return &data[length];
    }
};

enum Mode {
	MODE_INSERT = 0,
	MODE_NORMAL = 1,
	MODE_WINDOW_OPERATION = 2,
	MODE_COMMAND = 3,
    MODE_MULTIKEY_SHORTCUT,
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
	Array<Highlight> highlights;
	Bounds bounds;
	Buffer *buffer;
	u32 start;
	u32 end;

	Pane *parent;
	Pane *child;
};

enum ColorPalette : u32 {
	COLOR_BG = 0,	
	COLOR_FG,
	COLOR_KEYWORD,
	COLOR_DIRECTIVE,
	COLOR_NUMBER,
	COLOR_STRING,
	COLOR_COUNT
};

struct Settings {
	u32 colors[COLOR_COUNT];
	bool vsync;
	u32 tab_width;

	bool show;

	/* TODO: maybe rework this later */
	f32 bg_temp[3];
	f32 fg_temp[3];
};

void read_file_to_buffer(Buffer *buffer);
void write_buffer_to_file(Buffer *buffer);

/* TODO: Clean up globals */
static Buffer *current_buffer;
static InputEvent last_input_event;

#define MAX_PANES 12
static Pane pane_pool[MAX_PANES];
static u32 pane_count = 0;
static Pane *active_pane;
static Pane *root_pane;
static Pane *command_pane;

static Keymap *keymaps[MODES_COUNT];

static Settings settings;
static bool global_running = true;
