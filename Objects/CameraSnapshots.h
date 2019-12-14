#pragma once
#include <vector>
#include <string>
#include <glm/glm.hpp>

namespace ed
{
	// i know, ewww, unfortunately the only reason i made every method static is because of the FunctionVariableManager...
	// TODO: convert to nonstatic methods (FunctionVariableManager too) 
	class CameraSnapshots
	{
	public:
		static void Add(const std::string& name, const glm::mat4& mat);
		static glm::mat4 Get(const std::string& name);
		static void Remove(const std::string& name);
		static void Clear();

		static const std::vector<std::string>& GetList();

		static std::vector<std::string> Names;
		static std::vector<glm::mat4> Matrices;
	};
}