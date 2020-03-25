#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "glfw3.h"

// Stores everything we need to know about a file thats being tracked.
struct FileTrackData {
	char *filename;
	void *userData;
	bool(*callback)(const char *filename, void *userData);
	uint64_t lastChangeTime;
};

// Makeshift "vector" that stores all the tracked files.
// We want to use as little C++ features as possible.
static FileTrackData *trackedFiles;
static int numTrackedFiles;

// =------------------------------=
// |--- PLATFORM SPECIFIC CODE ---|
// =------------------------------=

// Return a timestamp for the time the file was last changed
static uint64_t getFileTime(const char *filename) {
	struct stat result;
	if (stat(filename, &result)) return 0;
	return (uint64_t)result.st_mtime;
}

// =-------------------------------------=
// |--- END OF PLATFORM SPECIFIC CODE ---|
// =-------------------------------------=

char *readWholeFile(const char *filename, size_t *outSize, double timeout) {
	FILE *f;
	uint64_t t0 = glfwGetTimerValue();
	uint64_t t1;
	// Spin-lock until we either open the file or the timeout expires.
	do {
		f = fopen(filename, "rb");
		t1 = glfwGetTimerValue();
	} while (!f && (t1 - t0) / (double)glfwGetTimerFrequency() < timeout);

	if (f) {
		// Adapted from: https://stackoverflow.com/a/14002993
		fseek(f, 0, SEEK_END);
		size_t fsize = (size_t)ftell(f);
		fseek(f, 0, SEEK_SET);
		char *string = (char *)malloc(fsize + 1);
		if (!string) return NULL;
		fread(string, 1, fsize, f);
		fclose(f);
		string[fsize] = 0;
		*outSize = fsize;
		return string;
	}
	else return NULL;
}

void trackFileChanges(const char *filename, void *userData, bool(*callback)(const char *filename, void *userData)) {
	// First check if the file even exists.
	FILE* f = fopen(filename, "r");
	if (!f) return;
	fclose(f);

	// If the file does exist then we add it to the list of tracked files.
	++numTrackedFiles;
	trackedFiles = (FileTrackData *)realloc(trackedFiles, numTrackedFiles * sizeof(*trackedFiles));
	FileTrackData *data = &trackedFiles[numTrackedFiles - 1];
	data->filename = (char *)malloc(1 + strlen(filename));
	strcpy(data->filename, filename);
	data->lastChangeTime = getFileTime(filename);
	data->userData = userData;
	data->callback = callback;
}

void checkTrackedFiles() {
	for (int i = 0; i < numTrackedFiles; ++i) {
		FileTrackData *data = &trackedFiles[i];
		uint64_t changeTime = getFileTime(data->filename);
		if (changeTime != data->lastChangeTime) {
			// Call the user function - if it returns true we keep tracking, if it
			// returns false we stop tracking this file.
			bool continueTracking = data->callback(data->filename, data->userData);
			if (continueTracking) {
				data->lastChangeTime = getFileTime(data->filename);
			} else {
				// Remove this file from the tracked list.
				free(data->filename);
				if (i < numTrackedFiles - 1) {
					memmove(&trackedFiles[i], &trackedFiles[i + 1], (size_t)numTrackedFiles - (size_t)i - 1);
				}
				--numTrackedFiles;
			}
		}
	}
}