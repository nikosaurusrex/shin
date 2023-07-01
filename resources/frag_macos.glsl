#version 410 core

const uint GLYPH_INVERT = 0x1;
const uint GLYPH_BLINK = 0x2;
const uint UINT_MAX = 4294967295u;

layout(origin_upper_left) in vec4 gl_FragCoord;
layout(location = 0) out vec4 color;

uniform sampler2D glyph_map;
uniform sampler2D cells;

uniform uvec2 cell_size;
uniform uvec2 grid_size;
uniform uvec2 win_size;
uniform float time;
uniform uint background_color;

vec3 unpack_color(uint cp) {
	uint r = (cp >> 16) & 0xFFu;
	uint g = (cp >> 8) & 0xFFu;
	uint b = cp & 0xFFu;

	return vec3(r, g, b) / 255.0;
}

void main() {
	vec3 default_bg = unpack_color(background_color);
	uvec2 screen_pos = uvec2(floor(gl_FragCoord.xy));
	
	vec3 pixel;
	if (screen_pos.x < grid_size.x * cell_size.x && screen_pos.y < grid_size.y * cell_size.y) {
		uvec2 cell_index = uvec2(floor(screen_pos / cell_size));
		uvec2 cell_pos = screen_pos % cell_size;

		vec4 cell = texelFetch(cells, ivec2(cell_index), 0);
		uint glyph_index = uint(cell.x * win_size.x);
		uint cell_flags = uint(cell.y * UINT_MAX);
		uint cell_fg = uint(cell.z * UINT_MAX);
		uint cell_bg = uint(cell.w * UINT_MAX);

		uint cell_x = glyph_index % 32u;
		uint cell_y = glyph_index / 32u;

		vec4 texel = texelFetch(glyph_map, ivec2(
			cell_x * cell_size.x + cell_pos.x,
			cell_y * cell_size.y + cell_pos.y),
		0);
		
		float blink_time = time * 3;
		float blink_curve = abs(sin(blink_time) + cos(blink_time + 1.05));
		float blink = 1.0 - float(cell_flags & GLYPH_BLINK) * blink_curve;
		vec3 invert = vec3(cell_flags & GLYPH_INVERT);

		vec3 fg = abs(invert - unpack_color(cell_fg)) * blink;
		vec3 bg = abs(invert - unpack_color(cell_bg)) * blink;

		vec3 bg_black = step(vec3(0.01), bg);
		bg += (vec3(1.0) - bg_black) * default_bg;

		pixel = (texel.r * fg) + (1.0 - texel.r) * bg;
	} else {
		pixel = default_bg; 
	}
	color = vec4(pixel, 1.0);
}