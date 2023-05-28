#version 430 core

layout (location = 0) in vec4 Pos;

void main() {
	gl_Position = Pos;
}
