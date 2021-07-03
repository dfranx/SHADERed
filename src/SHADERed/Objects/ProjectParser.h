#pragma once
#include <SHADERed/Engine/Model.h>
#include <SHADERed/GUIManager.h>
#include <SHADERed/Objects/MessageStack.h>
#include <SHADERed/Objects/ShaderVariable.h>

#include <pugixml/src/pugixml.hpp>
#include <string>
#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/glew.h>
#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

namespace ed {
	class PipelineManager;
	class RenderEngine;
	class ObjectManager;
	class PluginManager;
	class InputLayoutItem;
	class DebugInformation;
	struct PipelineItem;
	namespace pipe {
		struct ShaderPass;
		struct GeometryItem;
		struct Model;
		struct VertexBuffer;
	}

	class ProjectParser {
	public:
		ProjectParser(PipelineManager* pipeline, ObjectManager* objects, RenderEngine* renderer, PluginManager* plugins, MessageStack* msgs, DebugInformation* debugger, GUIManager* gui);
		~ProjectParser();

		void Open(const std::string& file);
		void OpenTemplate();
		inline void SetTemplate(const std::string& str) { m_template = str; }

		void Save();
		void SaveAs(const std::string& file, bool copyFiles = false);

		std::string LoadProjectFile(const std::string& file);
		std::string LoadFile(const std::string& file);
		char* LoadProjectFile(const std::string& file, size_t& len);
		eng::Model* LoadModel(const std::string& file);

		void SaveProjectFile(const std::string& file, const std::string& data);

		std::string GetRelativePath(const std::string& to);
		std::string GetProjectPath(const std::string& projectFile);
		bool FileExists(const std::string& file);
		std::string GetTexturePath(const std::string& texPath, const std::string& oldProjectPath);

		void ResetProjectDirectory();
		inline void SetProjectDirectory(const std::string& path) { m_projectPath = path; }
		inline const std::string& GetProjectDirectory() { return m_projectPath; }

		inline void SetOpenedFile(const std::string& file) { m_file = file; }
		inline const std::string& GetOpenedFile() { return m_file; }
		inline const std::string& GetTemplate() { return m_template; }

		inline void ModifyProject() { m_modified = true; }
		inline bool IsProjectModified() { return m_modified; }

	private:
		void m_parseV1(pugi::xml_node& projectNode); // old
		void m_parseV2(pugi::xml_node& projectNode); // current -> merge blend, rasterizer and depth states into one "render state" ||| remove input layout parsing ||| ignore shader entry property

		void m_parseVariableValue(pugi::xml_node& node, ShaderVariable* var);
		void m_exportVariableValue(pugi::xml_node& node, ShaderVariable* vars);
		void m_exportShaderVariables(pugi::xml_node& node, std::vector<ShaderVariable*>& vars);
		GLenum m_toBlend(const char* str);
		GLenum m_toBlendOp(const char* str);
		GLenum m_toComparisonFunc(const char* str);
		GLenum m_toStencilOp(const char* str);
		GLenum m_toCullMode(const char* str);

		void m_exportItems(pugi::xml_node& node, std::vector<PipelineItem*>& items, const std::string& oldProjectPath);
		void m_importItems(const char* owner, pipe::ShaderPass* data, const pugi::xml_node& node, const std::vector<InputLayoutItem>& inpLayout,
			std::map<pipe::GeometryItem*, std::pair<std::string, pipe::ShaderPass*>>& geoUBOs,
			std::map<pipe::Model*, std::pair<std::string, pipe::ShaderPass*>>& modelUBOs,
			std::map<pipe::VertexBuffer*, std::pair<std::string, pipe::ShaderPass*>>& vbUBOs,
			std::map<pipe::VertexBuffer*, std::pair<std::string, pipe::ShaderPass*>>& vbInstanceUBOs); // TODO: why not just use PipelineItem

		bool m_modified;

		GUIManager* m_ui;
		PipelineManager* m_pipe;
		ObjectManager* m_objects;
		PluginManager* m_plugins;
		RenderEngine* m_renderer;
		MessageStack* m_msgs;
		DebugInformation* m_debug;
		std::string m_file;
		std::string m_projectPath;
		std::string m_template;

		std::vector<std::string> m_pluginList;
		void m_addPlugin(const std::string& name);

		std::vector<std::pair<std::string, eng::Model*>> m_models;
	};
}