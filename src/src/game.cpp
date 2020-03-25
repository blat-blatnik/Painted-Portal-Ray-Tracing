#include "graphics.h"
#include "game.h"
#include "glm/glm.hpp"

enum GameMode {
	PlayMode,
	BuildMode
};

struct Ray {
	vec3 pos;
	vec3 dir;
};

struct Light {
	alignas(sizeof(vec4)) vec3 pos;
	alignas(sizeof(vec4)) vec3 color;
};

struct Material {
	alignas(sizeof(vec4)) vec4 color;
	float reflectance;
	float ior;
	int textureIndex;
};

struct Plane {
	alignas(sizeof(vec4)) vec3 normal;
	alignas(sizeof(vec4)) vec3 pos;
	uint material;
};

struct Sphere {
	alignas(sizeof(vec4)) vec3 pos;
	float radius;
	uint material;
};

struct Voxel {
	alignas(sizeof(vec4)) ivec3 pos;
	uint material;
};

struct Portal {
	alignas(sizeof(vec4)) vec3 pos;
	alignas(sizeof(vec4)) vec3 normal;
	float radius;
};

static GLFWwindow* window;
static double cursorX, cursorY;

static Shader raytraceShader;
static Shader paintShader;
static GpuBuffer fullscreenQuad;
static TextureArray textureAtlas;
static GpuSyncedList<Light> lights;
static GpuSyncedList<Material> materials;
static GpuSyncedList<Plane> planes;
static GpuSyncedList<Sphere> spheres;
static GpuSyncedList<Voxel> voxels;
static GpuSyncedList<Portal> portals;
static uint raytraceOutputFramebuffer;
static uint fullscreenQuadVAO;
static Texture raytraceOutputTexture;

static const float jumpVelocity = 10;
static const float gravity = 40.0f;
static const float playerHeight = 0.9f;
static const float floatMax = 3.402823466e+38;
static const float rayEpsilon = 0.001f;
static const float PI = 3.141592741f;

static vec3 cameraPos = vec3(0, 10, 0);
static vec3 cameraDir = vec3(0, 0, -1);
static vec3 cameraUp = vec3(0, 1, 0);
static vec3 cameraRight = normalize(cross(cameraDir, cameraUp));
static float cameraFoveaDist = 1.5f;
static float cameraPitch;
static float cameraYaw;
static float velocityY = 0;
static bool doubleJumpReady = true;
static GameMode gameMode = PlayMode;
static uint material = 2;
static Light lightBuffer[100];

