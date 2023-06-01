#define COMMAND(name) \
	void command_fn_##name(); \
	Command command_##name = {#name, command_fn_##name}; \
	void command_fn_##name()

#define CTRL  (1 << 8)
#define ALT   (1 << 9)
#define SHIFT (1 << 10)

COMMAND(null) {
	// shin_beep();
}

COMMAND(insert_char) {
	buffer_insert(current_buffer, current_buffer->cursor, last_input_event.ch);
}

COMMAND(delete_forwards) {
	buffer_delete_forwards(current_buffer, current_buffer->cursor);
}

COMMAND(delete_backwards) {
	buffer_delete_backwards(current_buffer, current_buffer->cursor);
}

COMMAND(cursor_back) {
	current_buffer->cursor = cursor_back(current_buffer, current_buffer->cursor);
}

COMMAND(cursor_next) {
	current_buffer->cursor = cursor_next(current_buffer, current_buffer->cursor);
}

COMMAND(next_line) {
	Buffer *buffer = current_buffer;
	u32 column = cursor_get_column(buffer, buffer->cursor);
	u32 beginning_of_next_line = cursor_get_beginning_of_next_line(buffer, buffer->cursor);
	u32 next_line_length = buffer_line_length(buffer, beginning_of_next_line);

	buffer_set_cursor(buffer, beginning_of_next_line + MIN(next_line_length, column));
}

COMMAND(prev_line) {
	Buffer *buffer = current_buffer;
	u32 column = cursor_get_column(buffer, buffer->cursor);
	u32 beginning_of_prev_line = cursor_get_beginning_of_prev_line(buffer, buffer->cursor);
	u32 prev_line_length = buffer_line_length(buffer, beginning_of_prev_line);

	buffer_set_cursor(buffer, beginning_of_prev_line + MIN(prev_line_length, column));
}

COMMAND(insert_new_line) {
	buffer_insert(current_buffer, current_buffer->cursor, '\n');
}

COMMAND(goto_beginning_of_line) {
	current_buffer->cursor = cursor_get_beginning_of_line(current_buffer, current_buffer->cursor);
	current_buffer->mode = MODE_INSERT;
}

COMMAND(goto_end_of_line) {
	current_buffer->cursor = cursor_get_end_of_line(current_buffer, current_buffer->cursor);
	current_buffer->mode = MODE_INSERT;
}

COMMAND(reload_buffer) {
	read_file_to_buffer(current_buffer);
}

COMMAND(save_buffer) {
	write_buffer_to_file(current_buffer);
}

COMMAND(insert_mode) {
	current_buffer->mode = MODE_INSERT;
}

COMMAND(insert_mode_next) {
	current_buffer->mode = MODE_INSERT;
	current_buffer->cursor = cursor_next(current_buffer, current_buffer->cursor);
}

COMMAND(normal_mode) {
	current_buffer->mode = MODE_NORMAL;
}

COMMAND(goto_buffer_begin) {
	current_buffer->cursor = 0;
}

COMMAND(goto_buffer_end) {
	current_buffer->cursor = buffer_length(current_buffer);
}

COMMAND(new_line_before) {
	command_fn_goto_beginning_of_line();
	command_fn_insert_new_line();
}

COMMAND(new_line_after) {
	command_fn_goto_end_of_line();
	command_fn_insert_new_line();
}

COMMAND(window_operation) {
	current_buffer->mode = MODE_WINDOW_OPERATION;
}

COMMAND(next_pane) {
	if (active_pane->child) {
		active_pane = active_pane->child;
	}

	current_buffer = active_pane->buffer;
	current_buffer->mode = MODE_NORMAL;
}

COMMAND(prev_pane) {
	if (active_pane->parent) {
		active_pane = active_pane->parent;
	}

	current_buffer = active_pane->buffer;
	current_buffer->mode = MODE_NORMAL;
}

COMMAND(split_vertically) {
	pane_split_vertically();
	active_pane = active_pane->child;
	current_buffer->mode = MODE_NORMAL;
}

COMMAND(split_horizontally) {
	pane_split_horizontally();
	active_pane = active_pane->child;
	current_buffer->mode = MODE_NORMAL;
}

COMMAND(show_settings) {
	settings.show = !settings.show;
}

COMMAND(quit) {
	shin_exit();
}

Command *keymap_get_command(Keymap *keymap, u16 key_comb) {
	assert(key_comb < MAX_KEY_COMBINATIONS);
	return keymap->commands + key_comb;
}

Keymap *keymap_create_empty() {
	Keymap *keymap = (Keymap *) malloc(sizeof(Keymap));

	for (u32 i = 0; i < MAX_KEY_COMBINATIONS; ++i) {
		keymap->commands[i] = command_null;
	}

	return keymap;
}

void keymap_dispatch_event(Keymap *keymap, InputEvent event) {
	if (event.type == INPUT_EVENT_PRESSED) {
		Command *command = keymap_get_command(keymap, event.key_comb);
		command->function();
	}
}
