#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "utils.h"
#include "glad.h"
#include <stdio.h>
#include <vector>

#ifndef NDEBUG
// Helper function-macro that calls glGetError and checks the result.
#define glCheckErrors()\
	do {\
		GLenum code = glGetError();\
		if (code != GL_NO_ERROR) {\
			const char *desc;\
			switch (code) {\
				case GL_INVALID_ENUM:      desc = "Invalid Enum";      break;\
				case GL_INVALID_VALUE:     desc = "Invalid Value";     break;\
				case GL_INVALID_OPERATION: desc = "Invalid Operation"; break;\
				case GL_STACK_OVERFLOW:    desc = "Stack Overflow";    break;\
				case GL_STACK_UNDERFLOW:   desc = "Stack Underflow";   break;\
				case GL_OUT_OF_MEMORY:     desc = "Out of Memory";     break;\
				case GL_INVALID_FRAMEBUFFER_OPERATION: desc = "Invalid Framebuffer Operation"; break;\
				default: desc = "Unknown Error"; break;\
			}\
			printf("OpenGL error 0x%X: %s in %s%d (%s)\n", code, desc, __FILE__, (int)__LINE__, __func__);\
			assert(0);\
		}\
	} while (0)
#else
#define glCheckErrors() do {} while(0)
#endif // NDEBUG

// For example GL_ARRAY_BUFFER, or GL_SHADER_STORAGE_BUFFER..
typedef GLenum BufferSlot;

// For example GL_FLOAT, or GL_FLOAT_VEC2, ..
typedef GLenum ShaderDataType;

// Must be one of GL_NEAREST, GL_LINEAR, or GL_LINEAR_MIPMAP_LINEAR.
typedef GLenum TextureFilter;

// For example GL_RGBA8, or GL_RGB16F
typedef GLenum TextureStoreFormat;

// Represents an OpenGL buffer object.
typedef GLuint GpuBuffer;

// Represents an OpenGL Texture2D object.
typedef GLuint Texture;

// Represents an OpenGL Texture2DArray object.
typedef GLuint TextureArray;

// Represents an OpenGL ShaderProgram object.
typedef GLuint Shader;

// glGenBuffers + glBufferData
GpuBuffer createGpuBuffer(void *data, size_t size);
// glBufferData
void recreateGpuBuffer(GpuBuffer buffer, const void *data, size_t size);
// glBufferSubData
void updateGpuBuffer(GpuBuffer buffer, size_t offset, const void *data, size_t size);
// glBindBuffer or glBindBufferBase depending on the slot and the binding
void bindGpuBuffer(GpuBuffer buffer, BufferSlot slot, int binding);
// glBindBufferRange
void bindGpuBuffer(GpuBuffer buffer, BufferSlot slot, int binding, size_t offset, size_t size);
// glDeleteBuffers
void destroyGpuBuffer(GpuBuffer buffer);

// glGenTextures + glTexImage2D
Texture createTexture(const void *pixels, uint width, uint height, TextureStoreFormat internalFormat);
// Reads the pixels from the given image file and then calls createTexture from them
Texture loadTexture(const char *filename, TextureStoreFormat internalFormat);
// glActiveTexture + glBindTexture
void bindTexture(Texture tex, uint unit);
// glDeleteTextures
void destroyTexture(Texture tex);

// glGenTextures + glTexStorage3D + glTexSubImage3D for each sub image in the array
TextureArray loadTextureArray(const char *filenames[], uint numFilenames, TextureStoreFormat internalFormat);
// glActiveTexture + glBindTexture
void bindTextureArray(TextureArray tex, uint unit);
// glDeleteTextures
void destroyTextureArray(TextureArray tex);

// Loads and compiles a shader program from the specified vertex and fragement shader.
// If compile or link errors occur they are printed to stdout.
Shader loadShader(const char *vertFile, const char *fragFile);
// glUseProgram
void bindShader(Shader s);
// glDeleteProgram
void destroyShader(Shader s);
// Calls the appropriate glUniform* based on the 'type'.
void setUniform(Shader s, uint location, ShaderDataType type, const void *value, size_t valueSize);

//
// Convinience functions that wraps setUniform above based on the type.
//
template <class T> inline void setUniform(Shader s, uint location, ShaderDataType type, const T &value) {
	setUniform(s, location, type, &value, sizeof(value));
}
inline void setUniform(Shader s, uint location, float value) {
	setUniform(s, location, GL_FLOAT, &value, sizeof(value));
}
inline void setUniform(Shader s, uint location, vec2 value) {
	setUniform(s, location, GL_FLOAT_VEC2, &value, sizeof(value));
}
inline void setUniform(Shader s, uint location, vec3 value) {
	setUniform(s, location, GL_FLOAT_VEC3, &value, sizeof(value));
}
inline void setUniform(Shader s, uint location, vec4 value) {
	setUniform(s, location, GL_FLOAT_VEC4, &value, sizeof(value));
}
inline void setUniform(Shader s, uint location, mat2 value) {
	setUniform(s, location, GL_FLOAT_MAT2, &value, sizeof(value));
}
inline void setUniform(Shader s, uint location, mat3 value) {
	setUniform(s, location, GL_FLOAT_MAT3, &value, sizeof(value));
}
inline void setUniform(Shader s, uint location, mat4 value) {
	setUniform(s, location, GL_FLOAT_MAT4, &value, sizeof(value));
}
inline void setUniform(Shader s, uint location, int value) {
	setUniform(s, location, GL_INT, &value, sizeof(value));
}
inline void setUniform(Shader s, uint location, ivec2 value) {
	setUniform(s, location, GL_INT_VEC2, &value, sizeof(value));
}
inline void setUniform(Shader s, uint location, ivec3 value) {
	setUniform(s, location, GL_INT_VEC3, &value, sizeof(value));
}
inline void setUniform(Shader s, uint location, ivec4 value) {
	setUniform(s, location, GL_INT_VEC4, &value, sizeof(value));
}
inline void setUniform(Shader s, uint location, uint value) {
	setUniform(s, location, GL_UNSIGNED_INT, &value, sizeof(value));
}
inline void setUniform(Shader s, uint location, uvec2 value) {
	setUniform(s, location, GL_UNSIGNED_INT_VEC2, &value, sizeof(value));
}
inline void setUniform(Shader s, uint location, uvec3 value) {
	setUniform(s, location, GL_UNSIGNED_INT_VEC3, &value, sizeof(value));
}
inline void setUniform(Shader s, uint location, uvec4 value) {
	setUniform(s, location, GL_UNSIGNED_INT_VEC4, &value, sizeof(value));
}

