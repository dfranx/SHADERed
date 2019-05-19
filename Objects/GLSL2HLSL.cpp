#include <iostream>
#include <Windows.h>
#include <fstream>
#include <vector>

#include "Settings.h"
#include "GLSL2HLSL.h"

#define PIPE_BUFFER_SIZE 4096

namespace ed
{
	std::string GLSL2HLSL::Transcompile(const std::string& filename, const std::string& entry, ml::Logger* log)
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

		/*
		
		test out -d flag (auto #version when there is none)
		TODO: warnings
		-e <entry point>

		*/
		std::string out = m_exec("./GLSL/glslangValidator.exe --source-entrypoint main -e " + entry + " -S " + stage + " -o " + spv + " -V " + filename);
		if (out.find("ERROR") != std::string::npos)
			log->Log(out);

		out = m_exec("./GLSL/spirv-cross.exe " + spv + " --hlsl --shader-model 50 --output " + tempLoc + stage + ".hlsl");
		// TODO: spirv errors if any

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

	std::string GLSL2HLSL::m_exec(const std::string& cmd)
	{
		/*
			https://stackoverflow.com/questions/14147138/capture-output-of-spawned-process-to-string
			https://docs.microsoft.com/en-us/windows/desktop/ProcThread/creating-a-child-process-with-redirected-input-and-output
		*/

		HANDLE hChildStd_OUT_Rd = NULL;
		HANDLE hChildStd_OUT_Wr = NULL;
		HANDLE hChildStd_ERR_Rd = NULL;
		HANDLE hChildStd_ERR_Wr = NULL;

		SECURITY_ATTRIBUTES sa;

		// Set the bInheritHandle flag so pipe handles are inherited 
		sa.nLength = sizeof(SECURITY_ATTRIBUTES);
		sa.bInheritHandle = TRUE;
		sa.lpSecurityDescriptor = NULL;

		// Create a pipe for the child process's STDERR. 
		if (!CreatePipe(&hChildStd_ERR_Rd, &hChildStd_ERR_Wr, &sa, 0))
			return "";

		// Ensure the read handle to the pipe for STDERR is not inherited.
		if (!SetHandleInformation(hChildStd_ERR_Rd, HANDLE_FLAG_INHERIT, 0))
			return "";

		// Create a pipe for the child process's STDOUT. 
		if (!CreatePipe(&hChildStd_OUT_Rd, &hChildStd_OUT_Wr, &sa, 0))
			return "";

		// Ensure the read handle to the pipe for STDOUT is not inherited
		if (!SetHandleInformation(hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0))
			return "";

		// Set the text I want to run
		char szCmdline[] = "./GLSL/glslangValidator.exe";
		PROCESS_INFORMATION piProcInfo;
		STARTUPINFOA siStartInfo;
		bool bSuccess = FALSE;

		// Set up members of the PROCESS_INFORMATION structure. 
		ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

		// Set up members of the STARTUPINFO structure. 
		// This structure specifies the STDERR and STDOUT handles for redirection.
		ZeroMemory(&siStartInfo, sizeof(STARTUPINFOA));
		siStartInfo.cb = sizeof(STARTUPINFOA);
		siStartInfo.hStdError = hChildStd_ERR_Wr;
		siStartInfo.hStdOutput = hChildStd_OUT_Wr;
		siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

		// Create the child process. 
		bSuccess = CreateProcessA(NULL,
			const_cast<LPSTR>(cmd.c_str()),     // command line 
			NULL,          // process security attributes 
			NULL,          // primary thread security attributes 
			TRUE,          // handles are inherited 
			0,             // creation flags 
			NULL,          // use parent's environment 
			NULL,          // use parent's current directory 
			&siStartInfo,  // STARTUPINFO pointer 
			&piProcInfo);  // receives PROCESS_INFORMATION

		CloseHandle(hChildStd_ERR_Wr);
		CloseHandle(hChildStd_OUT_Wr);

		// If an error occurs, exit the application. 
		if (!bSuccess)
			return "";

		DWORD dwRead = 0;
		CHAR chBuf[PIPE_BUFFER_SIZE];
		std::string out = "";
		for (;;) {
			bSuccess = ReadFile(hChildStd_OUT_Rd, chBuf, PIPE_BUFFER_SIZE, &dwRead, NULL);
			if (!bSuccess || dwRead == 0) break;

			std::string s(chBuf, dwRead);
			out += s;
		}

		CloseHandle(piProcInfo.hThread);
		CloseHandle(piProcInfo.hProcess);
		CloseHandle(hChildStd_ERR_Rd);
		CloseHandle(hChildStd_OUT_Rd);

		return out;
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
