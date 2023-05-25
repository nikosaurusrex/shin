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

void buffer_shift_gap_to_position(Buffer *buffer, u32 pos) {
	if (pos < buffer->gap_start) {
		u32 gap_delta = buffer->gap_start - pos;
		buffer->gap_start -= gap_delta;
		buffer->gap_end -= gap_delta;
		shin_memmove(buffer->data + buffer->gap_end, buffer->data + buffer->gap_start, gap_delta);
	} else if (pos > buffer->gap_start) {
		u32 gap_delta = pos - buffer->gap_start;
		shin_memmove(buffer->data + buffer->gap_start, buffer->data + buffer->gap_end, gap_delta);
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
