#version 410 core

layout(origin_upper_left) in vec4 gl_FragCoord;

layout(location = 0) out vec4 color;

layout(location=1) uniform sampler2D glyph_map;
uniform usampler2D cells;

uniform uvec2 cell_size;
uniform uvec2 win_size;
uniform float time;

vec3 unpack_color(uint cp) {
	uint r = (cp >> 16) & 0xFFu;
	uint g = (cp >> 8) & 0xFFu;
	uint b = cp & 0xFFu;

	return vec3(r, g, b) / 255.0;
}

void main() {
	uvec2 screen_pos = uvec2(floor(gl_FragCoord.xy));

	vec3 pixel;
	if (screen_pos.x < win_size.x && screen_pos.y < win_size.y) {
		uvec2 cell_index = uvec2(floor(screen_pos / cell_size));
		uvec2 cell_pos = screen_pos % cell_size;

		uvec4 cell = texelFetch(cells, ivec2(cell_index), 0);
		
		uint cell_x = cell.x % 32u;
		uint cell_y = cell.x / 32u;

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
