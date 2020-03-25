#version 430

layout(location = 0) in vec2 inPos;

out vec2 vertTexcoord;

void main() {
	vertTexcoord = 0.5 * (inPos + 1.0);
	gl_Position = vec4(inPos, 0, 1);
}