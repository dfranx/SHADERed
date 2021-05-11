#pragma once
#include <memory>
#include <SHADERed/Objects/DebugInformation.h>

namespace dap {
	class Session;
}

namespace ed {
	class DebugAdapterProtocol {
	public:
		DebugAdapterProtocol(DebugInformation* dbgr, bool* run);
		~DebugAdapterProtocol();

		void Initialize();

		void StartDebugging(const std::string& path, ShaderStage stage);
		void StopDebugging();

		void Terminate();

		inline bool IsStarted() { return m_started; }

		std::string DebugMessage;

		struct VariableValue {
			int64_t ID;
			std::string Name;
			std::string Value;
			std::string Type;

			std::vector<VariableValue*> Children;
		};

	private:
		DebugInformation* m_debugger;
		bool* m_run;

		std::unique_ptr<dap::Session> m_session;
		bool m_started;

		std::string m_path;
		ShaderStage m_stage;

		std::vector<VariableValue*> m_values, m_locals, m_args, m_globals;
		void m_cleanVariables(std::vector<VariableValue*>& vals);
		VariableValue* m_addVariable(const std::string& name, const std::string& val, const std::string& type, std::vector<VariableValue*>& vals);
		void m_updateVariableList();
		DebugAdapterProtocol::VariableValue* m_processSPIRVResult(const std::string& name, spvm_state_t vm, spvm_result_t type, spvm_member_t mems, spvm_word mem_count);

		std::string m_getTypeString(spvm_result_t type, bool returnEmpty = false);
	};
}