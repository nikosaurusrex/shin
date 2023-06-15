#include "shin.h"

#define ID_MAX_LENGTH 256

/* TODO: better solution */
const char *KEYWORDS[] = {
	"break", "case", "catch", "continue", "default", "delete", "do", "dynamic_cast",
	"static_cast", "const_cast", "else", "for", "goto", "if", "friend",
	"new", "operator", "private", "protected", "public", "reinterpret_cast",
	"return", "sizeof", "static_assert", "switch", "this", "throw", "try",
	"using", "while", 
};

const char *TYPES[] = {
	"auto", "bool", "char", "char16_t", "char32_t", "wchar_t", "class",
	"const", "constexpr", "double", "enum", "extern", "float", "int", "long",
	"inline", "explicit", "namespace", "short", "signed", "static", "template", "thread_local", 
	"typedef", "union", "unsigned", "virtual", "void", "volatile", "int8_t", "int16_t",
	"int32_t", "int64_t", "uint8_t", "uint16_t", "uint32_t", "uint64_t", "struct"
};

const char *CONSTANTS[] = {
	"NULL", "nullptr", "true", "false"
};

void highlighting_parse(Pane *pane) {
	pane->highlights.clear();

	Buffer *buffer = pane->buffer;

	u32 pos = pane->start;
	u32 len = buffer_length(buffer);
	while (pos < len && pos < pane->end) {
		char c = buffer_get_char(buffer, pos);

		if (isalpha(c)) {
            char id[ID_MAX_LENGTH];
            u32 id_index = 0;
			u32 start = pos;

			while ((isalnum(c) || c == '_') && pos < len) {
				id[id_index++] = c;
				pos++;
				c = buffer_get_char(buffer, pos);
			}
			id[id_index] = 0;

			for (u32 i = 0; i < sizeof(KEYWORDS) / sizeof(char *); ++i) {
				if (strcmp(KEYWORDS[i], id) == 0) {
					pane->highlights.add({start, pos - 1, COLOR_KEYWORD});
					break;
				}
			}
			
			for (u32 i = 0; i < sizeof(TYPES) / sizeof(char *); ++i) {
				if (strcmp(TYPES[i], id) == 0) {
					pane->highlights.add({start, pos - 1, COLOR_TYPE});
					break;
				}
			}
			
			for (u32 i = 0; i < sizeof(CONSTANTS) / sizeof(char *); ++i) {
				if (strcmp(CONSTANTS[i], id) == 0) {
					pane->highlights.add({start, pos - 1, COLOR_NUMBER});
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
				pos++;
				c = buffer_get_char(buffer, pos);
			}

			pane->highlights.add({start, pos - 1, COLOR_NUMBER});
		} else if (c == '"') {
			u32 start = pos;

			pos++;
			c = buffer_get_char(buffer, pos);
			while (c != '"' && pos < len) {
				pos++;
				c = buffer_get_char(buffer, pos);
			}

			pane->highlights.add({start, pos, COLOR_STRING});
			pos++;
		} else if (c == '#') {
			u32 start = pos;

			pos++;
			c = buffer_get_char(buffer, pos);
			while ((isalnum(c) || c == '_') && pos < len) {
				pos++;
				c = buffer_get_char(buffer, pos);
			}

			pane->highlights.add({start, pos, COLOR_DIRECTIVE});
			pos++;
		} else if (c == '/') {
			u32 start = pos;

			pos++;

			if (pos < len) {
				c = buffer_get_char(buffer, pos);
				if (c == '/') {
					while (c != '\n' && pos < len) {
						pos++;
						c = buffer_get_char(buffer, pos);
					}
				}
			}

			pane->highlights.add({start, pos - 1, COLOR_COMMENT});
		} else if (isspace(c)) {
			pos++;
		} else {
			pos++;
		}
	}
}
