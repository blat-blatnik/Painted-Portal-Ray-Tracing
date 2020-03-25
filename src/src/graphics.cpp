#include "graphics.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <unordered_map>

struct ShaderSources {
	const char *fragFile;
	const char *vertFile;
};

static std::unordered_map<GLuint, ShaderSources> shaderSources;

GpuBuffer createGpuBuffer(void *data, size_t size) {
	GpuBuffer buffer;
	glGenBuffers(1, &buffer);
	assert(buffer);

	bindGpuBuffer(buffer, GL_ARRAY_BUFFER, 0);
	glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)size, data, GL_DYNAMIC_DRAW);

	glCheckErrors();
	return buffer;
}
void recreateGpuBuffer(GpuBuffer buffer, const void *data, size_t size) {
	bindGpuBuffer(buffer, GL_ARRAY_BUFFER, 0);
	glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)size, data, GL_DYNAMIC_DRAW);
	glCheckErrors();
}
void updateGpuBuffer(GpuBuffer buffer, size_t offset, const void *data, size_t size) {
	bindGpuBuffer(buffer, GL_ARRAY_BUFFER, 0);
	GLint bufferSize;
	glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &bufferSize);
	assert(offset + size <= (size_t)bufferSize);
	glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)offset, (GLsizeiptr)size, data);
	glCheckErrors();
}
void bindGpuBuffer(GpuBuffer buffer, BufferSlot slot, int binding) {
	switch (slot) {
		case GL_SHADER_STORAGE_BUFFER:
		case GL_UNIFORM_BUFFER:
		case GL_ATOMIC_COUNTER_BUFFER:
		case GL_TRANSFORM_FEEDBACK_BUFFER:
			// glBindBufferBase only makes sense for the buffer types above.
			glBindBufferBase((GLenum)slot, (GLuint)binding, buffer);
			break;
		default:
			// glBindBuffer is for all other buffer types.
			glBindBuffer((GLenum)slot, buffer);
			break;
	}
	glCheckErrors();
}
void bindGpuBuffer(GpuBuffer buffer, BufferSlot slot, int binding, size_t offset, size_t size) {
	assert(slot == GL_SHADER_STORAGE_BUFFER || slot == GL_UNIFORM_BUFFER || slot == GL_ATOMIC_COUNTER_BUFFER || slot == GL_TRANSFORM_FEEDBACK_BUFFER);
	glBindBufferRange((GLenum)slot, (GLuint)binding, buffer, (GLintptr)offset, (GLsizeiptr)size);
	glCheckErrors();
}
void destroyGpuBuffer(GpuBuffer buffer) {
	glDeleteBuffers(1, &buffer);
	glCheckErrors();
}

Texture createTexture(const void *pixels, uint width, uint height, TextureStoreFormat internalFormat) {
	Texture tex;
	glGenTextures(1, &tex);
	assert(tex);

	bindTexture(tex, 0);

	// Bilinear filtering and clamp to edges by default.
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glTexImage2D(
		GL_TEXTURE_2D,		   // Target texture type 
		0,					   // Mipmap level - ALWAYS 0
		(GLint)internalFormat, // Internal format
		(GLsizei)width,	       // Image width
		(GLsizei)height,       // Image height
		0,				       // Border? - always 0 aparently.
		GL_RGBA,		       // Color components - important not to mess up
		GL_UNSIGNED_BYTE,      // Component format
		pixels                 // Image data
	);

	glCheckErrors();
	return tex;
}
Texture loadTexture(const char *filename, TextureStoreFormat internalFormat) {
	int width, height, comp;
	stbi_uc *pixels = stbi_load(filename, &width, &height, &comp, STBI_rgb_alpha);
	assert(pixels);
	Texture tex = createTexture(pixels, (uint)width, (uint)height, internalFormat);
	assert(tex);
	stbi_image_free(pixels);
	return tex;
}
void bindTexture(Texture tex, uint unit) {
	assert(unit < 80);
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(GL_TEXTURE_2D, tex);
	glCheckErrors();
}
void destroyTexture(Texture tex) {
	glDeleteTextures(1, &tex);
	glCheckErrors();
}

