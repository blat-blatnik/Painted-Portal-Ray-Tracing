#version 430

layout(location = 0) in vec2 inPos;

out vec3 vertRayPos;
out vec3 vertRayDir;

layout(location = 0) uniform vec2 resolution;
layout(location = 1) uniform float foveaDist;
layout(location = 2) uniform vec3 cameraPos;
layout(location = 3) uniform mat3 invView;

void main() {
	vec2 aspect = vec2(
		max(resolution.x / resolution.y, 1),
		max(resolution.y / resolution.x, 1));

	// Each ray originates from the camera and is passed through a grid that is
	// 'foveaDist' away from the camera. The direction of the rays is transformed
	// from camera space to world space using the inverse of the view matrix

	vertRayPos = cameraPos;
	vertRayDir = normalize(invView * normalize(vec3(inPos * aspect, -abs(foveaDist))));
	gl_Position = vec4(inPos, 0, 1);
}