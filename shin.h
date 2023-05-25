#include <assert.h>
#include <stdint.h>

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

struct Buffer {
	char *data;
	u32 cursor;
	u32 size;
	u32 gap_start;
	u32 gap_end;
};

enum {
	MAX_KEY_COMBINATIONS = 1 << (8 + 3)
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

/* TODO: Clean up globals */
Buffer *current_buffer;
Keymap *current_keymap;
InputEvent last_input_event;