TextureArray loadTextureArray(const char* filenames[], uint numFilenames, TextureStoreFormat internalFormat) {
	int width, height, comp;
	stbi_uc* pixels = stbi_load(filenames[0], &width, &height, &comp, STBI_rgb_alpha);
	assert(pixels);
	
	TextureArray tex;
	glGenTextures(1, &tex);
	assert(tex);

	// First allocate the full storage for the texture array.
	bindTextureArray(tex, 0);
	glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, (GLenum)internalFormat, width, height, (GLsizei)numFilenames);

	// Then read and copy the pixels over for each texture in the array.
	glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, width, height, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	stbi_image_free(pixels);
	for (uint i = 1; i < numFilenames; ++i) {
		int w, h, c;
		pixels = stbi_load(filenames[i], &w, &h, &c, STBI_rgb_alpha);
		assert(pixels);
		assert(w == width);
		assert(h == height);
		glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, (GLint)i, width, height, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
		stbi_image_free(pixels);
	}

	// Bilinear filtering and repeat wrapping by default.
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glCheckErrors();
	return tex;
}
void bindTextureArray(TextureArray tex, uint unit) {
	assert(unit < 80);
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
	glCheckErrors();
}
void destroyTextureArray(TextureArray tex) {
	glDeleteTextures(1, &tex);
	glCheckErrors();
}

// Return the size in bytes needed for a shader uniform of a given type.
static size_t sizeofShaderDataType(ShaderDataType type) {
	switch (type) {
		case GL_FLOAT:             return sizeof(float[1]);
		case GL_FLOAT_VEC2:        return sizeof(float[2]);
		case GL_FLOAT_VEC3:        return sizeof(float[3]);
		case GL_FLOAT_VEC4:        return sizeof(float[4]);
		case GL_FLOAT_MAT2:        return sizeof(float[2*2]);
		case GL_FLOAT_MAT3:        return sizeof(float[3*3]);
		case GL_FLOAT_MAT4:        return sizeof(float[4*4]);
		case GL_INT:               return sizeof(int[1]);
		case GL_INT_VEC2:          return sizeof(int[2]);
		case GL_INT_VEC3:          return sizeof(int[3]);
		case GL_INT_VEC4:          return sizeof(int[4]);
		case GL_UNSIGNED_INT:      return sizeof(uint[1]);
		case GL_UNSIGNED_INT_VEC2: return sizeof(uint[2]);
		case GL_UNSIGNED_INT_VEC3: return sizeof(uint[3]);
		case GL_UNSIGNED_INT_VEC4: return sizeof(uint[4]);
		case GL_SAMPLER_2D:
		case GL_SAMPLER_2D_ARRAY:
			return sizeof(int);
		default: assert(0); return 0;
	}
}
// Helper function that compiles and links a shader program.
static bool compileAndLinkShader(GLuint program, const char *vertFile, const char *fragFile) {
	size_t vertLen, fragLen;
	char *vertSrc = readWholeFile(vertFile, &vertLen, 0.01);
	char *fragSrc = readWholeFile(fragFile, &fragLen, 0.01);
	assert(vertSrc);
	assert(fragSrc);

	GLuint vert = glCreateShader(GL_VERTEX_SHADER);
	GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);
	assert(vert);
	assert(frag);

	glShaderSource(vert, 1, &vertSrc, NULL);
	glCompileShader(vert);
	GLint vertOk;
	glGetShaderiv(vert, GL_COMPILE_STATUS, &vertOk);
	if (!vertOk) {
	#ifndef NDEBUG
		GLint logLength;
		glGetShaderiv(vert, GL_INFO_LOG_LENGTH, &logLength);
		char *log = (char *)malloc((size_t)logLength);
		glGetShaderInfoLog(vert, logLength, NULL, (GLchar *)log);
		free(log);
		printf("GLSL vertex shader: %s\n", log);
	#endif
		glDeleteShader(vert);
		glDeleteShader(frag);
		return false;
	}

	glShaderSource(frag, 1, &fragSrc, NULL);
	glCompileShader(frag);
	GLint fragOk;
	glGetShaderiv(frag, GL_COMPILE_STATUS, &fragOk);
	if (!vertOk) {
	#ifndef NDEBUG
		GLint logLength;
		glGetShaderiv(vert, GL_INFO_LOG_LENGTH, &logLength);
		char *log = (char *)malloc((size_t)logLength);
		glGetShaderInfoLog(vert, logLength, NULL, (GLchar *)log);
		printf("GLSL vertex shader: %s\n", log);
		free(log);
	#endif
		glDeleteShader(vert);
		glDeleteShader(frag);
		return false;
	}

	glAttachShader(program, vert);
	glAttachShader(program, frag);
	glLinkProgram(program);
	GLint linkOk;
	glGetProgramiv(program, GL_LINK_STATUS, &linkOk);
	if (!linkOk) {
	#ifndef NDEBUG
		GLint logLength;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
		char *log = (char *)malloc((size_t)logLength);
		glGetProgramInfoLog(program, logLength, NULL, (GLchar *)log);
		printf("GLSL linker: %s\n", log);
		free(log);
	#endif
		glDeleteShader(vert);
		glDeleteShader(frag);
		return false;
	}

	free(vertSrc);
	free(fragSrc);

	glDetachShader(program, vert);
	glDetachShader(program, frag);
	glDeleteShader(vert);
	glDeleteShader(frag);

	glCheckErrors();
	return true;
}

