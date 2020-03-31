#pragma once

#include "glfw3.h"

// These are all of the events that the game hooks into.
// They will be called automatically at the appropriate time.

void gameOnKey(GLFWwindow *, int key, int scancode, int action, int mods);
void gameOnMouseMove(GLFWwindow *, double newX, double newY);
void gameOnMouseButton(GLFWwindow *, int button, int action, int mods);
void gameOnMouseWheel(GLFWwindow *, double dX, double dY);
void gameInit(GLFWwindow *window);
void gameTerminate();
void gameUpdate(double deltaTime);