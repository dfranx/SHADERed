#pragma once
#include <SHADERed/Engine/Timer.h>
#include <SHADERed/Objects/DebugInformation.h>
#include <SHADERed/Objects/MessageStack.h>
#include <SHADERed/Objects/PipelineManager.h>
#include <SHADERed/Objects/PluginManager.h>
#include <SHADERed/Objects/ProjectParser.h>
#include <SHADERed/Objects/PerformanceTimer.h>

#include <functional>
#include <unordered_map>

#include <glm/glm.hpp>
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
	class ObjectManager;

	uint32_t getPixelID(GLuint rt, uint8_t* data, int x, int y, int width);

	class RenderEngine {
	public:
		RenderEngine(PipelineManager* pipeline, ObjectManager* objects, ProjectParser* project, MessageStack* messages, PluginManager* plugins, DebugInformation* debugger);
		~RenderEngine();

		int DebugVertexPick(PipelineItem* pass, PipelineItem* item, glm::vec2 r, int group);
		int DebugInstancePick(PipelineItem* pass, PipelineItem* item, glm::vec2 r, int group);

		void Render(int width, int height, bool isDebug = false, PipelineItem* breakItem = nullptr);
		inline void Render(bool isDebug = false, PipelineItem* breakItem = nullptr) { Render(m_lastSize.x, m_lastSize.y, isDebug, breakItem); }
		void Recompile(const char* name);
		void RecompileFile(const char* fname);
		void RecompileFromSource(const char* name, const std::string& vs = "", const std::string& ps = "", const std::string& gs = "", const std::string& tcs = "", const std::string& tes = "");
		void Pick(float sx, float sy, bool multiPick, std::function<void(PipelineItem*)> func = nullptr);
		void Pick(PipelineItem* item, bool add = false);
		inline bool IsPicked(PipelineItem* item) { return std::count(m_pick.begin(), m_pick.end(), item); }

		void FlushCache();
		void AddPickedItem(PipelineItem* pipe, bool multiPick = false);

		std::pair<PipelineItem*, PipelineItem*> GetPipelineItemByDebugID(int id); // get pipeline item by it's debug id

		inline void AllowComputeShaders(bool cs) { m_computeSupported = cs; }
		inline void AllowTessellationShaders(bool ts) {
			m_tessellationSupported = ts;
			if (ts) glGetIntegerv(GL_MAX_PATCH_VERTICES, &m_tessMaxPatchVertices);
		}
		inline int GetMaxPatchVertices() { return m_tessMaxPatchVertices; }

		inline void RequestTextureResize() { m_lastSize = glm::ivec2(1, 1); }
		inline GLuint GetTexture() { return m_rtColor; }
		inline GLuint GetDepthTexture() { return m_rtDepth; }
		inline glm::ivec2 GetLastRenderSize() { return m_lastSize; }

		inline const std::vector<PerformanceTimer>& GetPerformanceTimers() { return m_perfTimers; }
		inline unsigned long long GetGPUTime() { return m_totalPerfTime; }

		inline bool IsPaused() { return m_paused; }
		void Pause(bool pause);

		// list of items waiting to be parsed
		std::vector<PipelineItem*> SPIRVQueue;

	public:
		struct ItemVariableValue {
			ItemVariableValue(ed::ShaderVariable* var)
			{
				Variable = var;
				OldValue = var->Data;
				NewValue = new ShaderVariable(var->GetType(), var->Name, var->System);
				NewValue->Function = var->Function;
				Item = nullptr;
			}
			PipelineItem* Item;
			ed::ShaderVariable* Variable;
			char* OldValue;
			ed::ShaderVariable* NewValue;
		};

		inline std::vector<ItemVariableValue>& GetItemVariableValues() { return m_itemValues; }
		inline void AddItemVariableValue(const ItemVariableValue& item) { m_itemValues.push_back(item); }
		inline void RemoveItemVariableValue(PipelineItem* item, ShaderVariable* var)
		{
			for (int i = 0; i < m_itemValues.size(); i++)
				if (m_itemValues[i].Item == item && m_itemValues[i].Variable == var) {
					m_itemValues.erase(m_itemValues.begin() + i);
					return;
				}
		}
		inline void RemoveItemVariableValues(PipelineItem* item)
		{
			for (int i = 0; i < m_itemValues.size(); i++)
				if (m_itemValues[i].Item == item) {
					m_itemValues.erase(m_itemValues.begin() + i);
					i--;
				}
		}

	private:
		PipelineManager* m_pipeline;
		ObjectManager* m_objects;
		ProjectParser* m_project;
		MessageStack* m_msgs;
		PluginManager* m_plugins;
		DebugInformation* m_debug;

		// are compute shaders supported?
		bool m_computeSupported;

		// are tessellation shaders supported?
		bool m_tessellationSupported;
		int m_tessMaxPatchVertices;

		// paused time?
		bool m_paused;

		/* 'window' FBO */
		glm::ivec2 m_lastSize;
		GLuint m_rtColor, m_rtDepth, m_rtColorMS, m_rtDepthMS;
		bool m_fbosNeedUpdate;

		// check for the #include's & change the source code accordingly (includeStack == prevent recursion)
		void m_includeCheck(std::string& src, std::vector<std::string> includeStack, int& lineBias);

		// apply macros to GLSL source code
		void m_applyMacros(std::string& source, pipe::ShaderPass* pass);
		void m_applyMacros(std::string& source, pipe::ComputePass* pass);
		void m_applyMacros(std::string& source, pipe::AudioPass* pass); // TODO: merge this function with the ones above

		// compile to spirv - plugin edition
		bool m_pluginCompileToSpirv(PipelineItem* owner, std::vector<GLuint>& spv, const std::string& path, const std::string& entry, plugin::ShaderStage stage, ed::ShaderMacro* macros, size_t macroCount, const std::string& actualSrc = "");
		const char* m_pluginProcessGLSL(const char* path, const char* src);

		/* picking */
		bool m_pickAwaiting;
		float m_pickDist;
		std::function<void(PipelineItem*)> m_pickHandle;
		glm::vec3 m_pickOrigin; // view space
		glm::vec3 m_pickDir;
		std::vector<PipelineItem*> m_pick;
		bool m_wasMultiPick;
		void m_pickItem(PipelineItem* item, bool multiPick);

		// cache
		std::vector<PipelineItem*> m_items;
		std::vector<GLuint> m_shaders;
		std::vector<GLuint> m_debugShaders;
		std::unordered_map<pipe::ShaderPass*, std::vector<GLuint>> m_fbos;
		std::unordered_map<pipe::ShaderPass*, GLuint> m_fboMS; // multisampled fbo's
		std::unordered_map<pipe::ShaderPass*, GLuint> m_fboCount;
		std::unordered_map<pipe::ComputePass*, int> m_uboMax;
		struct ShaderPack {
			ShaderPack() { VS = GS = PS = TCS = TES = 0; }
			GLuint VS, PS, GS, TCS, TES;
		};
		std::vector<ShaderPack> m_shaderSources;

		std::vector<PerformanceTimer> m_perfTimers;
		unsigned long long m_totalPerfTime;
		eng::Timer m_lastPerfMeasure;

		GLuint m_generalDebugShader;

		void m_updatePassFBO(ed::pipe::ShaderPass* pass);

		std::vector<ItemVariableValue> m_itemValues; // list of all values to apply once we start rendering

		eng::Timer m_cacheTimer;
		void m_cache();
	};
}