#define SHORTCUT(name) \
	void shortcut_fn_##name(); \
	Shortcut shortcut_##name = {#name, shortcut_fn_##name}; \
	void shortcut_fn_##name()

#define CTRL  (1 << 8)
#define ALT   (1 << 9)
#define SHIFT (1 << 10)

SHORTCUT(null) {
	// shin_beep();
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

SHORTCUT(insert_new_line) {
	buffer_insert(current_buffer, current_buffer->cursor, '\n');
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

SHORTCUT(normal_mode) {
	current_buffer->mode = MODE_NORMAL;
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
}

SHORTCUT(new_line_after) {
	shortcut_fn_goto_end_of_line();
	shortcut_fn_insert_new_line();
}

SHORTCUT(window_operation) {
	current_buffer->mode = MODE_WINDOW_OPERATION;
}

SHORTCUT(next_pane) {
	if (active_pane->child) {
		active_pane = active_pane->child;
	}

	current_buffer = active_pane->buffer;
	current_buffer->mode = MODE_NORMAL;
}

SHORTCUT(prev_pane) {
	if (active_pane->parent) {
		active_pane = active_pane->parent;
	}

	current_buffer = active_pane->buffer;
	current_buffer->mode = MODE_NORMAL;
}

SHORTCUT(split_vertically) {
	pane_split_vertically();
	active_pane = active_pane->child;
	current_buffer->mode = MODE_NORMAL;
}

SHORTCUT(split_horizontally) {
	pane_split_horizontally();
	active_pane = active_pane->child;
	current_buffer->mode = MODE_NORMAL;
}

SHORTCUT(show_settings) {
	settings.show = !settings.show;
}

SHORTCUT(begin_command) {
	command_pane->parent = active_pane;
	active_pane = command_pane;
	current_buffer = active_pane->buffer;
	buffer_insert(current_buffer, 0, ':');
}

SHORTCUT(command_confirm) {
	command_parse_and_run();
    buffer_clear(current_buffer);

	active_pane = command_pane->parent;
	current_buffer = active_pane->buffer;
}

SHORTCUT(quit) {
	global_running = false;
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
