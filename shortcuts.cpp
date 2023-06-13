#define SHORTCUT(name) \
	void shortcut_fn_##name(); \
	Shortcut shortcut_##name = {#name, shortcut_fn_##name}; \
	void shortcut_fn_##name()

#define CTRL  (1 << 8)
#define ALT   (1 << 9)
#define SHIFT (1 << 10)

#define MAX_NORMAL_LENGTH 16
static char normal_buffer[MAX_NORMAL_LENGTH];
static u32 normal_index = 0;

bool normal_mode_get_shortcut(Shortcut *shortcut);

SHORTCUT(null) {
	
}

SHORTCUT(insert_char) {
	buffer_insert(current_buffer, current_buffer->cursor, last_input_event.ch);
}

SHORTCUT(delete_forwards) {
	buffer_delete_forwards(current_buffer, current_buffer->cursor);
}

SHORTCUT(delete_backwards) {
	buffer_delete_backwards(current_buffer, current_buffer->cursor);
}
	
SHORTCUT(cursor_back) {
	current_buffer->cursor = cursor_back(current_buffer, current_buffer->cursor);
}

SHORTCUT(cursor_next) {
	current_buffer->cursor = cursor_next(current_buffer, current_buffer->cursor);
}

SHORTCUT(insert_new_line) {
	buffer_insert(current_buffer, current_buffer->cursor, '\n');
}

SHORTCUT(insert_tab) {
	buffer_insert(current_buffer, current_buffer->cursor, '\t');
}

SHORTCUT(goto_beginning_of_line) {
	current_buffer->cursor = cursor_get_beginning_of_line(current_buffer, current_buffer->cursor);
	current_buffer->mode = MODE_INSERT;
}

SHORTCUT(goto_end_of_line) {
	current_buffer->cursor = cursor_get_end_of_line(current_buffer, current_buffer->cursor);
	current_buffer->mode = MODE_INSERT;
}

SHORTCUT(insert_mode) {
	current_buffer->mode = MODE_INSERT;
}

SHORTCUT(insert_mode_next) {
	current_buffer->mode = MODE_INSERT;
	current_buffer->cursor = cursor_next(current_buffer, current_buffer->cursor);
}


SHORTCUT(next_line) {
	Buffer *buffer = current_buffer;
	u32 column = cursor_get_column(buffer, buffer->cursor);
	u32 beginning_of_next_line = cursor_get_beginning_of_next_line(buffer, buffer->cursor);
	u32 next_line_length = buffer_line_length(buffer, beginning_of_next_line);

	buffer_set_cursor(buffer, beginning_of_next_line + MIN(next_line_length, column));
}

SHORTCUT(prev_line) {
	Buffer *buffer = current_buffer;
	u32 column = cursor_get_column(buffer, buffer->cursor);
	u32 beginning_of_prev_line = cursor_get_beginning_of_prev_line(buffer, buffer->cursor);
	u32 prev_line_length = buffer_line_length(buffer, beginning_of_prev_line);

	buffer_set_cursor(buffer, beginning_of_prev_line + MIN(prev_line_length, column));
}
 
SHORTCUT(go_word_next) {
	Buffer *buffer = current_buffer;
	buffer->cursor = cursor_get_next_word(buffer, buffer->cursor);
}

SHORTCUT(go_word_end) {
	Buffer *buffer = current_buffer;
	
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
	Buffer *buffer = current_buffer;

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
	current_buffer->cursor = 0;
}

SHORTCUT(goto_buffer_end) {
	current_buffer->cursor = buffer_length(current_buffer);
}

SHORTCUT(new_line_before) {
	shortcut_fn_goto_beginning_of_line();
	shortcut_fn_insert_new_line();
	
	current_buffer->cursor = cursor_get_end_of_prev_line(current_buffer, current_buffer->cursor);
}

SHORTCUT(new_line_after) {
	shortcut_fn_goto_end_of_line();
	shortcut_fn_insert_new_line();
}

SHORTCUT(next_pane) {
	active_pane_index++;
	if (active_pane_index >= pane_count) {
		active_pane_index = 0;
	}

	current_buffer = pane_pool[active_pane_index].buffer;
	current_buffer->mode = MODE_NORMAL;
}

SHORTCUT(prev_pane) {
	active_pane_index++;
	if (active_pane_index >= pane_count) {
		active_pane_index = 0;
	}

	current_buffer = pane_pool[active_pane_index].buffer;
	current_buffer->mode = MODE_NORMAL;
}

SHORTCUT(split_vertically) {
	pane_split_vertically();
	current_buffer->mode = MODE_NORMAL;
}

SHORTCUT(split_horizontally) {
	pane_split_horizontally();
	current_buffer->mode = MODE_NORMAL;
}

SHORTCUT(normal_mode) {
	current_buffer->cursor_width = 0;
	current_buffer->mode = MODE_NORMAL;
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

	char ch = last_input_event.ch;
	u16 key_comb = last_input_event.key_comb;

	if (key_comb & CTRL) {
		normal_buffer[normal_index++] = '^';
		normal_buffer[normal_index++] = ch;

	} else {
		normal_buffer[normal_index++] = ch;
	}

	normal_buffer[normal_index] = 0;

	Shortcut shortcut;
	bool exists = normal_mode_get_shortcut(&shortcut);
	if (exists) {
		shortcut.function();
		shortcut_fn_normal_mode_clear();
	}
}