// An array-list datastructure that is synchronized between the GPU and CPU.
//
// Its basically an std::vector that is backed by a GPU buffer. It keeps track
// of which items in the std::vector have changed and then updates the GPU buffer
// accordingly. It can be bound just like a GPU buffer with the .bind() function.
//
// The synchronization is done in a lazy fashion - the underlying GPU buffer is
// only updated once .bind() is called. Until then, all changes to the list are
// only visible on the CPU.
//
// The way this works is that we store a separate flag for each item in the
// std::vector that tells us whether that particular item has changed since the
// last call to .bind(). Then, when .bind() is called, we loop through all of
// the items, and update all the items that have been flagged as "dirty".
template <class T> struct GpuSyncedList {

	// Initialize a GPU sync list with the given initial capacity.
	void create(size_t initialCapacity) {
		items.reserve(initialCapacity);
		dirtyBits.reserve(initialCapacity);
		gpuBuffer = createGpuBuffer(NULL, items.capacity() * sizeof(T));
		gpuBufferCapacity = items.capacity();
	}

	// destroy the sync list and free all of it's memory.
	void destroy() {
		destroyGpuBuffer(gpuBuffer);
		gpuBufferCapacity = 0;
	}

	// Push an item to the end of the GPU sync list.
	void push(T item) {
		items.push_back(item);
		dirtyBits.push_back(true);
	}

	// Pop the last item off of the GPU sync list.
	T pop() {
		assert(items.size() > 0);
		T item = items.back();
		items.pop_back();
		dirtyBits.pop_back();
		return item;
	}

	// Return number of items in the sync list.
	size_t length() {
		return items.size();
	}

	// Remove an item from the specified index.
	void remove(size_t index) {
		assert(index >= 0 && index < items.size());
		items.erase(items.begin() + (int)index);
		dirtyBits.pop_back();
		// The last part of the array was shifted by 1 element so we have to mark that whole region.
		for (size_t i = index; i < items.size(); ++i) {
			dirtyBits[i] = true;
		}
	}

	// Bind the GPU sync list to a GPU buffer slot.
	void bind(BufferSlot slot, int binding) {
		if (gpuBufferCapacity < items.capacity()) {
			// Capacity changed, so we have to reallocate the buffer on the GPU.
			recreateGpuBuffer(gpuBuffer, NULL, items.capacity() * sizeof(T));
			updateGpuBuffer(gpuBuffer, 0, items.data(), items.size() * sizeof(T));
			gpuBufferCapacity = items.capacity();
			dirtyBits.clear();
			dirtyBits.resize(items.size(), false);
		}
		else {
			// Update only the items that were marked as "dirty".
			// We dont update each item individually, but rather we update sequences of dirty items.
			// So for example if we had 8 items, and consecutive dirty items (marked as "D") like this:
			//
			// _ D D D _ _ D D
			//
			// We would update them in a batch like this:
			//
			// _[D D D]_ _[D D]
			//
			// This can save a few GPU transfer operations compared to doing each item individually.
			int startIdx = -1;
			for (size_t i = 0; i <= items.size(); ++i) { // this loop goes 1 past the end of the items
				bool dirty = i < items.size() ? dirtyBits[i] : false;
				if (dirty && startIdx < 0) {
					// record start of a range
					startIdx = (int)i;
				}
				else if (!dirty && startIdx >= 0) {
					// found the end of the range - update it
					size_t start = (size_t)startIdx;
					size_t size = i - start;
					updateGpuBuffer(gpuBuffer, start * sizeof(T), &items[start], size * sizeof(T));
					for (size_t j = 0; j < size; ++j) {
						dirtyBits[start + j] = false;
					}
				}
			}
		}

		//
		// ----- Everything is synchronized beyond this point -----
		//

		if (items.size() > 0) {
			// Apparently its an error to bind an empty range so we need to check for that..
			bindGpuBuffer(gpuBuffer, slot, binding, 0, items.size() * sizeof(T));
		}
	}

	// We want to control when each item is modified so we can mark it as "dirty",
	// but we still want to have nice and intuitive array access to the items. So we
	// wrap each item from the list into this special struct which can be implicitly
	// converted to the item type, and can be assigned to.
	struct Wrapper {
		GpuSyncedList* list;
		size_t index;
		operator T() {
			return list->items[index];
		}
		Wrapper& operator =(T item) {
			// This is the whole point of this Wrapper, we can detect when the items are over-written.
			list->items[index] = item;
			list->dirtyBits[index] = true;
			return *this;
		}
	};

	// Returns a wrapper to the item at the requested index.
	Wrapper operator [](size_t index) {
		assert(index < items.size());
		Wrapper w;
		w.list = this;
		w.index = index;
		return w;
	}

private:
	GpuBuffer gpuBuffer;
	size_t gpuBufferCapacity;
	std::vector<T> items;
	std::vector<bool> dirtyBits;
};

#endif