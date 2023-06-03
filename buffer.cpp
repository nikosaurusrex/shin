Buffer *buffer_create(u32 size) {
	Buffer *buffer = (Buffer *) malloc(sizeof(Buffer));

	buffer->data = (char *) malloc(size);
	buffer->file_path = 0;
	buffer->mode = MODE_NORMAL;
	buffer->cursor = 0;
	buffer->size = size;
	buffer->gap_start = 0;
	buffer->gap_end = size;

	return buffer;
}

void buffer_destroy(Buffer *buffer) {
	free(buffer->data);
	free(buffer);
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

INLINE void buffer_set_cursor(Buffer *buffer, u32 cursor) {
	buffer->cursor = MIN(cursor, buffer_length(buffer));
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
		memmove(buffer->data + buffer->gap_end, buffer->data + buffer->gap_start, gap_delta);
	} else if (pos > buffer->gap_start) {
		u32 gap_delta = pos - buffer->gap_start;
		memmove(buffer->data + buffer->gap_start, buffer->data + buffer->gap_end, gap_delta);
		buffer->gap_start += gap_delta;
		buffer->gap_end += gap_delta;
	}
	buffer_asserts(buffer);
}

void buffer_grow_if_needed(Buffer *buffer, u32 size_needed) {
	if (buffer_gap_size(buffer) < size_needed) {
		buffer_shift_gap_to_position(buffer, buffer_length(buffer));

		u32 new_size = MAX(buffer->size * 2, buffer->size + size_needed - buffer_gap_size(buffer));

		buffer->data = (char *) realloc(buffer->data, new_size);
		buffer->gap_end = new_size;
		buffer->size = new_size;
	}

	assert(buffer_gap_size(buffer) >= size_needed);
}

void buffer_clear(Buffer *buffer) {
	buffer->gap_start = 0;	
	buffer->gap_end = buffer->size;
	buffer->cursor = 0;
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

void buffer_replace(Buffer *buffer, u32 pos, char ch) {
	buffer_asserts(buffer);

	if (pos < buffer_length(buffer)) {
		buffer_set_char(buffer, pos, ch);
	}
}

void buffer_delete_forwards(Buffer *buffer, u32 pos) {
	buffer_asserts(buffer);

	if (pos < buffer_length(buffer)) {
		buffer_shift_gap_to_position(buffer, pos);
		buffer->gap_end++;

		if (buffer->cursor > pos) {
			buffer->cursor--;
		}
	}
}

void buffer_delete_backwards(Buffer *buffer, u32 pos) {
	buffer_asserts(buffer);

	if (pos > 0) {
		buffer_shift_gap_to_position(buffer, pos);
		buffer->gap_start--;
		if (buffer->cursor >= pos) {
			buffer->cursor--;
		}
	}
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

INLINE u32 cursor_get_beginning_of_next_line(Buffer *buffer, u32 cursor) {
	return cursor_next(buffer, cursor_get_end_of_line(buffer, cursor));
}

INLINE u32 cursor_get_beginning_of_prev_line(Buffer *buffer, u32 cursor) {
	return cursor_get_beginning_of_line(buffer, cursor_back(buffer, cursor_get_beginning_of_line(buffer, cursor)));
}

INLINE u32 cursor_get_end_of_prev_line(Buffer *buffer, u32 cursor) {
	return cursor_back(buffer, cursor_get_beginning_of_line(buffer, cursor));
}

INLINE u32 cursor_get_end_of_next_line(Buffer *buffer, u32 cursor) {
	return cursor_get_end_of_line(buffer, cursor_get_beginning_of_next_line(buffer, cursor));
}

INLINE u32 buffer_line_length(Buffer *buffer, u32 cursor) {
	return cursor_get_end_of_line(buffer, cursor) - cursor_get_beginning_of_line(buffer, cursor);
}

INLINE u32 cursor_get_column(Buffer *buffer, u32 cursor) {
	return cursor - cursor_get_beginning_of_line(buffer, cursor);
}

Pane *pane_create(Pane *parent, Bounds bounds, Buffer *buffer) {
	Pane *pane = &pane_pool[pane_count];

	pane->bounds = bounds;
	pane->buffer = buffer;
	pane->start = 0;
	pane->end = UINT32_MAX;
	pane->parent = parent;
	pane->child = 0;

	active_pane = pane;

	pane_count++;
	return pane;
}

void pane_destroy(Pane *pane) {
	pane_pool[pane - pane_pool] = pane[pane_count - 1];
	pane_count--;
}

u32 get_line_difference(Buffer *buffer, u32 start, u32 end) {
	u32 lines = 0;
	
	while (end > start) {
		if (buffer_get_char(buffer, end) == '\n') {
			lines++;
		}
		end = cursor_back(buffer, end);
	}

	if (cursor_get_end_of_line(buffer, buffer->cursor) == buffer_length(buffer)) {
		lines++;
	}

	return lines;
}

void pane_update_scroll(Pane *pane) {
	Buffer *buffer = pane->buffer;
	u32 start = MIN(buffer_length(buffer), pane->start);
	u32 end = MIN(buffer_length(buffer), pane->end);

	if (buffer->cursor < start) {
		while (start > buffer->cursor) {
			start = cursor_get_beginning_of_prev_line(buffer, start);
		}

		u32 line_diff = get_line_difference(buffer, start, end);
		while (line_diff > pane->bounds.height) {
			end = cursor_get_end_of_prev_line(buffer, end);
			line_diff--;
		}
	}

	if (buffer->cursor > end) {
		while (end < buffer->cursor) {
			end = cursor_get_end_of_next_line(buffer, end);
		}

		u32 line_diff = get_line_difference(buffer, start, end);
		while (line_diff > pane->bounds.height) {
			start = cursor_get_beginning_of_next_line(buffer, start);
			line_diff--;
		}
	}

	pane->start = start;
	pane->end = end;
}

void pane_split_vertically() {
	Pane *pane = active_pane;

	Bounds *left_bounds = &pane->bounds;
	u32 left_width = left_bounds->width / 2;

	Bounds right_bounds;
	right_bounds.left = left_width;
	right_bounds.top = left_bounds->top;
	right_bounds.width = left_bounds->width - left_width;
	right_bounds.height = left_bounds->height;

	pane->child = pane_create(pane, right_bounds, buffer_create(32));
	
	left_bounds->width = left_width;
}

void pane_split_horizontally() {
	Pane *pane = active_pane;

	Bounds *top_bounds = &pane->bounds;
	u32 top_height = top_bounds->height / 2;

	Bounds bot_bounds;
	bot_bounds.left = top_bounds->left;
	bot_bounds.top = top_height;
	bot_bounds.width = top_bounds->width;
	bot_bounds.height = top_bounds->height - top_height;

	pane->child = pane_create(pane, bot_bounds, buffer_create(32));
	
	top_bounds->height = top_height;
}
