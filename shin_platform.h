void *shin_alloc(u32 size);
void *shin_realloc(void *pointer, u32 size);
void shin_free(void *pointer);
void shin_memmove(void *dest, void *src, u32 length);

void shin_read_file_to_buffer(Buffer *buffer);
void shin_write_buffer_to_file(Buffer *buffer);

void shin_printf(const char *fmt, ...);
void shin_beep();

void shin_exit();

char shin_map_virtual_key(u32 key);

void create_default_keymaps();

void pane_draw(Pane *pane);
void buffer_draw(Buffer *buffer, f32 line_height, f32 x, f32 y, f32 width, f32 height);
