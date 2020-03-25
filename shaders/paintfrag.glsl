#version 430

in vec2 vertTexcoord;

out vec4 outFragColor;

layout(location = 0) uniform sampler2D raytraceOutput;
layout(location = 1) uniform vec2 resolution;

const float quality		= +0.85;
const float brushDetail = +0.6;
const float strokeBend  = +1.5;
const float brushSize   = +1.5;
const float minLayer	= +0.0;
const float maxLayer	= +11.0;

const float pi = 3.1415927;
const float strokeDeform = 1.0 / (1.0 - 0.25 * abs(strokeBend));

float rand(vec2 seed) {
	return fract(sin(dot(seed, vec2(12.9898, 78.233))) * 43758.5453);
}

vec3 getCol(vec2 pos) {
	return texture(raytraceOutput, pos / resolution).xyz;
}

float compsignedmax(vec3 c) {
	vec3 a = abs(c);
	if (a.x > a.y && a.x > a.z) return c.x;
	if (a.y > a.x && a.y > a.z) return c.y;
	return c.z;
}

void main() {

	vec2 pos = gl_FragCoord.xy;

	vec2 screenCenter = vec2(resolution.x, resolution.y) / 2;
	float dist = length(screenCenter - pos);
	if (dist < 4) {
		float i = 0.5 * dist;
		i = 1 - i * i;
		outFragColor = vec4(i, i, i, 1);
		return;	
	}

	outFragColor = texture(raytraceOutput, vertTexcoord);
	return;
	
	//number of grid positions on highest detail level
	float numGrid = 10000.0;

	float aspect = resolution.x / resolution.y;
	float numX = sqrt(float(numGrid) * aspect);
	float numY = sqrt(float(numGrid) / aspect);
	float gridW0 = resolution.x / numX;

	float numX2 = numX * pow(quality, 1.0 + maxLayer);
	float numY2 = numY * pow(quality, 1.0 + maxLayer);
	const float invQuality = 1.0 / quality;
	float brushStretchPow0 = pow(1.73205081, 1.0 / (2.0 + maxLayer));
	float brushStretchPow = brushStretchPow0;


	for (float layer = 11; layer >= 1; layer -= 1.0) {

		// get grid position
		numX2 *= invQuality;
		numY2 *= invQuality;
		brushStretchPow *= brushStretchPow0;
		float brushStretch = 1.22474487 * brushStretchPow;
		vec2 brushPosScale = resolution / vec2(numX2, numY2);
		float gridW = resolution.x / numX2;
		float n0 = dot(floor(vec2(pos / resolution * vec2(numX2, numY2))), vec2(1.0, numX2));
		vec2 p0 = floor(vec2(pos / resolution * vec2(numX2, numY2)));

		// width and length of brush stroke
		float bw = brushSize * (gridW - 0.8 * gridW0);
		vec2 brushScale = 1.0 / vec2(
			bw / brushStretch, 
			bw * brushStretch);
		
		vec2 delta = vec2(gridW, 0.0);
		vec2 gradient = (0.5 / gridW) * vec2(
			compsignedmax(getCol(brushPosScale * p0 + delta.xy) - getCol(brushPosScale * p0 - delta.xy)),
			compsignedmax(getCol(brushPosScale * p0 + delta.yx) - getCol(brushPosScale * p0 - delta.yx)));
		vec2 normal = normalize(gradient + 0.012 * pos / resolution); // add some gradient to plain areas				
		vec2 tangent = normal.yx * vec2(1, -1);

		for (float nx = -1.0; nx <= 1.0; nx += 1.0) {
			for (float ny = -1.0; ny <= 1.0; ny += 1.0) {
				// index centerd in cell
				vec2 pid = p0 + vec2(nx, ny);
				vec2 noise = vec2(rand(pid));	
				vec2 brushPos = brushPosScale * pid;
				brushPos += gridW * noise.xy; // add some noise to grid pos
				
				vec2 uv = 0.5 * vec2(dot(pos - brushPos, normal), dot(pos - brushPos, tangent)) * brushScale;
				// bending the brush stroke
				uv.x -= strokeBend * (uv.y * uv.y);
				uv.x *= strokeDeform;
				uv += 0.5;
				
				float alpha = 1.0;
				alpha *= uv.x * (1.0 - uv.x) * 12.0;
				alpha = min(1.0, alpha - 0.5);

				float strokeBlend = max(cos(uv.x * pi * 2.0), 0.0);
				uv.y = 1.0 - uv.y;

				vec2 st = step(-0.5, -abs(uv - 0.5));
				alpha = max(0.0, alpha * (strokeBlend + rand(uv)));
				alpha *= st.x * st.y;

				vec4 dfragColor = vec4(getCol(brushPos) * (0.8 + 0.2 * strokeBlend), alpha);
				outFragColor = mix(outFragColor, dfragColor, dfragColor.a); // do alpha blending
			}
		}
	}
}