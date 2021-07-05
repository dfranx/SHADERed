#include <SHADERed/Objects/DebugInformation.h>
#include <SHADERed/Objects/SystemVariableManager.h>
#include <SHADERed/Objects/Logger.h>

#include <iomanip>

#define GET_VALUE_WITH_CHECK_FLOAT(val, c) (val == nullptr ? 0.0f : val->members[c].value.f)
#define GET_VALUE2_WITH_CHECK_FLOAT(val, c, r) (val == nullptr ? 0.0f : val->members[c].members[r].value.f)
#define GET_VALUE_WITH_CHECK_DOUBLE(val, c) (val == nullptr ? 0.0 : val->members[c].value.d)
#define GET_VALUE2_WITH_CHECK_DOUBLE(val, c, r) (val == nullptr ? 0.0 : val->members[c].members[r].value.d)
#define GET_VALUE_WITH_CHECK_INT(val, c) (val == nullptr ? 0 : val->members[c].value.s)
#define GET_VALUE2_WITH_CHECK_INT(val, c, r) (val == nullptr ? 0 : val->members[c].members[r].value.s)

/* compute shader callbacks */
void allocateWorkgroupMemory(struct spvm_state* state, spvm_word result_id, spvm_word type_id)
{
	ed::DebugInformation* dbgr = (ed::DebugInformation*)state->owner->user_data;
	ed::DebugInformation::SharedMemoryEntry memEntry;

	memEntry.Destination = &state->results[result_id];
	memcpy(&memEntry.Data, memEntry.Destination, sizeof(spvm_result));
	spvm_result_allocate_typed_value(&memEntry.Data, state->results, type_id);
	memEntry.Slot = result_id;

	dbgr->SharedMemory.push_back(memEntry);
}
void writeWorkgroupMemory(struct spvm_state* state, spvm_word result_id, spvm_word val_id)
{
	spvm_result_t ptr = &state->results[result_id];
	if (ptr->source_location != nullptr) {
		spvm_word word_count = ptr->source_word_count;
		spvm_source code = ptr->source_location;

		spvm_word var_type = SPVM_READ_WORD(code);
		spvm_word id = SPVM_READ_WORD(code);
		spvm_word memory_id = SPVM_READ_WORD(code);

		spvm_result_t sharedData = nullptr;

		ed::DebugInformation* dbgr = (ed::DebugInformation*)state->owner->user_data;

		for (int i = 0; i < dbgr->SharedMemory.size(); i++) {
			if (dbgr->SharedMemory[i].Slot == memory_id) {
				sharedData = &dbgr->SharedMemory[i].Data;
				break;
			}
		}

		if (sharedData == nullptr)
			return;

		spvm_word index_count = word_count - 4;

		spvm_word index_id = SPVM_READ_WORD(code);
		spvm_word index = state->results[index_id].members[0].value.s;

		spvm_member_t result = sharedData->members + MIN(index, sharedData->member_count - 1);

		while (index_count) {
			index_id = SPVM_READ_WORD(code);
			index = state->results[index_id].members[0].value.s;

			result = result->members + MIN(index, result->member_count - 1);

			index_count--;
		}

		spvm_member* members = nullptr;
		spvm_word member_count = 0;
		if (result->member_count != 0) {
			member_count = result->member_count;
			members = result->members;
		} else {
			member_count = 1;
			members = result;
		}
		spvm_member_memcpy(members, state->results[val_id].members, member_count);
	}
}
void controlBarrier(struct spvm_state* state, spvm_word exec, spvm_word mem, spvm_word sem)
{
	ed::DebugInformation* dbgr = (ed::DebugInformation*)state->owner->user_data;

	// copy memory
	for (int i = 0; i < dbgr->SharedMemory.size(); i++) {
		const ed::DebugInformation::SharedMemoryEntry& entry = dbgr->SharedMemory[i];
		spvm_member_memcpy(entry.Data.members, entry.Destination->members, entry.Data.member_count);
	}

	// synchronize threads
	dbgr->SyncWorkgroup();

	// copy memory
	for (int i = 0; i < dbgr->SharedMemory.size(); i++) {
		const ed::DebugInformation::SharedMemoryEntry& entry = dbgr->SharedMemory[i];
		spvm_member_memcpy(entry.Destination->members, entry.Data.members, entry.Destination->member_count);
	}
}
void atomicOperation(spvm_word inst, spvm_word word_count, struct spvm_state* state)
{
	ed::DebugInformation* dbgr = (ed::DebugInformation*)state->owner->user_data;


	switch (inst) {
	case SpvOpAtomicLoad:
	case SpvOpAtomicExchange:
	case SpvOpAtomicIIncrement:
	case SpvOpAtomicIDecrement:
	case SpvOpAtomicIAdd:
	case SpvOpAtomicISub:
	case SpvOpAtomicSMin:
	case SpvOpAtomicUMin:
	case SpvOpAtomicSMax:
	case SpvOpAtomicUMax:
	case SpvOpAtomicAnd:
	case SpvOpAtomicOr:
	case SpvOpAtomicXor: {
		spvm_word result_type_id = SPVM_READ_WORD(state->code_current);
		spvm_word result_id = SPVM_READ_WORD(state->code_current);
		spvm_word pointer_id = SPVM_READ_WORD(state->code_current);
		spvm_word memory_id = SPVM_READ_WORD(state->code_current);
		spvm_word semantics_id = SPVM_READ_WORD(state->code_current);

		spvm_result_t result = &state->results[result_id];
		spvm_result_t pointer = &state->results[pointer_id];

		// copy old value
		spvm_member_memcpy(result->members, pointer->members, result->member_count);

		// find shared memory pointer
		spvm_result_t shared = nullptr;
		for (ed::DebugInformation::SharedMemoryEntry& entry : dbgr->SharedMemory) {
			if (pointer_id == entry.Slot) {
				shared = &entry.Data;
				break;
			}
		}
		spvm_result_t data = (shared == nullptr) ? pointer : shared;

		if (inst == SpvOpAtomicExchange) {
			spvm_word value_id = SPVM_READ_WORD(state->code_current);
			spvm_result_t value = &state->results[value_id];

			spvm_member_memcpy(data->members, value->members, data->member_count);
		} else if (inst == SpvOpAtomicIIncrement) {
			for (int i = 0; i < data->member_count; i++)
				data->members[i].value.u++;
		} else if (inst == SpvOpAtomicIDecrement) {
			for (int i = 0; i < data->member_count; i++)
				data->members[i].value.u--;
		} else if (inst == SpvOpAtomicIAdd) {
			spvm_word value_id = SPVM_READ_WORD(state->code_current);
			spvm_result_t value = &state->results[value_id];

			for (int i = 0; i < data->member_count; i++)
				data->members[i].value.u += value->members[i].value.u;
		} else if (inst == SpvOpAtomicISub) {
			spvm_word value_id = SPVM_READ_WORD(state->code_current);
			spvm_result_t value = &state->results[value_id];

			for (int i = 0; i < data->member_count; i++)
				data->members[i].value.u -= value->members[i].value.u;
		} else if (inst == SpvOpAtomicSMin) {
			spvm_word value_id = SPVM_READ_WORD(state->code_current);
			spvm_result_t value = &state->results[value_id];

			for (int i = 0; i < data->member_count; i++)
				data->members[i].value.s = MIN(value->members[i].value.s, data->members[i].value.s);
		} else if (inst == SpvOpAtomicUMin) {
			spvm_word value_id = SPVM_READ_WORD(state->code_current);
			spvm_result_t value = &state->results[value_id];

			for (int i = 0; i < data->member_count; i++)
				data->members[i].value.u = MIN(value->members[i].value.u, data->members[i].value.u);
		} else if (inst == SpvOpAtomicSMax) {
			spvm_word value_id = SPVM_READ_WORD(state->code_current);
			spvm_result_t value = &state->results[value_id];

			for (int i = 0; i < data->member_count; i++)
				data->members[i].value.s = MAX(value->members[i].value.s, data->members[i].value.s);
		} else if (inst == SpvOpAtomicUMax) {
			spvm_word value_id = SPVM_READ_WORD(state->code_current);
			spvm_result_t value = &state->results[value_id];

			for (int i = 0; i < data->member_count; i++)
				data->members[i].value.u = MAX(value->members[i].value.u, data->members[i].value.u);
		} else if (inst == SpvOpAtomicAnd) {
			spvm_word value_id = SPVM_READ_WORD(state->code_current);
			spvm_result_t value = &state->results[value_id];

			for (int i = 0; i < data->member_count; i++)
				data->members[i].value.u &= value->members[i].value.u;
		} else if (inst == SpvOpAtomicOr) {
			spvm_word value_id = SPVM_READ_WORD(state->code_current);
			spvm_result_t value = &state->results[value_id];

			for (int i = 0; i < data->member_count; i++)
				data->members[i].value.u |= value->members[i].value.u;
		} else if (inst == SpvOpAtomicXor) {
			spvm_word value_id = SPVM_READ_WORD(state->code_current);
			spvm_result_t value = &state->results[value_id];

			for (int i = 0; i < data->member_count; i++)
				data->members[i].value.u ^= value->members[i].value.u;
		}

		if (shared)
			spvm_member_memcpy(pointer->members, shared->members, pointer->member_count);
	} break;
	case SpvOpAtomicCompareExchange: {
		spvm_word result_type_id = SPVM_READ_WORD(state->code_current);
		spvm_word result_id = SPVM_READ_WORD(state->code_current);
		spvm_word pointer_id = SPVM_READ_WORD(state->code_current);
		spvm_word memory_id = SPVM_READ_WORD(state->code_current);
		SPVM_SKIP_WORD(state->code_current);
		SPVM_SKIP_WORD(state->code_current);
		spvm_word value_id = SPVM_READ_WORD(state->code_current);
		spvm_word comparator_id = SPVM_READ_WORD(state->code_current);

		spvm_result_t result = &state->results[result_id];
		spvm_result_t pointer = &state->results[pointer_id];
		spvm_result_t value = &state->results[value_id];
		spvm_result_t comparator = &state->results[comparator_id];

		// copy old value
		spvm_member_memcpy(result->members, pointer->members, result->member_count);

		bool equals = true;
		for (int i = 0; i < pointer->member_count; i++) {
			if (pointer->members[i].value.u != comparator->members[i].value.u) {
				equals = false;
				break;
			}
		}

		if (equals) {
			// find shared memory pointer
			spvm_result_t shared = nullptr;
			for (ed::DebugInformation::SharedMemoryEntry& entry : dbgr->SharedMemory) {
				if (pointer_id == entry.Slot) {
					shared = &entry.Data;
					break;
				}
			}
			spvm_result_t data = (shared == nullptr) ? pointer : shared;

			// copy data
			for (int i = 0; i < pointer->member_count; i++)
				data->members[i].value.u = value->members[i].value.u;

			if (shared)
				spvm_member_memcpy(pointer->members, shared->members, pointer->member_count);
		}
	} break;
	case SpvOpAtomicStore: {
		spvm_word pointer_id = SPVM_READ_WORD(state->code_current);
		SPVM_SKIP_WORD(state->code_current);
		SPVM_SKIP_WORD(state->code_current);
		spvm_word value_id = SPVM_READ_WORD(state->code_current);

		
		spvm_result_t pointer = &state->results[pointer_id];
		spvm_result_t value = &state->results[value_id];

		// find shared memory pointer
		spvm_result_t shared = nullptr;
		for (ed::DebugInformation::SharedMemoryEntry& entry : dbgr->SharedMemory) {
			if (pointer_id == entry.Slot) {
				shared = &entry.Data;
				break;
			}
		}
		spvm_result_t data = (shared == nullptr) ? pointer : shared;

		// copy data
		for (int i = 0; i < pointer->member_count; i++)
			data->members[i].value.u = value->members[i].value.u;

		if (shared)
			spvm_member_memcpy(pointer->members, shared->members, pointer->member_count);
	} break;
	}
}

/* geometry shader callbacks */
void emitVertex(struct spvm_state* state, spvm_word stream)
{
	// TODO: use spvm_word stream

	ed::DebugInformation* dbgr = (ed::DebugInformation*)state->owner->user_data;

	spvm_word mem_count = 0;
	spvm_member_t glPosObject = spvm_state_get_builtin(state, SpvBuiltInPosition, &mem_count);

	glm::vec4 glPos(0.0f);
	for (int i = 0; i < mem_count; i++)
		glPos[i] = glPosObject[i].value.f;

	dbgr->EmitVertex(glPos);
}
void endPrimitive(struct spvm_state* state, spvm_word stream)
{
	// TODO: use spvm_word stream

	ed::DebugInformation* dbgr = (ed::DebugInformation*)state->owner->user_data;
	dbgr->EndPrimitive();
}

/* analyzer functions */
void onUndefinedBehavior(struct spvm_state* state, spvm_word ub)
{
	ed::DebugInformation* dbgr = (ed::DebugInformation*)state->owner->user_data;
	dbgr->OnUndefinedBehavior(state, ub);
}

/* helper functions */
bool isPointInTriangle(glm::vec2 p, glm::vec2 p0, glm::vec2 p1, glm::vec2 p2)
{
	/* https://stackoverflow.com/questions/2049582/how-to-determine-if-a-point-is-in-a-2d-triangle#:~:text=A%20simple%20way%20is%20to,point%20is%20inside%20the%20triangle. */
	float s = p0.y * p2.x - p0.x * p2.y + (p2.y - p0.y) * p.x + (p0.x - p2.x) * p.y;
	float t = p0.x * p1.y - p0.y * p1.x + (p0.y - p1.y) * p.x + (p1.x - p0.x) * p.y;

	if ((s < 0) != (t < 0))
		return false;

	float a = -p1.y * p2.x + p0.y * (p2.x - p1.x) + p0.x * (p1.y - p2.y) + p1.x * p2.y;

	return a < 0 ? (s <= 0 && s + t >= a) : (s >= 0 && s + t <= a);
}
bool isPointOnLine(glm::vec2 p, glm::vec2 p0, glm::vec2 p1)
{
	float v = glm::distance(p0, p) + glm::distance(p, p1) - glm::distance(p0, p1);
	return -0.01 < v && v < 0.01;
}

namespace ed {
	DebugInformation::DebugInformation(ObjectManager* objs, RenderEngine* renderer, MessageStack* msgs)
	{
		m_objs = objs;
		m_renderer = renderer;
		m_isDebugging = false;
		m_vm = nullptr;
		m_shader = nullptr;
		m_pixel = nullptr;
		m_vmImmediate = nullptr;
		m_shaderImmediate = nullptr;
		m_msgs = msgs;
		m_workgroup = nullptr;
		m_updatedGeometryOutput = false;

		m_vmContext = spvm_context_initialize();
		m_vmGLSL = spvm_build_glsl450_ext();

		m_analyzer.on_undefined_behavior = onUndefinedBehavior;
	}
	DebugInformation::~DebugInformation()
	{
		ClearPixelList();

		m_resetVM();

		free(m_vmGLSL);
		spvm_context_deinitialize(m_vmContext);
	}

