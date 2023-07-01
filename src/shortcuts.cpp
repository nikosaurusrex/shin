#include "shin.h"

#include <glfw/glfw3.h>

#define SHORTCUT(name) \
	void shortcut_fn_##name(Editor *ed); \
	Shortcut shortcut_##name = {#name, shortcut_fn_##name}; \
	void shortcut_fn_##name(Editor *ed)

#define MAX_NORMAL_LENGTH 16
static char normal_buffer[MAX_NORMAL_LENGTH];
static u32 normal_index = 0;

bool normal_mode_get_shortcut(Editor *ed, Shortcut *shortcut);

SHORTCUT(null) {
	
}

SHORTCUT(insert_char) {
	Buffer *buffer = ed->current_buffer;
	InputEvent input_event = ed->last_input_event;

	buffer_insert(buffer, buffer->cursor, input_event.ch);
}

SHORTCUT(delete_forwards) {
	Buffer *buffer = ed->current_buffer;
	buffer_delete_forwards(buffer, buffer->cursor);
}

SHORTCUT(delete_backwards) {
	Buffer *buffer = ed->current_buffer;
	buffer_delete_backwards(buffer, buffer->cursor);
}
	
SHORTCUT(cursor_back) {
	Buffer *buffer = ed->current_buffer;
	buffer->cursor = cursor_back(buffer, buffer->cursor);
}

SHORTCUT(cursor_next) {
	Buffer *buffer = ed->current_buffer;
	buffer->cursor = cursor_next(buffer, buffer->cursor);
}

SHORTCUT(insert_new_line) {
	Buffer *buffer = ed->current_buffer;
	buffer_insert(buffer, buffer->cursor, '\n');
}

SHORTCUT(insert_tab) {
	Buffer *buffer = ed->current_buffer;
	buffer_insert(buffer, buffer->cursor, '\t');
}

SHORTCUT(goto_beginning_of_line) {
	Buffer *buffer = ed->current_buffer;
	buffer->cursor = cursor_get_beginning_of_line(buffer, buffer->cursor);
	buffer->mode = MODE_INSERT;
}

SHORTCUT(goto_end_of_line) {
	Buffer *buffer = ed->current_buffer;
	buffer->cursor = cursor_get_end_of_line(buffer, buffer->cursor);
	buffer->mode = MODE_INSERT;
}

SHORTCUT(insert_mode) {
	Buffer *buffer = ed->current_buffer;
	buffer->mode = MODE_INSERT;
}

SHORTCUT(insert_mode_next) {
	Buffer *buffer = ed->current_buffer;
	buffer->mode = MODE_INSERT;
	buffer->cursor = cursor_next(buffer, buffer->cursor);
}

SHORTCUT(next_line) {
	Buffer *buffer = ed->current_buffer;
	u32 column = cursor_get_column(buffer, buffer->cursor);
	u32 beginning_of_next_line = cursor_get_beginning_of_next_line(buffer, buffer->cursor);
	u32 next_line_length = buffer_line_length(buffer, beginning_of_next_line);

	buffer_set_cursor(buffer, beginning_of_next_line + MIN(next_line_length, column));
}

SHORTCUT(prev_line) {
	Buffer *buffer = ed->current_buffer;
	u32 column = cursor_get_column(buffer, buffer->cursor);
	u32 beginning_of_prev_line = cursor_get_beginning_of_prev_line(buffer, buffer->cursor);
	u32 prev_line_length = buffer_line_length(buffer, beginning_of_prev_line);

	buffer_set_cursor(buffer, beginning_of_prev_line + MIN(prev_line_length, column));
}
 
SHORTCUT(go_word_next) {
	Buffer *buffer = ed->current_buffer;
	buffer->cursor = cursor_get_next_word(buffer, buffer->cursor);
}

SHORTCUT(go_word_end) {
	Buffer *buffer = ed->current_buffer;
	
	if (isspace(buffer_get_char(buffer, buffer->cursor))) {
		buffer->cursor = cursor_get_next_word(buffer, buffer->cursor);
	} else if (buffer->cursor + 1 < buffer_length(buffer) &&
			   isspace(buffer_get_char(buffer, buffer->cursor + 1))) {
		buffer->cursor = cursor_get_next_word(buffer, buffer->cursor);
	}  else {
		buffer->cursor = cursor_get_end_of_word(buffer, buffer->cursor);
	}
}	

