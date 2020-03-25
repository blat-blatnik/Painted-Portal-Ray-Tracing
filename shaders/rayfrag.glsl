#version 430

in vec3 vertRayPos;
in vec3 vertRayDir;

out vec4 outFragColor;

layout(location = 8) uniform float time;
layout(location = 9) uniform sampler2DArray textureAtlas;

//
// --- RAY TRACING STRUCTURES ---
//

struct Ray {
	vec3 pos;
	vec3 dir;
	vec3 invDir; //NOTE: invDir must be set to 1/dir at all times!
};

struct Hit {
	float dist;
	vec3 normal;
	uint material;
	vec2 texcoord;
	int portalIndex;
};

//
// --- GAME OBJECTS ---
//

struct Light {
	vec3 pos;
	vec3 color;
};

struct Material {
	// Right now everything is a perfectly reflective mirror surface
	// and is opaque. Even though materials can have a transparent color
	// and an index of refraction, we don't have transparency support in
	// this shader.
	vec4 color;
	float reflectance;
	float ior;
	int textureIndex;
};

struct Plane {
	vec3 normal;
	vec3 pos;
	uint material;
};

struct Sphere {
	vec3 pos;
	float radius;
	uint material;
};

struct Voxel {
	ivec3 pos;
	uint material;
};

struct Portal {
	vec3 pos;
	vec3 normal;
	float radius;
};

layout(std430, binding=0) readonly buffer LIGHTS {
	Light lights[];
};
layout(std430, binding=1) readonly buffer MATERIALS {
	Material materials[];
};
layout(std430, binding=2) readonly buffer PLANES {
	Plane planes[]; //OPTIMIZE: There will always be only 1 plane at all times.
};
layout(std430, binding=3) readonly buffer SPHERES {
	Sphere spheres[]; //OPTIMIZE: Do we need these at all anymore??
};
layout(std430, binding=4) readonly buffer VOXELS {
	Voxel voxels[];
};
layout(std430, binding=5) readonly buffer PORTALS {
	Portal portals[]; //OPTIMIZE: There will always be exactly 2 portals at all times.
};

const float floatMax = 3.402823466e+38;
const uint numBounces = 2;
const uint portalRecursion = 4;
const float lightCutoffRadius = 0.001;
const float rayEpsilon = 0.001;
const float pi = 3.1415927;
const vec3 ambientLight = vec3(0.01);
const vec3 portalColors[] = {
	vec3(0.8, 0.3, 0.02),
	vec3(0.02, 0.3, 0.8)
};

// Return a "random" float in [0, 1) based on the given seed.
float rand(vec2 seed) {
	return fract(sin(dot(seed, vec2(12.9898, 78.233))) * 43758.5453);
}

// Plane-Ray intersection
float intersect(Ray r, Plane p) {
	const float epsilon = 0.001;
	float denom = dot(r.dir, p.normal);
	if (abs(denom) > epsilon) {
		float t = dot((p.pos - r.pos), p.normal) / denom;
		if (t > epsilon)
			return t;
	}
	return floatMax;
}

// Plane-Sphere intersection
float intersect(Ray r, Sphere s) {
	float a = 1;
	float b = 2 * dot(r.pos - s.pos, r.dir);
	float c = dot(s.pos, s.pos - 2 * r.pos) + dot(r.pos, r.pos) - s.radius * s.radius;
	float discriminant = b * b - 4 * a * c;
	if (discriminant < 0)
		return floatMax;
	else {
		float d = sqrt(discriminant);
		float dist = -0.5 * (b + d);
		return (dist < 0) ? -0.5 * (b - d) : dist;
	}
}

// Ray-Voxel intersetion
float intersect(Ray r, Voxel v) {
	//NOTE: If 'invDir' is 0 or INF, then this will completely bug out..
	vec3 ld = (vec3(v.pos) - r.pos) * r.invDir;
	vec3 rd = (vec3(v.pos) - r.pos) * r.invDir + r.invDir;
	vec3 mind = min(ld, rd);
	vec3 maxd = max(ld, rd);
	float dmin = max(max(mind.x, mind.y), mind.z);
	float dmax = min(min(maxd.x, maxd.y), maxd.z);
	if (dmin > dmax)
		return floatMax;
	else return dmin < 0 ? dmax : dmin;
}

