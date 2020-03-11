#pragma once
#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace ed {
	// i know, ewww, unfortunately the only reason i made this way is because of the FunctionVariableManager...
	// TODO:
	class CameraSnapshots {
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