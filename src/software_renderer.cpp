#include "shin.h"

#ifdef _WIN32
#define GLEW_STATIC
#include <glew/glew.h>
#endif

#ifdef __APPLE__
#include <OpenGL/gl3.h>
#include <OpenGL/gl3ext.h>
#endif

#include <glfw/glfw3.h>

void SoftwareRenderer::init(GLFWwindow *window, DrawBuffer *draw_buffer, GlyphMap *gylph_map) {
	this->window = window;
    this->buffer = draw_buffer;
    this->glyph_map = glyph_map;
}

void SoftwareRenderer::query_cell_data() {
}

void SoftwareRenderer::query_settings(s32 width, s32 height) {
	glfwSwapInterval(settings.vsync ? 1 : 0);
	glfwSetWindowOpacity(window, settings.opacity);
}

void SoftwareRenderer::update_time(f64 time) {
}

void SoftwareRenderer::end() {
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}
