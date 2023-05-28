#version 430 core

layout(origin_upper_left) in vec4 gl_FragCoord;

layout(location = 0) out vec4 Color;

layout(location = 1) uniform sampler2D GlyphMap;
layout(location = 2) uniform usampler2D Cells;

layout(location = 3) uniform uvec2 CellSize;
layout(location = 4) uniform uvec2 TermSize;
layout(location = 5) uniform float Time;

vec3 UnpackColor(uint Packed) {
	uint R = (Packed >> 16) & 0xFFu;
	uint G = (Packed >> 8) & 0xFFu;
	uint B = Packed & 0xFFu;

	return vec3(R, G, B) / 255.0;
}

void main() {
	uvec2 ScreenPos = uvec2(floor(gl_FragCoord.xy));

	vec3 Pixel;
	if (ScreenPos.x < TermSize.x && ScreenPos.y < TermSize.y) {
		uvec2 CellIndex = uvec2(floor(ScreenPos / CellSize));
		uvec2 CellPos = ScreenPos % CellSize;

		uvec4 Cell = texelFetch(Cells, ivec2(CellIndex), 0);
		vec4 Texel = texelFetch(GlyphMap, ivec2(Cell.x * CellSize.x + CellPos.x, CellPos.y), 0);
		 
		float Cursor = float(Cell.y) * abs(sin(Time * 4));

		vec3 FG = UnpackColor(Cell.z);
		vec3 BG = abs(Cursor - UnpackColor(Cell.w));

		Pixel = (Texel.r * FG) + (1.0 - Texel.r) * BG;
	} else {
		Pixel = vec3(1.0); 
	}
	Color = vec4(Pixel, 1.0);
}
