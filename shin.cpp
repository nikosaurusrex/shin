#include "shin.h"
#include "buffer.cpp"
#include "commands.cpp"

#define GLEW_STATIC
#include <glew/glew.h>
#include <glfw/glfw3.h>
#include <freetype/freetype.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

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

void read_file_to_buffer(Buffer *buffer) {
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
	FILE *file = fopen(buffer->file_path, "w");

	if (!file) return;

	fwrite(buffer->data, 1, buffer->gap_start, file);
	fwrite(buffer->data + buffer->gap_end, 1, buffer->size - buffer->gap_end, file);

	fclose(file);
}

void shin_exit() {
	/* TODO: exit */
	// PostQuitMessage(0);
}

void create_default_keymaps() {
	// insert keymap
	Keymap *keymap = keymap_create_empty();

	for (u32 key = 0; key < 256; ++key) {
		/* TODO: rework */
		// char ch = shin_map_virtual_key(key);
		char ch = key;
		if (' ' <= ch && ch <= '~') {
			keymap->commands[key] = command_insert_char;
			keymap->commands[key | SHIFT] = command_insert_char;
		}
	}
	/* TODO: Change keybinds */
#if 0
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
#endif
}

#if 0
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
#endif

FontMetrics font_metrics_get(FT_Face face) {
	FontMetrics metrics = {0};

	FT_Load_Char(face, 'M', FT_RENDER_MODE_NORMAL);
	metrics.descender = -face->size->metrics.descender >> 6;
	metrics.glyph_width = face->glyph->advance.x >> 6;
	metrics.glyph_height = (face->size->metrics.ascender >> 6) + metrics.descender;

	return metrics;
}

#define GLYPH_MAP_COUNT_X 32
#define GLYPH_MAP_COUNT_Y 16
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

	u32 chars_count = '~' - ' ';

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

int main() {
	FT_Library ft;
	FT_Init_FreeType(&ft);

	GlyphMap *map = glyph_map_create(ft, "Consolas.ttf", 22);

	stbi_write_png("out.png", map->width, map->height, 1, map->data, map->width);

	return 0;
}