SHORTCUT(go_word_prev) {
	Buffer *buffer = ed->current_buffer;

	if (isspace(buffer_get_char(buffer, buffer->cursor))) {
		buffer->cursor = cursor_get_prev_word(buffer, buffer->cursor);
	} else if (buffer->cursor - 1 >= 0 &&
			   isspace(buffer_get_char(buffer, buffer->cursor - 1))) {
		buffer->cursor = cursor_get_prev_word(buffer, buffer->cursor);
	}  else {
		buffer->cursor = cursor_get_beginning_of_word(buffer, buffer->cursor);
	}
}
	

SHORTCUT(goto_buffer_begin) {
	Buffer *buffer = ed->current_buffer;
	buffer->cursor = 0;
}

SHORTCUT(goto_buffer_end) {
	Buffer *buffer = ed->current_buffer;
	buffer->cursor = buffer_length(buffer);
}

SHORTCUT(new_line_before) {
	Buffer *buffer = ed->current_buffer;
	shortcut_fn_goto_beginning_of_line(ed);
	shortcut_fn_insert_new_line(ed);
	
	buffer->cursor = cursor_get_end_of_prev_line(buffer, buffer->cursor);
}

SHORTCUT(new_line_after) {
	Buffer *buffer = ed->current_buffer;
	shortcut_fn_goto_end_of_line(ed);
	shortcut_fn_insert_new_line(ed);
}

SHORTCUT(next_pane) {
	Buffer *buffer = ed->current_buffer;
	ed->active_pane_index++;
	if (ed->active_pane_index >= ed->pane_count) {
		ed->active_pane_index = 0;
	}

	ed->current_buffer = ed->pane_pool[ed->active_pane_index].buffer;
	ed->current_buffer->mode = MODE_NORMAL;
}

SHORTCUT(prev_pane) {
	Buffer *buffer = ed->current_buffer;
	ed->active_pane_index++;
	if (ed->active_pane_index >= ed->pane_count) {
		ed->active_pane_index = 0;
	}

	ed->current_buffer = ed->pane_pool[ed->active_pane_index].buffer;
	ed->current_buffer->mode = MODE_NORMAL;
}

SHORTCUT(split_vertically) {
	Buffer *buffer = ed->current_buffer;
	pane_split_vertically(ed);
	buffer->mode = MODE_NORMAL;
}

SHORTCUT(split_horizontally) {
	Buffer *buffer = ed->current_buffer;
	pane_split_horizontally(ed);
	buffer->mode = MODE_NORMAL;
}

SHORTCUT(normal_mode) {
	Buffer *buffer = ed->current_buffer;
	buffer->cursor_width = 0;
	buffer->cursor = cursor_back(buffer, buffer->cursor);
	buffer->mode = MODE_NORMAL;
	normal_index = 0;
	normal_buffer[normal_index] = 0;
}

SHORTCUT(normal_mode_clear) {
	normal_index = 0;
	normal_buffer[normal_index] = 0;
}

SHORTCUT(normal_insert) {
	if (normal_index + 2 >= MAX_NORMAL_LENGTH) {
		return;
	}

	InputEvent input_event = ed->last_input_event;
	char ch = input_event.ch;
	u16 key_comb = input_event.key_comb;

	if (key_comb & CTRL) {
		normal_buffer[normal_index++] = '^';
		normal_buffer[normal_index++] = ch;

	} else {
		normal_buffer[normal_index++] = ch;
	}

	normal_buffer[normal_index] = 0;

	Shortcut shortcut;
	bool exists = normal_mode_get_shortcut(ed, &shortcut);
	if (exists) {
		shortcut.function(ed);
		shortcut_fn_normal_mode_clear(ed);
	}
}

SHORTCUT(show_settings) {
	ed->settings.show = !ed->settings.show;
}

SHORTCUT(quit) {
	ed->running = false;
}

SHORTCUT(visual_mode) {
	Buffer *buffer = ed->current_buffer;
	buffer->mode = MODE_VISUAL;
	buffer->cursor_width = 0;
}

SHORTCUT(visual_mode_line) {
	Buffer *buffer = ed->current_buffer;

	buffer->mode = MODE_VISUAL;

	u32 end = cursor_get_end_of_line(buffer, buffer->cursor);
	buffer->cursor = cursor_get_beginning_of_line(buffer, buffer->cursor);
	buffer->cursor_width = end - buffer->cursor;
}

