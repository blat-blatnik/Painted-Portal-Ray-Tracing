#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define GLAD_IMPLEMENTATION
#include "glad.h"
#include "glfw3.h"
#include "utils.h"
#include "graphics.h"
#include "game.h"

// Request a dedicated GPU when avaliable
// See: https://stackoverflow.com/a/39047129
#ifdef _MSC_VER
extern "C" __declspec(dllexport) unsigned long NvOptimusEnablement = 1;
extern "C" __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
#endif

// Callback for when a GLFW error occurs
static void onGlfwError(int code, const char *desc) {
	printf("GLFW error 0x%X: %s\n", code, desc);
}
// Callback for when and OpenGL debug error occurs
static void onGlError(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam) {
	const char* severityMessage =
		severity == GL_DEBUG_SEVERITY_HIGH ?         "error" :
		severity == GL_DEBUG_SEVERITY_MEDIUM ?       "warning" :
		severity == GL_DEBUG_SEVERITY_LOW ?          "warning" :
		severity == GL_DEBUG_SEVERITY_NOTIFICATION ? "info" : 
		"unknown";
	const char *sourceMessage =
		source == GL_DEBUG_SOURCE_SHADER_COMPILER ? "glslc" :
		source == GL_DEBUG_SOURCE_API ?             "API" :
		source == GL_DEBUG_SOURCE_WINDOW_SYSTEM ?   "windows API" :
		source == GL_DEBUG_SOURCE_APPLICATION ?     "application" :
		source == GL_DEBUG_SOURCE_THIRD_PARTY ?     "third party" :
		"unknown";

	if (severity != GL_DEBUG_SEVERITY_NOTIFICATION)
		printf("OpenGL %s 0x%X: %s (source: %s)\n", severityMessage, (int)id, message, sourceMessage);
}

int main(int argc, char *argv[]) {
	// Initialize GLFW.
	glfwSetErrorCallback(onGlfwError);
	int glfwOk = glfwInit();
	assert(glfwOk == GLFW_TRUE);

	// We don't use depth and stencil buffers.
	glfwWindowHint(GLFW_DEPTH_BITS, 0);
	glfwWindowHint(GLFW_STENCIL_BITS, 0);
	// We need at least OpenGL 4.3 for shader buffers.
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifndef NDEBUG
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif

	// Create a window and make its OpenGL context current.
	GLFWwindow *window = glfwCreateWindow(1280, 720, "Painted Portal Tracer", NULL, NULL);
	assert(window);
	glfwMakeContextCurrent(window);

	// Enable Vsync.
	glfwSwapInterval(1);
	// Hide the mouse cursor and steal mouse focus.
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	// Load all OpenGL functions.
	int gladOk = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
	assert(gladOk);

	const char *renderer = (const char *)glGetString(GL_RENDERER);
	const char *version = (const char *)glGetString(GL_VERSION);
	printf("using OpenGL %s: %s\n", version, renderer);

	if (!(GLVersion.major >= 4 && GLVersion.minor >= 3)) {
		printf("ERROR: need at least OpenGL 4.3 to run .. exiting\n");
		abort();
	}

	// Enable gamma correction ..
	//NOTE: this is probably incorrect since we are rendering the scene TWICE
	//      so we will gamma correct it twice, making it too dark. Although I'm
	//      sure if this is actually what happens.
	glEnable(GL_FRAMEBUFFER_SRGB);

#ifndef NDEBUG
	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(onGlError, NULL);
	glCheckErrors();
#endif

	// Make GLFW call the games event functions.
	glfwSetKeyCallback(window, gameOnKey);
	glfwSetCursorPosCallback(window, gameOnMouseMove);
	glfwSetMouseButtonCallback(window, gameOnMouseButton);
	glfwSetScrollCallback(window, gameOnMouseWheel);
	gameInit(window);

	// Start the game loop.
	uint64_t t0 = glfwGetTimerValue();
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		checkTrackedFiles(); // Check if the shaders were changed and reload them.
		uint64_t t1 = glfwGetTimerValue();
		gameUpdate((t1 - t0) / (double)glfwGetTimerFrequency());
		glCheckErrors();
		t0 = t1;
	}

	// Destroy all used resources and end the program.
	gameTerminate();
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}