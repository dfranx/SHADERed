#pragma once
#include <ShaderDebugger/ShaderDebugger.h>
#include <ShaderDebugger/TextureCube.h>
#include "Debug/PixelInformation.h"
#include "ShaderLanguage.h"
#include "ObjectManager.h"
#include "RenderEngine.h"

namespace ed
{
	class DebugInformation
	{
	public:
		DebugInformation(ObjectManager* objs, RenderEngine* renderer);

		inline void ClearPixelList() { m_pixels.clear(); }
		inline void AddPixel(const PixelInformation& px) { m_pixels.push_back(px); }
		inline std::vector<PixelInformation>& GetPixelList() { return m_pixels; }

		bool SetSource(ed::ShaderLanguage lang, sd::ShaderType stage, const std::string& entry, const std::string& src);
		void InitEngine(PixelInformation& pixel, int id = 0); // set up input variables
		void Fetch(int id = 0);

		std::string VariableValueToString(const bv_variable& var, int indent = 0);

		inline const std::string& GetWatchValue(size_t index) { return m_watchValues[index]; }
		inline std::vector<char*>& GetWatchList() { return m_watchExprs; }
		void ClearWatchList();
		void RemoveWatch(size_t index);
		void AddWatch(const std::string& expr, bool execute = true);
		void UpdateWatchValue(size_t index);

		void AddBreakpoint(const std::string& file, int line, const std::string& condition, bool enabled = true);
		void RemoveBreakpoint(const std::string& file, int line);
		void SetBreakpointEnabled(const std::string& file, int line, bool enable);
		const sd::Breakpoint& GetBreakpoint(const std::string& file, int line);
		inline const std::vector<sd::Breakpoint>& GetBreakpointList(const std::string& file) { return m_breakpoints[file]; }
		inline const std::vector<bool>& GetBreakpointStateList(const std::string& file) { return m_breakpointStates[file]; }
		inline const std::unordered_map<std::string, std::vector<sd::Breakpoint>>& GetBreakpointList() { return m_breakpoints; }
		inline const std::unordered_map<std::string, std::vector<bool>>& GetBreakpointStateList() { return m_breakpointStates; }
		inline void ClearBreakpointList() { m_breakpoints.clear(); m_breakpointStates.clear(); }

		glm::vec2 GetVertexScreenPosition(PixelInformation& pixel, int id);

		sd::ShaderDebugger Engine;

		inline void SetDebugging(bool debug) { m_isDebugging = debug; }
		inline bool IsDebugging() { return m_isDebugging; }

		inline sd::ShaderType GetShaderStage() { return m_stage; }

	private:
		ObjectManager* m_objs;
		RenderEngine* m_renderer;

		bool m_isDebugging;

		bv_stack m_args, m_argsFetch;
		bv_library* m_libHLSL, *m_libGLSL;
		std::string m_entry;
		PixelInformation* m_pixel;
		ed::ShaderLanguage m_lang;
		sd::ShaderType m_stage;
		std::vector<PixelInformation> m_pixels;

		sd::Structure m_vsOutput; // hlsl VS output texture description

		void m_cleanTextures(sd::ShaderType stage);
		std::unordered_map<sd::ShaderType, std::vector<sd::Texture*>> m_textures;
		std::unordered_map<sd::ShaderType, std::vector<sd::TextureCube*>> m_cubemaps;

		std::vector<char*> m_watchExprs;
		std::vector<std::string> m_watchValues;

		std::unordered_map<std::string, std::vector<sd::Breakpoint>> m_breakpoints;
		std::unordered_map<std::string, std::vector<bool>> m_breakpointStates;
	};
}