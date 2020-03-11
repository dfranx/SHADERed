#include <SHADERed/Objects/CameraSnapshots.h>

namespace ed {
	std::vector<std::string> CameraSnapshots::Names = std::vector<std::string>();
	std::vector<glm::mat4> CameraSnapshots::Matrices = std::vector<glm::mat4>();

	glm::mat4 CameraSnapshots::Get(const std::string& name)
	{
		for (int i = 0; i < Names.size(); i++)
			if (Names[i] == name)
				return Matrices[i];
		return glm::mat4(1);
	}
	void CameraSnapshots::Remove(const std::string& name)
	{
		for (int i = 0; i < Names.size(); i++)
			if (Names[i] == name) {
				Names.erase(Names.begin() + i);
				Matrices.erase(Matrices.begin() + i);
				break;
			}
	}
	void CameraSnapshots::Add(const std::string& name, const glm::mat4& mat)
	{
		Names.push_back(name);
		Matrices.push_back(mat);
	}
	void CameraSnapshots::Clear()
	{
		Names.clear();
		Matrices.clear();
	}
	const std::vector<std::string>& CameraSnapshots::GetList()
	{
		return Names;
	}
}