SHORTCUT(show_settings) {
	settings.show = !settings.show;
}

SHORTCUT(quit) {
	global_running = false;
}

SHORTCUT(visual_mode) {
	current_buffer->mode = MODE_VISUAL;
	current_buffer->cursor_width = 0;
}

SHORTCUT(visual_mode_line) {
	Buffer *buffer = current_buffer;

	buffer->mode = MODE_VISUAL;

	u32 end = cursor_get_end_of_line(buffer, buffer->cursor);
	buffer->cursor = cursor_get_beginning_of_line(buffer, buffer->cursor);
	buffer->cursor_width = end - buffer->cursor;
}

SHORTCUT(visual_next) {
	if (current_buffer->cursor + current_buffer->cursor_width < buffer_length(current_buffer)) {
		current_buffer->cursor_width++;
	}
}

SHORTCUT(visual_back) {
	if (current_buffer->cursor + current_buffer->cursor_width > 0) {
		current_buffer->cursor_width--;
	}
}

SHORTCUT(visual_next_line) {
	Buffer *buffer = current_buffer;

	u32 cursor = (s32)(buffer->cursor) + buffer->cursor_width;
	u32 column = cursor_get_column(buffer, cursor);
	u32 beginning_of_next_line = cursor_get_beginning_of_next_line(buffer, cursor);
	u32 next_line_length = buffer_line_length(buffer, beginning_of_next_line);

	u32 end = beginning_of_next_line + MIN(next_line_length, column);

	buffer->cursor_width += end - cursor;
}

SHORTCUT(visual_prev_line) {
	Buffer *buffer = current_buffer;

	u32 cursor = (s32)(buffer->cursor) + buffer->cursor_width;

	u32 column = cursor_get_column(buffer, cursor);
	u32 beginning_of_prev_line = cursor_get_beginning_of_prev_line(buffer, cursor);
	u32 prev_line_length = buffer_line_length(buffer, beginning_of_prev_line);

	u32 end = beginning_of_prev_line + MIN(prev_line_length, column);

	buffer->cursor_width -= cursor - end;
}
 
SHORTCUT(visual_word_next) {
	Buffer *buffer = current_buffer;
	u32 cursor = (s32)(buffer->cursor) + buffer->cursor_width;
	u32 end = cursor_get_next_word(buffer, cursor);

	buffer->cursor_width += end - cursor;
}

SHORTCUT(visual_word_end) {
	Buffer *buffer = current_buffer;
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
	Buffer *buffer = current_buffer;
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


SHORTCUT(visual_delete) {
	Buffer *buffer = current_buffer;
	buffer_delete_multiple(buffer, buffer->cursor, buffer->cursor_width + 1);
	buffer->mode = MODE_NORMAL;
	buffer->cursor_width = 0;
}

Shortcut *keymap_get_shortcut(Keymap *keymap, u16 key_comb) {
	assert(key_comb < MAX_KEY_COMBINATIONS);
	return keymap->shortcuts + key_comb;
}

Keymap *keymap_create_empty() {
	Keymap *keymap = (Keymap *) malloc(sizeof(Keymap));

	for (u32 i = 0; i < MAX_KEY_COMBINATIONS; ++i) {
		keymap->shortcuts[i] = shortcut_null;
	}

	return keymap;
}

void keymap_dispatch_event(Keymap *keymap, InputEvent event) {
	if (event.type == INPUT_EVENT_PRESSED) {
		Shortcut *shortcut = keymap_get_shortcut(keymap, event.key_comb);
		shortcut->function();
	}
}

/* TODO: definetly rework this! */
bool normal_mode_get_shortcut(Shortcut *shortcut) {
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
						shortcut_fn_goto_buffer_begin();
						shortcut_fn_normal_mode_clear();
						return false;
					} else if (c1 == 'd') {
						if (c2 == 'd') {
							u32 from = cursor_get_beginning_of_line(current_buffer, current_buffer->cursor);
							u32 to = cursor_get_beginning_of_next_line(current_buffer, current_buffer->cursor);
							
							buffer_delete_multiple(current_buffer, from, (to - from));

							shortcut_fn_normal_mode_clear();
							return false;
						} else if (c2 == 'w') {
							u32 from = current_buffer->cursor;
							u32 to = cursor_get_next_word(current_buffer, current_buffer->cursor);

							buffer_delete_multiple(current_buffer, from, (to - from));

							shortcut_fn_normal_mode_clear();
							return false;
						} else  {
							return false;
						}
					} else if (c1 == 'c' && c2 == 'w') {
						u32 from = current_buffer->cursor;
						u32 to = cursor_get_end_of_word(current_buffer, current_buffer->cursor);

						buffer_delete_multiple(current_buffer, from, (to - from) + 1);

						shortcut_fn_normal_mode_clear();
						shortcut_fn_insert_mode(); 
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
									shortcut_fn_next_line();
								}
							} else if (dir == 'k') {
								for (u32 i = 0; i < number; ++i) {
									shortcut_fn_prev_line();
								}
							}

							shortcut_fn_normal_mode_clear();
						}
					}

					return false;
				}
			}
		}
	}

	return true;
}