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

	private:
		PipelineManager* m_pipe;
		std::string m_file;
	};
}