SHORTCUT(visual_next) {
	Buffer *buffer = ed->current_buffer;
	if (buffer->cursor + buffer->cursor_width < buffer_length(buffer)) {
		buffer->cursor_width++;
	}
}

SHORTCUT(visual_back) {
	Buffer *buffer = ed->current_buffer;
	if (buffer->cursor + buffer->cursor_width > 0) {
		buffer->cursor_width--;
	}
}

SHORTCUT(visual_next_line) {
	Buffer *buffer = ed->current_buffer;

	u32 cursor = (s32)(buffer->cursor) + buffer->cursor_width;
	u32 column = cursor_get_column(buffer, cursor);
	u32 end = cursor_get_end_of_next_line(buffer, cursor);

	buffer->cursor_width += end - cursor;
}

SHORTCUT(visual_prev_line) {
	Buffer *buffer = ed->current_buffer;

	u32 cursor = (s32)(buffer->cursor) + buffer->cursor_width;

	u32 column = cursor_get_column(buffer, cursor);
	u32 end = cursor_get_beginning_of_prev_line(buffer, cursor);

	buffer->cursor_width -= cursor - end;
}
 
SHORTCUT(visual_word_next) {
	Buffer *buffer = ed->current_buffer;
	u32 cursor = (s32)(buffer->cursor) + buffer->cursor_width;
	u32 end = cursor_get_next_word(buffer, cursor);

	buffer->cursor_width += end - cursor;
}

SHORTCUT(visual_word_end) {
	Buffer *buffer = ed->current_buffer;
	u32 cursor = (s32)(buffer->cursor) + buffer->cursor_width;
	
	u32 end;
	if (isspace(buffer_get_char(buffer, cursor))) {
		end = cursor_get_next_word(buffer, cursor);
	} else if (cursor + 1 < buffer_length(buffer) &&
			   isspace(buffer_get_char(buffer, cursor + 1))) {
		end = cursor_get_next_word(buffer, cursor);
	}  else {
		end = cursor_get_end_of_word(buffer, cursor);
	}
	
	buffer->cursor_width += end - cursor;
}	

SHORTCUT(visual_word_prev) {
	Buffer *buffer = ed->current_buffer;
	u32 cursor = (s32)(buffer->cursor) + buffer->cursor_width;

	u32 end;
	if (isspace(buffer_get_char(buffer, cursor))) {
		end = cursor_get_prev_word(buffer, cursor);
	} else if (cursor - 1 >= 0 &&
			   isspace(buffer_get_char(buffer, cursor - 1))) {
		end = cursor_get_prev_word(buffer, cursor);
	}  else {
		end = cursor_get_beginning_of_word(buffer, cursor);
	}
	
	buffer->cursor_width -= cursor - end;
}

SHORTCUT(visual_buffer_beginning) {
	Buffer *buffer = ed->current_buffer;
	u32 cursor = (s32)(buffer->cursor) + buffer->cursor_width;

	buffer->cursor_width -= cursor;
}

SHORTCUT(visual_buffer_end) {
	Buffer *buffer = ed->current_buffer;
	u32 cursor = (s32)(buffer->cursor) + buffer->cursor_width;

	u32 end = buffer_length(buffer);

	buffer->cursor_width += end - cursor;
}

SHORTCUT(visual_delete) {
	Buffer *buffer = ed->current_buffer;
	buffer_delete_multiple(buffer, buffer->cursor, buffer->cursor_width + 1);
	buffer->mode = MODE_NORMAL;
	buffer->cursor_width = 0;
}

SHORTCUT(command_begin) {
    command_begin(ed);
}

SHORTCUT(command_confirm) {
	command_confirm(ed);
}

SHORTCUT(command_exit) {
    command_exit(ed);
}

SHORTCUT(command_insert_char) {
	command_insert_char(ed->last_input_event);
}

SHORTCUT(command_delete) {
    command_delete();
}

Shortcut *keymap_get_shortcut(Keymap *keymap, u16 key_comb) {
	if (key_comb >= MAX_KEY_COMBINATIONS) {
		return keymap->shortcuts;
	}
	return keymap->shortcuts + key_comb;
}

Keymap *keymap_create_empty() {
	Keymap *keymap = (Keymap *) malloc(sizeof(Keymap));

	for (u32 i = 0; i < MAX_KEY_COMBINATIONS; ++i) {
		keymap->shortcuts[i] = shortcut_null;
	}

	return keymap;
}

