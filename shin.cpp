#include <assert.h>
#include <stdint.h>

#include <Windows.h>
#include <d2d1.h>
#include <dwrite.h>

/* TODO: make platform abstraction */

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

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

struct Buffer {
	char *data;
	u32 cursor;
	u32 size;
	u32 gap_start;
	u32 gap_end;
};

typedef void (*CommandFunction)();

enum {
	MAX_KEY_COMBINATIONS = 1 << (8 + 3)
};

struct Command {
	const char *name;
	CommandFunction function;
};

struct Keymap {
	Command commands[MAX_KEY_COMBINATIONS];
};

struct InputEvent {
	u16 key_comb;
	bool pressed;
	bool released;
	char ch;
};

ID2D1Factory *d2d_factory;
IDWriteFactory *dwrite_factory;
ID2D1HwndRenderTarget *render_target;
IDWriteTextFormat *text_format;
ID2D1SolidColorBrush *text_brush;

Buffer *current_buffer;
Keymap *current_keymap;
InputEvent last_input_event;

void *shin_alloc(u32 size) { return HeapAlloc(GetProcessHeap(), 0, size);
}

void *shin_realloc(void *pointer, u32 size) {
	return HeapReAlloc(GetProcessHeap(), 0, pointer, size);
}

void shin_free(void *pointer) {
	HeapFree(GetProcessHeap(), 0, pointer);
}

void shin_printf(const char *fmt, ...) {
	char temp[1024];
	va_list arg_list;
	va_start(arg_list, fmt);
	wvsprintfA(temp, fmt, arg_list);
	va_end(arg_list);
	OutputDebugStringA(temp);
}
 
Buffer *buffer_create(u32 size) {
	Buffer *buffer = (Buffer *) shin_alloc(sizeof(Buffer));

	buffer->data = (char *) shin_alloc(size);
	buffer->cursor = 0;
	buffer->size = size;
	buffer->gap_start = 0;
	buffer->gap_end = size;

	return buffer;
}

void buffer_destroy(Buffer *buffer) {
	shin_free(buffer->data);
	shin_free(buffer);
}

INLINE u32 buffer_gap_size(Buffer *buffer) {
	return buffer->gap_end - buffer->gap_start;
}

INLINE u32 buffer_length(Buffer *buffer) {
	return buffer->size - buffer_gap_size(buffer);
}

INLINE u32 buffer_data_index(Buffer *buffer, u32 cursor) {
	return (cursor < buffer->gap_start) ? cursor : (cursor + buffer_gap_size(buffer));
}

INLINE char buffer_get_char(Buffer *buffer, u32 cursor) {
	return buffer->data[buffer_data_index(buffer, cursor)];
}

INLINE void buffer_set_char(Buffer *buffer, u32 cursor, char ch) {
	buffer->data[buffer_data_index(buffer, cursor)] = ch;
}

void buffer_asserts(Buffer *buffer) {
	assert(buffer->data);
	assert(buffer->gap_start <= buffer->gap_end);
	assert(buffer->gap_end <= buffer->size);
	assert(buffer->cursor <= buffer_length(buffer));
}

u32 buffer_move_position_forward(Buffer *buffer, u32 pos) {
	assert(pos != buffer->size);

	pos++;
	if (pos == buffer->gap_start) {
		pos = buffer->gap_end;
	}

	return pos;
}

u32 buffer_get_line(Buffer *buffer, char *line, u32 line_size, u32 *cursor) {
	u32 local_cursor = *cursor;

	u32 i;
	for (i = 0; i < line_size && local_cursor < buffer_length(buffer); ++i) {
		char ch = buffer_get_char(buffer, local_cursor);
		if (ch == '\n') {
			break;
		}

		line[i] = ch;
		local_cursor++;
	}

	while (local_cursor < buffer_length(buffer) && buffer_get_char(buffer, local_cursor) != '\n') {
		local_cursor++;
	}

	*cursor = local_cursor;
	return i;
}

void buffer_draw(Buffer *buffer, f32 line_height, f32 x, f32 y, f32 width, f32 height) {
	char utf8[64];
	WCHAR utf16[64];

	D2D1_RECT_F layout_rect;
	layout_rect.left = x;
	layout_rect.right = x + width;
	layout_rect.top = y;
	layout_rect.bottom = y + height;

	for (u32 pos = 0; pos < buffer_length(buffer); ++pos) {
		u32 line_length = buffer_get_line(buffer, utf8, sizeof(utf8) - 1, &pos);
		utf8[line_length] = 0;

		MultiByteToWideChar(CP_UTF8, 0, utf8, sizeof(utf8), utf16, sizeof(utf16) / sizeof(*utf16));
		render_target->DrawText(
			utf16,
			(u32) wcslen(utf16),
			text_format,
			layout_rect,
			text_brush
		);
		layout_rect.top += line_height;
	}
}

