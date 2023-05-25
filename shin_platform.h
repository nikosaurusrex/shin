void *shin_alloc(u32 size);
void *shin_realloc(void *pointer, u32 size);
void shin_free(void *pointer);
void shin_memmove(void *dest, void *src, u32 length);

void shin_printf(const char *fmt, ...);
void shin_beep();

char shin_map_virtual_key(u32 key);

void buffer_draw(Buffer *buffer, f32 line_height, f32 x, f32 y, f32 width, f32 height);

