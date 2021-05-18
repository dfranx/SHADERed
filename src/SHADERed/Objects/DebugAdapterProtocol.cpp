#include <dap/io.h>
#include <dap/protocol.h>
#include <dap/session.h>
#include <SHADERed/Objects/DebugAdapterProtocol.h>
#include <SHADERed/GUIManager.h>
#include <SHADERed/UI/Debug/TessellationControlOutputUI.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#define DAP_THREAD_ID (int)m_stage
#define DAP_LOCALS_VAR_REF_ID 1
#define DAP_GLOBALS_VAR_REF_ID 2
#define DAP_ARGUMENTS_VAR_REF_ID 3
#define DAP_CUSTOM_VAR_REF_ID 1000

// clean up the code + remove some files from libs/cppdap and libs/json
// readme, videos, push the extension to vscode store, blog post

namespace ed {
	dap::Variable m_convertVariable(DebugAdapterProtocol::VariableValue* var)
	{
		dap::Variable ret;
		ret.name = var->Name;
		ret.value = var->Value;
		ret.type = var->Type;
		ret.variablesReference = (var->Children.size() > 0) ? var->ID : 0;
		return ret;
	}

	DebugAdapterProtocol::DebugAdapterProtocol(DebugInformation* dbgr, GUIManager* gui, bool* run)
	{
		m_debugger = dbgr;
		m_started = false;
		m_ui = gui;

		m_run = run;
	}
	DebugAdapterProtocol::~DebugAdapterProtocol()
	{
	}
	void DebugAdapterProtocol::Initialize()
	{
#ifdef _WIN32
		_setmode(_fileno(stdin), _O_BINARY);
		_setmode(_fileno(stdout), _O_BINARY);
#endif

		m_started = true;
		m_session = dap::Session::create();


		// https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Initialize
		// First message, respond with capabilities
		m_session->registerHandler([&](const dap::InitializeRequest& req) {
			dap::InitializeResponse response;
			response.supportsStepBack = false;
			response.supportsConfigurationDoneRequest = true;
			response.supportsRestartRequest = false;
			response.supportsRestartFrame = false;
			response.supportsEvaluateForHovers = true;
			response.supportsConditionalBreakpoints = true;
			response.supportsFunctionBreakpoints = false;
			response.supportsHitConditionalBreakpoints = false;
			response.supportsGotoTargetsRequest = true;
			return response;
		});

		
		// https://microsoft.github.io/debug-adapter-protocol/specification#Requests_ConfigurationDone
		// All configuration requests have been made
		m_session->registerHandler([&](const dap::ConfigurationDoneRequest&) {
			return dap::ConfigurationDoneResponse();
		});


		// https://microsoft.github.io/debug-adapter-protocol/specification#Events_Initialized
		// Send after InitializeResponse
		m_session->registerSentHandler(
			[&](const dap::ResponseOrError<dap::InitializeResponse>& res) {
				m_session->send(dap::InitializedEvent());
			});


		// https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Launch
		// client wants to start the debugger?
		m_session->registerHandler([&](const dap::LaunchRequest& req) {
			return dap::LaunchResponse(); 
		});

		// https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Disconnect
		// Handle disconnect requests
		m_session->registerHandler([&](const dap::DisconnectRequest& req) {
			m_debugger->SetDebugging(false);
			if (req.terminateDebuggee.value(false))
				*m_run = false;
			return dap::DisconnectResponse();
		});

		// https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Threads
		// get the list of active threads
		m_session->registerHandler([&](const dap::ThreadsRequest& req) {
			dap::ThreadsResponse response;

			if (!m_debugger->IsVMRunning()) {
				if (m_sessionEnded)
					m_lastStep = true;
				m_sessionEnded = true;

				if (m_lastStep && m_sessionEnded)
					return response;
			}

			// this is called on each step AFAIK, update the variable list here
			m_updateVariableList();

			// also update the stack
			m_updateStack();

			// create thread
			dap::Thread thread;
			thread.id = DAP_THREAD_ID;
			thread.name = "Shader";
			switch (m_stage) {
			case ShaderStage::Vertex:		thread.name = "Vertex Shader"; break;
			case ShaderStage::Pixel:		thread.name = "Pixel Shader"; break;
			case ShaderStage::Geometry:		thread.name = "Geometry Shader"; break;
			case ShaderStage::Compute:		thread.name = "Compute Shader"; break;
			case ShaderStage::Audio:		thread.name = "Audio Shader"; break;
			case ShaderStage::Plugin:		thread.name = "Plugin Shader"; break;
			case ShaderStage::TessellationControl:
				thread.name = "Tessellation Control Shader";
				break;
			case ShaderStage::TessellationEvaluation:
				thread.name = "Tessellation Evaluation Shader";
				break;
			}
			response.threads.push_back(thread);

			return response;
		});

		
		// https://microsoft.github.io/debug-adapter-protocol/specification#Requests_StackTrace
		// report the call stack for a given thread
		m_session->registerHandler(
			[&](const dap::StackTraceRequest& req)
				-> dap::ResponseOrError<dap::StackTraceResponse> {

				if (req.threadId != DAP_THREAD_ID)
					return dap::Error("Unknown threadId '%d'", int(req.threadId));

				dap::Source source;
				source.path = m_path;

				dap::StackTraceResponse response;

				for (const auto& info : m_stack) {
					dap::StackFrame frame;
					frame.line = info.Line;
					frame.column = 1;
					frame.name = info.Name;
					frame.id = info.ID;
					frame.source = source;
					response.stackFrames.push_back(frame);
				}

				return response;
			});

		
		// https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Scopes
		// report all the scopes of a given stack frame
		m_session->registerHandler([&](const dap::ScopesRequest& req)
									   -> dap::ResponseOrError<dap::ScopesResponse> {

			if (req.frameId > m_stack.size())
				return dap::Error("Unknown frameId '%d'", int(req.frameId));

			dap::ScopesResponse response;

			dap::Scope globals;
			globals.name = "Globals";
			globals.presentationHint = "registers";
			globals.variablesReference = DAP_GLOBALS_VAR_REF_ID;
			response.scopes.push_back(globals);

			dap::Scope args;
			args.name = "Arguments";
			args.presentationHint = "arguments";
			args.variablesReference = DAP_ARGUMENTS_VAR_REF_ID;
			if (m_stack.size() > 0) {
				args.line = m_parser.Functions[m_stack[0].RealName].LineStart;
				args.endLine = m_parser.Functions[m_stack[0].RealName].LineEnd;
			}
			response.scopes.push_back(args);

			dap::Scope locals;
			locals.name = "Locals";
			locals.presentationHint = "locals";
			locals.variablesReference = DAP_LOCALS_VAR_REF_ID;
			if (m_stack.size() > 0) {
				locals.line = m_parser.Functions[m_stack[0].RealName].LineStart;
				locals.endLine = m_parser.Functions[m_stack[0].RealName].LineEnd;
			}
			response.scopes.push_back(locals);

			return response;
		});

		
		// https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Variables
		// report all the variables for a given scope
		m_session->registerHandler([&](const dap::VariablesRequest& req)
									   -> dap::VariablesResponse {
			dap::VariablesResponse response;

			if (req.variablesReference == DAP_LOCALS_VAR_REF_ID ||
				req.variablesReference == DAP_GLOBALS_VAR_REF_ID ||
				req.variablesReference == DAP_ARGUMENTS_VAR_REF_ID)
			{
				std::vector<VariableValue*>* group = &m_locals;
				if (req.variablesReference == DAP_GLOBALS_VAR_REF_ID)
					group = &m_globals;
				else if (req.variablesReference == DAP_ARGUMENTS_VAR_REF_ID)
					group = &m_args;

				for (VariableValue* loc : *group)
					response.variables.push_back(m_convertVariable(loc));
			} else {
				for (VariableValue* data : m_values) {
					if (data->ID == req.variablesReference) {
						for (VariableValue* child : data->Children)
							response.variables.push_back(m_convertVariable(child));
						break;
					}
				}
			}

			return response;
		});

		
		// https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Evaluate
		// evaluate an expression
		m_session->registerHandler(
			[&](const dap::EvaluateRequest& req)
				-> dap::ResponseOrError<dap::EvaluateResponse>
			{
				spvm_result_t type = nullptr;
				spvm_result_t res = m_debugger->Immediate(req.expression, type);

				if (res == nullptr || type == nullptr)
					return dap::Error("Failed to execute the expression");

				std::stringstream strValue;
				m_debugger->GetVariableValueAsString(strValue, m_debugger->GetVMImmediate(), type, res->members, res->member_count, "");
				
				dap::EvaluateResponse response;
				response.result = strValue.str();
				response.type = m_getTypeString(type, false);

				response.variablesReference = 0;
				if (type->value_type == spvm_value_type_array ||
					type->value_type == spvm_value_type_runtime_array ||
					type->value_type == spvm_value_type_matrix ||
					type->value_type == spvm_value_type_vector)
				{
					VariableValue* eval = m_processSPIRVResult("evaluate", m_debugger->GetVMImmediate(), type, res->members, res->member_count);
					response.variablesReference = eval->ID;
				}
				
				return response;
			});


		// https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Next
		// step a single line
		m_session->registerHandler([&](const dap::NextRequest& req) {
			m_debugger->Step();

			this->SendStepEvent();

			return dap::NextResponse();
		});


		// https://microsoft.github.io/debug-adapter-protocol/specification#Requests_StepIn
		// step into a function
		m_session->registerHandler([&](const dap::StepInRequest& req) {
			m_debugger->StepInto();

			this->SendStepEvent();

			return dap::StepInResponse();
		});


		// https://microsoft.github.io/debug-adapter-protocol/specification#Requests_StepIn
		// step into a function
		m_session->registerHandler([&](const dap::StepOutRequest& req) {
			m_debugger->StepOut();

			this->SendStepEvent();

			return dap::StepOutResponse();
		});

		// https://microsoft.github.io/debug-adapter-protocol/specification#Requests_StepIn
		// step into a function
		m_session->registerHandler([&](const dap::ContinueRequest& req) {
			bool hitBkpt = m_debugger->Continue();

			if (hitBkpt)
				this->SendBreakpointEvent();
			else
				this->SendStepEvent();

			return dap::ContinueResponse();
		});

		
		// https://microsoft.github.io/debug-adapter-protocol/specification#Requests_GotoTargets
		// list of goto targets(?)
		m_session->registerHandler([&](const dap::GotoTargetsRequest& req) {
			dap::GotoTargetsResponse res;

			dap::GotoTarget target;
			target.id = req.line;
			target.line = req.line;
			target.label = req.source.path.value();
			res.targets.push_back(target);

			return res;
		});


		// https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Goto
		// jump to a cursor line?
		m_session->registerHandler([&](const dap::GotoRequest& req) {
			bool hitBkpt = m_debugger->Jump(req.targetId);

			if (hitBkpt)
				this->SendBreakpointEvent();
			else
				this->SendStepEvent();

			return dap::GotoResponse();
		});


		// https://microsoft.github.io/debug-adapter-protocol/specification#Requests_SetBreakpoints
		// set breakpoints request
		m_session->registerHandler([&](const dap::SetBreakpointsRequest& req) {
			dap::SetBreakpointsResponse res;

			auto breakpoints = req.breakpoints.value({});
			std::string path = std::filesystem::absolute(req.source.path.value("")).generic_u8string();

#ifdef _WIN32
			if (path.size() > 0 && islower(path[0])) // hax
				path[0] = toupper(path[0]);
#endif
			m_debugger->ClearBreakpointList(path);

			res.breakpoints.resize(breakpoints.size());
			for (size_t i = 0; i < breakpoints.size(); i++) {
				std::string condition = breakpoints[i].condition.value("");

				m_debugger->AddBreakpoint(path, breakpoints[i].line, breakpoints[i].condition.has_value() && condition.size() > 0, condition, true);

				res.breakpoints[i].verified = true;
			}

			return res;
		});

		// bind the sessions to stdin and stdout
		auto in = dap::file(stdin, false);
		auto out = dap::file(stdout, false);
		m_session->bind(in, out);
	}
	void DebugAdapterProtocol::StartDebugging(const std::string& path, ShaderStage stage)
	{
		if (!m_started)
			return;

		dap::StoppedEvent event;
		event.reason = "pause";
		event.threadId = (int)stage;
		m_session->send(event);

		m_path = path;
		m_stage = stage;
		m_sessionEnded = false;
		m_lastStep = false;

		if (m_debugger->GetSPIRV().size() > 0)
			m_parser.Parse(m_debugger->GetSPIRV(), false);
	}
	void DebugAdapterProtocol::SendStepEvent()
	{
		dap::StoppedEvent event;
		event.reason = "step";
		event.threadId = DAP_THREAD_ID;
		m_session->send(event);

		if (m_debugger->IsDebugging() && m_debugger->GetStage() == ShaderStage::TessellationControl)
			((DebugTessControlOutputUI*)m_ui->Get(ViewID::DebugTessControlOutput))->Refresh();
	}
	void DebugAdapterProtocol::SendBreakpointEvent()
	{
		dap::StoppedEvent event;
		event.reason = "breakpoint";
		event.threadId = DAP_THREAD_ID;
		m_session->send(event);

		if (m_debugger->IsDebugging() && m_debugger->GetStage() == ShaderStage::TessellationControl)
			((DebugTessControlOutputUI*)m_ui->Get(ViewID::DebugTessControlOutput))->Refresh();
	}
	void DebugAdapterProtocol::StopDebugging()
	{
		if (!m_started)
			return;

		dap::ThreadEvent event;
		event.reason = "exited";
		event.threadId = DAP_THREAD_ID;
		m_session->send(event);
	}
	void DebugAdapterProtocol::Terminate() 
	{
		if (!m_started)
			return;

		StopDebugging();

		dap::TerminatedEvent event;
		m_session->send(event);

		m_started = false;
	}
	void DebugAdapterProtocol::m_updateStack()
	{
		m_stack.clear();

		spvm_state_t vm = m_debugger->GetVM();

		if (vm == nullptr || vm->function_stack_info == nullptr)
			return;

		const std::vector<int>& lines = m_debugger->GetFunctionStackLines();

		for (int i = vm->function_stack_current; i >= 0; i--) {
			spvm_result_t func = vm->function_stack_info[i];
			if (!(vm->owner->language == SpvSourceLanguageHLSL && i == 0)) {
				std::string fname(func->name);
				if (fname.size() > 0 && fname[0] == '@') // clean up the @main(
					fname = fname.substr(1);
				size_t parenth = fname.find('(');
				if (parenth != std::string::npos)
					fname = fname.substr(0, parenth);

				int line = 0;
				if (i >= lines.size())
					line = vm->current_line;
				else
					line = lines[i];

				if (vm->owner->language == SpvSourceLanguageHLSL)
					line--;


				StackFrame frame;
				frame.ID = i;
				frame.Line = line;
				frame.Name = fname;
				frame.RealName = func->name;
				m_stack.push_back(frame);
			}
		}
	}
	void DebugAdapterProtocol::m_cleanVariables(std::vector<VariableValue*>& vals)
	{
		for (VariableValue* val : vals)
			delete val;
		vals.clear();
	}
	DebugAdapterProtocol::VariableValue* DebugAdapterProtocol::m_addVariable(const std::string& name, const std::string& val, const std::string& type, std::vector<VariableValue*>& vals)
	{
		VariableValue* var = new VariableValue();
		var->ID = DAP_CUSTOM_VAR_REF_ID + vals.size();
		var->Name = name;
		var->Value = val;
		var->Type = type;
		vals.push_back(var);

		return var;
	}
	void DebugAdapterProtocol::m_updateVariableList()
	{
		m_cleanVariables(m_values);
		m_locals.clear();
		m_args.clear();
		m_globals.clear();

		spvm_state_t vm = m_debugger->GetVM();

		if (vm == nullptr)
			return;

		for (spvm_word i = 0; i < vm->owner->bound; i++) {
			spvm_result_t slot = &vm->results[i];

			if (((slot->type == spvm_result_type_variable && (slot->owner == nullptr || slot->owner == vm->current_function)) || 
				(slot->type == spvm_result_type_function_parameter && slot->owner == vm->current_function)) && 
				slot->name != nullptr)
			{
				spvm_result_t vtype = spvm_state_get_type_info(vm->results, &vm->results[slot->pointer]);
				if (slot->name[0] == 0 && slot->owner == nullptr) {
					// cache those global structures with "" name
					for (spvm_word i = 0; i < vtype->member_count; i++) {
						std::string memberName = "";
						if (i < vtype->member_name_count)
							memberName = vtype->member_name[i];

						m_globals.push_back(m_processSPIRVResult(memberName, vm, &vm->results[slot->members[i].type], slot->members[i].members, slot->members[i].member_count));
					}
				} else {
					// cache variable value
					auto res = m_processSPIRVResult(slot->name ? slot->name : "[UNKNOWN]", vm, vtype, slot->members, slot->member_count);

					// push the new variable into relevant array
					if (slot->type == spvm_result_type_variable) {
						if (slot->owner == nullptr)
							m_globals.push_back(res);
						else
							m_locals.push_back(res);
					} else
						m_args.push_back(res);
				}
			}
		}
	}
	DebugAdapterProtocol::VariableValue* DebugAdapterProtocol::m_processSPIRVResult(const std::string& name, spvm_state_t vm, spvm_result_t vtype, spvm_member_t mems, spvm_word mem_count)
	{
		std::vector<VariableValue*> ret;

		std::string type = m_getTypeString(vtype, true);
		std::string value = type;

		// get the value only if needed
		if (type.size() == 0) {
			std::stringstream ss;
			m_debugger->GetVariableValueAsString(ss, vm, vtype, mems, mem_count, "");
			value = ss.str();
		}

		// create a new variable
		VariableValue* newVar = m_addVariable(name, value, type, m_values);

		// get the vector components
		if (vtype->value_type == spvm_value_type_vector) {
			const std::string vecComps[4] = { "x", "y", "z", "w" };
			
			char valType = vm->results[vtype->pointer].value_type;
			std::string compType = "float";
			if (valType == spvm_value_type_int) compType = "int";
			else if (valType == spvm_value_type_bool) compType = "bool";

			for (spvm_word i = 0; i < mem_count; i++)
				newVar->Children.push_back(m_addVariable(vecComps[i], std::to_string(mems[i].value.f), compType, m_values));
		}
		// get object components
		else if (vtype->value_type == spvm_value_type_struct) {
			for (spvm_word i = 0; i < vtype->member_count; i++) {
				std::string memberName = "";
				if (i < vtype->member_name_count)
					memberName = vtype->member_name[i];

				newVar->Children.push_back(m_processSPIRVResult(memberName, vm, &vm->results[mems[i].type], mems[i].members, mems[i].member_count));
			}
		}
		// get array components
		else if (vtype->value_type == spvm_value_type_array || vtype->value_type == spvm_value_type_runtime_array) {
			for (spvm_word i = 0; i < mem_count; i++)
				newVar->Children.push_back(m_processSPIRVResult("[" + std::to_string(i) + "]", vm, &vm->results[mems[i].type], mems[i].members, mems[i].member_count));
		}
		// get matrix components
		else if (vtype->value_type == spvm_value_type_matrix) {
			for (spvm_word i = 0; i < mem_count; i++)
				newVar->Children.push_back(m_processSPIRVResult("[" + std::to_string(i) + "]", vm, &vm->results[mems[i].type], mems[i].members, mems[i].member_count));
		}

		return newVar;
	}
	std::string DebugAdapterProtocol::m_getTypeString(spvm_result_t type, bool returnEmpty)
	{
		spvm_state_t vm = m_debugger->GetVM();
		if (vm && vm->owner->language == SpvSourceLanguageHLSL) {
			// HLSL names
			if (type->value_type == spvm_value_type_vector && !returnEmpty) {
				std::string ret = "float";

				spvm_result_t elType = spvm_state_get_type_info(vm->results, &vm->results[type->pointer]);
				if (elType->value_type == spvm_value_type_int)
					ret = "int";
				if (elType->value_type == spvm_value_type_bool)
					ret = "bool";

				return ret + std::to_string(type->member_count);
			}
		}

		if (type->value_type == spvm_value_type_array || type->value_type == spvm_value_type_runtime_array)
			return "Array";
		else if (type->value_type == spvm_value_type_bool && !returnEmpty)
			return "bool";
		else if (type->value_type == spvm_value_type_float && !returnEmpty)
			return "float";
		else if (type->value_type == spvm_value_type_image || type->value_type == spvm_value_type_sampled_image)
			return "Image";
		else if (type->value_type == spvm_value_type_int && !returnEmpty)
			return "int";
		else if (type->value_type == spvm_value_type_pointer)
			return "pointer";
		else if (type->value_type == spvm_value_type_struct)
			return "Object";
		else if (type->value_type == spvm_value_type_matrix)
			return "Matrix";
		else if (type->value_type == spvm_value_type_vector && !returnEmpty) {
			std::string ret = "vec" + std::to_string(type->member_count);

			spvm_result_t elType = spvm_state_get_type_info(vm->results, &vm->results[type->pointer]);
			if (elType->value_type == spvm_value_type_int)
				ret = "i" + ret;
			if (elType->value_type == spvm_value_type_bool)
				ret = "b" + ret;

			return ret;
		}

		return "";
	}
}