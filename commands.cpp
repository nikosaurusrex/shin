void command_fn_null() {
	shin_beep();
}

Command command_null = {"null", command_fn_null};

void command_fn_insert_char() {
	buffer_insert(current_buffer, current_buffer->cursor, last_input_event.ch);
}

Command command_insert_char = {"insert-char", command_fn_insert_char};

INLINE u16 get_key_combination(u8 key, bool ctrl, bool alt, bool shift) {
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

	for (u32 key = 0; key < 256; ++key) {
		char ch = shin_map_virtual_key(key);
		if (' ' <= ch && ch <= '~') {
			keymap->commands[get_key_combination(key, false, false, false)] = command_insert_char;
			keymap->commands[get_key_combination(key, false, false, true)] = command_insert_char;
		}
	}

	return keymap;
}

void keymap_dispatch_event(Keymap *keymap, InputEvent event) {
	if (event.type == INPUT_EVENT_PRESSED) {
		Command *command = keymap_get_command(keymap, event.key_comb);
		shin_printf("Key: '%x', Ch: '%c', Command: %s\n", event.key_comb, event.ch, command->name);
		command->function();
	}
}
