#pragma once
#include <string>
#include "../GUIManager.h"

namespace ed
{
	class PipelineManager;

	class ProjectParser
	{
	public:
		ProjectParser(PipelineManager* pipeline, GUIManager* gui);
		~ProjectParser();

		void Open(const std::string& file);

		void Save();
		void SaveAs(const std::string& file);

		std::string LoadProjectFile(const std::string& file);
		void SaveProjectFile(const std::string& file, const std::string& data);

		std::string GetRelativePath(const std::string& to);

		void ResetProjectDirectory();
		inline void SetProjectDirectory(const std::string& path) { m_projectPath = path; }
		inline std::string GetProjectDirectory() { return m_projectPath; }

		inline std::string GetOpenedFile() { return m_file; }

	private:
		GUIManager* m_ui;
		PipelineManager* m_pipe;
		std::string m_file;
		std::string m_projectPath;
	};
}