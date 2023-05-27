#include "shin.h"
#include "shin_platform.h"
#include "shin.cpp"
#include "commands.cpp"

#include <math.h>

#include <Windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <Strsafe.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

static ID2D1Factory *d2d_factory;
static IDWriteFactory *dwrite_factory;
static ID2D1HwndRenderTarget *render_target;
static IDWriteTextFormat *text_format;
static ID2D1SolidColorBrush *text_brush;
static ID2D1SolidColorBrush *cursor_brush;

void *shin_alloc(u32 size) {
	return HeapAlloc(GetProcessHeap(), 0, size);
}

void *shin_realloc(void *pointer, u32 size) {
	return HeapReAlloc(GetProcessHeap(), 0, pointer, size);
}

void shin_free(void *pointer) {
	HeapFree(GetProcessHeap(), 0, pointer);
}

void shin_memmove(void *dest, void *src, u32 length) {
	MoveMemory(dest, src, length);
}

void shin_read_file_to_buffer(Buffer *buffer) {
	HANDLE handle = CreateFileA(
		buffer->file_path,
		GENERIC_READ,
		FILE_SHARE_READ,
		0, OPEN_ALWAYS,
		0, 0	
	);

	if (handle == INVALID_HANDLE_VALUE) return;

	u32 file_size = GetFileSize(handle, 0);

	buffer_clear(buffer);
	buffer_grow_if_needed(buffer, file_size);

	DWORD size_read;
	ReadFile(
		handle,
		buffer->data,
		file_size,
		&size_read, 
		0
	);

	CloseHandle(handle);

	buffer->gap_start = size_read;
}

void shin_write_buffer_to_file(Buffer *buffer) {
	HANDLE handle = CreateFileA(
		buffer->file_path,
		GENERIC_READ | GENERIC_WRITE,
		0, 0,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, 0
	);

	if (handle == INVALID_HANDLE_VALUE) return;

	WriteFile(handle, buffer->data, buffer->gap_start, 0, 0);
	WriteFile(handle, buffer->data + buffer->gap_end, buffer->size - buffer->gap_end, 0, 0);

	CloseHandle(handle);
}

void shin_printf(const char *fmt, ...) {
	char temp[1024];
	va_list arg_list;
	va_start(arg_list, fmt);
	StringCbVPrintfA(temp, sizeof(temp), fmt, arg_list);
	va_end(arg_list);
	OutputDebugStringA(temp);
}

void shin_beep() {
	Beep(600, 300);
}

void shin_exit() {
	PostQuitMessage(0);
}

char shin_map_virtual_key(u32 key) {
	return MapVirtualKeyA(key, MAPVK_VK_TO_CHAR);
}

