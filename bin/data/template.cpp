#include <fstream>
#include <GL/glew.h>
#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
#include <glm/glm.hpp>

#ifdef SHADERED_USE_STB_IMAGE
#include "stb_image.h"
#endif

[$$shaders$$]

// required functions
void CreateVAO(GLuint& geoVAO, GLuint geoVBO);
GLuint CreatePlane(GLuint& vbo, float sx, float sy);
GLuint CreateScreenQuadNDC(GLuint& vbo);
GLuint CreateCube(GLuint& vbo, float sx, float sy, float sz);
std::string LoadFile(const std::string& filename);
GLuint CreateShader(const char* vsCode, const char* psCode);

int main()
{
#ifdef SHADERED_USE_STB_IMAGE
	stbi_set_flip_vertically_on_load(1);
#endif

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

	std::chrono::time_point<std::chrono::system_clock> timerStart = std::chrono::system_clock::now();

	// init
	[$$init$$]

	SDL_Event event;
	bool run = true;
	while (run) {
		while (SDL_PollEvent(&event))
		{
			if (event.type == SDL_QUIT)
				run = false;

			[$$event$$]
		}

		if (!run) break;
		
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

		// RENDER
		[$$render$$]

		float curTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - timerStart).count() / 1000000.0f;
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
		halfX, halfY, 0, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0,0,0, 0,0,0, 1,1,1,1,
		halfX, -halfY, 0, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0,0,0, 0,0,0, 1,1,1,1,
		-halfX, -halfY, 0, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0,0,0, 0,0,0, 1,1,1,1,
		-halfX, halfY, 0, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0,0,0, 0,0,0, 1,1,1,1,
		halfX, halfY, 0, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0,0,0, 0,0,0, 1,1,1,1,
		-halfX, -halfY, 0, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0,0,0, 0,0,0, 1,1,1,1,
	};

	// create vbo
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, 6 * 18 * sizeof(GLfloat), planeData, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	CreateVAO(vao, vbo);

	return vao;
}
GLuint CreateScreenQuadNDC(GLuint& vbo)
{
	GLfloat sqData[] = {
		-1, -1, 0.0f, 0.0f,
		1, -1, 1.0f, 0.0f,
		1, 1, 1.0f, 1.0f,
		-1, -1, 0.0f, 0.0f,
		1, 1, 1.0f, 1.0f,
		-1, 1, 0.0f, 1.0f,
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
		-halfX, -halfY, halfZ, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0,0,0, 0,0,0, 1,1,1,1,
		halfX, -halfY, halfZ, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0,0,0, 0,0,0, 1,1,1,1,
		halfX, halfY, halfZ, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0,0,0, 0,0,0, 1,1,1,1,
		-halfX, -halfY, halfZ, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0,0,0, 0,0,0, 1,1,1,1,
		halfX, halfY, halfZ, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0,0,0, 0,0,0, 1,1,1,1,
		-halfX, halfY, halfZ, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0,0,0, 0,0,0, 1,1,1,1,

		// back face
		halfX, halfY, -halfZ, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f, 0,0,0, 0,0,0, 1,1,1,1,
		halfX, -halfY, -halfZ, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0,0,0, 0,0,0, 1,1,1,1,
		-halfX, -halfY, -halfZ, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0,0,0, 0,0,0, 1,1,1,1,
		-halfX, halfY, -halfZ, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0,0,0, 0,0,0, 1,1,1,1,
		halfX, halfY, -halfZ, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f, 0,0,0, 0,0,0, 1,1,1,1,
		-halfX, -halfY, -halfZ, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0,0,0, 0,0,0, 1,1,1,1,

		// right face
		halfX, -halfY, halfZ, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0,0,0, 0,0,0, 1,1,1,1,
		halfX, -halfY, -halfZ, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0,0,0, 0,0,0, 1,1,1,1,
		halfX, halfY, -halfZ, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0,0,0, 0,0,0, 1,1,1,1,
		halfX, -halfY, halfZ, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0,0,0, 0,0,0, 1,1,1,1,
		halfX, halfY, -halfZ, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0,0,0, 0,0,0, 1,1,1,1,
		halfX, halfY, halfZ, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0,0,0, 0,0,0, 1,1,1,1,

		// left face
		-halfX, halfY, -halfZ, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0,0,0, 0,0,0, 1,1,1,1,
		-halfX, -halfY, -halfZ, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0,0,0, 0,0,0, 1,1,1,1,
		-halfX, -halfY, halfZ, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0,0,0, 0,0,0, 1,1,1,1,
		-halfX, halfY, halfZ, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0,0,0, 0,0,0, 1,1,1,1,
		-halfX, halfY, -halfZ, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0,0,0, 0,0,0, 1,1,1,1,
		-halfX, -halfY, halfZ, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0,0,0, 0,0,0, 1,1,1,1,

		// top face
		-halfX, halfY, halfZ, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0,0,0, 0,0,0, 1,1,1,1,
		halfX, halfY, halfZ, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0,0,0, 0,0,0, 1,1,1,1,
		halfX, halfY, -halfZ, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0,0,0, 0,0,0, 1,1,1,1,
		-halfX, halfY, halfZ, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0,0,0, 0,0,0, 1,1,1,1,
		halfX, halfY, -halfZ, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0,0,0, 0,0,0, 1,1,1,1,
		-halfX, halfY, -halfZ, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0,0,0, 0,0,0, 1,1,1,1,

		// bottom face
		halfX, -halfY, -halfZ, 0.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0,0,0, 0,0,0, 1,1,1,1,
		halfX, -halfY, halfZ, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0,0,0, 0,0,0, 1,1,1,1,
		-halfX, -halfY, halfZ, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0,0,0, 0,0,0, 1,1,1,1,
		-halfX, -halfY, -halfZ, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0,0,0, 0,0,0, 1,1,1,1,
		halfX, -halfY, -halfZ, 0.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0,0,0, 0,0,0, 1,1,1,1,
		-halfX, -halfY, halfZ, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0,0,0, 0,0,0, 1,1,1,1,
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
	std::ifstream file("file.txt");
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