// Ray-portal intersection
float intersect(Ray r, Portal p) {
	const float epsilon = 0.001;
	float denom = dot(r.dir, p.normal);
	if (abs(denom) > epsilon) {
		float t = dot((p.pos - r.pos), p.normal) / denom;
		if (t > epsilon) {
			vec3 v = r.pos + r.dir * t - p.pos;
			float d2 = dot(v, v);
			if (d2 <= p.radius * p.radius)
				return t;
		}
	}
	return floatMax;
}

// Get a portal that transforms into "portal space"
mat3 getPortalMatrix(Portal portal) {
	vec3 n = normalize(portal.normal);
	vec3 b = vec3(0, 1, 0);
	if (abs(dot(n, b)) > 0.99) {
		b = vec3(1, 0, 0);
	}
	vec3 t = normalize(cross(n, b));
	b = normalize(cross(t, n));
	return transpose(mat3(t, b, n));
}

// Get the closest hit data for the given ray.
//NOTE: This modifies the ray so that its facing its
//      correct reflected direction..
Hit getClosestHit(inout Ray ray) {
	
	Hit hit;
	hit.portalIndex = -1;
	bool rayHitPortal;
	uint numPortalsTravelled = 0;

	// Loop and do ray intersection with all objects.
	// If the closest hit object is a portal, then the
	// ray is teleported to the portal and we loop through
	// the scene again until the portal recursion limit is
	// reached, or the closest hit isn't a portal.
	do {
		hit.material = 0;
		hit.normal = vec3(0);
		hit.dist = floatMax;
		hit.texcoord = vec2(1);
		rayHitPortal = false;

		for (uint i = 0; i < planes.length(); ++i) {
			Plane plane = planes[i];
			float d = intersect(ray, plane);
			if (d > 0 && d < hit.dist) {
				hit.dist = d;
				hit.material = plane.material;
				hit.normal = plane.normal;
				hit.texcoord = vec2(1);
			}
		}

		for (uint i = 0; i < spheres.length(); ++i) {
			Sphere sphere = spheres[i];
			float d = intersect(ray, sphere);
			if (d > 0 && d < hit.dist) {
				hit.dist = d;
				hit.material = sphere.material;
				hit.normal = normalize(ray.pos + ray.dir * d - sphere.pos);
				vec3 d = -hit.normal;

				// Convert hit position to texture coordinates:
				// https://en.wikipedia.org/wiki/UV_mapping
				hit.texcoord = vec2(0.5 + atan(d.z, d.x) / (2 * pi), 0.5 - asin(d.y) / pi);
			}
		}
		
		for (uint i = 0; i < voxels.length(); ++i) {
			Voxel voxel = voxels[i];
			float d = intersect(ray, voxel);
			if (d > 0 && d < hit.dist) {
				hit.dist = d;
				Voxel voxel = voxels[i];
				float d = hit.dist;
				hit.material = voxel.material;
				vec3 hitPos = ray.pos + ray.dir * hit.dist;
				vec3 p = hitPos - vec3(voxel.pos);
				hit.normal = normalize(vec3(ivec3(2.0001 * (p - 0.5))));

				// Calculate texture coordinates of the voxel:
				// https://en.wikipedia.org/wiki/Cube_mapping#Memory_addressing
				float dotX = abs(dot(hit.normal, vec3(1, 0, 0)));
				float dotY = abs(dot(hit.normal, vec3(0, 1, 0)));
				float dotZ = abs(dot(hit.normal, vec3(0, 0, 1)));
				if (dotX > 0.8)
					hit.texcoord = abs(p.zy);
				if (dotY > 0.8)
					hit.texcoord = abs(p.zx);
				if (dotZ > 0.8)
					hit.texcoord = abs(p.xy);
			}
		}

		if (numPortalsTravelled < portalRecursion) {
			for (uint i = 0; i < portals.length(); ++i) {
				float d = intersect(ray, portals[i]);
				if (d > 0 && d < hit.dist) {
					
					// The ray hit a portal (P1), now we have to teleport it from
					// that portal to its pair portal (P2).
					Portal P1 = portals[i];
					Portal P2 = portals[1 - i];
					
					vec3 p = ray.pos + ray.dir * d - P1.pos;
					float dist2 = dot(p, p);
					float r = P1.radius * max(rand(p.xy + time), 0.80);
					// Add random noise around the portal as a makeshift animation
					if (dist2 < r * r) {
						if (hit.portalIndex < 0) {
							// Record which portal we hit for lighting.
							hit.portalIndex = int(i);
						}

						// Since the ray is going INTO one portal and OUT of the other
						// one of the portal normals has to be flipped.
						P1.normal = -P1.normal;
						mat3 M1 = getPortalMatrix(P1);
						mat3 M2 = getPortalMatrix(P2);

						// We first transform the ray from "world space" to "portal 1 space",
						// then we pretend these "portal 1 coordinates" are actually
						// "portal 2 coordinates" and undo the "portal 2 space" transformation
						// this effectively teleports the ray from portal 1 to portal 2, and
						// keeps the relative ray direction intact.
						ray.pos = ray.pos + ray.dir * d;
						ray.pos = P2.pos + inverse(M2) * M1 * (ray.pos - P1.pos);
						ray.dir = inverse(M2) * M1 * ray.dir;
						ray.invDir = 1 / ray.dir;

						ray.pos += ray.dir * rayEpsilon;
						rayHitPortal = true;
						numPortalsTravelled += 1;
					}
					break;
				}
			}
		}			
	} while(rayHitPortal);

	ray.pos += ray.dir * hit.dist;
	ray.dir = normalize(reflect(ray.dir, hit.normal));
	ray.invDir = 1 / ray.dir;
	return hit;
}

