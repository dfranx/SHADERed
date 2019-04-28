#include <iostream>
#include <Windows.h>
#include <fstream>
#include <vector>

#include "Settings.h"
#include "GLSL2HLSL.h"

#define PIPE_BUFFER_SIZE 1024 

namespace ed
{
	std::string GLSL2HLSL::Transcompile(const std::string& filename)
	{
		const std::string tempLoc = "./GLSL/temp/";

		int glsl = IsGLSL(filename);

		std::string stage = "vert";
		if (glsl == 2)
			stage = "frag";
		else if (glsl == 3)
			stage = "geom";


		std::string spv(tempLoc + stage + ".spv");
		std::string hlsl(tempLoc + stage + ".hlsl");

		m_exec("./GLSL/glslangValidator.exe", "-S " + stage + " -o " + spv + " -V " + filename);
		m_exec("./GLSL/spirv-cross.exe", spv + " --hlsl --shader-model 50 --output " + tempLoc + stage + ".hlsl");

		std::ifstream hlslStream(hlsl);
		if (!hlslStream.is_open()) {
			hlslStream.close();
			return "";
		}

		std::string code((std::istreambuf_iterator<char>(hlslStream)),
			std::istreambuf_iterator<char>());

		hlslStream.close();

		m_clear();

		return code;
	}
	int GLSL2HLSL::IsGLSL(const std::string& file)
	{
		if (file.find("." + std::string(Settings::Instance().Project.GLSLVS_Extenstion)) != std::string::npos)
			return 1;
		if (file.find("." + std::string(Settings::Instance().Project.GLSLPS_Extenstion)) != std::string::npos)
			return 2;
		if (file.find("." + std::string(Settings::Instance().Project.GLSLGS_Extenstion)) != std::string::npos)
			return 3;

		return 0;
	}
	void GLSL2HLSL::m_exec(const std::string& app, const std::string& args)
	{
		STARTUPINFOA si;
		PROCESS_INFORMATION pi;

		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(si);
		ZeroMemory(&pi, sizeof(pi));

		CreateProcessA
		(
			app.c_str(),
			const_cast<LPSTR>((" " + args).c_str()),
			NULL,
			NULL,
			FALSE,
			CREATE_NO_WINDOW,
			NULL,
			NULL,
			&si,
			&pi
		);

		WaitForSingleObject(pi.hProcess, INFINITE);

		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}
	void GLSL2HLSL::m_clear()
	{
		WIN32_FIND_DATAA fData;
		HANDLE hFind = FindFirstFileA("./GLSL/temp/*", &fData);
		DWORD Attributes; 
		
		std::vector<std::string> files;

		do {
			if (!(fData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				files.push_back(fData.cFileName);
		} while (FindNextFileA(hFind, &fData));

		for (int i = 0; i < files.size(); i++)
			DeleteFileA(("./GLSL/temp/" + files[i]).c_str());
	}
}
