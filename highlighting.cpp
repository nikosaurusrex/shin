#define ID_MAX_LENGTH 256

void highlighting_add_keyword(Pane *pane, u32 start, u32 pos) {
	pane->highlights.add({start, pos, COLOR_KEYWORD});
}

void highlighting_parse(Pane *pane) {
	pane->highlights.clear();

	Buffer *buffer = pane->buffer;

	u32 pos = 0;
	u32 len = buffer_length(buffer);
	while (pos < len) {
		char c = buffer_get_char(buffer, pos);

		if (isalpha(c)) {
            char id[ID_MAX_LENGTH];
            u32 id_index = 0;
			u32 start = pos;

			while (isalpha(c)) {
				id[id_index++] = c;
				pos = cursor_next(buffer, pos);
				c = buffer_get_char(buffer, pos);
			}
			id[id_index] = 0;

#define HAK highlighting_add_keyword(pane, start, pos) 
			if (strcmp(id, "char") == 0) HAK;
			else if (strcmp(id, "short") == 0) HAK;
			else if (strcmp(id, "int") == 0) HAK;
			else if (strcmp(id, "long") == 0) HAK;
			else if (strcmp(id, "float") == 0) HAK;
			else if (strcmp(id, "double") == 0) HAK;
			else if (strcmp(id, "void") == 0) HAK;
			else if (strcmp(id, "struct") == 0) HAK;
			else if (strcmp(id, "enum") == 0) HAK;
			else if (strcmp(id, "if") == 0) HAK;
			else if (strcmp(id, "else") == 0) HAK;
			else if (strcmp(id, "while") == 0) HAK;
			else if (strcmp(id, "for") == 0) HAK;
#undef HAK

		} else if (isspace(c)) {
			pos++;
		} else {
			pos++;
		}
	}
}
