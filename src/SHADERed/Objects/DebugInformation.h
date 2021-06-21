#pragma once
#include <SHADERed/Objects/Debug/PixelInformation.h>
#include <SHADERed/Objects/Debug/DebuggerSuggestion.h>
#include <SHADERed/Objects/Debug/Breakpoint.h>
#include <SHADERed/Objects/ObjectManager.h>
#include <SHADERed/Objects/RenderEngine.h>
#include <SHADERed/Objects/ShaderLanguage.h>
#include <SHADERed/Objects/Debug/ExpressionCompiler.h>

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
		inline spvm_context_t GetVMContext() { return m_vmContext; }
		inline spvm_ext_opcode_func* GetGLSLExtension() { return m_vmGLSL; }
		inline int GetCurrentLine() { return m_shader->language == SpvSourceLanguageHLSL ? (m_vm->current_line - 1) : m_vm->current_line; }
		inline bool IsVMRunning() { return m_vm != nullptr && m_vm->code_current != nullptr; }
		inline void SetCurrentFile(const std::string& file) { m_file = file; }
		inline const std::string& GetCurrentFile() { return m_file; }
		inline const std::vector<int>& GetFunctionStackLines() { return m_funcStackLines; }
		inline const std::vector<unsigned int>& GetSPIRV() { return m_spv; }
		
		spvm_member_t GetVariable(const std::string& str, size_t& count, spvm_result_t& outType);
		spvm_member_t GetVariable(const std::string& str, size_t& count);
		spvm_member_t GetVariableFromState(spvm_state_t state, const std::string& str, size_t& count, spvm_result_t& outType);
		spvm_member_t GetVariableFromState(spvm_state_t state, const std::string& str, size_t& count);
		void GetVariableValueAsString(std::stringstream& outString, spvm_state_t state, spvm_result_t type, spvm_member_t mems, spvm_word mem_count, const std::string& prefix);

		void PrepareVertexShader(PipelineItem* pass, PipelineItem* item, PixelInformation* px = nullptr);
		void SetVertexShaderInput(PixelInformation& pixel, int vertexIndex);
		glm::vec4 ExecuteVertexShader();
		void CopyVertexShaderOutput(PixelInformation& px, int vertexIndex);

		void PreparePixelShader(PipelineItem* pass, PipelineItem* item, PixelInformation* px = nullptr);
		float SetPixelShaderInput(PixelInformation& pixel);
		glm::vec4 ExecutePixelShader(int x, int y, int loc = 0);
		glm::vec4 GetPixelShaderOutput(int loc = 0);

		void PrepareGeometryShader(PipelineItem* pass, PipelineItem* item, PixelInformation* px = nullptr);
		void SetGeometryShaderInput(PixelInformation& pixel);
		void ExecuteGeometryShader();

		void PrepareTessellationControlShader(PipelineItem* pass, PipelineItem* item, PixelInformation* px = nullptr);
		void SetTessellationControlShaderInput(PixelInformation& pixel);
		void ExecuteTessellationControlShader(unsigned int invocationId);
		void CopyTessellationControlShaderOutput();

		void PrepareComputeShader(PipelineItem* pass, int x, int y, int z);

		spvm_result_t Immediate(const std::string& entry, spvm_result_t& outType);

		spvm_word GetEntryPoint(ShaderStage stage);
		void PrepareDebugger();

		bool Jump(int line);
		bool Continue();
		void Step();
		void StepInto();
		void StepOut();
		bool CheckBreakpoint(int line);
		
		void ClearPixelData(PixelInformation& px);
		void ClearPixelList();
		inline void AddPixel(const PixelInformation& px) { m_pixels.push_back(px); }
		inline std::vector<PixelInformation>& GetPixelList() { return m_pixels; }
		inline PixelInformation* GetPixel() { return m_pixel; }

		inline void AddSuggestion(const DebuggerSuggestion& px) { m_suggestions.push_back(px); }
		inline std::vector<DebuggerSuggestion>& GetSuggestionList() { return m_suggestions; }

		inline const std::string& GetWatchValue(size_t index) { return m_watchValues[index]; }
		inline std::vector<char*>& GetWatchList() { return m_watchExprs; }
		void ClearWatchList();
		void RemoveWatch(size_t index);
		void AddWatch(const std::string& expr, bool execute = true);
		void UpdateWatchValue(size_t index);

		inline const std::string& GetVectorWatchValue(size_t index) { return m_vectorWatchValues[index]; }
		inline std::vector<char*>& GetVectorWatchList() { return m_vectorWatchExprs; }
		inline std::vector<glm::vec4>& GetVectorWatchColors() { return m_vectorWatchColor; }
		inline std::vector<glm::vec4>& GetVectorWatchPositions() { return m_vectorWatchPositions; }
		void ClearVectorWatchList();
		void RemoveVectorWatch(size_t index);
		void AddVectorWatch(const std::string& expr, glm::vec4 color, bool execute = true);
		void UpdateVectorWatchValue(size_t index);

		void AddBreakpoint(const std::string& file, int line, bool useCondition, const std::string& condition, bool enabled = true);
		void RemoveBreakpoint(const std::string& file, int line);
		void SetBreakpointEnabled(const std::string& file, int line, bool enable);
		dbg::Breakpoint* GetBreakpoint(const std::string& file, int line, bool& enabled);
		inline const std::vector<dbg::Breakpoint>& GetBreakpointList(const std::string& file) { return m_breakpoints[file]; }
		inline const std::vector<bool>& GetBreakpointStateList(const std::string& file) { return m_breakpointStates[file]; }
		inline const std::unordered_map<std::string, std::vector<dbg::Breakpoint>>& GetBreakpointList() { return m_breakpoints; }
		inline const std::unordered_map<std::string, std::vector<bool>>& GetBreakpointStateList() { return m_breakpointStates; }
		inline void ClearBreakpointList(const std::string& file)
		{
			if (m_breakpoints.count(file))
				m_breakpoints.erase(file);
			if (m_breakpointStates.count(file))
				m_breakpointStates.erase(file);
		}
		inline void ClearBreakpointList()
		{
			m_breakpoints.clear();
			m_breakpointStates.clear();
		}

		inline void SetDebugging(bool debug) { m_isDebugging = debug; }
		inline bool IsDebugging() { return m_isDebugging; }
		inline ShaderStage GetStage() { return m_stage; }

		// compute shared stuff
		void SyncWorkgroup();
		struct SharedMemoryEntry {
			spvm_result Data;
			spvm_result_t Destination;
			spvm_word Slot;
		};
		std::vector<SharedMemoryEntry> SharedMemory;

		// geometry shader stuff
		void EmitVertex(const glm::vec4& position);
		void EndPrimitive();
		inline bool IsGeometryUpdated() { return m_updatedGeometryOutput; }
		inline void ResetGeometryUpdated() { m_updatedGeometryOutput = false; }

		// VertexShaderPosition variable
		glm::vec4 GetPositionThroughVertexShader(PipelineItem* pass, PipelineItem* item, const glm::vec3& pos);

		// analyzer stuff
		inline void ToggleAnalyzer(bool analyze)
		{
			if (m_vm != nullptr)
				m_vm->analyzer = analyze ? &m_analyzer : nullptr;
		}
		void OnUndefinedBehavior(spvm_state_t state, spvm_word ubID);
		inline spvm_word GetLastUndefinedBehaviorType() { return m_ubLastType; }
		inline spvm_word GetLastUndefinedBehaviorLine() { return m_ubLastLine; }
		inline spvm_word GetUndefinedBehaviorCount() { return m_ubCount; }

	private:
		ObjectManager* m_objs;
		RenderEngine* m_renderer;
		MessageStack* m_msgs;

		bool m_updatedGeometryOutput;
		ExpressionCompiler m_compiler;

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

		// keep track of replaced results
		struct OriginalValue {
			OriginalValue(spvm_state_t state, int slot, spvm_word member_count, spvm_member_t members)
			{
				State = state;
				Slot = slot;
				MemberCount = member_count;
				Members = members;
			}
			spvm_state_t State;
			int Slot;
			spvm_word MemberCount;
			spvm_member_t Members;
		};

		spvm_state_t* m_workgroup;
		int m_localThreadIndex;
		int m_threadX, m_threadY, m_threadZ, m_numGroupsX, m_numGroupsY, m_numGroupsZ;
		void m_setupWorkgroup();
		std::vector<OriginalValue> m_originalValues;
		void m_setThreadID(spvm_state_t state, int x, int y, int z, int numGroupsX, int numGroupsY, int numGroupsZ);
		
		void m_copyUniforms(PipelineItem* pass, PipelineItem* item, PixelInformation* px = nullptr);
		void m_setupVM(std::vector<unsigned int>& spv);
		void m_resetVM();
		spvm_state_t m_vm;
		spvm_program_t m_shader;
		spvm_analyzer m_analyzer;
		std::vector<unsigned int> m_spv;

		spvm_state_t m_vmImmediate;
		spvm_program_t m_shaderImmediate;
		std::vector<unsigned int> m_spvImmediate;

		PixelInformation* m_pixel;
		ShaderStage m_stage;
		std::string m_file;

		std::vector<int> m_funcStackLines;

		std::vector<DebuggerSuggestion> m_suggestions;
		std::vector<PixelInformation> m_pixels;

		std::vector<char*> m_watchExprs;
		std::vector<std::string> m_watchValues;

		std::vector<char*> m_vectorWatchExprs;
		std::vector<std::string> m_vectorWatchValues;
		std::vector<glm::vec4> m_vectorWatchColor;
		std::vector<glm::vec4> m_vectorWatchPositions;

		std::unordered_map<std::string, std::vector<dbg::Breakpoint>> m_breakpoints;
		std::unordered_map<std::string, std::vector<bool>> m_breakpointStates;

		spvm_word m_ubLastType; // currently only keep track of last UB that happened
		spvm_word m_ubLastLine; // line on which last undefined behavior happened
		spvm_word m_ubCount;	// number of undefined behaviors that happened
	};
}