void keymap_dispatch_event(Editor *ed) {
	InputEvent event = ed->last_input_event;
	Keymap *keymap = ed->keymaps[ed->current_buffer->mode];

	if (event.type == INPUT_EVENT_PRESSED) {
		Shortcut *shortcut = keymap_get_shortcut(keymap, event.key_comb);
		shortcut->function(ed);
	}
}

/* TODO: definetly rework this! */
bool normal_mode_get_shortcut(Editor *ed, Shortcut *shortcut) {
	Buffer *buffer = ed->current_buffer;

	switch (normal_buffer[0]) {
		// one letter shortcuts
		case 'x': *shortcut = shortcut_delete_forwards; break;
		case 'h': *shortcut = shortcut_cursor_back; break;
		case 'l': *shortcut = shortcut_cursor_next; break;
		case 'j': *shortcut = shortcut_next_line; break;
		case 'k': *shortcut = shortcut_prev_line; break;
		case 'w': *shortcut = shortcut_go_word_next; break;
		case 'e': *shortcut = shortcut_go_word_end; break;
		case 'b': *shortcut = shortcut_go_word_prev; break;
		case 'I': *shortcut = shortcut_goto_beginning_of_line; break;
		case 'A': *shortcut = shortcut_goto_end_of_line; break;
		case 'i': *shortcut = shortcut_insert_mode; break;
		case 'a': *shortcut = shortcut_insert_mode_next; break;
		case 'o': *shortcut = shortcut_new_line_after; break;
		case 'O': *shortcut = shortcut_new_line_before; break;
		case 'G': *shortcut = shortcut_goto_buffer_end; break;
		case 'v': *shortcut = shortcut_visual_mode; break;
		case 'V': *shortcut = shortcut_visual_mode_line; break;

		default: {
			
			// window operations
			if (strcmp(normal_buffer, "^Wv") == 0) { *shortcut = shortcut_split_vertically; break; }
			else if (strcmp(normal_buffer, "^W^V") == 0) { *shortcut = shortcut_split_vertically; break; }
			else if (strcmp(normal_buffer, "^Ws") == 0) { *shortcut = shortcut_split_horizontally; break; }
			else if (strcmp(normal_buffer, "^W^S") == 0) { *shortcut = shortcut_split_horizontally; break; }
			else if (strcmp(normal_buffer, "^Wl") == 0) { *shortcut = shortcut_next_pane; break; }
			else if (strcmp(normal_buffer, "^W^L") == 0) { *shortcut = shortcut_next_pane; break; }
			else if (strcmp(normal_buffer, "^Wh") == 0) { *shortcut = shortcut_prev_pane; break; }
			else if (strcmp(normal_buffer, "^W^H") == 0) { *shortcut = shortcut_prev_pane; break; }

			else {
				// two letter shortcuts
				char c1 = normal_buffer[0];
				if (normal_index == 2 && !isdigit(c1)) {
					char c2 = normal_buffer[1];

					if (c1 == 'g' && c2 == 'g') {
						shortcut_fn_goto_buffer_begin(ed);
						shortcut_fn_normal_mode_clear(ed);
						return false;
					} else if (c1 == 'd') {
						if (c2 == 'd') {
							u32 from = cursor_get_beginning_of_line(buffer, buffer->cursor);
							u32 to = cursor_get_beginning_of_next_line(buffer, buffer->cursor);
							
							buffer_delete_multiple(buffer, from, (to - from));

							shortcut_fn_normal_mode_clear(ed);
							return false;
						} else if (c2 == 'w') {
							u32 from = buffer->cursor;
							u32 to = cursor_get_next_word(buffer, buffer->cursor);

							buffer_delete_multiple(buffer, from, (to - from));

							shortcut_fn_normal_mode_clear(ed);
							return false;
						} else  {
							return false;
						}
					} else if (c1 == 'c' && c2 == 'w') {
						u32 from = buffer->cursor;
						u32 to = cursor_get_end_of_word(buffer, buffer->cursor);

						buffer_delete_multiple(buffer, from, (to - from) + 1);

						shortcut_fn_normal_mode_clear(ed);
						shortcut_fn_insert_mode(ed); 
						return false;
					} else {
						return false;
					}
				} else {
					// number followed by command

					if (isdigit(normal_buffer[0])) {
						u32 end = 0;

						while (isdigit(normal_buffer[end])) {
							end++;
						}

						char number_str[MAX_NUMBER_LENGTH];
						memcpy(number_str, normal_buffer, end);
						number_str[end] = 0;
						u32 number = atoi(number_str);

        				if (end < strlen(normal_buffer)) {
							char dir = normal_buffer[end];
							if (dir == 'j') {
								for (u32 i = 0; i < number; ++i) {
									shortcut_fn_next_line(ed);
								}
							} else if (dir == 'k') {
								for (u32 i = 0; i < number; ++i) {
									shortcut_fn_prev_line(ed);
								}
							}

							shortcut_fn_normal_mode_clear(ed);
						}
					}

					return false;
				}
			}
		}
	}

	return true;
}

