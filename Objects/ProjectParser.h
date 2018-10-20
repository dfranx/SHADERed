#pragma once
#include "PipelineManager.h"

namespace ed
{
	class ProjectParser
	{
	public:
		ProjectParser(PipelineManager* pipeline);
		~ProjectParser();

		void Open(const std::string& file);

		void Save();
		void SaveAs(const std::string& file);

		inline std::string GetOpenedFile() { return m_file; }

	private:
		PipelineManager* m_pipe;
		std::string m_file;
	};
}