void buffer_shift_gap_to_position(Buffer *buffer, u32 pos) {
	if (pos < buffer->gap_start) {
		u32 gap_delta = buffer->gap_start - pos;
		buffer->gap_start -= gap_delta;
		buffer->gap_end -= gap_delta;
		MoveMemory(buffer->data + buffer->gap_end, buffer->data + buffer->gap_start, gap_delta);
	} else if (pos > buffer->gap_start) {
		u32 gap_delta = pos - buffer->gap_start;
		MoveMemory(buffer->data + buffer->gap_start, buffer->data + buffer->gap_end, gap_delta);
		buffer->gap_start += gap_delta;
		buffer->gap_end += gap_delta;
	}
	buffer_asserts(buffer);
}

void buffer_grow_if_needed(Buffer *buffer, u32 size_needed) {
	if (buffer_gap_size(buffer) < size_needed) {
		buffer_shift_gap_to_position(buffer, buffer_length(buffer));

		u32 new_size = MAX(buffer->size * 2, buffer->size + size_needed);

		buffer->data = (char *) shin_realloc(buffer->data, new_size);
		buffer->gap_end = new_size;
		buffer->size = new_size;
	}

	assert(buffer_gap_size(buffer) >= size_needed);
}

void buffer_insert(Buffer *buffer, u32 pos, char ch) {
	buffer_asserts(buffer);

	buffer_grow_if_needed(buffer, 64);
	buffer_shift_gap_to_position(buffer, pos);

	buffer->data[buffer->gap_start] = ch;
	buffer->gap_start++;

	if (buffer->cursor >= pos) {
		buffer->cursor++;
	}
}

bool buffer_replace(Buffer *buffer, u32 pos, char ch) {
	buffer_asserts(buffer);

	if (pos < buffer_length(buffer)) {
		buffer_set_char(buffer, pos, ch);
		return true;
	}

	return false;
}

bool buffer_delete_forwards(Buffer *buffer, u32 pos) {
	buffer_asserts(buffer);

	if (pos < buffer_length(buffer)) {
		buffer_shift_gap_to_position(buffer, pos);
		buffer->gap_end++;

		if (buffer->cursor > pos) {
			buffer->cursor--;
		}
		return true;
	}

	return false;
}

bool buffer_delete_backwards(Buffer *buffer, u32 pos) {
	buffer_asserts(buffer);

	if (pos > 0) {
		buffer_shift_gap_to_position(buffer, pos);
		buffer->gap_start--;
		if (buffer->cursor >= pos) {
			buffer->cursor--;
		}
		return true;
	}

	return false;
}

u32 cursor_next(Buffer *buffer, u32 cursor) {
	buffer_asserts(buffer);

	if (cursor < buffer_length(buffer)) {
		cursor++;
	}

	return cursor;
}

u32 cursor_back(Buffer *buffer, u32 cursor) {
	buffer_asserts(buffer);

	if (cursor > 0) {
		cursor--;
	}

	return cursor;
}

u32 cursor_get_beginning_of_line(Buffer *buffer, u32 cursor) {
	buffer_asserts(buffer);

	cursor = cursor_back(buffer, cursor);

	while (cursor > 0) {
		char ch = buffer_get_char(buffer, cursor);
		if (ch == '\n') {
			return cursor_next(buffer, cursor);
		}
		
		cursor = cursor_back(buffer, cursor);
	}

	return 0;
}

u32 cursor_get_end_of_line(Buffer *buffer, u32 cursor) {
	buffer_asserts(buffer);

	while (cursor < buffer_length(buffer)) {
		char ch = buffer_get_char(buffer, cursor);
		if (ch == '\n') {
			return cursor;
		}
		
		cursor = cursor_next(buffer, cursor);
	}

	return buffer_length(buffer);
}

void command_fn_null() {
	Beep(600, 300);
}

Command command_null = {"null", command_fn_null};

void command_fn_insert_char() {
	buffer_insert(current_buffer, current_buffer->cursor, last_input_event.ch);
}

Command command_insert_char = {"insert-char", command_fn_insert_char};

INLINE u16 get_key_combination(u8 key, u8 ctrl, u8 alt, u8 shift) {
	return (u16)key | ((u16)ctrl << 8) | ((u16)alt << 9) | ((u16)shift << 10);
}

Command *keymap_get_command(Keymap *keymap, u16 key_comb) {
	assert(key_comb < MAX_KEY_COMBINATIONS);
	return keymap->commands + key_comb;
}

Keymap *keymap_create_empty() {
	Keymap *keymap = (Keymap *) shin_alloc(sizeof(Keymap));

	for (u32 i = 0; i < MAX_KEY_COMBINATIONS; ++i) {
		keymap->commands[i] = command_null;
	}

	return keymap;
}