void create_default_keymaps(Editor *ed) {
	// insert keymap
	Keymap *keymap = keymap_create_empty();

	for (char ch = ' '; ch <= '~'; ++ch) {
		if (ch == ':') continue;
		keymap->shortcuts[ch] = shortcut_insert_char;
		keymap->shortcuts[ch | SHIFT] = shortcut_insert_char;
	}
	keymap->shortcuts[GLFW_KEY_ENTER] = shortcut_insert_new_line;
	keymap->shortcuts[GLFW_KEY_TAB] = shortcut_insert_tab;
	keymap->shortcuts[GLFW_KEY_DELETE] = shortcut_delete_forwards;
	keymap->shortcuts[GLFW_KEY_BACKSPACE] = shortcut_delete_backwards;
	keymap->shortcuts[GLFW_KEY_BACKSPACE | SHIFT] = shortcut_delete_backwards;
	keymap->shortcuts[GLFW_KEY_LEFT] = shortcut_cursor_back;
	keymap->shortcuts[GLFW_KEY_RIGHT] = shortcut_cursor_next;
	keymap->shortcuts[GLFW_KEY_F4 | ALT] = shortcut_quit;
	keymap->shortcuts[GLFW_KEY_ESCAPE] = shortcut_normal_mode;

	ed->keymaps[MODE_INSERT] = keymap;
	
	// normal keymap
	keymap = keymap_create_empty();

	for (char ch = ' '; ch <= '~'; ++ch) {
		keymap->shortcuts[ch] = shortcut_normal_insert;
		keymap->shortcuts[ch | SHIFT] = shortcut_normal_insert;
		keymap->shortcuts[ch | CTRL] = shortcut_normal_insert;
	}
	keymap->shortcuts[GLFW_KEY_F4 | ALT] = shortcut_quit;
	keymap->shortcuts[GLFW_KEY_F3] = shortcut_show_settings;
	keymap->shortcuts[':' | SHIFT] = shortcut_command_begin;
	keymap->shortcuts[GLFW_KEY_ESCAPE] = shortcut_normal_mode_clear;

	ed->keymaps[MODE_NORMAL] = keymap;
	
	// command keymap
	keymap = keymap_create_empty();

	for (char ch = ' '; ch <= '~'; ++ch) {
		keymap->shortcuts[ch] = shortcut_command_insert_char;
		keymap->shortcuts[ch | SHIFT] = shortcut_command_insert_char;
	}
	keymap->shortcuts[GLFW_KEY_BACKSPACE] = shortcut_command_delete;
	keymap->shortcuts[GLFW_KEY_ENTER] = shortcut_command_confirm;
	keymap->shortcuts[GLFW_KEY_ESCAPE] = shortcut_command_exit;

	ed->keymaps[MODE_COMMAND] = keymap;

	// visual keymap
	keymap = keymap_create_empty();

	keymap->shortcuts['L'] = shortcut_visual_next;
	keymap->shortcuts['H'] = shortcut_visual_back;
	keymap->shortcuts['J'] = shortcut_visual_next_line;
	keymap->shortcuts['K'] = shortcut_visual_prev_line;
	keymap->shortcuts['W'] = shortcut_visual_word_next;
	keymap->shortcuts['E'] = shortcut_visual_word_end;
	keymap->shortcuts['B'] = shortcut_visual_word_prev;
	keymap->shortcuts['D'] = shortcut_visual_delete;
	keymap->shortcuts['G'] = shortcut_visual_buffer_beginning;
	keymap->shortcuts['G' | SHIFT] = shortcut_visual_buffer_end;
	keymap->shortcuts[GLFW_KEY_ESCAPE] = shortcut_normal_mode;

	ed->keymaps[MODE_VISUAL] = keymap;
}