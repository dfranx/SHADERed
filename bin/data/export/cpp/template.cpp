#include <chrono>
#include <fstream>
#include <string>

#include <GL/glew.h>
#include <SDL2/SDL.h>
#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

[$$shaders$$]

	// required functions
	void
	CreateVAO(GLuint& geoVAO, GLuint geoVBO);
GLuint CreatePlane(GLuint& vbo, float sx, float sy);
GLuint CreateScreenQuadNDC(GLuint& vbo);
GLuint CreateCube(GLuint& vbo, float sx, float sy, float sz);
std::string LoadFile(const std::string& filename);
GLuint CreateShader(const char* vsCode, const char* psCode);
GLuint LoadTexture(const std::string& filename);

const GLenum FBO_Buffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4, GL_COLOR_ATTACHMENT5, GL_COLOR_ATTACHMENT6, GL_COLOR_ATTACHMENT7, GL_COLOR_ATTACHMENT8, GL_COLOR_ATTACHMENT9, GL_COLOR_ATTACHMENT10, GL_COLOR_ATTACHMENT11, GL_COLOR_ATTACHMENT12, GL_COLOR_ATTACHMENT13, GL_COLOR_ATTACHMENT14, GL_COLOR_ATTACHMENT15 };

int main()
{
	stbi_set_flip_vertically_on_load(1);

	// init sdl2
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO) < 0) {
		printf("Failed to initialize SDL2\n");
		return 0;
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1); // double buffering

	// open window
	SDL_Window* wnd = SDL_CreateWindow("ShaderProject", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
	SDL_SetWindowMinimumSize(wnd, 200, 200);

	// get GL context
	SDL_GLContext glContext = SDL_GL_CreateContext(wnd);
	SDL_GL_MakeCurrent(wnd, glContext);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_STENCIL_TEST);

	// init glew
	glewExperimental = true;
	if (glewInit() != GLEW_OK) {
		printf("Failed to initialize GLEW\n");
		return 0;
	}

	// required:
	float sedWindowWidth = 800, sedWindowHeight = 600;
	float sedMouseX = 0, sedMouseY = 0;

	std::chrono::time_point<std::chrono::system_clock> timerStart = std::chrono::system_clock::now();

	// init
	[$$init$$]

		SDL_Event event;
	bool run = true;
	while (run) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT) {
				run = false;
			} else if (event.type == SDL_MOUSEMOTION) {
				sedMouseX = event.motion.x;
				sedMouseY = event.motion.y;
				sysMousePosition = glm::vec2(sedMouseX / sedWindowWidth, 1.f - (sedMouseY / sedWindowHeight));
			} else if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED) {
				sedWindowWidth = event.window.data1;
				sedWindowHeight = event.window.data2;

				[$$resize_event$$]
			}
		}

		if (!run) break;

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		glViewport(0, 0, sedWindowWidth, sedWindowHeight);

		// RENDER
		[$$render$$]

			float curTime
			= std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - timerStart).count() / 1000000.0f;
		sysTimeDelta = curTime - sysTime;
		sysTime = curTime;
		sysFrameIndex++;

		SDL_GL_SwapWindow(wnd);
	}

	// sdl2
	SDL_GL_DeleteContext(glContext);
	SDL_DestroyWindow(wnd);
	SDL_Quit();

	return 0;
}