	void DebugInformation::m_resetVM()
	{
		ed::Logger::Get().Log("Resetting the debugger");

		for (spvm_image_t img : m_images) {
			free(img->data);
			free(img);
		}
		m_images.clear();

		// clear shared memory
		for (SharedMemoryEntry& entry : SharedMemory)
			spvm_member_free(entry.Data.members, entry.Data.member_count);
		SharedMemory.clear();

		// clear workgroup
		if (m_workgroup) {
			for (int j = 0; j < m_originalValues.size(); j++)
			{
				const OriginalValue& originalData = m_originalValues[j];
				originalData.State->results[originalData.Slot].members = originalData.Members;
				originalData.State->results[originalData.Slot].member_count = originalData.MemberCount;
			}
			m_originalValues.clear();
			for (int i = 0; i < m_shader->local_size_x * m_shader->local_size_y * m_shader->local_size_z; i++) {
				if (m_workgroup[i] == nullptr)
					continue;

				spvm_state_delete(m_workgroup[i]);
			}
			free(m_workgroup);
			m_workgroup = nullptr;
		}

		// delete old program & state
		if (m_vm) {
			spvm_state_delete(m_vm);
			m_vm = nullptr;
		}
		if (m_shader) {
			spvm_program_delete(m_shader);
			m_shader = nullptr;
		}

		// delete old immediate program & state
		if (m_vmImmediate) {
			spvm_state_delete(m_vmImmediate);
			m_vmImmediate = nullptr;
		}
		if (m_shaderImmediate) {
			spvm_program_delete(m_shaderImmediate);
			m_shaderImmediate = nullptr;
		}

		// reset undefined behavior info
		m_ubLastType = m_ubLastLine = m_ubCount = 0;
	}
	void DebugInformation::m_setupVM(std::vector<unsigned int>& spv)
	{
		ed::Logger::Get().Log("Parsing the SPIR-V and setting up the debugger");

		m_spv = spv;
		
		// create program & state
		m_shader = spvm_program_create(m_vmContext, (spvm_source)m_spv.data(), m_spv.size());
		m_shader->user_data = this;
		m_shader->allocate_workgroup_memory = allocateWorkgroupMemory;
		m_shader->write_workgroup_memory = writeWorkgroupMemory;
		m_shader->atomic_operation = atomicOperation;

		m_vm = _spvm_state_create_base(m_shader, m_stage == ShaderStage::Pixel, 0);
		m_vm->control_barrier = controlBarrier;
		m_vm->emit_vertex = emitVertex;
		m_vm->end_primitive = endPrimitive;
		m_shader->allocate_workgroup_memory = nullptr; // "sub-programs" shouldn't handle this anymore

		// link GLSL.std.450
		spvm_state_set_extension(m_vm, "GLSL.std.450", m_vmGLSL);
	}
	void DebugInformation::m_setupWorkgroup()
	{
		ed::Logger::Get().Log("Setting up the shader workgroup in the debugger");

		int startX = (m_threadX / m_shader->local_size_x) * m_shader->local_size_x;
		int startY = (m_threadY / m_shader->local_size_y) * m_shader->local_size_y;
		int startZ = (m_threadZ / m_shader->local_size_z) * m_shader->local_size_z;

		int groupSize = m_shader->local_size_x * m_shader->local_size_y * m_shader->local_size_z;

		m_workgroup = (spvm_state_t*)calloc(groupSize, sizeof(spvm_state_t));

		for (int id_x = 0; id_x < m_shader->local_size_x; id_x++) {
			for (int id_y = 0; id_y < m_shader->local_size_y; id_y++) {
				for (int id_z = 0; id_z < m_shader->local_size_z; id_z++) {
					int id = id_z * m_shader->local_size_x * m_shader->local_size_y + id_y * m_shader->local_size_x + id_x;

					if (id == m_localThreadIndex)
						continue;

					m_workgroup[id] = spvm_state_create(m_shader);

					spvm_state_t worker = m_workgroup[id];
					spvm_result_t glsl_std_450 = spvm_state_get_result(worker, "GLSL.std.450");
					if (glsl_std_450)
						glsl_std_450->extension = m_vmGLSL;

					m_setThreadID(worker, startX + id_x, startY + id_y, startZ + id_z, m_numGroupsX, m_numGroupsY, m_numGroupsZ);

					spvm_word fnMain = spvm_state_get_result_location(worker, "main");
					spvm_state_prepare(worker, fnMain);
				}
			}
		}

		for (int i = 0; i < m_shader->bound; i++) {
			spvm_result_t slot = &m_vm->results[i];
			
			if (slot->pointer) {
				spvm_result_t pointerInfo = &m_vm->results[slot->pointer];

				bool wasCopied = false;
				
				if (pointerInfo->value_type == spvm_value_type_pointer) {
					spvm_result_t type_info = spvm_state_get_type_info(m_vm->results, pointerInfo);
					bool isBufferBlock = false;

					for (int i = 0; i < type_info->decoration_count; i++)
						if (type_info->decorations[i].type == SpvDecorationBufferBlock) {
							isBufferBlock = true;
							break;
						}

					if (pointerInfo->storage_class == SpvStorageClassUniformConstant && !isBufferBlock) {
						// textures
						if (type_info->value_type == spvm_value_type_sampled_image || type_info->value_type == spvm_value_type_image) {

							for (int j = 0; j < groupSize; j++)
								if (m_workgroup[j]) {
									m_originalValues.push_back(OriginalValue(m_workgroup[j], i, m_workgroup[j]->results[i].member_count, m_workgroup[j]->results[i].members));
									m_workgroup[j]->results[i].member_count = slot->member_count;
									m_workgroup[j]->results[i].members = slot->members;
									wasCopied = true;
								}
						}
					} else if (pointerInfo->storage_class == SpvStorageClassStorageBuffer || isBufferBlock) {
						// buffers
						for (int j = 0; j < groupSize; j++)
							if (m_workgroup[j]) {
								m_originalValues.push_back(OriginalValue(m_workgroup[j], i, m_workgroup[j]->results[i].member_count, m_workgroup[j]->results[i].members));
								m_workgroup[j]->results[i].member_count = slot->member_count;
								m_workgroup[j]->results[i].members = slot->members;
								wasCopied = true;
							}
					}
				}
				
				if (!wasCopied && (pointerInfo->storage_class == SpvStorageClassUniformConstant || pointerInfo->storage_class == SpvStorageClassUniform)) {
					for (int j = 0; j < groupSize; j++)
						if (m_workgroup[j])
							spvm_member_memcpy(m_workgroup[j]->results[i].members, slot->members, slot->member_count);
				}
			}
		}
	}
	void DebugInformation::m_setThreadID(spvm_state_t worker, int x, int y, int z, int numGroupsX, int numGroupsY, int numGroupsZ)
	{
		if (worker == m_vm) {
			m_threadX = x;
			m_threadY = y;
			m_threadZ = z;
		}

		int localIndex = (z % worker->owner->local_size_z) * worker->owner->local_size_x * worker->owner->local_size_z + 
			(y % worker->owner->local_size_y) * worker->owner->local_size_x + 
			(x % worker->owner->local_size_x);

		if (worker == m_vm)
			m_localThreadIndex = localIndex;

		// input variables
		for (spvm_word i = 0; i < worker->owner->bound; i++) {
			spvm_result_t slot = &worker->results[i];
			spvm_result_t pointer = nullptr;

			if (slot->pointer)
				pointer = &worker->results[worker->results[i].pointer];

			// input variable
			if (pointer) {
				if (pointer->storage_class == SpvStorageClassInput) {
					spvm_word builtinType = 0;
					bool isBuiltin = false;
					for (spvm_word j = 0; j < slot->decoration_count; j++) {
						if (slot->decorations[j].type == SpvDecorationBuiltIn) {
							builtinType = slot->decorations[j].literal1;
							isBuiltin = true;
						}
					}

					if (isBuiltin) {
						if (builtinType == SpvBuiltInGlobalInvocationId) {
							if (slot->member_count >= 3) {
								slot->members[0].value.u = x;
								slot->members[1].value.u = y;
								slot->members[2].value.u = z;
							}
						} else if (builtinType == SpvBuiltInWorkgroupId) {
							if (slot->member_count >= 3) {
								slot->members[0].value.u = x / worker->owner->local_size_x;
								slot->members[1].value.u = y / worker->owner->local_size_y;
								slot->members[2].value.u = z / worker->owner->local_size_z;
							}
						} else if (builtinType == SpvBuiltInLocalInvocationId) {
							if (slot->member_count >= 3) {
								slot->members[0].value.u = x % worker->owner->local_size_x;
								slot->members[1].value.u = y % worker->owner->local_size_y;
								slot->members[2].value.u = z % worker->owner->local_size_z;
							}
						} else if (builtinType == SpvBuiltInLocalInvocationIndex) {
							if (slot->member_count >= 1)
								slot->members[0].value.u = localIndex;
						} else if (builtinType == SpvBuiltInWorkgroupSize) {
							if (slot->member_count >= 3) {
								slot->members[0].value.u = worker->owner->local_size_x;
								slot->members[1].value.u = worker->owner->local_size_y;
								slot->members[2].value.u = worker->owner->local_size_z;
							}
						} else if (builtinType == SpvBuiltInNumWorkgroups) {
							if (slot->member_count >= 3) {
								slot->members[0].value.u = numGroupsX;
								slot->members[1].value.u = numGroupsY;
								slot->members[2].value.u = numGroupsZ;
							}
						}
					}
				}
			}
		}

	}
	void DebugInformation::m_copyUniforms(PipelineItem* owner, PipelineItem* item, PixelInformation* px)
	{
		bool pluginUsesCustomTextures = false;
		bool requiresVarCleanup = false;
		std::vector<ed::ShaderVariable*> vars;
		if (owner->Type == PipelineItem::ItemType::ShaderPass) {
			pipe::ShaderPass* pass = (pipe::ShaderPass*)owner->Data;
			vars = pass->Variables.GetVariables();
		} 
		else if (owner->Type == PipelineItem::ItemType::ComputePass) {
			pipe::ComputePass* pass = (pipe::ComputePass*)owner->Data;
			vars = pass->Variables.GetVariables();
		}
		else if (owner->Type == PipelineItem::ItemType::PluginItem) {
			pipe::PluginItemData* plData = (pipe::PluginItemData*)owner->Data;

			pluginUsesCustomTextures = plData->Owner->PipelineItem_DebugUsesCustomTextures(plData->Type, plData->PluginData);

			if (item != nullptr)
				plData->Owner->PipelineItem_DebugPrepareVariables(plData->Type, plData->PluginData, item->Name);
			
			int varCount = plData->Owner->PipelineItem_GetVariableCount(plData->Type, plData->PluginData);
			vars.resize(varCount);

			for (int i = 0; i < varCount; i++) {
				const char* varName = plData->Owner->PipelineItem_GetVariableName(plData->Type, plData->PluginData, i);
				ShaderVariable::ValueType type = (ShaderVariable::ValueType)plData->Owner->PipelineItem_GetVariableType(plData->Type, plData->PluginData, i);

				ed::ShaderVariable* var = new ed::ShaderVariable(type, varName);
				vars[i] = var;
				int cm = var->GetColumnCount();
				int rm = var->GetRowCount();

				ShaderVariable::ValueType baseType = var->GetBaseType();

				for (int c = 0; c < cm; c++) {
					for (int r = 0; r < rm; r++) {
						switch (baseType) {
						case ShaderVariable::ValueType::Float1: {
							float valFloat = plData->Owner->PipelineItem_GetVariableValueFloat(plData->Type, plData->PluginData, i, c, r);
							var->SetFloat(valFloat, c, r);
						} break;
						case ShaderVariable::ValueType::Integer1: {
							int valInteger = plData->Owner->PipelineItem_GetVariableValueInteger(plData->Type, plData->PluginData, i, c);
							var->SetIntegerValue(valInteger, c);
						} break;
						case ShaderVariable::ValueType::Boolean1: {
							bool valBoolean = plData->Owner->PipelineItem_GetVariableValueBoolean(plData->Type, plData->PluginData, i, c);
							var->SetBooleanValue(valBoolean, c);
						} break;
						}
					}
				}
			}

			requiresVarCleanup = true;
		}

		const auto& itemVars = m_renderer->GetItemVariableValues();
		const std::vector<GLuint>& srvs = m_objs->GetBindList(owner);
		const std::vector<GLuint>& ubos = m_objs->GetUniformBindList(owner);

		if (px)
			SystemVariableManager::Instance().SetViewportSize(px->RenderTextureSize.x, px->RenderTextureSize.y);

		// update variables
		// update system variables
		if (item) {
			if (item->Type == PipelineItem::ItemType::Geometry) {
				pipe::GeometryItem* geoData = reinterpret_cast<pipe::GeometryItem*>(item->Data);

				if (geoData->Type == pipe::GeometryItem::Rectangle) {
					glm::vec3 scaleRect(geoData->Scale.x * SystemVariableManager::Instance().GetViewportSize().x, geoData->Scale.y * SystemVariableManager::Instance().GetViewportSize().y, 1.0f);
					glm::vec3 posRect((geoData->Position.x + 0.5f) * m_renderer->GetLastRenderSize().x, (geoData->Position.y + 0.5f) * m_renderer->GetLastRenderSize().y, -1000.0f);
					SystemVariableManager::Instance().SetGeometryTransform(item, scaleRect, geoData->Rotation, posRect);
				} else
					SystemVariableManager::Instance().SetGeometryTransform(item, geoData->Scale, geoData->Rotation, geoData->Position);

				SystemVariableManager::Instance().SetPicked(m_renderer->IsPicked(item));
			} else if (item->Type == PipelineItem::ItemType::Model) {
				pipe::Model* objData = reinterpret_cast<pipe::Model*>(item->Data);

				SystemVariableManager::Instance().SetPicked(m_renderer->IsPicked(item));
				SystemVariableManager::Instance().SetGeometryTransform(item, objData->Scale, objData->Rotation, objData->Position);
			} else if (item->Type == PipelineItem::ItemType::VertexBuffer) {
				pipe::VertexBuffer* vbData = reinterpret_cast<pipe::VertexBuffer*>(item->Data);

				SystemVariableManager::Instance().SetPicked(m_renderer->IsPicked(item));
				SystemVariableManager::Instance().SetGeometryTransform(item, vbData->Scale, vbData->Rotation, vbData->Position);
			} else if (item->Type == PipelineItem::ItemType::PluginItem) {
				pipe::PluginItemData* plData = reinterpret_cast<pipe::PluginItemData*>(item->Data);

				float scale[3], pos[3], rota[3];
				plData->Owner->PipelineItem_GetTransform(plData->Type, plData->PluginData, pos, scale, rota);

				SystemVariableManager::Instance().SetPicked(m_renderer->IsPicked(item));
				SystemVariableManager::Instance().SetGeometryTransform(item, glm::make_vec3(scale), glm::make_vec3(rota), glm::make_vec3(pos));
			}
		}

		// copy variable values
		for (auto& var : vars) {
			std::string vname(var->Name); // example: "var.lights[0].diffuse", "lights[0].diffuse", etc...
			
			size_t curloc = 0;
			size_t seploc = vname.find_first_of(".[");

			std::string tokenName = "";
			if (seploc == std::string::npos) tokenName = vname;
			else tokenName = vname.substr(0, seploc);

			spvm_result_t result = spvm_state_get_result(m_vm, (const spvm_string)tokenName.c_str());
			
			spvm_member_t pointer = nullptr;
			spvm_result_t pointerType = nullptr;
			spvm_word pointerMCount = 0;

			if (result) {
				pointer = result->members;
				pointerMCount = result->member_count;
				
				if (result->pointer)
					pointerType = spvm_state_get_type_info(m_vm->results, &m_vm->results[result->pointer]);
			}
			else { // search in the nameless buffers
				bool foundBufferVariable = false;
				for (spvm_word i = 0; i < m_shader->bound; i++) {
					if (m_vm->results[i].name && strcmp(m_vm->results[i].name, "") == 0 && m_vm->results[i].type == spvm_result_type_variable) {
						spvm_result_t anonBuffer = &m_vm->results[i];
						spvm_result_t anonBufferType = spvm_state_get_type_info(m_vm->results, &m_vm->results[anonBuffer->pointer]);
						
						
						for (spvm_word j = 0; j < anonBufferType->member_name_count; j++)
							if (strcmp(anonBufferType->member_name[j], tokenName.c_str()) == 0) {
								pointer = anonBuffer->members[j].members;
								pointerMCount = anonBuffer->members[j].member_count;

								if (anonBuffer->members[j].type)
									pointerType = spvm_state_get_type_info(m_vm->results, &m_vm->results[anonBuffer->members[j].type]);
								
								foundBufferVariable = true;

								break;
							}
					}

					if (foundBufferVariable) break;
				}

			}

			if (!pointer)
				continue;

			curloc = seploc == std::string::npos ? std::string::npos : seploc+1;
			if (seploc != std::string::npos)
				seploc = vname.find_first_of(".[", curloc+1);

			while (seploc != std::string::npos) {
				tokenName = vname.substr(curloc, seploc-curloc);

				// index
				if (tokenName.size() > 0 && isdigit(tokenName[0])) {
					tokenName = tokenName.substr(0, tokenName.size() - 1);

					bool allDigits = true;
					for (int i = 0; i < tokenName.size(); i++)
						if (!isdigit(tokenName[i])) {
							allDigits = false;
							break;
						}

					int index = 0;
					if (allDigits && !tokenName.empty())
						index = std::stoi(tokenName);

					if (index < pointerMCount) {
						spvm_member_t mem = pointer + index;
						
						pointerMCount = mem->member_count;
						if (mem->type)
							pointerType = spvm_state_get_type_info(m_vm->results, &m_vm->results[mem->type]);
						pointer = mem->members;
					}
				} 
				// member name
				else if (pointerType) {
					for (int i = 0; i < pointerType->member_name_count; i++) {
						if (strcmp(pointerType->member_name[i], tokenName.c_str()) == 0) {
							spvm_member_t mem = pointer + i;
							
							pointerMCount = mem->member_count;
							if (mem->type)
								pointerType = spvm_state_get_type_info(m_vm->results, &m_vm->results[mem->type]);
							pointer = mem->members;
						}
					}
				}

				curloc = seploc+1;
				seploc = vname.find_first_of(".[", curloc + 1);
			}

			// leftover
			if (curloc < vname.size() && pointerType) {
				tokenName = vname.substr(curloc);

				// index
				if (tokenName.size() > 0 && isdigit(tokenName[0])) {
					tokenName = tokenName.substr(0, tokenName.size() - 1);

					bool allDigits = true;
					for (int i = 0; i < tokenName.size(); i++)
						if (!isdigit(tokenName[i])) {
							allDigits = false;
							break;
						}

					int index = 0;
					if (allDigits && !tokenName.empty())
						index = std::stoi(tokenName);

					if (index < pointerMCount) {
						spvm_member_t mem = pointer + index;

						pointerMCount = mem->member_count;
						if (mem->type)
							pointerType = spvm_state_get_type_info(m_vm->results, &m_vm->results[mem->type]);
						pointer = mem->members;
					}
				} 
				// member name
				else {
					for (int i = 0; i < pointerType->member_name_count; i++) {
						if (strcmp(pointerType->member_name[i], tokenName.c_str()) == 0) {
							spvm_member_t mem = pointer + i;

							pointerMCount = mem->member_count;
							if (mem->type)
								pointerType = spvm_state_get_type_info(m_vm->results, &m_vm->results[mem->type]);
							pointer = mem->members;
						}
					}
				}
			}

			// variable type
			bool useFloat = false, useInt = false, useBool = false;
			switch (var->GetType()) {
			case ShaderVariable::ValueType::Boolean1:
			case ShaderVariable::ValueType::Boolean2:
			case ShaderVariable::ValueType::Boolean3:
			case ShaderVariable::ValueType::Boolean4: 
				useBool = true;
				break;

			case ShaderVariable::ValueType::Integer1:
			case ShaderVariable::ValueType::Integer2:
			case ShaderVariable::ValueType::Integer3:
			case ShaderVariable::ValueType::Integer4: 
				useInt = true;
				break;

			case ShaderVariable::ValueType::Float1:
			case ShaderVariable::ValueType::Float2:
			case ShaderVariable::ValueType::Float3:
			case ShaderVariable::ValueType::Float4:
			case ShaderVariable::ValueType::Float2x2:
			case ShaderVariable::ValueType::Float3x3:
			case ShaderVariable::ValueType::Float4x4:
				useFloat = true;
				break;
			}

			ShaderVariable* actualVar = var;

			if (item) {
				// update system variable value
				SystemVariableManager::Instance().Update(var, item);

				// item variable value
				for (const auto& iVar : itemVars) {
					if (iVar.Item == item && iVar.Variable == var) {
						actualVar = iVar.NewValue;
						break;
					}
				}
			}

			// copy the value now that we have a pointer to actual memory part
			int cCount = std::min<int>(actualVar->GetColumnCount(), pointerMCount);
			for (int c = 0; c < cCount; c++) {
				if (pointer[c].member_count == 0) {
					if (useFloat)
						pointer[c].value.f = actualVar->AsFloat(c);
					else if (useInt)
						pointer[c].value.s = actualVar->AsInteger(c);
					else if (useBool)
						pointer[c].value.b = actualVar->AsBoolean(c);
				} else {
					for (int r = 0; r < pointer[c].member_count; r++)
						pointer[c].members[r].value.f = actualVar->AsFloat(r, c);
				}
			}
		}

		// textures, buffers, etc...
		int sampler2Dloc = 0;
		for (spvm_word i = 0; i < m_shader->bound; i++) {
			spvm_result_t slot = &m_vm->results[i];
			spvm_result_t pointerInfo = nullptr;
			if (slot->pointer)
				pointerInfo = &m_vm->results[slot->pointer];

			if (pointerInfo && pointerInfo->value_type == spvm_value_type_pointer) {
				spvm_result_t type_info = spvm_state_get_type_info(m_vm->results, pointerInfo);
				bool isBufferBlock = false;
				
				for (int i = 0; i < type_info->decoration_count; i++)
					if (type_info->decorations[i].type == SpvDecorationBufferBlock) {
						isBufferBlock = true;
						break;
					}

				if (pointerInfo->storage_class == SpvStorageClassUniformConstant && !isBufferBlock) {
					// textures
					if (type_info->value_type == spvm_value_type_sampled_image || type_info->value_type == spvm_value_type_image) {
						for (int j = 0; j < slot->decoration_count; j++) {
							if (slot->decorations[j].type == SpvDecorationLocation) {
								sampler2Dloc = slot->decorations[j].literal1;
								break;
							}
						}

						GLuint textureID = 0;
						bool wrongBind = false;

						bool isSampled = false;
						if (m_stage != ShaderStage::Compute || type_info->value_type == spvm_value_type_sampled_image || (slot->image_info != nullptr && slot->image_info->sampled == 1))
							isSampled = true;

						if (isSampled) { // srvs
							if (sampler2Dloc < srvs.size())
								textureID = srvs[sampler2Dloc];
							else
								wrongBind = true;
						} else { // ubos
							if (sampler2Dloc < ubos.size())
								textureID = ubos[sampler2Dloc];
							else
								wrongBind = true;
						}

						if (!wrongBind && textureID != 0) {
							ObjectManagerItem* itemData = m_objs->GetByTextureID(textureID);
							if (itemData) {
								if (!(itemData->Type == ObjectType::Texture || itemData->Type == ObjectType::Texture3D || itemData->Type == ObjectType::RenderTexture || itemData->Type == ObjectType::Image || itemData->Type == ObjectType::Image3D || itemData->Type == ObjectType::KeyboardTexture)) {
									wrongBind = true;
									textureID = 0;
								}
							}
						}

						if ((wrongBind || textureID == 0) && !pluginUsesCustomTextures) {
							spvm_image_t img = (spvm_image_t)malloc(sizeof(spvm_image));

							// get texture size
							glm::ivec2 size(1, 1);
							float* imgData = (float*)calloc(1, sizeof(float) * size.x * size.y * 4);

							img->user_data = nullptr;

							spvm_image_create(img, imgData, size.x, size.y, 1);
							free(imgData);

							slot->members[0].image_data = img;

							m_images.push_back(img);

							sampler2Dloc++;
						} 
						else {
							if (slot->members == nullptr) // if slot->members == nullptr it means that it's a pointer/function argument
								continue;

							spvm_image_t img = (spvm_image_t)malloc(sizeof(spvm_image));
							
							if (type_info->image_info == NULL)
								type_info = &m_vm->results[type_info->pointer];
							
							glm::ivec3 imgSize(1, 1, 1);
							float* imgData = nullptr;
							GLuint pluginCustomTexture = 0;
							if (pluginUsesCustomTextures) {
								// cubemaps
								if (type_info->image_info->dim == SpvDimCube) {
									pipe::PluginItemData* plData = (pipe::PluginItemData*)owner->Data;

									pluginCustomTexture = plData->Owner->PipelineItem_DebugGetTexture(plData->Type, plData->PluginData, sampler2Dloc, slot->name);
									plData->Owner->PipelineItem_DebugGetTextureSize(plData->Type, plData->PluginData, sampler2Dloc, slot->name, imgSize.x, imgSize.y, imgSize.z);
									imgSize.z = 6;

									imgData = (float*)malloc(sizeof(float) * imgSize.x * imgSize.y * 6 * 4); // 6 faces

									// get the data from the GPU
									glBindTexture(GL_TEXTURE_CUBE_MAP, pluginCustomTexture);
									for (int i = 0; i < 6; i++)
										glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA, GL_FLOAT, imgData + imgSize.x * imgSize.y * 4 * i);
									glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
								}
								// 3d textures
								else if (type_info->image_info->dim == SpvDim3D) {
									pipe::PluginItemData* plData = (pipe::PluginItemData*)owner->Data;

									pluginCustomTexture = plData->Owner->PipelineItem_DebugGetTexture(plData->Type, plData->PluginData, sampler2Dloc, slot->name);
									plData->Owner->PipelineItem_DebugGetTextureSize(plData->Type, plData->PluginData, sampler2Dloc, slot->name, imgSize.x, imgSize.y, imgSize.z);
									
									imgData = (float*)malloc(sizeof(float) * imgSize.x * imgSize.y * imgSize.z * 4);

									// get the data from the GPU
									glBindTexture(GL_TEXTURE_3D, pluginCustomTexture);
									glGetTexImage(GL_TEXTURE_3D, 0, GL_RGBA, GL_FLOAT, imgData);
									glBindTexture(GL_TEXTURE_3D, 0);
								}
								// 2d textures
								else {
									pipe::PluginItemData* plData = (pipe::PluginItemData*)owner->Data;

									pluginCustomTexture = plData->Owner->PipelineItem_DebugGetTexture(plData->Type, plData->PluginData, sampler2Dloc, slot->name);
									plData->Owner->PipelineItem_DebugGetTextureSize(plData->Type, plData->PluginData, sampler2Dloc, slot->name, imgSize.x, imgSize.y, imgSize.z);
									imgSize.z = 1;

									imgData = (float*)malloc(sizeof(float) * imgSize.x * imgSize.y * 4);

									// get the data from the GPU
									glBindTexture(GL_TEXTURE_2D, pluginCustomTexture);
									glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, imgData);
									glBindTexture(GL_TEXTURE_2D, 0);
								}
							} else {
								// cubemaps
								if (type_info->image_info->dim == SpvDimCube) {
									ObjectManagerItem* itemData = m_objs->GetByTextureID(textureID);

									// get texture size
									glm::ivec2 size(1, 1);
									if (itemData != nullptr) {
										if (itemData->RT != nullptr)
											size = m_objs->GetRenderTextureSize(itemData);
										else if (itemData->Image != nullptr)
											size = itemData->Image->Size;
										else if (itemData->Sound != nullptr)
											size = glm::ivec2(512, 2);
										else
											size = itemData->TextureSize;
									}
									imgSize.x = size.x;
									imgSize.y = size.y;
									imgSize.z = 6;

									imgData = (float*)malloc(sizeof(float) * size.x * size.y * 6 * 4); // 6 faces

									// get the data from the GPU
									glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);
									for (int i = 0; i < 6; i++)
										glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA, GL_FLOAT, imgData + size.x * size.y * 4 * i);
									glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
								}
								// 3d textures
								else if (type_info->image_info->dim == SpvDim3D) {
									// get texture size
									ObjectManagerItem* itemData = m_objs->GetByTextureID(textureID);
									
									if (itemData != nullptr) {
										if (itemData->Image3D != nullptr)
											imgSize = itemData->Image3D->Size;
										else if (itemData->Type == ed::ObjectType::Texture3D)
											imgSize = glm::ivec3(itemData->TextureSize.x, itemData->TextureSize.y, itemData->Depth);
									}

									imgData = (float*)malloc(sizeof(float) * imgSize.x * imgSize.y * imgSize.z * 4);

									// get the data from the GPU
									glBindTexture(GL_TEXTURE_3D, textureID);
									glGetTexImage(GL_TEXTURE_3D, 0, GL_RGBA, GL_FLOAT, imgData);
									glBindTexture(GL_TEXTURE_3D, 0);
								}
								// 2d textures
								else {
									// get texture size
									ObjectManagerItem* itemData = m_objs->GetByTextureID(textureID);
									glm::ivec2 size(1, 1);
									if (itemData != nullptr) {
										if (itemData->RT != nullptr)
											size = m_objs->GetRenderTextureSize(itemData);
										else if (itemData->Image != nullptr)
											size = itemData->Image->Size;
										else if (itemData->Sound != nullptr)
											size = glm::ivec2(512, 2);
										else
											size = itemData->TextureSize;
									}
									imgSize.x = size.x;
									imgSize.y = size.y;

									imgData = (float*)malloc(sizeof(float) * size.x * size.y * 4);

									// get the data from the GPU
									glBindTexture(GL_TEXTURE_2D, textureID);
									glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, imgData);
									glBindTexture(GL_TEXTURE_2D, 0);
								}
							}

							if (imgData != nullptr) {
								spvm_image_create(img, imgData, imgSize.x, imgSize.y, imgSize.z);
								free(imgData);
							}

							if (pluginUsesCustomTextures)
								img->user_data = (void*)pluginCustomTexture;
							else
								img->user_data = (void*)textureID;

							slot->members[0].image_data = img;
							m_images.push_back(img);
							sampler2Dloc++;
						}
					}
				} 
				else if (pointerInfo->storage_class == SpvStorageClassStorageBuffer || isBufferBlock) {
					int binding = 0;
					for (int i = 0; i < slot->decoration_count; i++) {
						if (slot->decorations[i].type == SpvDecorationBinding) {
							binding = slot->decorations[i].literal1;
							break;
						}
					}

					if (binding < ubos.size()) {
						ed::BufferObject* obj = m_objs->GetByBufferID(ubos[binding])->Buffer;

						float* data = (float*)calloc(1, obj->Size);
						glBindBuffer(GL_SHADER_STORAGE_BUFFER, obj->ID);
						glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, obj->Size, data);
						glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

						spvm_word offset = 0;
						for (spvm_word j = 0; j < slot->member_count; j++) {
							spvm_result_t arrayType = spvm_state_get_type_info(m_vm->results, &m_vm->results[slot->members[j].type]);
							spvm_result_t elementType = spvm_state_get_type_info(m_vm->results, &m_vm->results[arrayType->pointer]);
							
							spvm_word bufferElementMCount = slot->members[j].member_count;
							spvm_member_t bufferElement = &slot->members[j];

							if (arrayType->value_type == spvm_value_type_runtime_array) {
								int elCount = obj->Size / 4 - offset;
								int elSize = spvm_result_calculate_size(m_vm->results, arrayType->pointer);
							
								int memCount = elCount / elSize;
								bufferElement->member_count = memCount;
								bufferElement->members = (spvm_member*)calloc(memCount, sizeof(spvm_member));
								for (int k = 0; k < memCount; k++)
									spvm_member_allocate_typed_value(&bufferElement->members[k], m_vm->results, arrayType->pointer);
							}

							spvm_member_recursive_fill(m_vm->results, data, slot->members[j].members, slot->members[j].member_count, arrayType->pointer, &offset);
						}

						free(data);
					}
					// create 1 element sized empty buffer
					else {
						for (spvm_word j = 0; j < slot->member_count; j++) {
							spvm_result_t arrayType = spvm_state_get_type_info(m_vm->results, &m_vm->results[slot->members[j].type]);
							spvm_result_t elementType = spvm_state_get_type_info(m_vm->results, &m_vm->results[arrayType->pointer]);

							spvm_word bufferElementMCount = slot->members[j].member_count;
							spvm_member_t bufferElement = &slot->members[j];

							if (arrayType->value_type == spvm_value_type_runtime_array) {
								bufferElement->member_count = 1;
								bufferElement->members = (spvm_member*)calloc(1, sizeof(spvm_member));
								spvm_member_allocate_typed_value(&bufferElement->members[0], m_vm->results, arrayType->pointer);
							}
						}
					}
				}
			}
		}

		if (requiresVarCleanup) {
			for (int i = 0; i < vars.size(); i++)
				delete vars[i];
			vars.clear();
		}
	}
	
	spvm_member_t DebugInformation::GetVariable(const std::string& vname, size_t& outCount, spvm_result_t& outType)
	{
		return GetVariableFromState(m_vm, vname, outCount, outType);
	}
	spvm_member_t DebugInformation::GetVariable(const std::string& str, size_t& count)
	{
		return GetVariableFromState(m_vm, str, count);
	}
	spvm_member_t DebugInformation::GetVariableFromState(spvm_state_t state, const std::string& vname, size_t& outCount, spvm_result_t& outType)
	{
		bool skipLastTypeGetter = false;

		spvm_word mem_index = 0, mem_count = 0;
		spvm_result_t val = spvm_state_get_local_result(state, state->current_function, (spvm_string)vname.c_str());
		if (val == nullptr)
			val = spvm_state_get_result_with_value(state, (spvm_string)vname.c_str());

		if (val == nullptr) {
			// anon buffer object uniforms
			bool foundBufferVariable = false;
			for (spvm_word i = 0; i < m_shader->bound; i++) {
				if (state->results[i].name) {
					if (strcmp(state->results[i].name, "") == 0 && state->results[i].type == spvm_result_type_variable) {
						val = &state->results[i];
						spvm_result_t buffer_info = spvm_state_get_type_info(state->results, &state->results[val->pointer]);
						for (spvm_word i = 0; i < buffer_info->member_name_count; i++) {
							if (strcmp(vname.c_str(), buffer_info->member_name[i]) == 0) {
								mem_index = i;
								mem_count = 1;

								outType = spvm_state_get_type_info(state->results, &state->results[val->members[mem_index].type]);
								skipLastTypeGetter = true;

								foundBufferVariable = true;
								break;
							}
						}
					}
				}

				if (foundBufferVariable) break;
			}
		} else {
			mem_index = 0;
			mem_count = val->member_count;
		}

		if (val && val->members != nullptr && mem_count != 0) {
			/*if (mem_count == 1 && val->members[mem_index].member_count != 0) {
				outType = spvm_state_get_type_info(state->results, &state->results[val->members[mem_index].type]);
				outCount = val->members[mem_index].member_count;
				return &val->members[mem_index].members[0];
			}*/

			if (!skipLastTypeGetter)
				outType = spvm_state_get_type_info(state->results, &state->results[val->pointer]);
			outCount = mem_count;
			return &val->members[mem_index];
		}

		return nullptr;
	}
	spvm_member_t DebugInformation::GetVariableFromState(spvm_state_t state, const std::string& str, size_t& count)
	{
		spvm_result_t type;
		return GetVariableFromState(state, str, count, type);
	}
	void DebugInformation::GetVariableValueAsString(std::stringstream& outString, spvm_state_t state, spvm_result_t type, spvm_member_t mems, spvm_word mem_count, const std::string& prefix)
	{
		/*
			TODO: use BFS - might have more control over output + faster
			(was lazy, copied from github.com/dfranx/sdbg)
		*/
		
		if (!mems) return;

		bool outPrefix = false;
		bool allRec = true;
		for (spvm_word i = 0; i < mem_count; i++) {

			spvm_result_t vtype = type;
			if (mems[i].type != 0)
				vtype = spvm_state_get_type_info(state->results, &state->results[mems[i].type]);
			if (vtype->member_count > 1 && vtype->pointer != 0 && vtype->value_type != spvm_value_type_matrix && vtype->value_type != spvm_value_type_array)
				vtype = spvm_state_get_type_info(state->results, &state->results[vtype->pointer]);

			if (mems[i].member_count == 0) {
				if (!outPrefix && !prefix.empty()) {
					outPrefix = true;
					outString << prefix << " = ";
				}

				if (vtype->value_type == spvm_value_type_float && type->value_bitcount <= 32)
					outString << std::setw(6) << std::setprecision(3) << std::fixed << mems[i].value.f << " ";
				else if (vtype->value_type == spvm_value_type_float)
					outString << std::setw(6) << std::setprecision(3) << std::fixed << mems[i].value.d << " ";
				else
					outString << mems[i].value.s << " ";
				allRec = false;
			} else {
				if ((type->value_type == spvm_value_type_runtime_array || type->value_type == spvm_value_type_array) && i > 2 && i < mem_count - 3)
					continue;

				std::string newPrefix = prefix;
				if (type->value_type == spvm_value_type_struct)
					newPrefix += "." + std::string(type->member_name[i]);
				if (type->value_type == spvm_value_type_matrix || type->value_type == spvm_value_type_array || type->value_type == spvm_value_type_runtime_array)
					newPrefix += "[" + std::to_string(i) + "]";

				if ((type->value_type == spvm_value_type_runtime_array || type->value_type == spvm_value_type_array) && mem_count > 6 && i == mem_count - 3)
					outString << "...\n";

				GetVariableValueAsString(outString, state, vtype, mems[i].members, mems[i].member_count, newPrefix);
			}
		}
		if (!allRec) outString << "\n";
	}

	spvm_result_t DebugInformation::Immediate(const std::string& entry, spvm_result_t& outType)
	{
		if (m_vm == nullptr)
			return nullptr;

		bool usePlugin = false;
		ed::IPlugin2* plugin2 = nullptr;
		if (m_pixel != nullptr && m_pixel->Pass == nullptr && m_pixel->Pass->Type == PipelineItem::ItemType::PluginItem) {
			pipe::PluginItemData* plData = (pipe::PluginItemData*)m_pixel->Pass->Data;
			if (plData->Owner->GetVersion() >= 2) {
				plugin2 = ((ed::IPlugin2*)plData->Owner);
				
				if (!plugin2->PipelineItem_SupportsImmediateMode(plData->Type, plData->PluginData, (ed::plugin::ShaderStage)m_stage))
					return nullptr;

				if (plugin2->PipelineItem_HasCustomImmediateModeCompiler(plData->Type, plData->PluginData, (ed::plugin::ShaderStage)m_stage)) {
					if (!plugin2->PipelineItem_ImmediateModeCompile(plData->Type, plData->PluginData, (ed::plugin::ShaderStage)m_stage, entry.c_str()))
						return nullptr;
					else
						usePlugin = true;
				}
			}
		}

		int resultID = 0;

		std::vector<std::string> varList;
		if (!usePlugin) {
			std::string curFunction = "";
			if (m_vm && m_vm->current_function && m_vm->current_function->name)
				curFunction = m_vm->current_function->name;

			// compile the expression
			resultID = m_compiler.Compile(entry, curFunction);
			m_compiler.GetSPIRV(m_spvImmediate);

			varList = m_compiler.GetVariableList();
		} else if (plugin2) {
			unsigned int spvSize = plugin2->ImmediateMode_GetSPIRVSize();
			std::vector<unsigned int> spv;
			if (spvSize != 0) {
				unsigned int* spvPtr = plugin2->ImmediateMode_GetSPIRV();
				m_spvImmediate = std::vector<unsigned int>(spvPtr, spvPtr + spvSize);
			}
			resultID = plugin2->ImmediateMode_GetResultID();

			for (unsigned int i = 0u; i < plugin2->ImmediateMode_GetVariableCount(); i++)
				varList.push_back(plugin2->ImmediateMode_GetVariableName(i));
		}

		// error occured
		if (resultID <= 0)
			return nullptr;

		// delete old program & state
		if (m_vmImmediate) {
			spvm_state_delete(m_vmImmediate);
			m_vmImmediate = nullptr;
		}
		if (m_shaderImmediate) {
			spvm_program_delete(m_shaderImmediate);
			m_shaderImmediate = nullptr;
		}

		// create program & state
		m_shaderImmediate = spvm_program_create(m_vmContext, (spvm_source)m_spvImmediate.data(), m_spvImmediate.size());
		m_vmImmediate = _spvm_state_create_base(m_shaderImmediate, m_stage == ShaderStage::Pixel, 0);
		
		// can't use set_extenstion() function because for some reason two GLSL.std.450 instructions are generated with spvgentwo
		for (int i = 0; i < m_shaderImmediate->bound; i++)
			if (m_vmImmediate->results[i].name)
				if (strcmp(m_vmImmediate->results[i].name, "GLSL.std.450") == 0)
					m_vmImmediate->results[i].extension = m_vmGLSL;

		// copy variable values
		spvm_state_group_sync(m_vm);
		for (int i = 0; i < varList.size(); i++) {
			size_t varValueCount = 0;
			spvm_result_t varType = nullptr;
			spvm_member_t varValue = GetVariable(varList[i], varValueCount, varType);

			if (varValue == nullptr)
				continue;

			spvm_result_t pointerToVariable[4] = { nullptr };

			for (int j = 0; j < m_shaderImmediate->bound; j++) {
				if (m_vmImmediate->results[j].name == nullptr)
					continue;
				
				spvm_result_t res = &m_vmImmediate->results[j];
				spvm_result_t resType = spvm_state_get_type_info(m_vmImmediate->results, &m_vmImmediate->results[res->pointer]);

				// TODO: also check for the type, or there might be some crashes caused by two vars with different type (?) (mat4 and vec4 for example)
				
				if (res->member_count == varValueCount && resType->value_type == varType->value_type && res->members != nullptr && strcmp(varList[i].c_str(), res->name) == 0) {
					spvm_member_memcpy(res->members, varValue, varValueCount);

					pointerToVariable[0] = res;

					if (m_vmImmediate->derivative_used) {
						if (m_vmImmediate->derivative_group_x) {
							spvm_member_memcpy(m_vmImmediate->derivative_group_x->results[j].members, GetVariableFromState(m_vm->derivative_group_x, varList[i], varValueCount), varValueCount);
							pointerToVariable[1] = &m_vmImmediate->derivative_group_x->results[j];
						}
						if (m_vmImmediate->derivative_group_y) {
							spvm_member_memcpy(m_vmImmediate->derivative_group_y->results[j].members, GetVariableFromState(m_vm->derivative_group_y, varList[i], varValueCount), varValueCount);
							pointerToVariable[2] = &m_vmImmediate->derivative_group_y->results[j];
						}
						if (m_vmImmediate->derivative_group_d) {
							spvm_member_memcpy(m_vmImmediate->derivative_group_d->results[j].members, GetVariableFromState(m_vm->derivative_group_d, varList[i], varValueCount), varValueCount);
							pointerToVariable[3] = &m_vmImmediate->derivative_group_d->results[j];
						}
					}
				}
			}
			
			// function parameters (which are pointers)
			if (pointerToVariable[0] != nullptr) {
				for (int j = 0; j < m_shaderImmediate->bound; j++) {
					if (m_vmImmediate->results[j].name == nullptr)
						continue;

					spvm_result_t res = &m_vmImmediate->results[j];

					// TODO: also check for the type, or there might be some crashes caused by two vars with different type (?) (mat4 and vec4 for example)

					if (res->member_count == varValueCount && res->members == nullptr && strcmp(varList[i].c_str(), res->name) == 0) {
						res->members = pointerToVariable[0]->members;

						if (m_vmImmediate->derivative_used) {
							if (m_vmImmediate->derivative_group_x) m_vmImmediate->derivative_group_x->results[j].members = pointerToVariable[1]->members;
							if (m_vmImmediate->derivative_group_y) m_vmImmediate->derivative_group_y->results[j].members = pointerToVariable[2]->members;
							if (m_vmImmediate->derivative_group_d) m_vmImmediate->derivative_group_d->results[j].members = pointerToVariable[3]->members;
						}
					}
				}
			}
		}
		
		// copy HLSL no-named cbuffers
		spvm_result_t cbufferSource = spvm_state_get_result_with_value(m_vm, "");
		spvm_result_t cbufferTarget = spvm_state_get_result_with_value(m_vmImmediate, "");
		if (cbufferSource && cbufferTarget)
			spvm_member_memcpy(cbufferTarget->members, cbufferSource->members, cbufferTarget->member_count);

		// execute $$_shadered_immediate
		spvm_word fnImmediate = spvm_state_get_result_location(m_vmImmediate, "$$_shadered_immediate");
		spvm_state_prepare(m_vmImmediate, fnImmediate);
		spvm_state_call_function(m_vmImmediate);

		// get type and return value
		spvm_result_t val = &m_vmImmediate->results[resultID];
		outType = spvm_state_get_type_info(m_vmImmediate->results, &m_vmImmediate->results[val->pointer]);
		return val;
	}

	void DebugInformation::PrepareVertexShader(PipelineItem* owner, PipelineItem* item, PixelInformation* px)
	{
		m_stage = ShaderStage::Vertex;

		m_resetVM();
		if (owner->Type == PipelineItem::ItemType::ShaderPass)
			m_setupVM(((pipe::ShaderPass*)owner->Data)->VSSPV);
		else if (owner->Type == PipelineItem::ItemType::PluginItem) {
			pipe::PluginItemData* plData = (pipe::PluginItemData*)owner->Data;
			
			unsigned int spvSize = plData->Owner->PipelineItem_GetSPIRVSize(plData->Type, plData->PluginData, plugin::ShaderStage::Vertex);
			std::vector<unsigned int> spv;
			if (spvSize != 0) {
				unsigned int* spvPtr = plData->Owner->PipelineItem_GetSPIRV(plData->Type, plData->PluginData, plugin::ShaderStage::Vertex);
				spv = std::vector<unsigned int>(spvPtr, spvPtr + spvSize);
			}
			m_setupVM(spv);
 		}

		// uniforms
		m_copyUniforms(owner, item, px);
	}
	void DebugInformation::SetVertexShaderInput(PixelInformation& pixel, int vertexIndex)
	{
		m_pixel = &pixel;

		// input variables
		for (spvm_word i = 0; i < m_shader->bound; i++) {
			spvm_result_t slot = &m_vm->results[i];
			spvm_result_t pointer = nullptr;

			if (slot->pointer)
				pointer = &m_vm->results[m_vm->results[i].pointer];

			// input variable
			if (pointer && pointer->storage_class == SpvStorageClassInput) {
				spvm_word location = 0, builtinType = 0;
				bool hasLocation = false, isBuiltin = false;
				for (spvm_word j = 0; j < slot->decoration_count; j++) {
					if (slot->decorations[j].type == SpvDecorationLocation) {
						location = slot->decorations[j].literal1;
						hasLocation = true;
					}
					else if (slot->decorations[j].type == SpvDecorationBuiltIn) {
						builtinType = slot->decorations[j].literal1;
						isBuiltin = true;
					}
				}

				if (hasLocation) {
					glm::vec4 value(0.0f);
					size_t layOffset = 0;

					InputLayoutValue inputType = InputLayoutValue::MaxCount;

					if (pixel.Pass->Type == PipelineItem::ItemType::ShaderPass) {
						pipe::ShaderPass* passData = (pipe::ShaderPass*)pixel.Pass->Data;
						if (location < passData->InputLayout.size()) {
							inputType = passData->InputLayout[location].Value;
							for (spvm_word j = 0; j < location; j++)
								layOffset += InputLayoutItem::GetValueSize(passData->InputLayout[j].Value);
						} else if (pixel.InstanceBuffer != nullptr) {
							int bufferLocation = location - passData->InputLayout.size();

							std::vector<ShaderVariable::ValueType> tData = m_objs->ParseBufferFormat(((ed::BufferObject*)pixel.InstanceBuffer)->ViewFormat);

							int stride = 0;
							for (const auto& dataEl : tData)
								stride += ShaderVariable::GetSize(dataEl, true);

							GLfloat* bufPtr = (GLfloat*)malloc(stride);

							glBindBuffer(GL_ARRAY_BUFFER, ((ed::BufferObject*)pixel.InstanceBuffer)->ID);
							glGetBufferSubData(GL_ARRAY_BUFFER, pixel.InstanceID * stride, stride, &bufPtr[0]);
							glBindBuffer(GL_ARRAY_BUFFER, 0);

							int iOffset = 0;
							for (int j = 0; j < tData.size(); j++) {
								int elCount = ShaderVariable::GetSize(tData[j]) / 4;

								if (j == bufferLocation) {
									if (elCount == 4)
										value = glm::make_vec4(bufPtr + iOffset);
									else if (elCount == 3)
										value = glm::vec4(glm::make_vec3(bufPtr + iOffset), 0.0f);
									else if (elCount == 2)
										value = glm::vec4(glm::make_vec2(bufPtr + iOffset), 0.0f, 0.0f);
									else
										value = glm::vec4(*(bufPtr + iOffset), 0.0f, 0.0f, 0.0f);
									break;
								}

								iOffset += elCount;
							}

							free(bufPtr);
						}
					} else if (pixel.Pass->Type == PipelineItem::ItemType::PluginItem) {
						pipe::PluginItemData* plData = (pipe::PluginItemData*)pixel.Pass->Data;
						if (location < plData->Owner->PipelineItem_GetInputLayoutSize(plData->Type, plData->PluginData)) {
							plugin::InputLayoutItem inputItem;
							plData->Owner->PipelineItem_GetInputLayoutItem(plData->Type, plData->PluginData, location, inputItem);

							inputType = (InputLayoutValue)inputItem.Value;
						}
					}

					switch (inputType) {
					case InputLayoutValue::Position: value = glm::vec4(pixel.Vertex[vertexIndex].Position, 0.0f); break;
					case InputLayoutValue::Normal: value = glm::vec4(pixel.Vertex[vertexIndex].Normal, 0.0f); break;
					case InputLayoutValue::Texcoord: value = glm::vec4(pixel.Vertex[vertexIndex].TexCoords, 0.0f, 0.0f); break;
					case InputLayoutValue::Tangent: value = glm::vec4(pixel.Vertex[vertexIndex].Tangent, 0.0f); break;
					case InputLayoutValue::Binormal: value = glm::vec4(pixel.Vertex[vertexIndex].Binormal, 0.0f); break;
					case InputLayoutValue::Color: value = glm::vec4(pixel.Vertex[vertexIndex].Color); break;
					default: {
						ed::BufferObject* vbData = nullptr;

						if (pixel.Object != nullptr && pixel.Object->Type == PipelineItem::ItemType::VertexBuffer)
							vbData = (BufferObject*)((pipe::VertexBuffer*)pixel.Object->Data)->Buffer;

						if (vbData != nullptr) {
							std::vector<ShaderVariable::ValueType> tData = m_objs->ParseBufferFormat(vbData->ViewFormat);

							int stride = 0;
							for (const auto& dataEl : tData)
								stride += ShaderVariable::GetSize(dataEl, true);

							GLfloat* bufPtr = (GLfloat*)malloc(pixel.VertexCount * stride);

							glBindBuffer(GL_ARRAY_BUFFER, vbData->ID);
							glGetBufferSubData(GL_ARRAY_BUFFER, pixel.VertexID * stride, pixel.VertexCount * stride, &bufPtr[0]);
							glBindBuffer(GL_ARRAY_BUFFER, 0);

							int valCount = 0;
							if (inputType >= InputLayoutValue::BufferInt && inputType <= InputLayoutValue::BufferInt4)
								valCount = ((int)inputType - (int)InputLayoutValue::BufferInt) + 1;
							else if (inputType >= InputLayoutValue::BufferFloat && inputType <= InputLayoutValue::BufferFloat4)
								valCount = ((int)inputType - (int)InputLayoutValue::BufferFloat) + 1;

							for (int v = 0; v < valCount; v++)
								value[v] = *(bufPtr + vertexIndex * (stride/sizeof(float)) + layOffset + v);

							free(bufPtr);
						}
					} break;
					}

					for (spvm_word j = 0; j < slot->member_count; j++)
						slot->members[j].value.f = value[j];
				} else if (isBuiltin) {
					if (builtinType == SpvBuiltInVertexId)
						slot->members[0].value.s = pixel.VertexID + vertexIndex;
					else if (builtinType == SpvBuiltInInstanceId)
						slot->members[0].value.s = pixel.InstanceID;
				}
			}
		}
	}
	glm::vec4 DebugInformation::ExecuteVertexShader()
	{
		if (m_vm == nullptr)
			return glm::vec4(0.0f);

		spvm_word fnMain = GetEntryPoint(m_stage);
		if (fnMain == 0) {
			fnMain = spvm_state_get_result_location(m_vm, "main");
			if (fnMain == 0)
				return glm::vec4(0.0f);
		}

		spvm_state_prepare(m_vm, fnMain);
		spvm_state_call_function(m_vm);

		glm::vec4 ret(0.0f);

		spvm_word mCount = 0;
		spvm_member_t pointer = nullptr;

		spvm_word mem_count = 0;
		spvm_member_t glPosition = spvm_state_get_builtin(m_vm, SpvBuiltInPosition, &mem_count);

		if (glPosition != nullptr) {
			for (int j = 0; j < mem_count; j++)
				ret[j] = glPosition[j].value.f;
		}

		return ret;
	}
	void DebugInformation::CopyVertexShaderOutput(PixelInformation& px, int vertexIndex)
	{
		// free the memory
		for (auto& vsOut : px.VertexShaderOutput[vertexIndex]) {
			if (vsOut.name) {
				free(vsOut.name);
				vsOut.name = nullptr;
			}
			if (vsOut.members) {
				spvm_member_free(vsOut.members, vsOut.member_count);
				vsOut.member_count = 0;
				vsOut.members = nullptr;
			}
		}
		px.VertexShaderOutput[vertexIndex].clear();

		// copy the output registers
		for (int i = 0; i < m_shader->bound; i++) {
			spvm_result_t slot = &m_vm->results[i];
			spvm_result_t pointer = nullptr;

			if (slot->pointer)
				pointer = &m_vm->results[m_vm->results[i].pointer];

			// output variable
			if (pointer && pointer->storage_class == SpvStorageClassOutput) {
				int loc = -1;
				for (int j = 0; j < slot->decoration_count; j++)
					if (slot->decorations[j].type == SpvDecorationLocation) {
						loc = slot->decorations[j].literal1;
						break;
					}

				struct spvm_result copy;
				memcpy(&copy, slot, sizeof(struct spvm_result));
				copy.decorations = nullptr;
				copy.decoration_count = 0;
				copy.members = nullptr;

				const char* nameCpy = slot->name;
				if (px.GeometryShaderUsed) { // get interface block
					if (pointer->pointer && slot->name && strcmp(slot->name, "") != 0) { // there surely must be a better way to do this?
						spvm_result_t block = &m_vm->results[pointer->pointer];
						bool isBlock = false;

						for (spvm_word j = 0; j < block->decoration_count; j++)
							if (block->decorations[j].type == SpvDecorationBlock) {
								isBlock = true;
								break;
							}

						if (isBlock && block->name)
							nameCpy = block->name;
					}
				}

				if (nameCpy) {
					copy.name = (spvm_string)calloc(1, strlen(nameCpy) + 1);
					strcpy(copy.name, nameCpy);
				}

				// hax: store location in spvm_result::return_type -> this can be done in a better way, but im lazy right now [TODO]
				copy.return_type = loc;

				spvm_result_allocate_typed_value(&copy, m_vm->results, copy.pointer);
				spvm_member_memcpy(copy.members, slot->members, copy.member_count);

				px.VertexShaderOutput[vertexIndex].push_back(copy);
			}
		}
	}

	glm::vec3 DebugInformation::m_getWeights(glm::vec2 a, glm::vec2 b, glm::vec2 c, glm::vec2 p)
	{
		glm::vec2 ab = b - a;
		glm::vec2 ac = c - a;
		glm::vec2 ap = p - a;
		float factor = 1 / (ab.x * ac.y - ab.y * ac.x);
		float s = (ac.y * ap.x - ac.x * ap.y) * factor;
		float t = (ab.x * ap.y - ab.y * ap.x) * factor;
		return glm::vec3(1 - s - t, s, t);
	}
	void DebugInformation::PreparePixelShader(PipelineItem* owner, PipelineItem* item, PixelInformation* px)
	{
		m_stage = ShaderStage::Pixel;

		m_resetVM();
		if (owner->Type == PipelineItem::ItemType::ShaderPass)
			m_setupVM(((pipe::ShaderPass*)owner->Data)->PSSPV);
		else if (owner->Type == PipelineItem::ItemType::PluginItem) {
			pipe::PluginItemData* plData = (pipe::PluginItemData*)owner->Data;

			unsigned int spvSize = plData->Owner->PipelineItem_GetSPIRVSize(plData->Type, plData->PluginData, plugin::ShaderStage::Pixel);
			std::vector<unsigned int> spv;
			if (spvSize != 0) {
				unsigned int* spvPtr = plData->Owner->PipelineItem_GetSPIRV(plData->Type, plData->PluginData, plugin::ShaderStage::Pixel);
				spv = std::vector<unsigned int>(spvPtr, spvPtr + spvSize);
			}
			m_setupVM(spv);
		}

		// uniforms
		m_copyUniforms(owner, item, px);

		// dfdx/y
		if (m_vm->derivative_used) {
			for (spvm_word i = 0; i < m_vm->owner->bound; i++) {
				spvm_result_t slot = &m_vm->results[i];

				spvm_result_t ptrInfo = nullptr;
				if (slot->pointer)
					ptrInfo = &m_vm->results[slot->pointer];

				bool needsCopy = false;

				if (ptrInfo)
					needsCopy = (ptrInfo->storage_class == SpvStorageClassUniform || ptrInfo->storage_class == SpvStorageClassUniformConstant) && ptrInfo->value_type == spvm_value_type_pointer;

				if (needsCopy && slot->members != nullptr) {
					if (m_vm->derivative_group_x) spvm_member_memcpy(m_vm->derivative_group_x->results[i].members, slot->members, slot->member_count);
					if (m_vm->derivative_group_y) spvm_member_memcpy(m_vm->derivative_group_y->results[i].members, slot->members, slot->member_count);
					if (m_vm->derivative_group_d) spvm_member_memcpy(m_vm->derivative_group_d->results[i].members, slot->members, slot->member_count);
				}
			}
		}
	}
	float DebugInformation::SetPixelShaderInput(PixelInformation& pixel)
	{
		m_pixel = &pixel;
		m_ubLastType = m_ubLastLine = m_ubCount = 0;

		glm::vec3 weights = m_processWeight(glm::ivec2(0, 0));
		m_interpolateValues(m_vm, weights);

		float depth = weights.x * pixel.FinalPosition[0].z + weights.y * pixel.FinalPosition[1].z + weights.z * pixel.FinalPosition[2].z;
		
		if (m_vm->derivative_used && !m_vm->_derivative_is_group_member) {
			spvm_byte isOddX = ((int)pixel.Coordinate.x) % 2 != 0;
			spvm_byte isOddY = ((int)pixel.Coordinate.y) % 2 != 0;
			float modX = 1, modY = 1;

			// setup frag_coord
			if (isOddX) modX = -1;
			if (isOddY) modY = -1;
			
			if (m_vm->derivative_group_x) {
				weights = m_processWeight(glm::ivec2(modX, 0));
				m_interpolateValues(m_vm->derivative_group_x, weights);
			}
			if (m_vm->derivative_group_y) {
				weights = m_processWeight(glm::ivec2(0, modY));
				m_interpolateValues(m_vm->derivative_group_y, weights);
			}
			if (m_vm->derivative_group_d) {
				weights = m_processWeight(glm::ivec2(modX, modY));
				m_interpolateValues(m_vm->derivative_group_d, weights);
			}
		}

		return depth / (weights.x + weights.y + weights.z);
	}
	glm::vec3 DebugInformation::m_processWeight(glm::ivec2 offset)
	{
		// !!! m_pixel must be set !!!

		glm::vec2 pxPosition = glm::vec2(m_pixel->Coordinate + offset) / glm::vec2(m_pixel->RenderTextureSize - 1);

		// weigths
		glm::vec2 scrnPos1 = m_getScreenCoord(m_pixel->FinalPosition[0]);
		glm::vec2 scrnPos2 = m_getScreenCoord(m_pixel->FinalPosition[1]);
		glm::vec2 scrnPos3 = m_getScreenCoord(m_pixel->FinalPosition[2]);
		glm::vec3 weights = m_getWeights(scrnPos1, scrnPos2, scrnPos3, pxPosition);
		weights *= glm::vec3(m_pixel->FinalPosition[0].w == 0.0f ? 0.0f : (1.0f / m_pixel->FinalPosition[0].w), m_pixel->FinalPosition[1].w == 0.0f ? 0.0f : (1.0f / m_pixel->FinalPosition[1].w), m_pixel->FinalPosition[2].w == 0.0f ? 0.0f : (1.0f / m_pixel->FinalPosition[2].w));
	
		return weights;
	}
	void DebugInformation::m_interpolateValues(spvm_state_t state, glm::vec3 weights)
	{
		// !!! m_pixel must be set !!!

		float weightSum = weights.x + weights.y + weights.z;

		auto* mainStageOutput = &m_pixel->VertexShaderOutput[0];
		if (m_pixel->GeometryShaderUsed && m_pixel->GeometrySelectedPrimitive != -1 && m_pixel->GeometrySelectedVertex != -1)
			mainStageOutput = &m_pixel->GeometryOutput[m_pixel->GeometrySelectedPrimitive].Output[m_pixel->GeometrySelectedVertex];

		// match the ps input with vs output
		for (int i = 0; i < state->owner->bound; i++) {
			spvm_result_t slot = &state->results[i];
			spvm_result_t pointer = nullptr;

			if (slot->pointer)
				pointer = &state->results[state->results[i].pointer];

			// input variable
			if (pointer && pointer->storage_class == SpvStorageClassInput) {
				int loc = -1, outputIndex = -1;

				// get location
				for (int j = 0; j < slot->decoration_count; j++)
					if (slot->decorations[j].type == SpvDecorationLocation) {
						loc = slot->decorations[j].literal1;
						break;
					}

				// get vs output index
				for (int j = 0; j < mainStageOutput->size(); j++) {
					const struct spvm_result* vsOutput = &(*mainStageOutput)[j];
					if (vsOutput->return_type == loc) {
						if (loc == -1) {
							if (vsOutput->name && slot->name)
								if (strcmp(vsOutput->name, slot->name) == 0) {
									outputIndex = j;
									break;
								}
						} else {
							outputIndex = j;
							break;
						}
					}
				}

				// copy and interpolate values
				if (outputIndex >= 0) {
					auto* outputPtr0 = &m_pixel->VertexShaderOutput[0];
					auto* outputPtr1 = &m_pixel->VertexShaderOutput[1];
					auto* outputPtr2 = &m_pixel->VertexShaderOutput[2];

					if (m_pixel->GeometryShaderUsed && m_pixel->GeometrySelectedPrimitive != -1 && m_pixel->GeometrySelectedVertex != -1) {
						if (m_pixel->GeometryOutputType == GeometryShaderOutput::Points) {
							outputPtr0 = &m_pixel->GeometryOutput[m_pixel->GeometrySelectedPrimitive].Output[m_pixel->GeometrySelectedVertex];
							outputPtr1 = nullptr;
							outputPtr2 = nullptr;
						} else if (m_pixel->GeometryOutputType == GeometryShaderOutput::LineStrip) {
							outputPtr0 = &m_pixel->GeometryOutput[m_pixel->GeometrySelectedPrimitive].Output[m_pixel->GeometrySelectedVertex - 1];
							outputPtr1 = &m_pixel->GeometryOutput[m_pixel->GeometrySelectedPrimitive].Output[m_pixel->GeometrySelectedVertex];
							outputPtr2 = nullptr;
						} else if (m_pixel->GeometryOutputType == GeometryShaderOutput::TriangleStrip) {
							outputPtr0 = &m_pixel->GeometryOutput[m_pixel->GeometrySelectedPrimitive].Output[m_pixel->GeometrySelectedVertex - 2];
							outputPtr1 = &m_pixel->GeometryOutput[m_pixel->GeometrySelectedPrimitive].Output[m_pixel->GeometrySelectedVertex - 1];
							outputPtr2 = &m_pixel->GeometryOutput[m_pixel->GeometrySelectedPrimitive].Output[m_pixel->GeometrySelectedVertex];
						}
					}

					const struct spvm_result* value0 = (outputPtr0 == nullptr || outputPtr0->empty()) ? nullptr : &(*outputPtr0)[outputIndex];
					const struct spvm_result* value1 = (outputPtr1 == nullptr || outputPtr1->empty()) ? nullptr : &(*outputPtr1)[outputIndex];
					const struct spvm_result* value2 = (outputPtr2 == nullptr || outputPtr2->empty()) ? nullptr : &(*outputPtr2)[outputIndex];
					
					// get type
					spvm_result_t memType = spvm_state_get_type_info(state->results, pointer);
					spvm_value_type elType = (spvm_value_type)memType->value_type;
					spvm_word vbcount = memType->value_bitcount;
					if (elType == spvm_value_type_vector || elType == spvm_value_type_matrix) {
						memType = spvm_state_get_type_info(state->results, &state->results[memType->pointer]);
						elType = (spvm_value_type)memType->value_type;
						vbcount = memType->value_bitcount;
					}

					// apply values
					for (int c = 0; c < slot->member_count; c++) {
						if (slot->members[c].member_count == 0) {
							if (elType == spvm_value_type_float && vbcount > 32)
								slot->members[c].value.d = (GET_VALUE_WITH_CHECK_DOUBLE(value0, c) * weights.x + GET_VALUE_WITH_CHECK_DOUBLE(value1, c) * weights.y + GET_VALUE_WITH_CHECK_DOUBLE(value2, c) * weights.z) / weightSum;
							else if (elType == spvm_value_type_float)
								slot->members[c].value.f = (GET_VALUE_WITH_CHECK_FLOAT(value0, c) * weights.x + GET_VALUE_WITH_CHECK_FLOAT(value1, c) * weights.y + GET_VALUE_WITH_CHECK_FLOAT(value2, c) * weights.z) / weightSum;
							else
								slot->members[c].value.s = (GET_VALUE_WITH_CHECK_INT(value0, c) * weights.x + GET_VALUE_WITH_CHECK_INT(value1, c) * weights.y + GET_VALUE_WITH_CHECK_INT(value2, c) * weights.z) / weightSum;
						} else {
							for (int r = 0; r < slot->members[c].member_count; c++) {
								if (elType == spvm_value_type_float && vbcount > 32)
									slot->members[c].members[r].value.d = (GET_VALUE2_WITH_CHECK_DOUBLE(value0, c, r) * weights.x + GET_VALUE2_WITH_CHECK_DOUBLE(value1, c, r) * weights.y + GET_VALUE2_WITH_CHECK_DOUBLE(value2, c, r) * weights.z) / weightSum;
								else if (elType == spvm_value_type_float)
									slot->members[c].members[r].value.f = (GET_VALUE2_WITH_CHECK_FLOAT(value0, c, r) * weights.x + GET_VALUE2_WITH_CHECK_FLOAT(value1, c, r) * weights.y + GET_VALUE2_WITH_CHECK_FLOAT(value2, c, r) * weights.z) / weightSum;
								else
									slot->members[c].members[r].value.s = (GET_VALUE2_WITH_CHECK_INT(value0, c, r) * weights.x + GET_VALUE2_WITH_CHECK_INT(value1, c, r) * weights.y + GET_VALUE2_WITH_CHECK_INT(value2, c, r) * weights.z) / weightSum;
							}
						}
					}
				}
			}
		}

	}
	glm::vec4 DebugInformation::ExecutePixelShader(int x, int y, int loc)
	{
		if (m_vm == nullptr)
			return glm::vec4(0.0f);

		spvm_word fnMain = GetEntryPoint(m_stage);
		if (fnMain == 0) {
			fnMain = spvm_state_get_result_location(m_vm, "main");
			if (fnMain == 0)
				return glm::vec4(0.0f);
		}

		spvm_state_prepare(m_vm, fnMain);
		spvm_state_set_frag_coord(m_vm, x + 0.5f, y + 0.5f, 1.0f, 1.0f); // TODO: z and w components
		spvm_state_call_function(m_vm);

		return GetPixelShaderOutput(loc);
	}

	glm::vec4 DebugInformation::GetPixelShaderOutput(int loc)
	{
		glm::vec4 ret(0.0f);

		spvm_word mCount = 0;
		spvm_member_t pointer = nullptr;

		for (spvm_word i = 0; i < m_shader->bound; i++) {
			spvm_result_t slot = &m_vm->results[i];
			spvm_result_t type = nullptr, pointerType = nullptr;
			if (slot->pointer) {
				type = spvm_state_get_type_info(m_vm->results, &m_vm->results[slot->pointer]);
				pointerType = &m_vm->results[slot->pointer];
			}

			if (slot->member_count == 0 || pointerType == nullptr || pointerType->storage_class != SpvStorageClassOutput)
				continue;

			int decLoc = -1;
			for (spvm_word j = 0; j < slot->decoration_count; j++) {
				if (slot->decorations[j].type == SpvDecorationLocation) {
					decLoc = slot->decorations[j].literal1;
					break;
				}
			}

			if (decLoc != loc && decLoc != -1)
				continue;

			for (int j = 0; j < slot->member_count; j++)
				ret[j] = slot->members[j].value.f;

			break;
		}

		return glm::clamp(ret, 0.0f, 1.0f);
	}

	void DebugInformation::PrepareGeometryShader(PipelineItem* owner, PipelineItem* item, PixelInformation* px)
	{
		m_stage = ShaderStage::Geometry;

		m_resetVM();
		if (owner->Type == PipelineItem::ItemType::ShaderPass)
			m_setupVM(((pipe::ShaderPass*)owner->Data)->GSSPV);
		else if (owner->Type == PipelineItem::ItemType::PluginItem) {
			pipe::PluginItemData* plData = (pipe::PluginItemData*)owner->Data;

			unsigned int spvSize = plData->Owner->PipelineItem_GetSPIRVSize(plData->Type, plData->PluginData, plugin::ShaderStage::Geometry);
			std::vector<unsigned int> spv;
			if (spvSize != 0) {
				unsigned int* spvPtr = plData->Owner->PipelineItem_GetSPIRV(plData->Type, plData->PluginData, plugin::ShaderStage::Geometry);
				spv = std::vector<unsigned int>(spvPtr, spvPtr + spvSize);
			}
			m_setupVM(spv);
		}

		// uniforms
		m_copyUniforms(owner, item, px);
	}
	void DebugInformation::SetGeometryShaderInput(PixelInformation& pixel)
	{
		m_pixel = &pixel;

		// clear some old info
		for (auto& prim : pixel.GeometryOutput)
			for (int i = 0; i < prim.Output.size(); i++)
				for (auto& out : prim.Output[i]) {
					if (out.name) {
						free(out.name);
						out.name = nullptr;
					}
					if (out.members) {
						spvm_member_free(out.members, out.member_count);
						out.member_count = 0;
						out.members = nullptr;
					}
				}
		pixel.GeometryOutput.clear();

		// fill some info into PixelInformation
		if (m_vm) {
			if (m_vm->owner->geometry_output == SpvExecutionModeOutputPoints)
				pixel.GeometryOutputType = GeometryShaderOutput::Points;
			else if (m_vm->owner->geometry_output == SpvExecutionModeOutputTriangleStrip)
				pixel.GeometryOutputType = GeometryShaderOutput::TriangleStrip;
			else if (m_vm->owner->geometry_output == SpvExecutionModeOutputLineStrip)
				pixel.GeometryOutputType = GeometryShaderOutput::LineStrip;
			pixel.GeometryOutput.push_back(GeometryShaderPrimitive());
		}
		m_updatedGeometryOutput = true;

		// input variables
		for (spvm_word i = 0; i < m_shader->bound; i++) {
			spvm_result_t slot = &m_vm->results[i];
			spvm_result_t pointer = nullptr;

			if (slot->pointer)
				pointer = &m_vm->results[m_vm->results[i].pointer];

			// input variable
			if (pointer && pointer->storage_class == SpvStorageClassInput) {
				spvm_word location = 0, builtinType = 0;
				bool hasLocation = false, isBuiltin = false;
				for (spvm_word j = 0; j < slot->decoration_count; j++) {
					if (slot->decorations[j].type == SpvDecorationLocation) {
						location = slot->decorations[j].literal1;
						hasLocation = true;
					} else if (slot->decorations[j].type == SpvDecorationBuiltIn) {
						builtinType = slot->decorations[j].literal1;
						isBuiltin = true;
					}
				}

				if (slot->name && strcmp(slot->name, "gl_in") == 0) {
					for (spvm_word vert = 0; vert < slot->member_count; vert++) {
						spvm_member_t gl_PerVertex = &slot->members[vert];
						spvm_result_t gl_PerVertex_Type = &m_vm->results[gl_PerVertex->type];

						for (spvm_word j = 0; j < gl_PerVertex->member_count; j++) {
							bool is_glPosition = false;

							for (spvm_word k = 0; k < gl_PerVertex_Type->decoration_count; k++) {
								if (gl_PerVertex_Type->decorations[k].index == j) {
									if (gl_PerVertex_Type->decorations[k].type == SpvDecorationBuiltIn && gl_PerVertex_Type->decorations[k].literal1 == SpvBuiltInPosition)
										is_glPosition = true;
								}
							}

							// TODO: gl_PointSize, gl_ClipDistance

							if (is_glPosition) {
								spvm_member_t gl_Position = gl_PerVertex->members[j].members;
								spvm_word gl_Position_comps = gl_PerVertex->members[j].member_count;

								for (spvm_word k = 0; k < gl_Position_comps; k++)
									gl_Position[k].value.f = pixel.VertexShaderPosition[vert][k];
							}
						}
					}
				} 
				else {
					const char* blockName = nullptr;
					spvm_result_t currentBlock = slot;
					
					// find the interface block name
					do {
						currentBlock = &m_vm->results[currentBlock->pointer];

						if (currentBlock->name) {
							blockName = currentBlock->name;
							break;
						}
					} while (currentBlock->pointer);

					if (blockName == nullptr)
						blockName = slot->name;

					// copy the VS shader output
					for (spvm_word vert = 0; vert < slot->member_count; vert++) {
						// find the interface block from the VertexShaderOutput
						spvm_result_t blockData = nullptr;
						for (spvm_word j = 0; j < pixel.VertexShaderOutput[vert].size(); j++) {
							const char* vsOutName = pixel.VertexShaderOutput[vert][j].name;

							// check by name or by location
							if ((vsOutName && blockName && strcmp(vsOutName, blockName) == 0) ||
								(pixel.VertexShaderOutput[vert][j].return_type != -1 && pixel.VertexShaderOutput[vert][j].return_type == location)) {
								blockData = &pixel.VertexShaderOutput[vert][j];
								break;
							}
						}

						if (blockData && slot->members[vert].member_count == blockData->member_count)
							spvm_member_memcpy(slot->members[vert].members, blockData->members, blockData->member_count);
					}
				}
			}
		}
	}
	void DebugInformation::ExecuteGeometryShader()
	{
		if (m_vm == nullptr)
			return;

		spvm_word fnMain = GetEntryPoint(m_stage);
		if (fnMain == 0) {
			fnMain = spvm_state_get_result_location(m_vm, "main");
			if (fnMain == 0)
				return;
		}

		spvm_state_prepare(m_vm, fnMain);
		spvm_state_call_function(m_vm);

		// check where the new primitive is located
		float depth = INFINITY;
		for (int p = 0; p < m_pixel->GeometryOutput.size(); p++) {
			auto* prim = &m_pixel->GeometryOutput[p];

			// triangles
			if (m_pixel->GeometryOutputType == GeometryShaderOutput::TriangleStrip) {
				for (int v = 2; v < prim->Position.size(); v++) {
					glm::vec4 p0 = (prim->Position[v - 2] / prim->Position[v - 2].w + 1.0f) * 0.5f;
					glm::vec4 p1 = (prim->Position[v - 1] / prim->Position[v - 1].w + 1.0f) * 0.5f;
					glm::vec4 p2 = (prim->Position[v] / prim->Position[v].w + 1.0f) * 0.5f;

					if (isPointInTriangle(m_pixel->RelativeCoordinate, p0, p1, p2)) {
						if ((p0.z + p1.z + p2.z) / 3.0f < depth) { // TODO: this is obviously inaccurate, we should calculate Z that is near the RelativeCoordinate
							depth = (p0.z + p1.z + p2.z) / 3.0f;
							m_pixel->GeometrySelectedPrimitive = p;
							m_pixel->GeometrySelectedVertex = v;

							m_pixel->FinalPosition[0] = prim->Position[v - 2];
							m_pixel->FinalPosition[1] = prim->Position[v - 1];
							m_pixel->FinalPosition[2] = prim->Position[v];
						}
					}
				}
			}

			// lines
			else if (m_pixel->GeometryOutputType == GeometryShaderOutput::LineStrip) {
				for (int v = 1; v < prim->Position.size(); v++) {
					glm::vec4 p0 = (prim->Position[v - 1] / prim->Position[v - 1].w + 1.0f) * 0.5f;
					glm::vec4 p1 = (prim->Position[v] / prim->Position[v].w + 1.0f) * 0.5f;

					if (isPointOnLine(m_pixel->RelativeCoordinate, p0, p1)) {
						if ((p0.z + p1.z) / 2.0f < depth) { // TODO: this is obviously inaccurate, we should calculate Z that is near the RelativeCoordinate
							depth = (p0.z + p1.z) / 2.0f;
							m_pixel->GeometrySelectedPrimitive = p;
							m_pixel->GeometrySelectedVertex = v;

							m_pixel->FinalPosition[0] = prim->Position[v - 1];
							m_pixel->FinalPosition[1] = prim->Position[v];
						}
					}
				}
			}

			// points
			else if (m_pixel->GeometryOutputType == GeometryShaderOutput::Points) {
				for (int v = 0; v < prim->Position.size(); v++) {
					glm::vec4 p0 = (prim->Position[v] / prim->Position[v].w + 1.0f) * 0.5f;

					if (abs(m_pixel->RelativeCoordinate.x - p0.x) < 0.01f && abs(m_pixel->RelativeCoordinate.y - p0.y) < 0.01f) {
						if (p0.z < depth) { // TODO: this is obviously inaccurate, we should calculate Z that is near the RelativeCoordinate
							depth = p0.z / 2.0f;
							m_pixel->GeometrySelectedPrimitive = p;
							m_pixel->GeometrySelectedVertex = v;

							m_pixel->FinalPosition[0] = prim->Position[v];
						}
					}
				}
			}
		}
	}

	void DebugInformation::PrepareTessellationControlShader(PipelineItem* owner, PipelineItem* item, PixelInformation* px)
	{
		m_stage = ShaderStage::TessellationControl;

		m_resetVM();
		if (owner->Type == PipelineItem::ItemType::ShaderPass)
			m_setupVM(((pipe::ShaderPass*)owner->Data)->TCSSPV);
		else if (owner->Type == PipelineItem::ItemType::PluginItem) {
			pipe::PluginItemData* plData = (pipe::PluginItemData*)owner->Data;

			unsigned int spvSize = plData->Owner->PipelineItem_GetSPIRVSize(plData->Type, plData->PluginData, plugin::ShaderStage::TessellationControl);
			std::vector<unsigned int> spv;
			if (spvSize != 0) {
				unsigned int* spvPtr = plData->Owner->PipelineItem_GetSPIRV(plData->Type, plData->PluginData, plugin::ShaderStage::TessellationControl);
				spv = std::vector<unsigned int>(spvPtr, spvPtr + spvSize);
			}
			m_setupVM(spv);
		}

		// uniforms
		m_copyUniforms(owner, item, px);
	}
	void DebugInformation::SetTessellationControlShaderInput(PixelInformation& pixel)
	{
		m_pixel = &pixel;

		auto* mainStageOutput = &m_pixel->VertexShaderOutput[0];
		if (m_pixel->GeometryShaderUsed && m_pixel->GeometrySelectedPrimitive != -1 && m_pixel->GeometrySelectedVertex != -1)
			mainStageOutput = &m_pixel->GeometryOutput[m_pixel->GeometrySelectedPrimitive].Output[m_pixel->GeometrySelectedVertex];

		// match the tcs input with vs/gs output
		for (int i = 0; i < m_vm->owner->bound; i++) {
			spvm_result_t slot = &m_vm->results[i];
			spvm_result_t pointer = nullptr;

			if (slot->pointer)
				pointer = &m_vm->results[m_vm->results[i].pointer];

			// input variable
			if (pointer && pointer->storage_class == SpvStorageClassInput) {
				int loc = -1, outputIndex = -1;

				// get location
				for (int j = 0; j < slot->decoration_count; j++)
					if (slot->decorations[j].type == SpvDecorationLocation) {
						loc = slot->decorations[j].literal1;
						break;
					}

				// get vs output index
				for (int j = 0; j < mainStageOutput->size(); j++) {
					const struct spvm_result* vsOutput = &(*mainStageOutput)[j];
					if (vsOutput->return_type == loc) {
						if (loc == -1) {
							if (vsOutput->name && slot->name)
								if (strcmp(vsOutput->name, slot->name) == 0) {
									outputIndex = j;
									break;
								}
						} else {
							outputIndex = j;
							break;
						}
					}
				}

				// copy and interpolate values
				if (outputIndex >= 0) {
					auto* outputPtr0 = &m_pixel->VertexShaderOutput[0];
					auto* outputPtr1 = &m_pixel->VertexShaderOutput[1];
					auto* outputPtr2 = &m_pixel->VertexShaderOutput[2];

					if (m_pixel->GeometryShaderUsed && m_pixel->GeometrySelectedPrimitive != -1 && m_pixel->GeometrySelectedVertex != -1) {
						if (m_pixel->GeometryOutputType == GeometryShaderOutput::Points) {
							outputPtr0 = &m_pixel->GeometryOutput[m_pixel->GeometrySelectedPrimitive].Output[m_pixel->GeometrySelectedVertex];
							outputPtr1 = nullptr;
							outputPtr2 = nullptr;
						} else if (m_pixel->GeometryOutputType == GeometryShaderOutput::LineStrip) {
							outputPtr0 = &m_pixel->GeometryOutput[m_pixel->GeometrySelectedPrimitive].Output[m_pixel->GeometrySelectedVertex - 1];
							outputPtr1 = &m_pixel->GeometryOutput[m_pixel->GeometrySelectedPrimitive].Output[m_pixel->GeometrySelectedVertex];
							outputPtr2 = nullptr;
						} else if (m_pixel->GeometryOutputType == GeometryShaderOutput::TriangleStrip) {
							outputPtr0 = &m_pixel->GeometryOutput[m_pixel->GeometrySelectedPrimitive].Output[m_pixel->GeometrySelectedVertex - 2];
							outputPtr1 = &m_pixel->GeometryOutput[m_pixel->GeometrySelectedPrimitive].Output[m_pixel->GeometrySelectedVertex - 1];
							outputPtr2 = &m_pixel->GeometryOutput[m_pixel->GeometrySelectedPrimitive].Output[m_pixel->GeometrySelectedVertex];
						}
					}
					
					const struct spvm_result* value0 = (outputPtr0 == nullptr || outputPtr0->empty()) ? nullptr : &(*outputPtr0)[outputIndex];
					const struct spvm_result* value1 = (outputPtr1 == nullptr || outputPtr1->empty()) ? nullptr : &(*outputPtr1)[outputIndex];
					const struct spvm_result* value2 = (outputPtr2 == nullptr || outputPtr2->empty()) ? nullptr : &(*outputPtr2)[outputIndex];

					if (value0)
						spvm_member_memcpy(slot->members[0].members, value0->members, value0->member_count);
					if (value1)
						spvm_member_memcpy(slot->members[1].members, value1->members, value1->member_count);
					if (value2)
						spvm_member_memcpy(slot->members[2].members, value2->members, value2->member_count);
				}
			}
		}
	}
	void DebugInformation::ExecuteTessellationControlShader(unsigned int invocationId)
	{
		if (m_vm == nullptr)
			return;

		spvm_word fnMain = GetEntryPoint(m_stage);
		if (fnMain == 0) {
			fnMain = spvm_state_get_result_location(m_vm, "main");
			if (fnMain == 0)
				return;
		}

		spvm_word invIdMemCount = 0;
		spvm_member_t mems = spvm_state_get_builtin(m_vm, SpvBuiltInInvocationId, &invIdMemCount);
		if (mems)
			mems->value.u = invocationId;

		spvm_state_prepare(m_vm, fnMain);
		spvm_state_call_function(m_vm);
	}
	void DebugInformation::CopyTessellationControlShaderOutput()
	{
		if (m_pixel == nullptr)
			return;

		// get inner/outer levels
		spvm_word innerMemCount = 0, outerMemCount = 0;
		spvm_member_t innerMem = spvm_state_get_builtin(m_vm, SpvBuiltInTessLevelInner, &innerMemCount);
		spvm_member_t outerMem = spvm_state_get_builtin(m_vm, SpvBuiltInTessLevelOuter, &outerMemCount);
		for (spvm_word i = 0; i < std::min<spvm_word>(2, innerMemCount); i++)
			m_pixel->TessLevelInner[i] = innerMem[i].members[0].value.f;
		for (spvm_word i = 0; i < std::min<spvm_word>(4, outerMemCount); i++)
			m_pixel->TessLevelOuter[i] = outerMem[i].members[0].value.f;

		// TODO: copy output arrays (variables)
	}

	void DebugInformation::PrepareComputeShader(PipelineItem* pass, int x, int y, int z)
	{
		m_stage = ShaderStage::Compute;
		pipe::ComputePass* data = (pipe::ComputePass*)pass->Data;

		// TODO: plugins

		m_resetVM();
		m_setupVM(data->SPV);
		m_copyUniforms(pass, nullptr);
		m_setThreadID(m_vm, x, y, z, data->WorkX, data->WorkY, data->WorkZ);

		m_numGroupsX = data->WorkX;
		m_numGroupsY = data->WorkY;
		m_numGroupsZ = data->WorkZ;

		if (data->SPV.size() > 0) {
			SPIRVParser parser;
			parser.Parse(data->SPV);
			if (parser.BarrierUsed)
				m_setupWorkgroup();
		}
	}

	spvm_word DebugInformation::GetEntryPoint(ShaderStage stage)
	{
		if (m_shader == nullptr)
			return 0;

		for (spvm_word i = 0; i < m_shader->entry_point_count; i++)
			if ((stage == ShaderStage::Pixel && m_shader->entry_points[i].exec_model == SpvExecutionModelFragment) || 
				(stage == ShaderStage::Vertex && m_shader->entry_points[i].exec_model == SpvExecutionModelVertex) || 
				(stage == ShaderStage::Compute && m_shader->entry_points[i].exec_model == SpvExecutionModelGLCompute) || 
				(stage == ShaderStage::Geometry && m_shader->entry_points[i].exec_model == SpvExecutionModelGeometry) || 
				(stage == ShaderStage::TessellationControl && m_shader->entry_points[i].exec_model == SpvExecutionModelTessellationControl) || 
				(stage == ShaderStage::TessellationEvaluation && m_shader->entry_points[i].exec_model == SpvExecutionModelTessellationEvaluation))
			{
				return m_shader->entry_points[i].id;
			}

		return 0;
	}

	void DebugInformation::PrepareDebugger()
	{
		spvm_word fnMain = GetEntryPoint(m_stage);
		if (fnMain == 0) {
			fnMain = spvm_state_get_result_location(m_vm, "main");
			if (fnMain == 0)
				return;
		}

		m_funcStackLines.clear();
		m_funcStackLines.push_back(-1);

		spvm_state_prepare(m_vm, fnMain);

		if (m_stage == ShaderStage::Pixel && m_pixel != nullptr)
			spvm_state_set_frag_coord(m_vm, m_pixel->Coordinate.x + 0.5f, m_pixel->Coordinate.y + 0.5f, 1.0f, 1.0f); // TODO: z and w components

		// move to cursor to first line in the function
		spvm_state_step_into(m_vm);
		if (m_vm->owner->language == SpvSourceLanguageHLSL) {
			// since actual main is encapsulated into another main function
			while (m_vm->code_current && m_vm->function_stack_current == 0)
				spvm_state_step_into(m_vm);
		}

		m_funcStackLines[0] = m_vm->current_line;

		// prepare immediate mode compiler
		m_compiler.SetSPIRV(m_spv);
	}

	void DebugInformation::ClearWatchList()
	{
		for (auto& expr : m_watchExprs)
			free(expr);

		m_watchExprs.clear();
		m_watchValues.clear();
	}
	void DebugInformation::RemoveWatch(size_t index)
	{
		free(m_watchExprs[index]);
		m_watchExprs.erase(m_watchExprs.begin() + index);
		m_watchValues.erase(m_watchValues.begin() + index);
	}
	void DebugInformation::AddWatch(const std::string& expr, bool execute)
	{
		char* data = (char*)calloc(512, sizeof(char));
		strcpy(data, expr.c_str());

		m_watchExprs.push_back(data);
		m_watchValues.push_back("");

		if (execute)
			UpdateWatchValue(m_watchExprs.size() - 1);
	}
	void DebugInformation::UpdateWatchValue(size_t index)
	{
		char* expr = m_watchExprs[index];

		spvm_result_t resType = nullptr;
		spvm_result_t exprVal = Immediate(std::string(expr), resType);
		
		if (exprVal != nullptr && resType != nullptr) {
			std::stringstream ss;
			GetVariableValueAsString(ss, m_vmImmediate, resType, exprVal->members, exprVal->member_count, "");
			m_watchValues[index] = ss.str();
		} else
			m_watchValues[index] = "ERROR";
	}

	void DebugInformation::ClearVectorWatchList()
	{
		for (auto& expr : m_vectorWatchExprs)
			free(expr);

		m_vectorWatchExprs.clear();
		m_vectorWatchValues.clear();
		m_vectorWatchPositions.clear();
		m_vectorWatchColor.clear();
	}
	void DebugInformation::RemoveVectorWatch(size_t index)
	{
		free(m_vectorWatchExprs[index]);
		m_vectorWatchExprs.erase(m_vectorWatchExprs.begin() + index);
		m_vectorWatchValues.erase(m_vectorWatchValues.begin() + index);
		m_vectorWatchPositions.erase(m_vectorWatchPositions.begin() + index);
		m_vectorWatchColor.erase(m_vectorWatchColor.begin() + index);
	}
	void DebugInformation::AddVectorWatch(const std::string& expr, glm::vec4 color, bool execute)
	{
		char* data = (char*)calloc(512, sizeof(char));
		strcpy(data, expr.c_str());

		m_vectorWatchExprs.push_back(data);
		m_vectorWatchColor.push_back(color);
		m_vectorWatchValues.push_back("");
		m_vectorWatchPositions.push_back(glm::vec4(0.0f));

		if (execute)
			UpdateVectorWatchValue(m_vectorWatchExprs.size() - 1);
	}
	void DebugInformation::UpdateVectorWatchValue(size_t index)
	{
		char* expr = m_vectorWatchExprs[index];

		spvm_result_t resType = nullptr;
		spvm_result_t exprVal = Immediate(std::string(expr), resType);

		if (exprVal != nullptr && resType != nullptr) {
			glm::vec4 actualValue = glm::vec4(0, 0, 0, 1);

			if (resType->value_type == spvm_value_type_int || resType->value_type == spvm_value_type_float ||
				resType->value_type == spvm_value_type_vector) {

				if (resType->value_type == spvm_value_type_vector) {
					if (m_vmImmediate->results[resType->pointer].value_type == spvm_value_type_float) {
						for (int i = 0; i < exprVal->member_count; i++)
							actualValue[i] = exprVal->members[i].value.f;
					} else {
						for (int i = 0; i < exprVal->member_count; i++)
							actualValue[i] = exprVal->members[i].value.s;
					}
					if (exprVal->member_count == 3)
						actualValue.w = 0.0f;
				} else if (resType->value_type == spvm_value_type_float) {
					for (int i = 0; i < exprVal->member_count; i++)
						actualValue[i] = exprVal->members[i].value.f;
				} else {
					for (int i = 0; i < exprVal->member_count; i++)
						actualValue[i] = exprVal->members[i].value.s;
				} 
			}
			std::stringstream ss;
			GetVariableValueAsString(ss, m_vmImmediate, resType, exprVal->members, exprVal->member_count, "");
			m_vectorWatchValues[index] = ss.str();
			m_vectorWatchPositions[index] = actualValue;
		} else
			m_vectorWatchValues[index] = "ERROR";
	}

	bool DebugInformation::Jump(int line)
	{
		while (m_vm->code_current != nullptr && m_vm->current_line != line) {
			StepInto();
			if (CheckBreakpoint(GetCurrentLine()))
				return true;
		}
		return false;
	}
	bool DebugInformation::Continue()
	{
		while (m_vm->code_current != nullptr) {
			StepInto();
			if (CheckBreakpoint(GetCurrentLine()))
				return true;
		}
		return false;
	}
	void DebugInformation::Step()
	{
		int old = m_vm->function_stack_current;
		char* old_file = m_vm->current_file;
		int old_line = m_vm->current_line;

		StepInto();
		while (m_vm->code_current != nullptr && (m_vm->function_stack_current > old || old_file != m_vm->current_file || old_line == m_vm->current_line)) {
			StepInto();
			if (CheckBreakpoint(GetCurrentLine()))
				break;
		}
	}
	void DebugInformation::StepInto()
	{
		spvm_state_step_into(m_vm);

		if (m_vm->function_stack_current >= m_funcStackLines.size())
			m_funcStackLines.resize(m_vm->function_stack_current + 1);
		
		if (m_vm->function_stack_current >= 0)
			m_funcStackLines[m_vm->function_stack_current] = m_vm->current_line;
	}
	void DebugInformation::StepOut()
	{
		int old = m_vm->function_stack_current;
		while (m_vm->code_current != nullptr && m_vm->function_stack_current >= old) {
			spvm_state_step_opcode(m_vm);
			if (CheckBreakpoint(GetCurrentLine()))
				break;
		}

		if (m_vm->function_stack_current != -1)
			m_vm->current_line = m_funcStackLines[m_vm->function_stack_current];
	}
	bool DebugInformation::CheckBreakpoint(int line)
	{
		bool enabled = false;
		dbg::Breakpoint* brk = GetBreakpoint(m_file, line, enabled);

		if (brk != nullptr && enabled) {
			if (brk->IsConditional && !brk->Condition.empty()) {
				spvm_result_t resType = nullptr;
				spvm_result_t brkResult = Immediate(brk->Condition, resType);

				if (brkResult != nullptr && resType != nullptr && (resType->value_type == spvm_value_type_bool || resType->value_type == spvm_value_type_int) && brkResult->member_count == 1)
					return brkResult->members[0].value.b;
				else {
					m_msgs->Add(MessageStack::Type::Warning, "", "Invalid breakpoint condition on line " + std::to_string(line));
					return false;
				}
			}
			
			return true;
		}

		return false;
	}

	void DebugInformation::ClearPixelData(PixelInformation& px)
	{
		// vertex shader output
		for (int i = 0; i < px.VertexCount; i++) {
			for (auto& out : px.VertexShaderOutput[i]) {
				if (out.name) {
					free(out.name);
					out.name = nullptr;
				}
				if (out.members) {
					spvm_member_free(out.members, out.member_count);
					out.member_count = 0;
					out.members = nullptr;
				}
			}
		}

		// geometry shader output
		for (auto& prim : px.GeometryOutput) {
			for (int i = 0; i < prim.Output.size(); i++) {
				for (auto& out : prim.Output[i]) {
					if (out.name) {
						free(out.name);
						out.name = nullptr;
					}
					if (out.members) {
						spvm_member_free(out.members, out.member_count);
						out.member_count = 0;
						out.members = nullptr;
					}
				}
			}
		}
	}
	void DebugInformation::ClearPixelList()
	{
		for (PixelInformation& px : m_pixels)
			this->ClearPixelData(px);
		m_suggestions.clear();
		m_pixels.clear();
	}
		
	void DebugInformation::AddBreakpoint(const std::string& file, int line, bool useCondition, const std::string& condition, bool enabled)
	{
		std::vector<dbg::Breakpoint>& bkpts = m_breakpoints[file];

		bool alreadyExists = false;
		for (size_t i = 0; i < bkpts.size(); i++) {
			if (bkpts[i].Line == line) {
				bkpts[i].Condition = condition;
				bkpts[i].IsConditional = useCondition;
				m_breakpointStates[file][i] = enabled;
				alreadyExists = true;
				break;
			}
		}

		if (!alreadyExists) {
			dbg::Breakpoint bkpt;
			bkpt.Line = line;
			bkpt.Condition = condition;
			bkpt.IsConditional = useCondition;
			bkpts.push_back(bkpt);
			m_breakpointStates[file].push_back(enabled);
		}
	}
	void DebugInformation::RemoveBreakpoint(const std::string& file, int line)
	{
		std::vector<dbg::Breakpoint>& bkpts = m_breakpoints[file];
		std::vector<bool>& states = m_breakpointStates[file];

		for (size_t i = 0; i < bkpts.size(); i++) {
			if (bkpts[i].Line == line) {
				bkpts.erase(bkpts.begin() + i);
				states.erase(states.begin() + i);
				break;
			}
		}
	}
	void DebugInformation::SetBreakpointEnabled(const std::string& file, int line, bool enable)
	{
		std::vector<dbg::Breakpoint>& bkpts = m_breakpoints[file];
		std::vector<bool>& states = m_breakpointStates[file];

		for (size_t i = 0; i < bkpts.size(); i++) {
			if (bkpts[i].Line == line) {
				states[i] = enable;
				break;
			}
		}
	}
	dbg::Breakpoint* DebugInformation::GetBreakpoint(const std::string& file, int line, bool& enabled)
	{
		std::vector<dbg::Breakpoint>& bkpts = m_breakpoints[file];
		std::vector<bool>& bkptStates = m_breakpointStates[file];
		for (size_t i = 0; i < bkpts.size(); i++)
			if (bkpts[i].Line == line) {
				enabled = bkptStates[i];
				return &bkpts[i];
			}
		enabled = false;
		return nullptr;
	}
	
	void DebugInformation::SyncWorkgroup()
	{
		if (m_workgroup)
			for (int i = 0; i < m_shader->local_size_x * m_shader->local_size_y * m_shader->local_size_z; i++)
				if (m_workgroup[i])
					spvm_state_jump_to_instruction(m_workgroup[i], m_vm->instruction_count);
	}
	void DebugInformation::EmitVertex(const glm::vec4& position)
	{
		if (m_pixel == nullptr)
			return;

		GeometryShaderPrimitive* primitive = &m_pixel->GeometryOutput.back();

		primitive->Position.push_back(position);
		primitive->Output.push_back(std::vector<struct spvm_result>());

		std::vector<struct spvm_result>* out = &primitive->Output.back();
		
		// copy the spir-v registers
		for (int i = 0; i < m_shader->bound; i++) {
			spvm_result_t slot = &m_vm->results[i];
			spvm_result_t pointer = nullptr;

			if (slot->pointer)
				pointer = &m_vm->results[m_vm->results[i].pointer];

			// output variable
			if (pointer && pointer->storage_class == SpvStorageClassOutput) {
				int loc = -1;
				for (int j = 0; j < slot->decoration_count; j++)
					if (slot->decorations[j].type == SpvDecorationLocation) {
						loc = slot->decorations[j].literal1;
						break;
					}

				struct spvm_result copy;
				memcpy(&copy, slot, sizeof(struct spvm_result));
				copy.decorations = nullptr;
				copy.decoration_count = 0;
				copy.members = nullptr;

				if (slot->name) {
					copy.name = (spvm_string)calloc(1, strlen(slot->name) + 1);
					strcpy(copy.name, slot->name);
				}

				// hax: store location in spvm_result::return_type -> this can be done in a better way, but im lazy right now [TODO]
				copy.return_type = loc;

				spvm_result_allocate_typed_value(&copy, m_vm->results, copy.pointer);
				spvm_member_memcpy(copy.members, slot->members, copy.member_count);

				out->push_back(copy);
			}
		}

		m_updatedGeometryOutput = true;
	}
	void DebugInformation::EndPrimitive() {
		m_pixel->GeometryOutput.push_back(GeometryShaderPrimitive());
	}

	glm::vec4 DebugInformation::GetPositionThroughVertexShader(PipelineItem* pass, PipelineItem* item, const glm::vec3& pos)
	{
		glm::vec4 ret(0.0f);

		if (pass != nullptr && item != nullptr && pass->Type == PipelineItem::ItemType::ShaderPass) {
			if (((pipe::ShaderPass*)pass->Data)->VSSPV.size() > 0) {
				PixelInformation px;
				px.Pass = pass;
				px.Object = item;
				px.InTopology = GL_POINTS;
				px.VertexID = 0;
				px.InstanceID = 0;
				px.VertexCount = 1;
				px.RenderTextureSize = m_renderer->GetLastRenderSize();
				px.Vertex[0].Position = pos;
				px.Vertex[1].Position = pos;
				px.Vertex[2].Position = pos;

				this->PrepareVertexShader(pass, item, &px);
				this->SetVertexShaderInput(px, 0);
				ret = glm::inverse(SystemVariableManager::Instance().GetViewProjectionMatrix()) * this->ExecuteVertexShader();
			}
		}

		return ret;
	}
	void DebugInformation::OnUndefinedBehavior(spvm_state_t state, spvm_word ubID)
	{
		m_ubLastType = ubID;
		m_ubLastLine = state->current_line;
		m_ubCount = std::min<spvm_word>(m_ubCount + 1, 11);
	}
}