// Calculate lighting for a given ray position and direction hitting a surface
vec3 getLightColor(Light light, vec3 pos, vec3 dir, vec3 normal) {
	float lightDist = length(light.pos - pos);
	float attenuation = 1 / (lightDist * lightDist);
	if (attenuation > lightCutoffRadius) { // Light cutoff radius
		Ray lightRay;
		lightRay.pos = light.pos;
		lightRay.dir = normalize(pos - lightRay.pos);
		lightRay.invDir = 1 / lightRay.dir;
		vec3 lightDir = lightRay.dir;

		// Cast a light ray to check if the light is occluded.
		Hit hit = getClosestHit(lightRay);
		if (abs(hit.dist - lightDist) < rayEpsilon) {
			// "Full" Phong lighting, but everything has the same specular value..
			float diffuse = max(0, dot(-lightDir, normal));
			float specular = pow(max(0, dot(-dir, reflect(lightDir, normal))), 16.0);
			return light.color * attenuation * (diffuse + specular);
		}	
	}

	// The light is too far away.
	return vec3(0);
}

// Trace the ray through the scene and bounce it around, accumulating color
void trace(Ray ray, out vec3 color) {
	float reflectance = 1;
	color = vec3(0);
	
	// Bounce the ray around until the bounce limit is reached or the reflectance gets low.
	for (uint bounce = 0; bounce < 1 + numBounces && reflectance > 0.05; ++bounce) {
		
		// Trace the ray to the nearest object.
		vec3 rayDir = ray.dir;
		Hit hit = getClosestHit(ray);

		// Calculate light contribution from all lights.
		vec3 lighting = ambientLight;
		for (uint i = 0; i < lights.length(); ++i) {
			Light light = lights[i];
			lighting += getLightColor(lights[i], ray.pos, rayDir, hit.normal);
		}
		// Portals also give off a light.
		for (uint i = 0; i < portals.length(); ++i) {
			Light light;
			light.pos = portals[i].pos;
			light.color = 9 * portalColors[i];
			float cone = dot(portals[i].normal, normalize(ray.pos - portals[i].pos));
			cone *= cone;
			lighting += getLightColor(light, ray.pos, rayDir, hit.normal) * cone;
		}

		// Check if the material is textured.
		int texidx = materials[hit.material].textureIndex;
		vec3 texcolor = texidx < 0 ? vec3(1) : texture(textureAtlas, vec3(hit.texcoord, texidx)).rgb;
		
		color += reflectance * lighting * texcolor * materials[hit.material].color.rgb;
		if (hit.portalIndex >= 0) {
			// Portal tint.
			color = portalColors[hit.portalIndex] * (color + 0.5 * portalColors[hit.portalIndex]);
		}
		reflectance *= materials[hit.material].reflectance;
		ray.pos += ray.dir * rayEpsilon;
	}
}

void main() {
	Ray ray;
	ray.pos = vertRayPos;
	ray.dir = normalize(vertRayDir);
	ray.invDir = 1.0 / ray.dir;
	vec3 fragColor;
	trace(ray, fragColor);
	outFragColor = vec4(fragColor, 1.0);
}