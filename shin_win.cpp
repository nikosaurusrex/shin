#include "shin.h"
#include "shin_platform.h"
#include "shin.cpp"
#include "commands.cpp"

#include <Windows.h>
#include <d2d1.h>
#include <dwrite.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

ID2D1Factory *d2d_factory;
IDWriteFactory *dwrite_factory;
ID2D1HwndRenderTarget *render_target;
IDWriteTextFormat *text_format;
ID2D1SolidColorBrush *text_brush;

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

void shin_printf(const char *fmt, ...) {
	char temp[1024];
	va_list arg_list;
	va_start(arg_list, fmt);
	wvsprintfA(temp, fmt, arg_list);
	va_end(arg_list);
	OutputDebugStringA(temp);
}

void shin_beep() {
	Beep(600, 300);
}

char shin_map_virtual_key(u32 key) {
	return MapVirtualKeyA(key, MAPVK_VK_TO_CHAR);
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
	/* TODO: really don't like this */
	static bool alt = false;
	static bool shift = false;
	static bool ctrl = false;

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
	case WM_SYSKEYDOWN:
	case WM_KEYDOWN: {
		switch (wparam) {
			case VK_CONTROL: ctrl = true; break;
			case VK_SHIFT: shift = true; break;
			case VK_MENU: alt = true; break;
			default: {
				last_input_event.type = INPUT_EVENT_PRESSED;
				last_input_event.key_comb = get_key_combination((u8) wparam, ctrl, alt, shift);

				BYTE keyboard_state[256];
				GetKeyboardState(keyboard_state);
				WORD characters;
				if (ToAscii((u8) wparam, (lparam >> 16) & 0xFF, keyboard_state, &characters, 0) == 1) {
					last_input_event.ch = (char) characters;
				} else {
					last_input_event.ch = 0;
				}

				keymap_dispatch_event(current_keymap, last_input_event);
				window_render(window);
			} break;
		}
	} break;
	case WM_SYSKEYUP:
	case WM_KEYUP: {
		switch (wparam) {
			case VK_CONTROL: ctrl = false; break;
			case VK_SHIFT: shift = false; break;
			case VK_MENU: alt = false; break;
			default: break;
		}
	} break;

#if 0
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
#endif
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
