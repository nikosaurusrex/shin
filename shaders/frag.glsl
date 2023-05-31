#version 410 core

layout(origin_upper_left) in vec4 gl_FragCoord;

layout(location = 0) out vec4 color;

uniform sampler2D glyph_map;
uniform isampler2D cells;

uniform ivec2 cell_size;
uniform ivec2 win_size;
uniform float time;

vec3 unpack_color(int cp) {
	int r = (cp >> 16) & 0xFF;
	int g = (cp >> 8) & 0xFF;
	int b = cp & 0xFF;

	return vec3(r, g, b) / 255.0;
}

void main() {
	ivec2 screen_pos = ivec2(floor(gl_FragCoord.xy));

	vec3 pixel;
	if (screen_pos.x < win_size.x && screen_pos.y < win_size.y) {
		ivec2 cell_index = ivec2(floor(screen_pos / cell_size));
		ivec2 cell_pos = screen_pos % cell_size;

		ivec4 cell = texelFetch(cells, ivec2(cell_index), 0);
		
		int cell_x = cell.x % 32;
		int cell_y = cell.x / 32;

		vec4 texel = texelFetch(glyph_map, ivec2(
			cell_x * cell_size.x + cell_pos.x,
			cell_y * cell_size.y + cell_pos.y),
		0);
		 
		// float cursor = float(cell.y) * abs(sin(time * 4));

		vec3 fg = abs(vec3(cell.y) - unpack_color(cell.z));
		vec3 bg = abs(vec3(cell.y) - unpack_color(cell.w));

		pixel = (texel.r * fg) + (1.0 - texel.r) * bg;
	} else {
		pixel = vec3(1.0); 
	}
	color = vec4(pixel, 1.0);
}