static mat3 getPortalMatrix(Portal portal) {
	vec3 n = normalize(portal.normal);
	vec3 b = vec3(0, 1, 0);
	if (abs(dot(n, b)) > 0.99) {
		b = vec3(1, 0, 0);
	}
	vec3 t = normalize(cross(n, b));
	b = normalize(cross(t, n));
	return transpose(mat3(t, b, n));
}
static float intersect(Ray r, Plane p) {
	const float epsilon = 0.001;
	float denom = dot(r.dir, p.normal);
	if (abs(denom) > epsilon) {
		float t = dot((p.pos - r.pos), p.normal) / denom;
		if (t > epsilon)
			return t;
	}
	return floatMax;
}
static float intersect(Ray r, Sphere s) {
	float a = 1;
	float b = 2 * dot(r.pos - s.pos, r.dir);
	float c = dot(s.pos, (s.pos - (float)2 * r.pos)) + dot(r.pos, r.pos) - s.radius * s.radius;
	float discriminant = b * b - 4 * a * c;
	if (discriminant < 0)
		return floatMax;
	else
		return (float)-0.5 * (b + sqrt(discriminant));
}
static float intersect(Ray r, Voxel v) {
	const float epsilon = 0.001;
	if (r.dir.x == 0) r.dir.x = epsilon;
	if (r.dir.y == 0) r.dir.y = epsilon;
	if (r.dir.z == 0) r.dir.z = epsilon;
	vec3 invDir = 1.0f / r.dir;
	vec3 ld = (vec3(v.pos) - r.pos) * invDir;
	vec3 rd = (vec3(v.pos) - r.pos) * invDir + invDir;
	vec3 mind = min(ld, rd);
	vec3 maxd = max(ld, rd);
	float dmin = max(max(mind.x, mind.y), mind.z);
	float dmax = min(min(maxd.x, maxd.y), maxd.z);
	if (dmin > dmax)
		return floatMax;
	else
		return dmin;
}
static float intersect(Ray r, Portal p) {
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
static Ray trace(Ray ray) {
	vec3 hitNormal;
	float hitDist = floatMax;

	for (uint i = 0; i < planes.length(); ++i) {
		Plane plane = planes[i];
		float d = intersect(ray, plane);
		if (d > 0 && d < hitDist) {
			hitDist = d;
			hitNormal = plane.normal;
		}
	}

	for (uint i = 0; i < spheres.length(); ++i) {
		Sphere sphere = spheres[i];
		float d = intersect(ray, sphere);
		if (d > 0 && d < hitDist) {
			hitDist = d;
			hitNormal = normalize(ray.pos + ray.dir * d - sphere.pos);
		}
	}

	for (uint i = 0; i < voxels.length(); ++i) {
		Voxel voxel = voxels[i];
		float d = intersect(ray, voxel);
		if (d > 0 && d < hitDist) {
			hitDist = d;
			vec3 hitPos = ray.pos + ray.dir * hitDist;
			hitNormal = normalize(vec3(ivec3(2.0001f * (hitPos - vec3(voxel.pos) - 0.5f))));
		}
	}

	ray.pos += ray.dir * hitDist;
	ray.dir = hitNormal;
	return ray;
}
static int getClosestPortal(vec3 pos) {
	Portal P1 = portals[0];
	Portal P2 = portals[1];
	if (distance(P1.pos, pos) < distance(P2.pos, pos))
		return 0;
	else
		return 1;
}
static void printPickedMaterial() {
	switch (material) {
		case 1:  printf("selected Cyan\n"); break;
		case 2:	 printf("selected Dirt\n"); break;
		case 3:	 printf("selected Dark Wood\n"); break;
		case 4:	 printf("selected Wood\n"); break;
		case 5:	 printf("selected Stone\n"); break;
		case 6:	 printf("selected Chisled Stone\n"); break;
		case 7:	 printf("selected Bricks\n"); break;
		case 8:	 printf("selected Quartz\n"); break;
		case 9:	 printf("selected Purple\n"); break;
		case 10: printf("selected Candy\n"); break;
		default: printf("selected material %d\n", (int)material); break;
	}
}
static float getDistanceToNearestObject(vec3 from, vec3 dir) {
	Ray r0 = { from, normalize(dir) };
	Ray r1 = trace(r0);
	return distance(r0.pos, r1.pos);
}
static vec3 moveWithCollisionCheck(vec3 from, vec3 dir, float epsilon = rayEpsilon) {
	float dist = length(dir);
	dir = normalize(dir);
	Ray movementRay = { from, dir };
	Ray movementRayEnd = trace(movementRay);
	float maxDist = max(0.0f, distance(movementRayEnd.pos, movementRay.pos) - epsilon);
	return from + min(dist, maxDist) * dir;
}
static void loadScene() {
	lights.create(24);
	lights.push({ { -1.81297, 5.7906, -4.21272 }, { 0.579913, 1.69076, 0.00375378 } });
	lights.push({ { -4.36842, 5.08229, -9.05002 }, { 1.43962, 1.75503, 2.42622 } });
	lights.push({ { 2.10183, 7.69307, -9.4391 }, { 2.46852, 2.68789, 1.05087 } });
	lights.push({ { 7.10277, 7.28197, -6.67717 }, { 2.57683, 0.522324, 2.23981 } });
	lights.push({ { 8.96205, 5.83229, 4.55122 }, { 0.911985, 1.5406, 2.1315 } });
	lights.push({ { 11.2218, 6.36449, 8.88403 }, { 1.09336, 0.274209, 0.0449538 } });
	lights.push({ { 7.63116, 8.77453, 9.26327 }, { 2.96558, 0.497696, 0.441939 } });
	lights.push({ { -0.356331, 6.98776, 5.23919 }, { 0.014008, 0.35725, 1.33708 } });
	lights.push({ { 0.0614676, 6.0846, 5.48466 }, { 1.59499, 1.13364, 0.0267342 } });
	lights.push({ { -6.23985, 5.89135, 6.44163 }, { 1.8215, 1.80529, 1.71355 } });
	lights.push({ { -10.1994, 6.52015, 9.18334 }, { 1.35237, 1.98914, 0.498703 } });
	lights.push({ { -15.5818, 4.15572, 11.3748 }, { 1.82305, 0.171117, 1.05637 } });
	lights.push({ { -18.3772, 8.64235, 9.35531 }, { 1.55965, 2.40782, 2.34996 } });
	lights.push({ { -16.2988, 9.69448, 5.41378 }, { 2.18003, 2.62792, 0.90585 } });
	lights.push({ { -17.5426, 8.04461, 0.952491 }, { 1.61806, 2.77715, 2.8677 } });
	lights.push({ { -18.897, 6.62192, -2.58986 }, { 0.705985, 1.38624, 0.427015 } });
	lights.push({ { -23.3005, 7.29912, -0.549724 }, { 2.33897, 0.628803, 2.58672 } });
	lights.push({ { -36.7211, 8.86283, -3.16538 }, { 2.99908, 2.99039, 2.53096 } });
	lights.push({ { -30.832, 5.55614, -7.45064 }, { 0.798639, 1.17731, 1.8345 } });
	lights.push({ { 9.27975, 9.88058, -11.054 }, { 0.0712302, 2.52043, 0.891842 } });
	lights.push({ { 7.80433, 6.77498, -4.62547 }, { 2.03162, 0.277871, 1.1276 } });
	lights.push({ { -4.00477, 2.49793, -2.55 }, { 2.75637, 0.026368, 0.168645 } });
	lights.push({ { -4.36956, 2.6189, 3.97399 }, { 1.76373, 0.818689, 0.827662 } });
	lights.push({ { -12.9949, 5.94974, -8.08428 }, { 2.17948, 2.51283, 2.07355 } });
	materials.create(12);
	materials.push({ { 0, 0, 0, 1 }, 0.00f, 1, -1 });
	materials.push({ { 0, 1, 1, 1 }, 0.20f, 1, -1 }); // CYAN
	materials.push({ { 1, 1, 1, 1 }, 0.00f, 1, 0 });  // DIRT
	materials.push({ { 1, 1, 1, 1 }, 0.05f, 1, 1 });  // WOOD DARK
	materials.push({ { 1, 1, 1, 1 }, 0.05f, 1, 2 });  // WOOD
	materials.push({ { 1, 1, 1, 1 }, 0.05f, 1, 3 });  // STONE
	materials.push({ { 1, 1, 1, 1 }, 0.10f, 1, 4 });  // CHISLED STONE
	materials.push({ { 1, 1, 1, 1 }, 0.05f, 1, 5 });  // BRICKS
	materials.push({ { 1, 1, 1, 1 }, 0.25f, 1, 6 });  // QUARTZ
	materials.push({ { 1, 1, 1, 1 }, 0.20f, 1, 7 });  // PURPLE
	materials.push({ { 1, 1, 1, 1 }, 0.30f, 1, 8 });  // CANDY
	materials.push({ { 0.5f, 0.2f, 0.1f, 1 }, 0.4f, 1, -1 });
	materials.push({ { 0.2f, 0.2f, 0.8f, 1 }, 0.3f, 1, -1 });
	planes.create(1);
	planes.push({ { 0, 1, 0 }, { 0, 0, 0 }, 1 });
	spheres.create(3);
	spheres.push({ { -0.5, 0.1, -3 }, 0.5, 12 });
	spheres.push({ { 0.5, 0.5, -4 }, 0.7, 11 });
	spheres.push({ { 0.1, 0.3, -2 }, 0.3, 10 });
	voxels.create(247);
	voxels.push({ { -132, 0, 71 }, 7 });
	voxels.push({ { -4, 0, -3 }, 2 });
	voxels.push({ { -5, 0, -3 }, 2 });
	voxels.push({ { -6, 0, -3 }, 2 });
	voxels.push({ { -4, 0, -4 }, 2 });
	voxels.push({ { -5, 0, -4 }, 2 });
	voxels.push({ { -6, 0, -4 }, 2 });
	voxels.push({ { -4, 0, -5 }, 2 });
	voxels.push({ { -5, 0, -5 }, 2 });
	voxels.push({ { -6, 0, -5 }, 2 });
	voxels.push({ { -5, 1, -6 }, 4 });
	voxels.push({ { -5, 3, -7 }, 4 });
	voxels.push({ { -5, 2, -6 }, 4 });
	voxels.push({ { -4, 1, -6 }, 4 });
	voxels.push({ { -4, 2, -6 }, 4 });
	voxels.push({ { -6, 1, -6 }, 4 });
	voxels.push({ { -6, 2, -6 }, 4 });
	voxels.push({ { -6, 3, -7 }, 4 });
	voxels.push({ { -4, 3, -7 }, 4 });
	voxels.push({ { -7, 3, -7 }, 4 });
	voxels.push({ { -7, 0, -6 }, 4 });
	voxels.push({ { -7, 1, -6 }, 4 });
	voxels.push({ { -7, 2, -6 }, 4 });
	voxels.push({ { -12, 0, -8 }, 6 });
	voxels.push({ { -12, 1, -8 }, 6 });
	voxels.push({ { -12, 4, -8 }, 6 });
	voxels.push({ { -12, 3, -8 }, 6 });
	voxels.push({ { -12, 2, -8 }, 6 });
	voxels.push({ { -13, 4, -9 }, 7 });
	voxels.push({ { -13, 4, -8 }, 7 });
	voxels.push({ { -14, 4, -9 }, 7 });
	voxels.push({ { -14, 4, -8 }, 7 });
	voxels.push({ { -19, 4, -9 }, 7 });
	voxels.push({ { -19, 4, -8 }, 7 });
	voxels.push({ { -20, 4, -9 }, 6 });
	voxels.push({ { -20, 4, -8 }, 6 });
	voxels.push({ { -20, 3, -9 }, 6 });
	voxels.push({ { -20, 3, -8 }, 6 });
	voxels.push({ { -21, 2, -9 }, 6 });
	voxels.push({ { -19, 5, 9 }, 9 });
	voxels.push({ { -17, 6, 5 }, 9 });
	voxels.push({ { -17, 5, 0 }, 9 });
	voxels.push({ { -17, 5, 1 }, 9 });
	voxels.push({ { -18, 5, 0 }, 9 });
	voxels.push({ { -18, 5, 1 }, 9 });
	voxels.push({ { -20, 4, -3 }, 9 });
	voxels.push({ { -13, 1, 12 }, 3 });
	voxels.push({ { -12, 1, 12 }, 3 });
	voxels.push({ { -12, 0, 13 }, 3 });
	voxels.push({ { -13, 0, 13 }, 3 });
	voxels.push({ { -13, 1, 13 }, 3 });
	voxels.push({ { -12, 1, 13 }, 3 });
	voxels.push({ { -12, 2, 11 }, 3 });
	voxels.push({ { -13, 2, 11 }, 3 });
	voxels.push({ { -12, 3, 10 }, 3 });
	voxels.push({ { -13, 3, 10 }, 3 });
	voxels.push({ { -12, 3, 11 }, 3 });
	voxels.push({ { -13, 3, 11 }, 3 });
	voxels.push({ { -13, 3, 9 }, 4 });
	voxels.push({ { -12, 3, 9 }, 4 });
	voxels.push({ { -12, 3, 8 }, 4 });
	voxels.push({ { -13, 3, 7 }, 4 });
	voxels.push({ { -11, 3, 6 }, 4 });
	voxels.push({ { -11, 3, 5 }, 4 });
	voxels.push({ { -10, 3, 5 }, 4 });
	voxels.push({ { -9, 3, 6 }, 4 });
	voxels.push({ { -8, 3, 6 }, 4 });
	voxels.push({ { -8, 3, 5 }, 4 });
	voxels.push({ { -7, 3, 6 }, 4 });
	voxels.push({ { -6, 3, 6 }, 3 });
	voxels.push({ { -5, 0, 6 }, 3 });
	voxels.push({ { -5, 1, 6 }, 3 });
	voxels.push({ { -5, 2, 6 }, 3 });
	voxels.push({ { -5, 3, 6 }, 3 });
	voxels.push({ { -4, 3, 6 }, 3 });
	voxels.push({ { -3, 3, 6 }, 3 });
	voxels.push({ { -2, 3, 6 }, 3 });
	voxels.push({ { -1, 3, 6 }, 3 });
	voxels.push({ { 0, 3, 6 }, 3 });
	voxels.push({ { 1, 3, 6 }, 7 });
	voxels.push({ { 1, 3, 7 }, 7 });
	voxels.push({ { 1, 3, 5 }, 7 });
	voxels.push({ { 2, 4, 7 }, 7 });
	voxels.push({ { 2, 4, 5 }, 7 });
	voxels.push({ { 2, 4, 6 }, 6 });
	voxels.push({ { 2, 5, 5 }, 6 });
	voxels.push({ { 2, 5, 6 }, 6 });
	voxels.push({ { 2, 5, 7 }, 6 });
	voxels.push({ { 2, 6, 6 }, 6 });
	voxels.push({ { -24, 4, -1 }, 8 });
	voxels.push({ { -29, 5, -5 }, 8 });
	voxels.push({ { -35, 5, 0 }, 8 });
	voxels.push({ { -40, 6, -5 }, 9 });
	voxels.push({ { -40, 6, -6 }, 9 });
	voxels.push({ { -40, 6, -4 }, 9 });
	voxels.push({ { -41, 7, -4 }, 9 });
	voxels.push({ { -41, 7, -5 }, 9 });
	voxels.push({ { -41, 7, -6 }, 9 });
	voxels.push({ { -41, 8, -5 }, 9 });
	voxels.push({ { -41, 8, -6 }, 6 });
	voxels.push({ { -41, 8, -4 }, 6 });
	voxels.push({ { -6, 3, -8 }, 4 });
	voxels.push({ { -5, 3, -8 }, 4 });
	voxels.push({ { -4, 3, -8 }, 4 });
	voxels.push({ { -6, 3, -9 }, 4 });
	voxels.push({ { -5, 3, -9 }, 4 });
	voxels.push({ { -4, 3, -9 }, 4 });
	voxels.push({ { -6, 3, -10 }, 4 });
	voxels.push({ { -5, 3, -10 }, 4 });
	voxels.push({ { -4, 3, -10 }, 4 });
	voxels.push({ { -3, 3, -7 }, 4 });
	voxels.push({ { -3, 3, -8 }, 4 });
	voxels.push({ { -3, 3, -9 }, 4 });
	voxels.push({ { -3, 3, -10 }, 4 });
	voxels.push({ { -2, 3, -7 }, 4 });
	voxels.push({ { -2, 3, -8 }, 4 });
	voxels.push({ { -2, 3, -9 }, 4 });
	voxels.push({ { -2, 3, -10 }, 4 });
	voxels.push({ { -1, 3, -7 }, 4 });
	voxels.push({ { -1, 3, -8 }, 4 });
	voxels.push({ { -1, 3, -9 }, 4 });
	voxels.push({ { -1, 3, -10 }, 4 });
	voxels.push({ { -6, 3, -11 }, 4 });
	voxels.push({ { -5, 3, -11 }, 4 });
	voxels.push({ { -4, 3, -11 }, 4 });
	voxels.push({ { -3, 3, -11 }, 4 });
	voxels.push({ { -2, 3, -11 }, 4 });
	voxels.push({ { -1, 3, -11 }, 4 });
	voxels.push({ { 0, 3, -7 }, 7 });
	voxels.push({ { 0, 3, -9 }, 7 });
	voxels.push({ { 0, 3, -8 }, 7 });
	voxels.push({ { 0, 3, -10 }, 7 });
	voxels.push({ { 0, 3, -11 }, 7 });
	voxels.push({ { -1, 3, -12 }, 7 });
	voxels.push({ { -5, 3, -12 }, 7 });
	voxels.push({ { -4, 3, -12 }, 7 });
	voxels.push({ { -3, 3, -12 }, 7 });
	voxels.push({ { -2, 3, -12 }, 7 });
	voxels.push({ { 0, 3, -12 }, 7 });
	voxels.push({ { -6, 3, -12 }, 7 });
	voxels.push({ { 1, 4, -7 }, 9 });
	voxels.push({ { 1, 4, -8 }, 9 });
	voxels.push({ { 1, 4, -9 }, 9 });
	voxels.push({ { 1, 4, -10 }, 9 });
	voxels.push({ { 1, 4, -11 }, 9 });
	voxels.push({ { 1, 4, -12 }, 9 });
	voxels.push({ { 2, 4, -12 }, 9 });
	voxels.push({ { 2, 4, -11 }, 9 });
	voxels.push({ { 2, 4, -10 }, 9 });
	voxels.push({ { 2, 4, -9 }, 9 });
	voxels.push({ { 3, 4, -7 }, 3 });
	voxels.push({ { 3, 4, -9 }, 3 });
	voxels.push({ { 3, 4, -11 }, 3 });
	voxels.push({ { 3, 4, -12 }, 3 });
	voxels.push({ { 3, 4, -10 }, 3 });
	voxels.push({ { 3, 4, -8 }, 3 });
	voxels.push({ { 4, 4, -12 }, 3 });
	voxels.push({ { 4, 4, -11 }, 3 });
	voxels.push({ { 4, 4, -10 }, 3 });
	voxels.push({ { 4, 4, -9 }, 3 });
	voxels.push({ { 4, 4, -8 }, 3 });
	voxels.push({ { 4, 4, -7 }, 3 });
	voxels.push({ { 5, 4, -12 }, 3 });
	voxels.push({ { 5, 4, -10 }, 3 });
	voxels.push({ { 5, 4, -9 }, 3 });
	voxels.push({ { 5, 4, -8 }, 3 });
	voxels.push({ { 5, 4, -7 }, 3 });
	voxels.push({ { 5, 4, -11 }, 3 });
	voxels.push({ { 6, 4, -12 }, 3 });
	voxels.push({ { 6, 4, -11 }, 3 });
	voxels.push({ { 6, 4, -10 }, 3 });
	voxels.push({ { 6, 4, -9 }, 3 });
	voxels.push({ { 6, 4, -8 }, 3 });
	voxels.push({ { 6, 4, -7 }, 3 });
	voxels.push({ { 7, 4, -10 }, 3 });
	voxels.push({ { 7, 4, -9 }, 3 });
	voxels.push({ { 7, 4, -8 }, 3 });
	voxels.push({ { 7, 4, -7 }, 3 });
	voxels.push({ { 6, 4, -6 }, 3 });
	voxels.push({ { 7, 4, -6 }, 3 });
	voxels.push({ { 8, 4, -8 }, 3 });
	voxels.push({ { 8, 4, -7 }, 3 });
	voxels.push({ { 8, 4, -6 }, 3 });
	voxels.push({ { 9, 4, -8 }, 3 });
	voxels.push({ { 9, 4, -7 }, 3 });
	voxels.push({ { 6, 4, -5 }, 3 });
	voxels.push({ { 7, 4, -5 }, 3 });
	voxels.push({ { 8, 4, -5 }, 3 });
	voxels.push({ { 9, 4, -6 }, 3 });
	voxels.push({ { 9, 4, -5 }, 3 });
	voxels.push({ { 7, 2, -4 }, 2 });
	voxels.push({ { 7, 3, -4 }, 2 });
	voxels.push({ { 8, 3, -4 }, 2 });
	voxels.push({ { 8, 2, -4 }, 2 });
	voxels.push({ { 7, 4, -4 }, 2 });
	voxels.push({ { 8, 4, -4 }, 2 });
	voxels.push({ { 6, 4, -4 }, 2 });
	voxels.push({ { 9, 4, -4 }, 2 });
	voxels.push({ { 8, 3, 7 }, 10 });
	voxels.push({ { 9, 3, 7 }, 10 });
	voxels.push({ { 8, 3, 6 }, 10 });
	voxels.push({ { 7, 3, 7 }, 10 });
	voxels.push({ { 8, 3, 8 }, 10 });
	voxels.push({ { 8, 3, 5 }, 10 });
	voxels.push({ { 10, 3, 7 }, 10 });
	voxels.push({ { 8, 3, 9 }, 10 });
	voxels.push({ { 6, 3, 7 }, 10 });
	voxels.push({ { 9, 3, 6 }, 9 });
	voxels.push({ { 7, 3, 6 }, 9 });
	voxels.push({ { 9, 3, 8 }, 9 });
	voxels.push({ { 9, 3, 9 }, 9 });
	voxels.push({ { 10, 3, 9 }, 9 });
	voxels.push({ { 10, 3, 8 }, 9 });
	voxels.push({ { 9, 3, 5 }, 9 });
	voxels.push({ { 10, 3, 5 }, 9 });
	voxels.push({ { 10, 3, 6 }, 9 });
	voxels.push({ { 6, 3, 6 }, 9 });
	voxels.push({ { 7, 3, 5 }, 9 });
	voxels.push({ { 6, 3, 5 }, 9 });
	voxels.push({ { 7, 4, 8 }, 4 });
	voxels.push({ { 7, 4, 9 }, 4 });
	voxels.push({ { 7, 5, 8 }, 4 });
	voxels.push({ { 7, 5, 9 }, 4 });
	voxels.push({ { 6, 6, 8 }, 4 });
	voxels.push({ { 6, 6, 9 }, 4 });
	voxels.push({ { 5, 6, 8 }, 4 });
	voxels.push({ { 5, 6, 9 }, 4 });
	voxels.push({ { 4, 6, 8 }, 4 });
	voxels.push({ { 4, 6, 9 }, 4 });
	voxels.push({ { -31, 2, -8 }, 8 });
	voxels.push({ { 9, 6, -12 }, 6 });
	voxels.push({ { 9, 6, -11 }, 6 });
	voxels.push({ { 9, 6, -10 }, 6 });
	voxels.push({ { 9, 6, -9 }, 6 });
	voxels.push({ { 7, 0, -3 }, 4 });
	voxels.push({ { 8, 0, -3 }, 4 });
	voxels.push({ { 7, 1, -3 }, 4 });
	voxels.push({ { 8, 1, -3 }, 4 });
	voxels.push({ { -21, 2, -8 }, 6 });
	voxels.push({ { 2, 4, -8 }, 9 });
	voxels.push({ { 2, 4, -7 }, 9 });
	voxels.push({ { 7, 4, -11 }, 3 });
	voxels.push({ { 7, 4, -12 }, 3 });
	voxels.push({ { 8, 6, -12 }, 10 });
	voxels.push({ { 8, 6, -9 }, 10 });
	voxels.push({ { 8, 6, -11 }, 10 });
	voxels.push({ { 8, 6, -10 }, 10 });
	portals.create(2);
	portals.push({ { 1.999f, 5.46093f, 6.43585f }, { -1, 0, 0 }, 0.6f });
	portals.push({ { -39.999f, 7.67798f, -4.46772f }, { 1, 0, 0 }, 0.6f });

	for (size_t i = 0; i < lights.length(); ++i) {
		lightBuffer[i] = lights[i];
		lightBuffer[i].color.x = 5 * (rand() / (float)RAND_MAX - 0.5f);
		lightBuffer[i].color.y = 5 * (rand() / (float)RAND_MAX - 0.5f);
		lightBuffer[i].color.z = 5 * (rand() / (float)RAND_MAX - 0.5f);
	}
}

void gameOnKey(GLFWwindow*, int key, int scancode, int action, int mods) {
	if (action == GLFW_PRESS) {
		if (key == GLFW_KEY_ESCAPE) {
			glfwSetWindowShouldClose(window, GLFW_TRUE);
		}
		else if (key == GLFW_KEY_F) {
			GLFWmonitor* monitor = glfwGetWindowMonitor(window);
			GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
			int width, height;
			glfwGetMonitorWorkarea(primaryMonitor, NULL, NULL, &width, &height);
			if (!monitor) {
				glfwSetWindowMonitor(window, primaryMonitor, 0, 0, width, height, 60);
			}
			else {
				glfwSetWindowMonitor(window, NULL, (width - 1280) / 2, (height - 720) / 2, 1280, 720, 60);
			}
		}
		else if (key == GLFW_KEY_SPACE) {
			if (velocityY == 0) {
				velocityY = jumpVelocity;
			}
			else if (doubleJumpReady) {
				velocityY = jumpVelocity;
				doubleJumpReady = false;
			}
		}
		else if (key == GLFW_KEY_P) {
			gameMode = PlayMode;
			printf("now in Play Mode\n");
		}
		else if (key == GLFW_KEY_B) {
			gameMode = BuildMode;
			printf("now in Build Mode\n");
		}

		if (gameMode == BuildMode) {
			switch (key) {
			case GLFW_KEY_0:
			case GLFW_KEY_1:
			case GLFW_KEY_2:
			case GLFW_KEY_3:
			case GLFW_KEY_4:
			case GLFW_KEY_5:
			case GLFW_KEY_6:
			case GLFW_KEY_7:
			case GLFW_KEY_8:
			case GLFW_KEY_9:
				material = (uint)(1 + key - GLFW_KEY_0);
				printPickedMaterial();
				break;
			default: break;
			}
		}
	}
}
void gameOnMouseMove(GLFWwindow*, double newX, double newY) {
	double dX = newX - cursorX;
	double dY = newY - cursorY;
	cursorX = newX;
	cursorY = newY;

	const double sensitivity = 0.002;
	cameraPitch -= (float)(sensitivity * dX);
	cameraYaw -= (float)(sensitivity * dY);
	if (cameraYaw >= 0.99f * PI / 2) {
		cameraYaw = 0.99f * PI / 2;
	}
	else if (cameraYaw <= -0.99f * PI / 2) {
		cameraYaw = -0.99f * PI / 2;
	}

	mat4 rot = mat4(1);
	rot = rotate(rot, cameraPitch, vec3(0, 1, 0));
	rot = rotate(rot, cameraYaw, vec3(1, 0, 0));
	cameraDir = normalize(vec3(rot * vec4(0, 0, -1, 0)));
	cameraRight = normalize(cross(cameraDir, cameraUp));
}
void gameOnMouseButton(GLFWwindow*, int button, int action, int mods) {
	if (action == GLFW_PRESS) {
		Ray r1 = { cameraPos, cameraDir };
		Ray r = trace(r1);
		if (gameMode == PlayMode) {
			Portal newPortal;
			newPortal.pos = r.pos;
			newPortal.normal = r.dir;
			newPortal.pos += rayEpsilon * r.dir;
			newPortal.radius = 0.7f;
			if (button == GLFW_MOUSE_BUTTON_LEFT) {
				portals[0] = newPortal;
			}
			if (button == GLFW_MOUSE_BUTTON_RIGHT) {
				portals[1] = newPortal;
			}
		}
		else if (gameMode == BuildMode) {
			if (button == GLFW_MOUSE_BUTTON_LEFT) {
				Voxel newV;
				newV.pos = (ivec3)floor((r.pos + 0.5f * r.dir));
				newV.material = material;
				voxels.push(newV);
			}
			if (button == GLFW_MOUSE_BUTTON_RIGHT) {
				float hitDist = floatMax;
				float closestDist = hitDist;
				size_t voxIdx = voxels.length();
				for (size_t i = 0; i < voxels.length(); ++i) {
					Voxel voxel = voxels[i];
					float d = intersect(r1, voxel);
					if (d > 0 && d < hitDist) {
						if (closestDist > d) {
							closestDist = d;
							voxIdx = i;
						}
					}
				}

				if (voxIdx < voxels.length())
					voxels.remove(voxIdx);
			}
		}
	}
}
void gameOnMouseWheel(GLFWwindow*, double dX, double dY) {
	if (gameMode == PlayMode) {
		float delta = 1.1f;
		if (dY > 0) cameraFoveaDist *= delta;
		if (dY < 0) cameraFoveaDist /= delta;
	}
	else if (gameMode == BuildMode) {
		if (dY > 0) material++;
		if (dY < 0) material--;

		if (material < 1) material = 1;
		if (material > 10) material = 10;

		printPickedMaterial();
	}
}
void gameOnInit(GLFWwindow *w) {
	window = w;
	glfwGetCursorPos(window, &cursorX, &cursorY);

	// Create a framebuffer for the raytracer output
	glGenFramebuffers(1, &raytraceOutputFramebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, raytraceOutputFramebuffer);
	raytraceOutputTexture = createTexture(NULL, 256, 256, GL_RGB16F);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, raytraceOutputTexture, 0);
	assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glCheckErrors();

	const char* textureFiles[] = {
		"textures/dirt.png",          // 0
		"textures/wood_dark.png",     // 1
		"textures/wood.png",          // 2
		"textures/stone.png",         // 3
		"textures/chisled_stone.png", // 4
		"textures/bricks.png",        // 5
		"textures/quartz.png",        // 6
		"textures/purple.png",        // 7
		"textures/candy.png",         // 8
	};

	uint numTextures = sizeof(textureFiles) / sizeof(textureFiles[0]);
	textureAtlas = loadTextureArray(textureFiles, numTextures, GL_RGB8);
	glCheckErrors();

	raytraceShader = loadShader("shaders/rayvert.glsl", "shaders/rayfrag.glsl");
	paintShader = loadShader("shaders/paintvert.glsl", "shaders/paintfrag.glsl");
	
	vec2 vertData[] = {
		{ -1, 1 },
		{ -1,-1 },
		{  1, 1 },
		{  1,-1 }
	};
	glGenVertexArrays(1, &fullscreenQuadVAO);
	glBindVertexArray(fullscreenQuadVAO);
	fullscreenQuad = createGpuBuffer(vertData, sizeof(vertData));
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, false, sizeof(vec2), 0);

	loadScene();
}
void gameOnTerminate() {
	destroyShader(paintShader);
	destroyShader(raytraceShader);
	destroyGpuBuffer(fullscreenQuad);
	destroyTextureArray(textureAtlas);
	lights.destroy();
	materials.destroy();
	planes.destroy();
	spheres.destroy();
	voxels.destroy();
	portals.destroy();
	glCheckErrors();
}
void gameOnUpdate(double dt) {
	float moveSpeed = (float)dt * 5;
	if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
		moveSpeed *= 2.0f;

	vec3 moveDir = vec3(0);
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
		moveDir.z += 1;
	}
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
		moveDir.z -= 1;
	}
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
		moveDir.x -= 1;
	}
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
		moveDir.x += 1;
	}
	if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
		moveDir.y -= 1;
	}
	if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
		moveDir.y += 1;
	}

	if (gameMode == PlayMode)
		moveDir.y = 0;
	if (moveDir != vec3(0))
		moveDir = moveSpeed * normalize(moveDir);

	vec3 deltaPos =
		moveDir.x * cameraRight +
		moveDir.y * cameraUp +
		moveDir.z * cross(cameraUp, cameraRight);

	if (gameMode == PlayMode) {
		deltaPos.y = 0;

		vec3 dpos = vec3(deltaPos.x, velocityY * (float)dt, deltaPos.z);
		Ray cameraRay = { cameraPos, normalize(dpos) };
		int portalInIndex = getClosestPortal(cameraPos);
		Portal portalIn = portals[(size_t)portalInIndex];
		Portal portalOut = portals[(size_t)1 - portalInIndex];
		float cameraDistanceToPortal = intersect(cameraRay, portalIn);
		if (cameraDistanceToPortal < 0.5f) {

			portalIn.normal = -portalIn.normal;
			mat3 M1 = getPortalMatrix(portalIn);
			mat3 M2 = getPortalMatrix(portalOut);

			cameraPos = cameraRay.pos + cameraRay.dir * cameraDistanceToPortal;
			cameraPos = portalOut.pos + inverse(M2) * M1 * (cameraPos - portalIn.pos);
			cameraDir = normalize(inverse(M2) * M1 * cameraDir);
			cameraPos += cameraRay.dir * 0.1f;
			cameraYaw = atan(cameraDir.y);
			cameraPitch = PI + atan2(cameraDir.x, cameraDir.z);
			cameraPos += portalOut.normal * 0.1f;
		}

		if (velocityY > 0) {
			cameraPos = moveWithCollisionCheck(cameraPos, vec3(0, (float)dt * velocityY, 0));
		}
		else if (velocityY < 0) {
			vec3 feetPos0 = cameraPos - vec3(0, playerHeight, 0);
			vec3 feetPos1 = moveWithCollisionCheck(feetPos0, vec3(0, (float)dt * velocityY, 0), 0.01f);
			cameraPos += feetPos1 - feetPos0;
			if (distance(feetPos0, feetPos1) < 0.001f) {
				velocityY = 0;
				doubleJumpReady = true;
			}
		}

		if (deltaPos.x != 0)
			cameraPos = moveWithCollisionCheck(cameraPos, vec3(deltaPos.x, 0, 0), 0.1f);
		if (deltaPos.z != 0)
			cameraPos = moveWithCollisionCheck(cameraPos, vec3(0, 0, deltaPos.z), 0.1f);

		float headDist = getDistanceToNearestObject(cameraPos, vec3(0, 1, 0));
		if (headDist < 0.01f) {
			velocityY = min(0.0f, velocityY);
		}
		float feetDist = getDistanceToNearestObject(cameraPos, vec3(0, -1, 0));
		float fallingDist = feetDist;
		float edgeTolerance = 0.25f;
		fallingDist = min(fallingDist, getDistanceToNearestObject(cameraPos + vec3(-edgeTolerance, 0, -edgeTolerance), vec3(0, -1, 0)));
		fallingDist = min(fallingDist, getDistanceToNearestObject(cameraPos + vec3(-edgeTolerance, 0, +edgeTolerance), vec3(0, -1, 0)));
		fallingDist = min(fallingDist, getDistanceToNearestObject(cameraPos + vec3(+edgeTolerance, 0, -edgeTolerance), vec3(0, -1, 0)));
		fallingDist = min(fallingDist, getDistanceToNearestObject(cameraPos + vec3(+edgeTolerance, 0, +edgeTolerance), vec3(0, -1, 0)));
		if (fallingDist > playerHeight + 0.1f) {
			velocityY -= (float)dt * gravity;
		}
		if (feetDist < playerHeight) {
			headDist = getDistanceToNearestObject(cameraPos, vec3(0, -1, 0));
			cameraPos = cameraPos + feetDist * vec3(0, -1, 0) + vec3(0, playerHeight, 0);
			velocityY = max(velocityY, 0.0f);
			doubleJumpReady = true;
		}
	}
	else {
		cameraPos += deltaPos;
	}

	float t = (float)glfwGetTime();
	for (size_t i = 0; i < lights.length(); ++i) {
		Light l = lights[i];
		float freqx = lightBuffer[i].color.x;
		float freqz = lightBuffer[i].color.y;
		float amplitudex = max(0.4f, 0.4f * (freqx + freqz) * lightBuffer[i].color.z);
		float amplitudez = max(0.4f, 0.8f * (freqz - freqz) * lightBuffer[i].color.z);
		l.pos.x = lightBuffer[i].pos.x + amplitudex * cos(freqx * t);
		l.pos.z = lightBuffer[i].pos.z + amplitudez * sin(freqz * t);
		lights[i] = l;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, raytraceOutputFramebuffer);
	glViewport(0, 0, 256, 256);
	//glClear(GL_COLOR_BUFFER_BIT);

	bindShader(raytraceShader);
	lights.bind(GL_SHADER_STORAGE_BUFFER, 0);
	materials.bind(GL_SHADER_STORAGE_BUFFER, 1);
	planes.bind(GL_SHADER_STORAGE_BUFFER, 2);
	spheres.bind(GL_SHADER_STORAGE_BUFFER, 3);
	voxels.bind(GL_SHADER_STORAGE_BUFFER, 4);
	portals.bind(GL_SHADER_STORAGE_BUFFER, 5);
	bindTextureArray(textureAtlas, 0);

	int width, height;
	glfwGetFramebufferSize(window, &width, &height);
	mat4 view = lookAt(cameraPos, cameraPos + cameraDir, cameraUp);
	setUniform(raytraceShader, 0, vec2(width, height));
	setUniform(raytraceShader, 1, cameraFoveaDist);
	setUniform(raytraceShader, 2, cameraPos);
	setUniform(raytraceShader, 3, mat3(inverse(view)));
	setUniform(raytraceShader, 8, (float)glfwGetTime());
	setUniform(raytraceShader, 9, 0);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, width, height);
	bindShader(paintShader);
	setUniform(paintShader, 0, 0);
	setUniform(paintShader, 1, vec2(width, height));
	bindTexture(raytraceOutputTexture, 0);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glfwSwapBuffers(window);

	static int frameAcc = 0;
	static double timeAcc = 0;
	++frameAcc;
	timeAcc += dt;
	while (timeAcc >= 1) {
		char buffer[256];
		sprintf(buffer, "Painted Portal Tracer [%.1lf fps] - %s mode", frameAcc / timeAcc,
			gameMode == PlayMode ? "play" :
			gameMode == BuildMode ? "build" :
			"???");
		glfwSetWindowTitle(window, buffer);
		timeAcc = 0;
		frameAcc = 0;
	}
}