void CreateVAO(GLuint& geoVAO, GLuint geoVBO)
{
	if (geoVAO != 0)
		glDeleteVertexArrays(1, &geoVAO);

	glGenVertexArrays(1, &geoVAO);
	glBindVertexArray(geoVAO);
	glBindBuffer(GL_ARRAY_BUFFER, geoVBO);

	// POSITION
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 18 * sizeof(float), (void*)(0 * sizeof(GLfloat)));
	glEnableVertexAttribArray(0);

	// NORMAL
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 18 * sizeof(float), (void*)(3 * sizeof(GLfloat)));
	glEnableVertexAttribArray(1);

	// TEXCOORD
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 18 * sizeof(float), (void*)(6 * sizeof(GLfloat)));
	glEnableVertexAttribArray(2);

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}
GLuint CreatePlane(GLuint& vbo, float sx, float sy)
{
	float halfX = sx / 2;
	float halfY = sy / 2;

	GLfloat planeData[] = {
		halfX,
		halfY,
		0,
		0.0f,
		0.0f,
		1.0f,
		1.0f,
		1.0f,
		0,
		0,
		0,
		0,
		0,
		0,
		1,
		1,
		1,
		1,
		halfX,
		-halfY,
		0,
		0.0f,
		0.0f,
		1.0f,
		1.0f,
		0.0f,
		0,
		0,
		0,
		0,
		0,
		0,
		1,
		1,
		1,
		1,
		-halfX,
		-halfY,
		0,
		0.0f,
		0.0f,
		1.0f,
		0.0f,
		0.0f,
		0,
		0,
		0,
		0,
		0,
		0,
		1,
		1,
		1,
		1,
		-halfX,
		halfY,
		0,
		0.0f,
		0.0f,
		1.0f,
		0.0f,
		1.0f,
		0,
		0,
		0,
		0,
		0,
		0,
		1,
		1,
		1,
		1,
		halfX,
		halfY,
		0,
		0.0f,
		0.0f,
		1.0f,
		1.0f,
		1.0f,
		0,
		0,
		0,
		0,
		0,
		0,
		1,
		1,
		1,
		1,
		-halfX,
		-halfY,
		0,
		0.0f,
		0.0f,
		1.0f,
		0.0f,
		0.0f,
		0,
		0,
		0,
		0,
		0,
		0,
		1,
		1,
		1,
		1,
	};

	// create vbo
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, 6 * 18 * sizeof(GLfloat), planeData, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	GLuint vao = 0;
	CreateVAO(vao, vbo);

	return vao;
}
GLuint CreateScreenQuadNDC(GLuint& vbo)
{
	GLfloat sqData[] = {
		-1,
		-1,
		0.0f,
		0.0f,
		1,
		-1,
		1.0f,
		0.0f,
		1,
		1,
		1.0f,
		1.0f,
		-1,
		-1,
		0.0f,
		0.0f,
		1,
		1,
		1.0f,
		1.0f,
		-1,
		1,
		0.0f,
		1.0f,
	};

	GLuint vao;

	// create vao
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	// create vbo
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);

	// vbo data
	glBufferData(GL_ARRAY_BUFFER, 6 * 4 * sizeof(GLfloat), sqData, GL_STATIC_DRAW);

	// vertex positions
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)0);
	glEnableVertexAttribArray(0);

	// vertex texture coords
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));
	glEnableVertexAttribArray(1);

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	return vao;
}
GLuint CreateCube(GLuint& vbo, float sx, float sy, float sz)
{
	float halfX = sx / 2.0f;
	float halfY = sy / 2.0f;
	float halfZ = sz / 2.0f;

	// vec3, vec3, vec2, vec3, vec3, vec4
	GLfloat cubeData[] = {
		// front face
		-halfX,
		-halfY,
		halfZ,
		0.0f,
		0.0f,
		1.0f,
		0.0f,
		0.0f,
		0,
		0,
		0,
		0,
		0,
		0,
		1,
		1,
		1,
		1,
		halfX,
		-halfY,
		halfZ,
		0.0f,
		0.0f,
		1.0f,
		1.0f,
		0.0f,
		0,
		0,
		0,
		0,
		0,
		0,
		1,
		1,
		1,
		1,
		halfX,
		halfY,
		halfZ,
		0.0f,
		0.0f,
		1.0f,
		1.0f,
		1.0f,
		0,
		0,
		0,
		0,
		0,
		0,
		1,
		1,
		1,
		1,
		-halfX,
		-halfY,
		halfZ,
		0.0f,
		0.0f,
		1.0f,
		0.0f,
		0.0f,
		0,
		0,
		0,
		0,
		0,
		0,
		1,
		1,
		1,
		1,
		halfX,
		halfY,
		halfZ,
		0.0f,
		0.0f,
		1.0f,
		1.0f,
		1.0f,
		0,
		0,
		0,
		0,
		0,
		0,
		1,
		1,
		1,
		1,
		-halfX,
		halfY,
		halfZ,
		0.0f,
		0.0f,
		1.0f,
		0.0f,
		1.0f,
		0,
		0,
		0,
		0,
		0,
		0,
		1,
		1,
		1,
		1,

		// back face
		halfX,
		halfY,
		-halfZ,
		0.0f,
		0.0f,
		-1.0f,
		1.0f,
		1.0f,
		0,
		0,
		0,
		0,
		0,
		0,
		1,
		1,
		1,
		1,
		halfX,
		-halfY,
		-halfZ,
		0.0f,
		0.0f,
		-1.0f,
		1.0f,
		0.0f,
		0,
		0,
		0,
		0,
		0,
		0,
		1,
		1,
		1,
		1,
		-halfX,
		-halfY,
		-halfZ,
		0.0f,
		0.0f,
		-1.0f,
		0.0f,
		0.0f,
		0,
		0,
		0,
		0,
		0,
		0,
		1,
		1,
		1,
		1,
		-halfX,
		halfY,
		-halfZ,
		0.0f,
		0.0f,
		-1.0f,
		0.0f,
		1.0f,
		0,
		0,
		0,
		0,
		0,
		0,
		1,
		1,
		1,
		1,
		halfX,
		halfY,
		-halfZ,
		0.0f,
		0.0f,
		-1.0f,
		1.0f,
		1.0f,
		0,
		0,
		0,
		0,
		0,
		0,
		1,
		1,
		1,
		1,
		-halfX,
		-halfY,
		-halfZ,
		0.0f,
		0.0f,
		-1.0f,
		0.0f,
		0.0f,
		0,
		0,
		0,
		0,
		0,
		0,
		1,
		1,
		1,
		1,

		// right face
		halfX,
		-halfY,
		halfZ,
		1.0f,
		0.0f,
		0.0f,
		0.0f,
		0.0f,
		0,
		0,
		0,
		0,
		0,
		0,
		1,
		1,
		1,
		1,
		halfX,
		-halfY,
		-halfZ,
		1.0f,
		0.0f,
		0.0f,
		1.0f,
		0.0f,
		0,
		0,
		0,
		0,
		0,
		0,
		1,
		1,
		1,
		1,
		halfX,
		halfY,
		-halfZ,
		1.0f,
		0.0f,
		0.0f,
		1.0f,
		1.0f,
		0,
		0,
		0,
		0,
		0,
		0,
		1,
		1,
		1,
		1,
		halfX,
		-halfY,
		halfZ,
		1.0f,
		0.0f,
		0.0f,
		0.0f,
		0.0f,
		0,
		0,
		0,
		0,
		0,
		0,
		1,
		1,
		1,
		1,
		halfX,
		halfY,
		-halfZ,
		1.0f,
		0.0f,
		0.0f,
		1.0f,
		1.0f,
		0,
		0,
		0,
		0,
		0,
		0,
		1,
		1,
		1,
		1,
		halfX,
		halfY,
		halfZ,
		1.0f,
		0.0f,
		0.0f,
		0.0f,
		1.0f,
		0,
		0,
		0,
		0,
		0,
		0,
		1,
		1,
		1,
		1,

		// left face
		-halfX,
		halfY,
		-halfZ,
		-1.0f,
		0.0f,
		0.0f,
		1.0f,
		1.0f,
		0,
		0,
		0,
		0,
		0,
		0,
		1,
		1,
		1,
		1,
		-halfX,
		-halfY,
		-halfZ,
		-1.0f,
		0.0f,
		0.0f,
		1.0f,
		0.0f,
		0,
		0,
		0,
		0,
		0,
		0,
		1,
		1,
		1,
		1,
		-halfX,
		-halfY,
		halfZ,
		-1.0f,
		0.0f,
		0.0f,
		0.0f,
		0.0f,
		0,
		0,
		0,
		0,
		0,
		0,
		1,
		1,
		1,
		1,
		-halfX,
		halfY,
		halfZ,
		-1.0f,
		0.0f,
		0.0f,
		0.0f,
		1.0f,
		0,
		0,
		0,
		0,
		0,
		0,
		1,
		1,
		1,
		1,
		-halfX,
		halfY,
		-halfZ,
		-1.0f,
		0.0f,
		0.0f,
		1.0f,
		1.0f,
		0,
		0,
		0,
		0,
		0,
		0,
		1,
		1,
		1,
		1,
		-halfX,
		-halfY,
		halfZ,
		-1.0f,
		0.0f,
		0.0f,
		0.0f,
		0.0f,
		0,
		0,
		0,
		0,
		0,
		0,
		1,
		1,
		1,
		1,

		// top face
		-halfX,
		halfY,
		halfZ,
		0.0f,
		1.0f,
		0.0f,
		0.0f,
		0.0f,
		0,
		0,
		0,
		0,
		0,
		0,
		1,
		1,
		1,
		1,
		halfX,
		halfY,
		halfZ,
		0.0f,
		1.0f,
		0.0f,
		1.0f,
		0.0f,
		0,
		0,
		0,
		0,
		0,
		0,
		1,
		1,
		1,
		1,
		halfX,
		halfY,
		-halfZ,
		0.0f,
		1.0f,
		0.0f,
		1.0f,
		1.0f,
		0,
		0,
		0,
		0,
		0,
		0,
		1,
		1,
		1,
		1,
		-halfX,
		halfY,
		halfZ,
		0.0f,
		1.0f,
		0.0f,
		0.0f,
		0.0f,
		0,
		0,
		0,
		0,
		0,
		0,
		1,
		1,
		1,
		1,
		halfX,
		halfY,
		-halfZ,
		0.0f,
		1.0f,
		0.0f,
		1.0f,
		1.0f,
		0,
		0,
		0,
		0,
		0,
		0,
		1,
		1,
		1,
		1,
		-halfX,
		halfY,
		-halfZ,
		0.0f,
		1.0f,
		0.0f,
		0.0f,
		1.0f,
		0,
		0,
		0,
		0,
		0,
		0,
		1,
		1,
		1,
		1,

		// bottom face
		halfX,
		-halfY,
		-halfZ,
		0.0f,
		-1.0f,
		0.0f,
		1.0f,
		1.0f,
		0,
		0,
		0,
		0,
		0,
		0,
		1,
		1,
		1,
		1,
		halfX,
		-halfY,
		halfZ,
		0.0f,
		-1.0f,
		0.0f,
		1.0f,
		0.0f,
		0,
		0,
		0,
		0,
		0,
		0,
		1,
		1,
		1,
		1,
		-halfX,
		-halfY,
		halfZ,
		0.0f,
		-1.0f,
		0.0f,
		0.0f,
		0.0f,
		0,
		0,
		0,
		0,
		0,
		0,
		1,
		1,
		1,
		1,
		-halfX,
		-halfY,
		-halfZ,
		0.0f,
		-1.0f,
		0.0f,
		0.0f,
		1.0f,
		0,
		0,
		0,
		0,
		0,
		0,
		1,
		1,
		1,
		1,
		halfX,
		-halfY,
		-halfZ,
		0.0f,
		-1.0f,
		0.0f,
		1.0f,
		1.0f,
		0,
		0,
		0,
		0,
		0,
		0,
		1,
		1,
		1,
		1,
		-halfX,
		-halfY,
		halfZ,
		0.0f,
		-1.0f,
		0.0f,
		0.0f,
		0.0f,
		0,
		0,
		0,
		0,
		0,
		0,
		1,
		1,
		1,
		1,
	};

	// create vbo
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, 36 * 18 * sizeof(GLfloat), cubeData, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	GLuint vao = 0;
	CreateVAO(vao, vbo);

	return vao;
}
std::string LoadFile(const std::string& filename)
{
	std::ifstream file(filename);
	std::string src((std::istreambuf_iterator<char>(file)),
		std::istreambuf_iterator<char>());
	file.close();
	return src;
}
GLuint CreateShader(const char* vsCode, const char* psCode)
{
	GLint success = 0;
	char infoLog[512];

	// create vertex shader
	unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vs, 1, &vsCode, nullptr);
	glCompileShader(vs);
	glGetShaderiv(vs, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(vs, 512, NULL, infoLog);
		printf("Failed to compile the shader.\n");
		return 0;
	}

	// create pixel shader
	unsigned int ps = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(ps, 1, &psCode, nullptr);
	glCompileShader(ps);
	glGetShaderiv(ps, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(ps, 512, NULL, infoLog);
		printf("Failed to compile the shader.\n");
		return 0;
	}

	// create a shader program for gizmo
	GLuint retShader = glCreateProgram();
	glAttachShader(retShader, vs);
	glAttachShader(retShader, ps);
	glLinkProgram(retShader);
	glGetProgramiv(retShader, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(retShader, 512, NULL, infoLog);
		printf("Failed to create the shader.\n");
		return 0;
	}

	glDeleteShader(vs);
	glDeleteShader(ps);

	return retShader;
}
GLuint LoadTexture(const std::string& file)
{
	int width, height, nrChannels;
	unsigned char* data = stbi_load(file.c_str(), &width, &height, &nrChannels, 0);
	unsigned char* paddedData = nullptr;

	if (data == nullptr)
		return 0;

	GLenum fmt = GL_RGB;
	if (nrChannels == 4)
		fmt = GL_RGBA;
	else if (nrChannels == 1)
		fmt = GL_LUMINANCE;

	if (nrChannels != 4) {
		paddedData = (unsigned char*)malloc(width * height * 4);
		for (int x = 0; x < width; x++) {
			for (int y = 0; y < height; y++) {
				if (nrChannels == 3) {
					paddedData[(y * width + x) * 4 + 0] = data[(y * width + x) * 3 + 0];
					paddedData[(y * width + x) * 4 + 1] = data[(y * width + x) * 3 + 1];
					paddedData[(y * width + x) * 4 + 2] = data[(y * width + x) * 3 + 2];
				} else if (nrChannels == 1) {
					unsigned char val = data[(y * width + x) * 1 + 0];
					paddedData[(y * width + x) * 4 + 0] = val;
					paddedData[(y * width + x) * 4 + 1] = val;
					paddedData[(y * width + x) * 4 + 2] = val;
				}
				paddedData[(y * width + x) * 4 + 3] = 255;
			}
		}
	}

	// normal texture
	GLuint ret = 0;
	glGenTextures(1, &ret);
	glBindTexture(GL_TEXTURE_2D, ret);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, paddedData == nullptr ? data : paddedData);
	glBindTexture(GL_TEXTURE_2D, 0);

	if (paddedData != nullptr)
		free(paddedData);
	stbi_image_free(data);

	return ret;
}
