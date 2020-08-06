#pragma once
#include <SHADERed/Objects/Debug/PixelInformation.h>
#include <SHADERed/Objects/Debug/Breakpoint.h>
#include <SHADERed/Objects/ObjectManager.h>
#include <SHADERed/Objects/RenderEngine.h>
#include <SHADERed/Objects/ShaderLanguage.h>
#ifdef BUILD_IMMEDIATE_MODE
	#include <SHADERed/Objects/Debug/ExpressionCompiler.h>
#endif

#include <sstream>

extern "C" {
	#include <spvm/program.h>
	#include <spvm/state.h>
	#include <spvm/ext/GLSL450.h>
}

namespace ed {
	class DebugInformation {
	public:
		DebugInformation(ObjectManager* objs, RenderEngine* renderer, MessageStack* msgs);
		~DebugInformation();

		inline spvm_state_t GetVM() { return m_vm; }
		inline spvm_state_t GetVMImmediate() { return m_vmImmediate; }
		inline int GetCurrentLine() { return m_shader->language == SpvSourceLanguageHLSL ? (m_vm->current_line - 1) : m_vm->current_line; }
		inline bool IsVMRunning() { return m_vm->code_current != nullptr; }
		inline void SetCurrentFile(const std::string& file) { m_file = file; }
		inline const std::string& GetCurrentFile() { return m_file; }
		inline const std::vector<int>& GetFunctionStackLines() { return m_funcStackLines; }

		spvm_member_t GetVariable(const std::string& str, size_t& count, spvm_result_t& outType);
		spvm_member_t GetVariable(const std::string& str, size_t& count);
		spvm_member_t GetVariableFromState(spvm_state_t state, const std::string& str, size_t& count, spvm_result_t& outType);
		spvm_member_t GetVariableFromState(spvm_state_t state, const std::string& str, size_t& count);
		void GetVariableValueAsString(std::stringstream& outString, spvm_state_t state, spvm_result_t type, spvm_member_t mems, spvm_word mem_count, const std::string& prefix);

		void PrepareVertexShader(PipelineItem* pass, PipelineItem* item, PixelInformation* px = nullptr);
		void SetVertexShaderInput(PipelineItem* pass, eng::Model::Mesh::Vertex vertex, int vertexID, int instanceID, ed::BufferObject* instanceBuffer = nullptr);
		glm::vec4 ExecuteVertexShader();
		void CopyVertexShaderOutput(PixelInformation& px, int vertexIndex);

		void PreparePixelShader(PipelineItem* pass, PipelineItem* item, PixelInformation* px = nullptr);
		void SetPixelShaderInput(PixelInformation& pixel);
		glm::vec4 ExecutePixelShader(int x, int y, int loc = 0);

		spvm_result_t Immediate(const std::string& entry, spvm_result_t& outType);

		void PrepareDebugger();

		void Jump(int line);
		void Continue();
		void Step();
		void StepInto();
		void StepOut();
		bool CheckBreakpoint(int line);
		
		void ClearPixelList();
		inline void AddPixel(const PixelInformation& px) { m_pixels.push_back(px); }
		inline std::vector<PixelInformation>& GetPixelList() { return m_pixels; }

		inline const std::string& GetWatchValue(size_t index) { return m_watchValues[index]; }
		inline std::vector<char*>& GetWatchList() { return m_watchExprs; }
		void ClearWatchList();
		void RemoveWatch(size_t index);
		void AddWatch(const std::string& expr, bool execute = true);
		void UpdateWatchValue(size_t index);

		void AddBreakpoint(const std::string& file, int line, const std::string& condition, bool enabled = true);
		void RemoveBreakpoint(const std::string& file, int line);
		void SetBreakpointEnabled(const std::string& file, int line, bool enable);
		dbg::Breakpoint* GetBreakpoint(const std::string& file, int line, bool& enabled);
		inline const std::vector<dbg::Breakpoint>& GetBreakpointList(const std::string& file) { return m_breakpoints[file]; }
		inline const std::vector<bool>& GetBreakpointStateList(const std::string& file) { return m_breakpointStates[file]; }
		inline const std::unordered_map<std::string, std::vector<dbg::Breakpoint>>& GetBreakpointList() { return m_breakpoints; }
		inline const std::unordered_map<std::string, std::vector<bool>>& GetBreakpointStateList() { return m_breakpointStates; }
		inline void ClearBreakpointList()
		{
			m_breakpoints.clear();
			m_breakpointStates.clear();
		}

		inline void SetDebugging(bool debug) { m_isDebugging = debug; }
		inline bool IsDebugging() { return m_isDebugging; }
		inline ShaderStage GetStage() { return m_stage; }

	private:
		ObjectManager* m_objs;
		RenderEngine* m_renderer;
		MessageStack* m_msgs;

#ifdef BUILD_IMMEDIATE_MODE
		ExpressionCompiler m_compiler;
#endif

		bool m_isDebugging;

		inline glm::vec2 m_getScreenCoord(const glm::vec4& inp)
		{
			return glm::vec2(((inp / inp.w) + 1.0f) * 0.5f);
		}
		glm::vec3 m_getWeights(glm::vec2 a, glm::vec2 b, glm::vec2 c, glm::vec2 p);

		glm::vec3 m_processWeight(glm::ivec2 offset);
		void m_interpolateValues(spvm_state_t state, glm::vec3 weights);

		std::vector<spvm_image_t> m_images; // TODO: clear these + smart cache


		spvm_context_t m_vmContext;
		spvm_ext_opcode_func* m_vmGLSL;

		void m_copyUniforms(PipelineItem* pass, PipelineItem* item, PixelInformation* px = nullptr);
		void m_setupVM(std::vector<unsigned int>& spv);
		void m_resetVM();
		spvm_state_t m_vm;
		spvm_program_t m_shader;
		std::vector<unsigned int> m_spv;

		spvm_state_t m_vmImmediate;
		spvm_program_t m_shaderImmediate;
		std::vector<unsigned int> m_spvImmediate;

		PixelInformation* m_pixel;
		ShaderStage m_stage;
		std::string m_file;

		std::vector<int> m_funcStackLines;

		std::vector<PixelInformation> m_pixels;

		std::vector<char*> m_watchExprs;
		std::vector<std::string> m_watchValues;

		std::unordered_map<std::string, std::vector<dbg::Breakpoint>> m_breakpoints;
		std::unordered_map<std::string, std::vector<bool>> m_breakpointStates;
	};
}