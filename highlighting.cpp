#define ID_MAX_LENGTH 256

/* TODO: better solution */
const char *KEYWORDS[] = {
	"auto", "bool", "break", "case", "catch", "char", "char16_t", "char32_t", "wchar_t", "class",
	"const", "constexpr", "continue", "default", "delete", "do", "double", "dynamic_cast", "static_cast", "const_cast",
	"else", "enum", "explicit", "extern", "false", "float", "for", "friend", "goto", "if", "inline", "int", "long",
	"namespace", "new", "nullptr", "operator", "private", "protected", "public", "reinterpret_cast", "return", "short",
	"signed", "sizeof", "static", "static_assert", "struct", "switch", "template", "this", "thread_local", "throw",
	"true", "try", "typedef", "union", "unsigned", "using", "virtual", "void", "volatile", "while", "int8_t", "int16_t",
	"int32_t", "int64_t", "uint8_t", "uint16_t", "uint32_t", "uint64_t"
};

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

			while (isalnum(c) || c == '_' && pos < len) {
				id[id_index++] = c;
				pos = cursor_next(buffer, pos);
				c = buffer_get_char(buffer, pos);
			}
			id[id_index] = 0;

			u32 keyword_count = sizeof(KEYWORDS) / sizeof(char *);
			for (u32 i = 0; i < keyword_count; ++i) {
				if (strcmp(KEYWORDS[i], id) == 0) {
					pane->highlights.add({start, pos - 1, COLOR_KEYWORD});
					break;
				}
			}

		} else if (isdigit(c)) {
			u32 start = pos;

			while ((isdigit(c) ||
					c == '.' ||
					c == 'o' ||
					c == 'x' ||
					c == 'b') &&
					pos < len) {
				pos = cursor_next(buffer, pos);
				c = buffer_get_char(buffer, pos);
			}

			pane->highlights.add({start, pos - 1, COLOR_NUMBER});
		} else if (c == '"') {
			u32 start = pos;

			pos = cursor_next(buffer, pos);
			c = buffer_get_char(buffer, pos);
			while (c != '"' && pos < len) {
				pos = cursor_next(buffer, pos);
				c = buffer_get_char(buffer, pos);
			}

			pane->highlights.add({start, pos, COLOR_STRING});
			pos++;
		} else if (isspace(c)) {
			pos++;
		} else {
			pos++;
		}
	}
}
