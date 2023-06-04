struct Highlight {
    u32 start;
    u32 end;
    u32 color_index;
};

static Array<Highlight> highlights;

void highlighting_parse() {
	highlights.clear();

	highlights.add({2, 5, COLOR_KEYWORD});
	highlights.add({20, 25, COLOR_KEYWORD});
}
