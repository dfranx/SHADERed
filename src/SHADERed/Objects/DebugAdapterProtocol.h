#pragma once
#include <memory>
#include <SHADERed/Objects/DebugInformation.h>
#include <SHADERed/Objects/SPIRVParser.h>

namespace dap { class Session; }
namespace ed {
	class GUIManager;

	class DebugAdapterProtocol {
	public:
		DebugAdapterProtocol(DebugInformation* dbgr, GUIManager* gui, bool* run);
		~DebugAdapterProtocol();

		void Initialize();

		void StartDebugging(const std::string& path, ShaderStage stage);
		void StopDebugging();

		void SendStepEvent();
		void SendBreakpointEvent();

		void Terminate();

		inline bool IsStarted() { return m_started; }

		struct VariableValue {
			int64_t ID;
			std::string Name;
			std::string Value;
			std::string Type;

			std::vector<VariableValue*> Children;
		};

		struct StackFrame {
			int64_t ID;
			std::string Name;
			std::string RealName;
			int Line;
		};

	private:
		GUIManager* m_ui;
		DebugInformation* m_debugger;
		bool* m_run;

		ed::SPIRVParser m_parser;

		std::unique_ptr<dap::Session> m_session;
		bool m_started;

		bool m_sessionEnded, m_lastStep;

		std::string m_path;
		ShaderStage m_stage;

		std::vector<StackFrame> m_stack;
		void m_updateStack();

		std::vector<VariableValue*> m_values, m_locals, m_args, m_globals;
		void m_cleanVariables(std::vector<VariableValue*>& vals);
		VariableValue* m_addVariable(const std::string& name, const std::string& val, const std::string& type, std::vector<VariableValue*>& vals);
		void m_updateVariableList();
		DebugAdapterProtocol::VariableValue* m_processSPIRVResult(const std::string& name, spvm_state_t vm, spvm_result_t type, spvm_member_t mems, spvm_word mem_count);

		std::string m_getTypeString(spvm_result_t type, bool returnEmpty = false);
	};
}