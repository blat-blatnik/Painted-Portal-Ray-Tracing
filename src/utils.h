#ifndef UTILS_H
#define UTILS_H

#include <assert.h>
#include <stdio.h>
#include "glad.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

using namespace glm;

// Read the whole file into a malloc'ed buffer and return it. If access to the file is blocked for whatever reason
// the function will keep trying to open the file for "timeout" seconds before giving up and returning a NULL pointer.
// The timeout is necessary because when a file is changed it might take a bit of time for the program that changed 
// it to close its handle to the file, and until that happens we won't be able to open it - so we have to wait.
char *readWholeFile(const char *filename, size_t *outSize, double timeout);

// Track if the contents of the given file ever change, and call a specified function when this happens.
void trackFileChanges(const char *filename, void *userData, bool(*callback)(const char *filename, void *userData));

// Checks if the contents of all tracked files have changed and triggers the appropriate callbacks.
// This should ideally be called once per frame.
void checkTrackedFiles();

#endif