Shader loadShader(const char *vertFile, const char *fragFile) {
	Shader program = glCreateProgram();
	assert(program);

	bool linkOk = compileAndLinkShader(program, vertFile, fragFile);
	if (!linkOk) {
		glDeleteProgram(program);
		program = 0;
	} else {
		ShaderSources sources;
		sources.vertFile = vertFile;
		sources.fragFile = fragFile;
		shaderSources[program] = sources;

		auto recompileShader = [](const char *filename, void *shaderID) {
			GLuint id = (GLuint)(size_t)shaderID;
			ShaderSources sources = shaderSources[id];
			compileAndLinkShader(id, sources.vertFile, sources.fragFile);
			return true;
		};

		trackFileChanges(vertFile, (void *)(size_t)program, recompileShader);
		trackFileChanges(fragFile, (void *)(size_t)program, recompileShader);
	}

	glCheckErrors();
	return program;
}
void setUniform(Shader s, uint location, ShaderDataType type, const void *value, size_t valueSize) {
	assert(valueSize == sizeofShaderDataType(type));
	bindShader(s);

	switch (type) {
		case GL_FLOAT:				glUniform1fv((GLint)location, 1, (const GLfloat *)value); break;
		case GL_FLOAT_VEC2:			glUniform2fv((GLint)location, 1, (const GLfloat *)value); break;
		case GL_FLOAT_VEC3:			glUniform3fv((GLint)location, 1, (const GLfloat *)value); break;
		case GL_FLOAT_VEC4:			glUniform4fv((GLint)location, 1, (const GLfloat *)value); break;
		case GL_FLOAT_MAT2:			glUniformMatrix2fv((GLint)location, 1, GL_FALSE, (const GLfloat *)value); break;
		case GL_FLOAT_MAT3:			glUniformMatrix3fv((GLint)location, 1, GL_FALSE, (const GLfloat *)value); break;
		case GL_FLOAT_MAT4:			glUniformMatrix4fv((GLint)location, 1, GL_FALSE, (const GLfloat *)value); break;
		case GL_INT:				glUniform1iv((GLint)location, 1, (const GLint *)value); break;
		case GL_INT_VEC2:			glUniform2iv((GLint)location, 1, (const GLint *)value); break;
		case GL_INT_VEC3:			glUniform3iv((GLint)location, 1, (const GLint *)value); break;
		case GL_INT_VEC4:			glUniform4iv((GLint)location, 1, (const GLint *)value); break;
		case GL_UNSIGNED_INT:		glUniform1uiv((GLint)location, 1, (const GLuint *)value); break;
		case GL_UNSIGNED_INT_VEC2:	glUniform2uiv((GLint)location, 1, (const GLuint *)value); break;
		case GL_UNSIGNED_INT_VEC3:	glUniform3uiv((GLint)location, 1, (const GLuint *)value); break;
		case GL_UNSIGNED_INT_VEC4:	glUniform4uiv((GLint)location, 1, (const GLuint *)value); break;
		case GL_BOOL:				glUniform1iv((GLint)location, 1, (const GLint *)value); break;
		case GL_SAMPLER_2D:			
		case GL_SAMPLER_2D_ARRAY:
			glUniform1iv((GLint)location, 1, (const GLint *)value); break;
		default: assert(0); break;
	}

	glCheckErrors();
}
void bindShader(Shader s) {
	glUseProgram(s);
	glCheckErrors();
}
void destroyShader(Shader s) {
	glDeleteProgram(s);
	glCheckErrors();
}