void create_default_keymaps() {
	// insert keymap
	Keymap *keymap = keymap_create_empty();

	for (u32 key = 0; key < 256; ++key) {
		char ch = shin_map_virtual_key(key);
		if (' ' <= ch && ch <= '~') {
			keymap->commands[key] = command_insert_char;
			keymap->commands[key | SHIFT] = command_insert_char;
		}
	}
	keymap->commands[VK_RETURN]		= command_insert_new_line;
	keymap->commands[VK_DELETE]		= command_delete_forwards;
	keymap->commands[VK_BACK]  		= command_delete_backwards;
	keymap->commands[VK_LEFT]  		= command_cursor_back;
	keymap->commands[VK_RIGHT] 		= command_cursor_next;
	keymap->commands[VK_F4 | ALT]    = command_quit;
	keymap->commands['S' | CTRL]     = command_save_buffer;
	keymap->commands['R' | CTRL]     = command_reload_buffer;
	keymap->commands[VK_ESCAPE]     = command_normal_mode;

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
	keymap->commands[VK_F4 | ALT] = command_quit;
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

HRESULT change_font(const wchar_t *font_name, u32 font_height) {
	HRESULT result =  dwrite_factory->CreateTextFormat(
		font_name,
		0,
		DWRITE_FONT_WEIGHT_REGULAR,
		DWRITE_FONT_STYLE_NORMAL,
		DWRITE_FONT_STRETCH_NORMAL,
		(f32) font_height,
		L"en-us",
		&text_format
	);

	text_format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

	return result;
}

void adjust_panes_to_font() {
	IDWriteTextLayout *text_layout;
	dwrite_factory->CreateTextLayout(
		L"Mg", 2,
		text_format,
		200,
		200,
		&text_layout
	);

	DWRITE_TEXT_METRICS metrics = {0};
	text_layout->GetMetrics(&metrics);

	for (u32 i = 0; i < pane_count; ++i) {
		Bounds bounds = panes[i].bounds;
		f32 height = (f32) (bounds.bottom - bounds.top);
		panes[i].showable_lines = (u32) floorf(height / metrics.height);
	}
}


void pane_draw(Pane *pane) {
	Buffer *buffer = pane->buffer;

	pane_update_scroll(pane);

	const f32 border = 2.0f;
	char utf8[64];
	WCHAR utf16[64];

	u32 start = pane->start;

	D2D1_RECT_F layout_rect;
	layout_rect.left = (f32) pane->bounds.left;
	layout_rect.right = (f32) pane->bounds.right;
	layout_rect.top = (f32) pane->bounds.top;
	layout_rect.bottom = (f32) pane->bounds.bottom;

	if (pane == &panes[active_pane]) {
		render_target->DrawRectangle(layout_rect, text_brush, border);
	}

	layout_rect.left += border;
	layout_rect.right -= border;
	layout_rect.top += border;
	layout_rect.bottom -= border;

	render_target->PushAxisAlignedClip(layout_rect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

	u32 lines_drawn = 0;
	u32 pos;
	for (pos = start; pos < buffer_length(buffer) && layout_rect.top < layout_rect.bottom; pos = cursor_next(buffer, pos)) {
		u32 prev_pos = pos;

		u32 utf8_length = buffer_get_line(buffer, utf8, sizeof(utf8) - 1, &pos);
		utf8[utf8_length] = 0;

		u32 utf16_length = MultiByteToWideChar(CP_UTF8, 0, utf8, utf8_length, utf16, sizeof(utf16) / sizeof(*utf16));

		IDWriteTextLayout *text_layout;
		dwrite_factory->CreateTextLayout(
			utf16, utf16_length,
			text_format,
			layout_rect.right - layout_rect.left,
			layout_rect.bottom - layout_rect.top,
			&text_layout
		);

		if (pane == &panes[active_pane] && prev_pos <= buffer->cursor && buffer->cursor <= pos) {
			DWRITE_HIT_TEST_METRICS caret_metrics;
			f32 caret_x, caret_y;

			text_layout->HitTestTextPosition(buffer->cursor - prev_pos, 0, &caret_x, &caret_y, &caret_metrics);

			D2D1_RECT_F caret_rect;
			if (caret_metrics.left == 0) {
				caret_metrics.left = 1;	
			}
			caret_rect.left = layout_rect.left + caret_metrics.left;

			caret_rect.top = layout_rect.top + caret_metrics.top;
			caret_rect.bottom = layout_rect.top + caret_metrics.height;


			if (current_buffer->mode == MODE_NORMAL) {
				caret_rect.right = caret_rect.left + caret_metrics.width;
				render_target->FillRectangle(caret_rect, cursor_brush);
			} else {
				caret_rect.right = caret_rect.left;
			 	render_target->DrawRectangle(caret_rect, cursor_brush);
			}
		}

		render_target->DrawTextLayout(D2D1::Point2F(layout_rect.left, layout_rect.top), text_layout, text_brush);

		DWRITE_LINE_METRICS line_metrics;
		UINT32 line_count;
		text_layout->GetLineMetrics(&line_metrics, 1, &line_count);

		if (line_count == 1) {
			layout_rect.top += line_metrics.height;
			lines_drawn++;
		}

		text_layout->Release();

		if (lines_drawn >= pane->showable_lines) {
			break;
		}
	}

	pane->end = cursor_get_end_of_line(buffer, pos);
	render_target->PopAxisAlignedClip();
}

void window_render(HWND window) {
	RECT client_rectangle;
	GetClientRect(window, &client_rectangle);

	render_target->BeginDraw();
	render_target->Clear(D2D1::ColorF(D2D1::ColorF::Black));

	/* TODO: maybe in the future, dont redraw every pane? */
	for (u32 i = 0; i < pane_count; ++i) {
		pane_draw(&panes[i]);
	}

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

		Pane *ap = &panes[active_pane];
		ap->bounds.left = client_rectangle.left;
		ap->bounds.right = client_rectangle.right;
		ap->bounds.top = client_rectangle.top;
		ap->bounds.bottom = client_rectangle.bottom;

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

		if (cursor_brush) {
			cursor_brush->Release();
		}

		hresult = render_target->CreateSolidColorBrush(
			D2D1::ColorF(D2D1::ColorF::White),
			&text_brush
		);

		hresult = render_target->CreateSolidColorBrush(
			D2D1::ColorF(D2D1::ColorF::Pink),
			&cursor_brush
		);

		adjust_panes_to_font();

		ValidateRect(window, 0);
		window_render(window);
	} break;
	case WM_PAINT: {
		window_render(window);
	} break;
	case WM_SYSKEYDOWN:
	case WM_KEYDOWN: {
		if (GetKeyState(VK_CONTROL) >> 15) {
			wparam |= CTRL;
		}
		if (GetKeyState(VK_MENU) >> 15) {
			wparam |= ALT;
		}
		if (GetKeyState(VK_SHIFT) >> 15) {
			wparam |= SHIFT;
		}

		last_input_event.type = INPUT_EVENT_PRESSED;
		last_input_event.key_comb = (u16) wparam;

		BYTE keyboard_state[256];
		GetKeyboardState(keyboard_state);
		WORD characters;
		if (ToAscii((u8) wparam, (lparam >> 16) & 0xFF, keyboard_state, &characters, 0) == 1) {
			last_input_event.ch = (char) characters;
		} else {
			last_input_event.ch = 0;
		}

		Keymap *keymap;
		if (current_buffer->mode == MODE_NORMAL) {
			keymap = normal_keymap;
		} else {
			keymap = insert_keymap;
		}
		keymap_dispatch_event(keymap, last_input_event);
		window_render(window);
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
	create_default_keymaps();

	current_buffer = buffer_create(32);
	current_buffer->file_path = (char *) "test.txt";

	pane_create({0, 500, 0, 500}, current_buffer);

	HRESULT hresult = 0;
	hresult = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2d_factory);
	hresult = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown **)&dwrite_factory);

	hresult = change_font(L"Cascadia Code", 20);

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