Keymap *keymap_create_default() {
	Keymap *keymap = keymap_create_empty();

	for (u32 i = ' '; i < '~'; ++i) {
		keymap->commands[i] = command_insert_char;
	}

	return keymap;
}

void keymap_dispatch_event(Keymap *keymap, InputEvent event) {
	if (event.pressed) {
		Command *command = keymap_get_command(keymap, event.key_comb);
		shin_printf("Command: %s\n", command->name);
		command->function();
	}
}

void window_render(HWND window) {
	RECT client_rectangle;
	GetClientRect(window, &client_rectangle);

	render_target->BeginDraw();
	render_target->Clear(D2D1::ColorF(D2D1::ColorF::Black));

	buffer_draw(
		current_buffer,
		30.0f,
		(f32) client_rectangle.left,
		(f32) client_rectangle.top,
		(f32) (client_rectangle.right - client_rectangle.left),
		(f32) (client_rectangle.bottom - client_rectangle.top)
	);

	render_target->EndDraw();
}

/* TODO: Cleanup windows api shit */
LRESULT CALLBACK ShinWindowProc(HWND window, UINT message_type, WPARAM wparam, LPARAM lparam) {
	LRESULT result = 0;
	HRESULT hresult = 0;

	switch (message_type) {
	case WM_SIZE: {
		RECT client_rectangle;
		GetClientRect(window, &client_rectangle);
		D2D1_SIZE_U window_size;
		window_size.width = client_rectangle.right - client_rectangle.left;
		window_size.height = client_rectangle.bottom - client_rectangle.top;

		if (render_target) {
			render_target->Release();
		}

		hresult = d2d_factory->CreateHwndRenderTarget(
			D2D1::RenderTargetProperties(),
			D2D1::HwndRenderTargetProperties(window, window_size),
			&render_target
		);

		if (text_brush) {
			text_brush->Release();
		}

		hresult = render_target->CreateSolidColorBrush(
			D2D1::ColorF(D2D1::ColorF::White),
			&text_brush
		);

		ValidateRect(window, 0);
		window_render(window);
	} break;
	case WM_PAINT: {
		window_render(window);
	} break;
	case WM_KEYDOWN: {
		Buffer *buffer = current_buffer;

		switch (wparam) {
			case VK_DELETE:
				buffer_delete_forwards(buffer, buffer->cursor);
				break;
			case VK_BACK:
				buffer_delete_backwards(buffer, buffer->cursor);
				break;
			case VK_LEFT:
				buffer->cursor = cursor_back(buffer, buffer->cursor);
				break;
			case VK_RIGHT:
				buffer->cursor = cursor_next(buffer, buffer->cursor);
				break;
			case VK_RETURN:
				buffer_insert(buffer, buffer->cursor, '\n');
				break;
			case VK_HOME:
				buffer->cursor = cursor_get_beginning_of_line(buffer, buffer->cursor);
				break;
			case VK_END:
				buffer->cursor = cursor_get_end_of_line(buffer, buffer->cursor);
				break;
		}

		window_render(window);
	} break;
	case WM_CHAR: {
		char ch;
		WideCharToMultiByte(CP_UTF8, 0, (WCHAR *)&wparam, 1, &ch, 1, 0, 0);
		if (' ' <= ch && ch <= '~') {
			Buffer *buffer = current_buffer;

			buffer_insert(buffer, buffer->cursor, ch);
			
			window_render(window);
		}
	} break;
	case WM_CLOSE:
		DestroyWindow(window);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		result = DefWindowProcA(window, message_type, wparam, lparam);
		break;
	}

	return result;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int cmd_show) {
	current_keymap = keymap_create_default();
	current_buffer = buffer_create(32);

	HRESULT hresult = 0;
	hresult = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2d_factory);
	hresult = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown **)&dwrite_factory);
	hresult = dwrite_factory->CreateTextFormat(
		L"Consolas",
		0,
		DWRITE_FONT_WEIGHT_REGULAR,
		DWRITE_FONT_STYLE_NORMAL,
		DWRITE_FONT_STRETCH_NORMAL,
		30.0f,
		L"en-us",
		&text_format
	);

	text_format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

	WNDCLASSA window_class = { 0 };
	window_class.style = CS_HREDRAW | CS_VREDRAW;
	window_class.hInstance = instance;
	window_class.lpfnWndProc = ShinWindowProc;
	window_class.lpszClassName = "shin";
	RegisterClassA(&window_class);

	HWND window = CreateWindowA(
		"shin",
		"shin",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		0, 0, instance, 0
	);

	ShowWindow(window, cmd_show);
	UpdateWindow(window);

	MSG message;
	while (GetMessage(&message, 0, 0, 0)) {
		TranslateMessage(&message);
		DispatchMessage(&message);
	}

	return 0;
}
