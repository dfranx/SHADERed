#include <SHADERed/Objects/FrameAnalysis.h>
#include <SHADERed/Objects/Debug/PixelInformation.h>
#include <SHADERed/Objects/ShaderCompiler.h>
#include <SHADERed/Objects/Names.h>
#include <SHADERed/Objects/BinaryVectorReader.h>
#include <SHADERed/Engine/GeometryFactory.h>

#include <thread>
#include <algorithm>
#include <glm/gtc/type_ptr.hpp>

#include <common/BinaryVectorWriter.h>
#include <spvgentwo/Templates.h>

namespace ed {
	FrameAnalysis::EdgeEquation::EdgeEquation(const glm::ivec2& v0, const glm::ivec2& v1)
	{
		a = v0.y - v1.y;
		b = v1.x - v0.x;
		c = -(a * (v0.x + v1.x) + b * (v0.y + v1.y)) / 2.0f;
		tie = a != 0 ? a > 0 : b > 0;
	}
	glm::vec3 getHeatmapColor(float value)
	{
		const glm::vec3 color[5] = { glm::vec3(0, 0, 1), glm::vec3(0, 1, 1), glm::vec3(0, 1, 0), glm::vec3(1, 1, 0), glm::vec3(1, 0, 0) };
		int id1 = 0;
		int id2 = 0;

		if (value <= 0) 
			id1 = id2 = 0;
		else if (value >= 1)
			id1 = id2 = 4;
		else {
			value *= 4;
			id1 = std::floor(value);
			id2 = id1 + 1;
			value -= id1;
		}

		if (id1 < 0 || id1 > 4 || id2 < 0 || id2 > 4)
			return color[0];

		return (color[id2] - color[id1]) * value + color[id1];
	}

	FrameAnalysis::FrameAnalysis(DebugInformation* dbgr, RenderEngine* renderer, PipelineManager* pipeline, ObjectManager* objects, MessageStack* msgs)
	{
		m_depth = nullptr;
		m_color = nullptr;
		m_instCount = nullptr;
		m_ub = nullptr;
		m_pass = nullptr;
		m_bkpt = nullptr;

		m_width = 0;
		m_height = 0;
		m_hasBreakpoints = false;
		m_isRegion = false;
		m_instCountAvg = m_instCountAvgN = m_instCountMax = 0;
		m_pixelCount = m_pixelsDiscarded = m_pixelsUB = m_pixelsFailedDepthTest = 0;
		m_triangleCount = m_trianglesDiscarded = 0;

		m_debugger = dbgr;
		m_renderer = renderer;
		m_pipeline = pipeline;
		m_objects = objects;
		m_msgs = msgs;

		m_pixelHistoryLocation = glm::ivec2(-1, -1);
	}
	FrameAnalysis::~FrameAnalysis()
	{
		m_cleanBreakpoints();
		m_clean();
	}

	std::vector<unsigned int>* FrameAnalysis::m_getPixelShaderSPV(const char* path)
	{
		for (auto& pass : m_pipeline->GetList()) {
			if (pass->Type == PipelineItem::ItemType::ShaderPass) {
				pipe::ShaderPass* data = (pipe::ShaderPass*)pass->Data;
				if (strcmp(path, data->PSPath) == 0) {
					return &data->PSSPV;
				}
			}
		}

		return nullptr;
	}

	void FrameAnalysis::m_cacheBreakpoint(int i)
	{
		ExpressionCompiler compiler;
		compiler.SetSPIRV(*m_getPixelShaderSPV(m_breakpoint[i].PSPath));

		std::string curFunction = "";
		if (m_debugger->GetVM()->current_function != nullptr)
			curFunction = m_debugger->GetVM()->current_function->name;
		m_breakpoint[i].ResultID = compiler.Compile(m_breakpoint[i].Breakpoint->Condition, curFunction);

		compiler.GetSPIRV(m_breakpoint[i].SPIRV);

		if (m_breakpoint[i].SPIRV.size() <= 1) {
			m_breakpoint[i].SPIRV.clear();
			return;
		}

		m_breakpoint[i].VariableList = compiler.GetVariableList();
		m_breakpoint[i].Shader = spvm_program_create(m_debugger->GetVMContext(), (spvm_source)m_breakpoint[i].SPIRV.data(), m_breakpoint[i].SPIRV.size());
		m_breakpoint[i].VM = _spvm_state_create_base(m_breakpoint[i].Shader, true, 0);

		// can't use set_extenstion() function because for some reason two GLSL.std.450 instructions are generated with spvgentwo
		for (int j = 0; j < m_breakpoint[i].Shader->bound; j++)
			if (m_breakpoint[i].VM->results[j].name)
				if (strcmp(m_breakpoint[i].VM->results[j].name, "GLSL.std.450") == 0)
					m_breakpoint[i].VM->results[j].extension = m_debugger->GetGLSLExtension();

		m_breakpoint[i].Cached = true;
	}
	spvm_result_t FrameAnalysis::m_executeBreakpoint(int index, spvm_result_t& returnType)
	{
		spvm_state_t vm = m_breakpoint[index].VM;
		spvm_program_t program = m_breakpoint[index].Shader;
		const std::vector<std::string>& varList = m_breakpoint[index].VariableList;

		// copy variable values
		spvm_state_group_sync(vm);
		for (int i = 0; i < varList.size(); i++) {
			size_t varValueCount = 0;
			spvm_result_t varType = nullptr;
			spvm_member_t varValue = m_debugger->GetVariable(varList[i], varValueCount, varType);

			if (varValue == nullptr)
				continue;

			spvm_result_t pointerToVariable[4] = { nullptr };

			for (int j = 0; j < program->bound; j++) {
				if (vm->results[j].name == nullptr)
					continue;

				spvm_result_t res = &vm->results[j];
				spvm_result_t resType = spvm_state_get_type_info(vm->results, &vm->results[res->pointer]);

				// TODO: also check for the type, or there might be some crashes caused by two vars with different type (?) (mat4 and vec4 for example)

				if (res->member_count == varValueCount && resType->value_type == varType->value_type && res->members != nullptr && strcmp(varList[i].c_str(), res->name) == 0) {
					spvm_member_memcpy(res->members, varValue, varValueCount);

					pointerToVariable[0] = res;

					if (vm->derivative_used) {
						if (vm->derivative_group_x) {
							spvm_member_memcpy(vm->derivative_group_x->results[j].members, m_debugger->GetVariableFromState(m_debugger->GetVM()->derivative_group_x, varList[i], varValueCount), varValueCount);
							pointerToVariable[1] = &vm->derivative_group_x->results[j];
						}
						if (vm->derivative_group_y) {
							spvm_member_memcpy(vm->derivative_group_y->results[j].members, m_debugger->GetVariableFromState(m_debugger->GetVM()->derivative_group_y, varList[i], varValueCount), varValueCount);
							pointerToVariable[2] = &vm->derivative_group_y->results[j];
						}
						if (vm->derivative_group_d) {
							spvm_member_memcpy(vm->derivative_group_d->results[j].members, m_debugger->GetVariableFromState(m_debugger->GetVM()->derivative_group_d, varList[i], varValueCount), varValueCount);
							pointerToVariable[3] = &vm->derivative_group_d->results[j];
						}
					}
				}
			}

			// function parameters (which are pointers)
			if (pointerToVariable[0] != nullptr) {
				for (int j = 0; j < program->bound; j++) {
					if (vm->results[j].name == nullptr)
						continue;

					spvm_result_t res = &vm->results[j];

					// TODO: also check for the type, or there might be some crashes caused by two vars with different type (?) (mat4 and vec4 for example)

					if (res->member_count == varValueCount && res->members == nullptr && strcmp(varList[i].c_str(), res->name) == 0) {
						res->members = pointerToVariable[0]->members;

						if (vm->derivative_used) {
							if (vm->derivative_group_x) vm->derivative_group_x->results[j].members = pointerToVariable[1]->members;
							if (vm->derivative_group_y) vm->derivative_group_y->results[j].members = pointerToVariable[2]->members;
							if (vm->derivative_group_d) vm->derivative_group_d->results[j].members = pointerToVariable[3]->members;
						}
					}
				}
			}
		}

		// execute $$_shadered_immediate
		spvm_word fnImmediate = spvm_state_get_result_location(vm, "$$_shadered_immediate");
		spvm_state_prepare(vm, fnImmediate);
		spvm_state_call_function(vm);

		// get type and return value
		spvm_result_t val = &vm->results[m_breakpoint[index].ResultID];
		returnType = spvm_state_get_type_info(vm->results, &vm->results[val->pointer]);
		return val;
	}
	void FrameAnalysis::m_cleanBreakpoints()
	{
		for (int i = 0; i < m_breakpoint.size(); i++) {
			if (m_breakpoint[i].SPIRV.size() > 1) {
				spvm_state_delete(m_breakpoint[i].VM);
				spvm_program_delete(m_breakpoint[i].Shader);
			}
		}

		m_breakpoint.clear();
		m_hasBreakpoints = false;
	}

	void FrameAnalysis::m_clean()
	{
		if (m_depth != nullptr) {
			free(m_depth);
			m_depth = nullptr;
		}

		if (m_color != nullptr) {
			free(m_color);
			m_color = nullptr;
		}

		if (m_instCount != nullptr) {
			free(m_instCount);
			m_instCount = nullptr;
		}

		if (m_ub != nullptr) {
			free(m_ub);
			m_ub = nullptr;
		}
		
		if (m_bkpt != nullptr) {
			free(m_bkpt);
			m_bkpt = nullptr;
		}
	}
	void FrameAnalysis::m_copyVBOData(eng::Model::Mesh::Vertex& vertex, GLfloat* vbo, int stride)
	{
		if (stride == 18) {
			vertex.Position = glm::vec3(vbo[0], vbo[1], vbo[2]);
			vertex.Normal = glm::vec3(vbo[3], vbo[4], vbo[5]);
			vertex.TexCoords = glm::vec2(vbo[6], vbo[7]);
			vertex.Tangent = glm::vec3(vbo[8], vbo[9], vbo[10]);
			vertex.Binormal = glm::vec3(vbo[11], vbo[12], vbo[13]);
			vertex.Color = glm::vec4(vbo[14], vbo[15], vbo[16], vbo[17]);
		} else {
			vertex.Position = glm::vec3(vbo[0], vbo[1], 0.0f);
			vertex.TexCoords = glm::vec2(vbo[2], vbo[3]);			
		}
	}

	glm::vec4 FrameAnalysis::m_executePixelShaderWithBreakpoints(int x, int y, uint8_t& res, int loc)
	{
		spvm_state_t vm = m_debugger->GetVM();
		if (vm == nullptr)
			return glm::vec4(0.0f);

		spvm_word fnMain = spvm_state_get_result_location(vm, "main");
		if (fnMain == 0)
			return glm::vec4(0.0f);

		spvm_state_prepare(vm, fnMain);
		spvm_state_set_frag_coord(vm, x + 0.5f, y + 0.5f, 1.0f, 1.0f); // TODO: z and w components
		spvm_word prevLine = vm->current_line;
		while (vm->code_current != nullptr) {
			spvm_state_step_into(vm);
			if (vm->current_line != prevLine) {
				for (uint8_t i = 0; i < m_breakpoint.size(); i++) {
					if (m_breakpoint[i].Breakpoint->Line == vm->current_line) {
						if (m_breakpoint[i].Breakpoint->IsConditional && (res & (1 << i)) == 0) { // only run conditional breakpoint if needed
							if (!m_breakpoint[i].Cached) {
								m_cacheBreakpoint(i);
								m_breakpoint[i].Cached = true;
							}
							
							spvm_result_t resultType = nullptr;
							spvm_result_t result = m_executeBreakpoint(i, resultType);
							if (result && resultType->value_type == spvm_value_type_bool && result->member_count == 1)
								res |= (result->members[0].value.b << i);
						} else
							res |= (1 << i);
					}
				}
				prevLine = vm->current_line;
			}
		}

		return m_debugger->GetPixelShaderOutput(loc);
	}

	void FrameAnalysis::Init(size_t width, size_t height, const glm::vec4& clearColor)
	{
		m_clean();

		m_depth = (float*)malloc(width * height * sizeof(float));
		m_color = (uint32_t*)malloc(width * height * sizeof(uint32_t));
		m_instCount = (uint32_t*)calloc(width * height, sizeof(uint32_t));
		m_ub = (uint32_t*)calloc(width * height, sizeof(uint32_t));

		if (m_hasBreakpoints)
			m_bkpt = (uint8_t*)calloc(width * height, sizeof(uint8_t));

		uint32_t clearColorU32 = m_encodeColor(clearColor);

		for (size_t x = 0; x < width; x++)
			for (size_t y = 0; y < height; y++) {
				m_color[y * width + x] = clearColorU32;
				m_depth[y * width + x] = FLT_MAX;
			}

		m_width = width;
		m_height = height;

		m_instCountAvg = m_instCountAvgN = m_instCountMax = 0;
		m_pixelCount = m_pixelsDiscarded = m_pixelsUB = m_pixelsFailedDepthTest = 0;
		m_triangleCount = m_trianglesDiscarded = 0;

		m_isRegion = false;

		// check if we need to collect pixel history
		m_pixelHistoryLocation = glm::ivec2(-1, -1);
		for (const auto& pixel : m_debugger->GetPixelList()) {
			if (pixel.RenderTexture == nullptr)
				m_pixelHistoryLocation = pixel.Coordinate;
		}
	}
	void FrameAnalysis::Copy(GLuint tex, int width, int height)
	{
		uint8_t* pixels = (uint8_t*)malloc(width * height * 4);

		glBindTexture(GL_TEXTURE_2D, tex);
		glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
		glBindTexture(GL_TEXTURE_2D, 0);
		for (size_t x = 0; x < width; x++)
			for (size_t y = 0; y < height; y++) {
				uint8_t* px = &pixels[(y * width + x) * 4];
				m_color[y * width + x] = px[0] | px[1] << 8 | px[2] << 16 | 0x77000000;
			}

		free(pixels);
	}
	void FrameAnalysis::SetRegion(int x, int y, int endX, int endY)
	{
		m_isRegion = true;
		m_regionX = x;
		m_regionY = y;
		m_regionEndX = endX;
		m_regionEndY = endY;
	}
	void FrameAnalysis::SetBreakpoints(const std::vector<const dbg::Breakpoint*>& breakpoints, const std::vector<glm::vec3>& bkptColors, const std::vector<const char*>& bkptPaths)
	{
		m_cleanBreakpoints();

		m_breakpoint.resize(breakpoints.size());
		for (int i = 0; i < m_breakpoint.size(); i++) {
			m_breakpoint[i].Breakpoint = breakpoints[i];
			m_breakpoint[i].Color = bkptColors[i];
			m_breakpoint[i].PSPath = bkptPaths[i];

			m_breakpoint[i].VM = nullptr;
			m_breakpoint[i].Shader = nullptr;
		}
		m_hasBreakpoints = m_breakpoint.size() > 0;
	}

	void FrameAnalysis::RenderPass(PipelineItem* pass)
	{
		m_pass = pass;

		if (pass->Type == PipelineItem::ItemType::ShaderPass) {
			pipe::ShaderPass* data = (pipe::ShaderPass*)pass->Data;

			m_pixel.GeometryShaderUsed = data->GSUsed;
			m_pixel.TessellationShaderUsed = data->TSUsed;

			for (PipelineItem* item : data->Items) {
				// built-in geometry
				if (item->Type == PipelineItem::ItemType::Geometry) {
					pipe::GeometryItem* geom = (pipe::GeometryItem*)item->Data;
					const int vCount = ed::eng::GeometryFactory::VertexCount[geom->Type];
					const int vStride = geom->Type == pipe::GeometryItem::GeometryType::ScreenQuadNDC ? 4 : 18;
					float* vbo = (float*)malloc(vCount * vStride * sizeof(float));
					
					// get vertex data on the cpu
					glBindBuffer(GL_ARRAY_BUFFER, geom->VBO); // TODO: don't bother GPU so much
					glGetBufferSubData(GL_ARRAY_BUFFER, 0, vCount * vStride * sizeof(float), &vbo[0]);
					glBindBuffer(GL_ARRAY_BUFFER, 0);

					// loop through all vertices
					const uint8_t pSize = 3;
					for (unsigned int p = 0; p < vCount; p += pSize) {
						for (int v = 0; v < 3; v++)
							m_copyVBOData(m_pixel.Vertex[v], vbo + (p + v) * vStride, vStride);
						RenderPrimitive(item, p, pSize, geom->Topology);
					}

					// free memory
					free(vbo);
				}
				// 3D model
				else if (item->Type == PipelineItem::ItemType::Model) {
					pipe::Model* mdl = (pipe::Model*)item->Data;
					// get vertex data on the cpu
					if (mdl->Data) {
						for (auto& mesh : mdl->Data->Meshes) {
							// loop through all vertices
							for (unsigned int p = 0; p < mesh.Indices.size(); p += 3) {
								m_pixel.Vertex[0] = mesh.Vertices[mesh.Indices[p + 0]];
								m_pixel.Vertex[1] = mesh.Vertices[mesh.Indices[p + 1]];
								m_pixel.Vertex[2] = mesh.Vertices[mesh.Indices[p + 2]];

								RenderPrimitive(item, p, 3, GL_TRIANGLES);
							}
						}
					}
				}
				// vertex buffer
				else if (item->Type == PipelineItem::ItemType::VertexBuffer) {
					pipe::VertexBuffer* vBuffer = ((pipe::VertexBuffer*)item->Data);
					ed::BufferObject* bufData = (ed::BufferObject*)vBuffer->Buffer;

					// get vertex count
					int topologySelection = 0;
					for (; topologySelection < (sizeof(TOPOLOGY_ITEM_VALUES) / sizeof(*TOPOLOGY_ITEM_VALUES)); topologySelection++)
						if (TOPOLOGY_ITEM_VALUES[topologySelection] == vBuffer->Topology)
							break;
					uint8_t vertexCount = TOPOLOGY_SINGLE_VERTEX_COUNT[topologySelection];

					std::vector<ShaderVariable::ValueType> tData = m_objects->ParseBufferFormat(bufData->ViewFormat);

					int stride = 0;
					for (const auto& dataEl : tData)
						stride += ShaderVariable::GetSize(dataEl, true);

					GLfloat* bufPtr = (GLfloat*)malloc(bufData->Size);

					glBindBuffer(GL_ARRAY_BUFFER, bufData->ID);
					glGetBufferSubData(GL_ARRAY_BUFFER, 0, bufData->Size, &bufPtr[0]);
					glBindBuffer(GL_ARRAY_BUFFER, 0);

					for (int p = 0; p < bufData->Size / stride; p += vertexCount) {
						// copy primitive data
						for (int i = 0; i < vertexCount; i++) {
							int iOffset = 0;
							for (int j = 0; j < tData.size(); j++) {
								// TODO: use input layout
								if (j == 0) /* POSITION */
									m_pixel.Vertex[i].Position = glm::make_vec3(bufPtr + (p+i) * stride / 4 + iOffset);
								else if (j == 1)
									m_pixel.Vertex[i].Normal = glm::make_vec3(bufPtr + (p+i) * stride / 4 + iOffset);
								else if (j == 2)
									m_pixel.Vertex[i].TexCoords = glm::make_vec2(bufPtr + (p+i) * stride / 4 + iOffset);

								iOffset += ShaderVariable::GetSize(tData[j]) / 4;
							}
						}

						// render it
						RenderPrimitive(item, p, vertexCount, vBuffer->Topology);
					}

					free(bufPtr);
				} 
			}
		}
	}
	void FrameAnalysis::RenderPrimitive(PipelineItem* item, unsigned int vertexStart, uint8_t vertexCount, unsigned int topology)
	{
		m_pixel.VertexCount = vertexCount;
		m_pixel.Pass = m_pass;
		m_pixel.Object = item;
		m_pixel.InTopology = topology;
		m_pixel.OutTopology = topology;
		m_pixel.InstanceID = 0;
		m_pixel.RenderTextureSize = m_renderer->GetLastRenderSize();
		m_pixel.RenderTextureIndex = 0;
		m_pixel.VertexID = vertexStart;
		m_pixel.Fetched = false;

		// run the vertex shader
		m_debugger->PrepareVertexShader(m_pass, item, &m_pixel);
		for (unsigned int v = 0; v < vertexCount; v++) {
			m_debugger->SetVertexShaderInput(m_pixel, v);
			m_pixel.VertexShaderPosition[v] = m_debugger->ExecuteVertexShader();
			m_debugger->CopyVertexShaderOutput(m_pixel, v);
		}
		memcpy(m_pixel.FinalPosition, m_pixel.VertexShaderPosition, sizeof(glm::vec4) * 3);

		// render the triangle
		if (!m_pixel.GeometryShaderUsed)
			RenderTriangle(item);
		else {
			// run the geometry shader first
			m_debugger->PrepareGeometryShader(m_pixel.Pass, m_pixel.Object);
			m_debugger->SetGeometryShaderInput(m_pixel);
			m_debugger->ExecuteGeometryShader();

			// then render each generated triangle
			for (int p = 0; p < m_pixel.GeometryOutput.size(); p++) {
				auto* prim = &m_pixel.GeometryOutput[p];

				// triangles
				if (m_pixel.GeometryOutputType == GeometryShaderOutput::TriangleStrip) {
					for (int v = 2; v < prim->Position.size(); v++) {
						m_pixel.GeometrySelectedPrimitive = p;
						m_pixel.GeometrySelectedVertex = v;

						// fix the winding order when GS is used
						int d1 = 2, d2 = 1;
						if (v % 2 == 1) {
							d1 = 1;
							d2 = 2;
						}
						m_pixel.FinalPosition[0] = prim->Position[v - d1];
						m_pixel.FinalPosition[1] = prim->Position[v - d2];
						m_pixel.FinalPosition[2] = prim->Position[v];

						RenderTriangle(item);
					}
				}
			}
		}

		// cleanup
		m_debugger->ClearPixelData(m_pixel);
	}
	void FrameAnalysis::RenderTriangle(PipelineItem* item)
	{
		glm::ivec2 vert[3];

		for (unsigned int v = 0; v < 3; v++)
			vert[v] = ((glm::vec2(m_pixel.FinalPosition[v]) / m_pixel.FinalPosition[v].w + 1.0f) * 0.5f) * glm::vec2(m_pixel.RenderTextureSize);

		// triangle bounding box
		int minX = std::max<int>(0, std::min<int>(std::min<int>(vert[0].x, vert[1].x), vert[2].x));
		int maxX = std::min<int>(m_width - 1, std::max<int>(std::max<int>(vert[0].x, vert[1].x), vert[2].x));
		int minY = std::max<int>(0, std::min<int>(std::min<int>(vert[0].y, vert[1].y), vert[2].y));
		int maxY = std::min<int>(m_height - 1, std::max<int>(std::max<int>(vert[0].y, vert[1].y), vert[2].y));
		if (minX < 0 || maxX < 0 || minY < 0 || maxY < 0)
			return;

		// edge equations
		EdgeEquation edge1(vert[0], vert[1]);
		EdgeEquation edge2(vert[1], vert[2]);
		EdgeEquation edge3(vert[2], vert[0]);

		// check if backfacing
		m_triangleCount++;
		if (edge1.c + edge2.c + edge3.c < 0.0f) {
			m_trianglesDiscarded++;
			return;
		}

		// clip to region limits
		if (m_isRegion) {
			minX = std::max<int>(minX, m_regionX);
			minY = std::max<int>(minY, m_regionY);
			maxX = std::min<int>(maxX, m_regionEndX);
			maxY = std::min<int>(maxY, m_regionEndY);
		}

		// round to block size
		minX &= ~(RASTER_BLOCK_SIZE - 1);
		maxX &= ~(RASTER_BLOCK_SIZE - 1);
		minY &= ~(RASTER_BLOCK_SIZE - 1);
		maxY &= ~(RASTER_BLOCK_SIZE - 1);

		// init the renderer
		// TODO: tested with threads, it was 3-4 times faster though there were some artifacts which appeared only *sometimes*... it's really slow rn :(
		m_debugger->PreparePixelShader(m_pass, item, &m_pixel);
		m_debugger->ToggleAnalyzer(true); // turn on the analyzer
		for (int x = minX; x <= maxX; x += RASTER_BLOCK_SIZE) {
			for (int y = minY; y <= maxY; y += RASTER_BLOCK_SIZE) {
				// check if block is inside the triangle
				// inspired by github.com/trenki2/SoftwareRenderer
				bool btmLeft1 = edge1.Test(x, y), btmLeft2 = edge2.Test(x, y), btmLeft3 = edge3.Test(x, y), btmLeft = btmLeft1 && btmLeft2 && btmLeft3;
				bool btmRight1 = edge1.Test(x + RASTER_BLOCK_STEP, y), btmRight2 = edge2.Test(x + RASTER_BLOCK_STEP, y), btmRight3 = edge3.Test(x + RASTER_BLOCK_STEP, y), btmRight = btmRight1 && btmRight2 && btmRight3;
				bool topLeft1 = edge1.Test(x, y + RASTER_BLOCK_STEP), topLeft2 = edge2.Test(x, y + RASTER_BLOCK_STEP), topLeft3 = edge3.Test(x, y + RASTER_BLOCK_STEP), topLeft = topLeft1 && topLeft2 && topLeft3;
				bool topRight1 = edge1.Test(x + RASTER_BLOCK_STEP, y + RASTER_BLOCK_STEP), topRight2 = edge2.Test(x + RASTER_BLOCK_STEP, y + RASTER_BLOCK_STEP), topRight3 = edge3.Test(x + RASTER_BLOCK_STEP, y + RASTER_BLOCK_STEP), topRight = topRight1 && topRight2 && topRight3;

				int result = btmLeft + btmRight + topLeft + topRight;

				// TODO: check if triangle and and rectangle don't intersect - skip the m_renderBlock
				if (m_hasBreakpoints)
					m_renderBlock<true>(m_debugger, x, y, result == 4, edge1, edge2, edge3);
				else
					m_renderBlock<false>(m_debugger, x, y, result == 4, edge1, edge2, edge3);
			}
		}
		m_debugger->ToggleAnalyzer(false); // turn off the analyzer
	}

	float* FrameAnalysis::AllocateHeatmap()
	{
		float* tex = (float*)malloc(m_width * m_height * 3 * sizeof(float));

		for (int y = 0; y < m_height; y++) {
			for (int x = 0; x < m_width; x++) {
				float val = m_instCount[y * m_width + x] / (float)m_instCountMax;
				glm::vec3 color = getHeatmapColor(val);
				tex[(y * m_width + x) * 3 + 0] = color.r;
				tex[(y * m_width + x) * 3 + 1] = color.g;
				tex[(y * m_width + x) * 3 + 2] = color.b; 
			}
		}

		return tex;
	}
	uint32_t* FrameAnalysis::AllocateUndefinedBehaviorMap()
	{
		uint32_t* tex = (uint32_t*)malloc(m_width * m_height * sizeof(uint32_t));

		for (int y = 0; y < m_height; y++) {
			for (int x = 0; x < m_width; x++) {
				uint32_t ubType = GetUndefinedBehaviorLastType(x, y);

				if (ubType)
					tex[y * m_width + x] = 0xFFFFFFFF;
				else
					tex[y * m_width + x] = (m_color[y * m_width + x] & 0x00FFFFFF) | 0x66000000; // darken the texture
			}
		}

		return tex;
	}
	uint32_t* FrameAnalysis::AllocateGlobalBreakpointsMap()
	{
		if (!m_hasBreakpoints)
			return nullptr;

		uint32_t* tex = (uint32_t*)malloc(m_width * m_height * sizeof(uint32_t));

		for (int y = 0; y < m_height; y++) {
			for (int x = 0; x < m_width; x++) {
				uint8_t bkpt = m_bkpt[y * m_width + x];

				if (bkpt) {
					for (uint8_t i = 0; i < 8; i++)
						if (bkpt & (1 << i))
							tex[y * m_width + x] = m_encodeColor(glm::vec4(m_breakpoint[i].Color, 1.0f));
				} else
					tex[y * m_width + x] = (m_color[y * m_width + x] & 0x00FFFFFF) | 0x66000000; // darken the texture
			}
		}

		return tex;
	}
	void FrameAnalysis::m_variableViewerProcess(spvgentwo::Module* module, const spvgentwo::Function& func, const std::string& variableName, unsigned int line, spvgentwo::Instruction* outputInstruction, spvgentwo::Instruction*& inputInstruction, uint8_t& components)
	{
		bool passedOpReturn = false;
		unsigned int currentLine = 0;

		// loop through global variables
		auto& globalVariables = module->getGlobalVariables();
		for (auto& glob : globalVariables) {
			const char* globName = glob.getName();
			if (globName != nullptr && strcmp(globName, variableName.c_str()) == 0) {
				inputInstruction = &glob;
				break;
			}
		}

		// loop through BasicBlocks
		std::vector<bool> clearBB(func.size(), true);
		int lastBBIndex = 0, currentBBIndex = 0;
		for (auto bbIterator = func.begin(); bbIterator != func.end(); bbIterator++, currentBBIndex++) {
			spvgentwo::BasicBlock& bb = *bbIterator;

			// loop through instructions
			auto it = bb.begin();
			while (it != bb.end()) {
				spvgentwo::Instruction& inst = *it;
				unsigned int actualOpCode = (inst.getOpCode() & spvgentwo::spv::OpCodeMask);

				if (actualOpCode == (unsigned int)spvgentwo::spv::Op::OpLine) {
					for (auto& op : inst) {
						if (op.isLiteral()) {
							currentLine = op.getLiteral().value;
							break;
						}
					}
				}
				const char* iName = inst.getName();
				if (iName != nullptr && strcmp(iName, variableName.c_str()) == 0)
					inputInstruction = &inst;

				bool isOpReturn = actualOpCode == (unsigned int)spvgentwo::spv::Op::OpReturn;
				bool isOpFunctionEnd = actualOpCode == (unsigned int)spvgentwo::spv::Op::OpFunctionEnd;

				if (currentLine > line + 1 || passedOpReturn || ((isOpReturn || isOpFunctionEnd) && inputInstruction != nullptr)) { // since line starts from 0 and currentLine (SPIR-V) is from 1
					it++;
					bb.remove(&inst);
					passedOpReturn = isOpReturn;
				} else {
					if (currentLine == line + 1)
						lastBBIndex = currentBBIndex;
					it++;
					clearBB[currentBBIndex] = false;
				}
			}
		}

		// generate new instructions
		if (inputInstruction && outputInstruction) {
			if (lastBBIndex >= func.size())
				lastBBIndex = func.size() - 1;
			for (int i = 0; i < clearBB.size(); i++) {
				if (!clearBB[i]) {
					lastBBIndex = i;
					continue;
				}
				spvgentwo::BasicBlock& bb = *(func.begin() + (unsigned int)i);
				bb.clear();
			}

			spvgentwo::BasicBlock& bb = *(func.begin() + (unsigned int)lastBBIndex);

			spvgentwo::Instruction* loadedInput = bb->opLoad(inputInstruction);
			spvgentwo::Instruction* newValue = nullptr;

			if (loadedInput->getType()->isScalar()) {
				newValue = bb->opCompositeConstruct(module->type<spvgentwo::vector_t<float, 4>>(), loadedInput, loadedInput, loadedInput, loadedInput);
				components = 1;
			} else if (loadedInput->getType()->getScalarOrVectorLength() == 2) {
				newValue = bb->opCompositeConstruct(module->type<spvgentwo::vector_t<float, 4>>(), loadedInput, module->constant<float>(0.0f), module->constant<float>(1.0f));
				components = 2;
			} else if (loadedInput->getType()->getScalarOrVectorLength() == 3) {
				newValue = bb->opCompositeConstruct(module->type<spvgentwo::vector_t<float, 4>>(), loadedInput, module->constant<float>(1.0f));
				components = 3;
			} else if (loadedInput->getType()->getScalarOrVectorLength() == 4) {
				newValue = loadedInput;
				components = 4;
			}


			if (func.getReturnType().isVoid()) {
				bb->opStore(outputInstruction, newValue);
				bb->opReturn();
			} else
				bb->opReturnValue(newValue);
			bb->opFunctionEnd();
		}
	}
	float* FrameAnalysis::AllocateVariableValueMap(PipelineItem* pass, const std::string& variableName, unsigned int line, uint8_t& components)
	{
		if (pass == nullptr || variableName.size() == 0 || line == 0 || pass->Type != PipelineItem::ItemType::ShaderPass)
			return nullptr;

		pipe::ShaderPass* passData = ((pipe::ShaderPass*)pass->Data);
		std::vector<unsigned int> oldSPV = passData->PSSPV;
		if (oldSPV.size() <= 1)
			return nullptr;

		spvgentwo::HeapAllocator* allocator = new spvgentwo::HeapAllocator();
		spvgentwo::Grammar* grammar = new spvgentwo::Grammar(allocator);
		spvgentwo::Module* module = new spvgentwo::Module(allocator);


		BinaryVectorReader reader(oldSPV);

		module->reset();
		module->read(reader, *grammar);
		module->resolveIDs();
		module->reconstructTypeAndConstantInfo();
		module->reconstructNames();

		spvgentwo::Instruction* outputInstruction = nullptr, *inputInstruction = nullptr;
		module->iterateInstructions([&](spvgentwo::Instruction& inst) {
			if (inst.getOperation() == spvgentwo::spv::Op::OpVariable) {
				spvgentwo::spv::StorageClass sc = inst.getStorageClass();
				if (sc == spvgentwo::spv::StorageClass::Output)
					outputInstruction = &inst;
			}
		});


		// if HLSL, check @name functions
		if (ed::ShaderCompiler::GetShaderLanguageFromExtension(passData->PSPath) == ShaderLanguage::HLSL) {
			std::string entryName = "@" + std::string(passData->PSEntry);
			auto& funcs = module->getFunctions();
			for (auto& f : funcs) {
				std::string fName = std::string(f.getName());
				if (fName.find(entryName) == 0)
					;//m_variableViewerProcess(module, f, variableName, line, outputInstruction, inputInstruction, components);
			}
		}
		// otherwise check entry points
		else {
			auto& entryPoints = module->getEntryPoints();
			for (auto& f : entryPoints) {
				std::string fName = std::string(f.getName());
				if (fName == "main")
					; //m_variableViewerProcess(module, f, variableName, line, outputInstruction, inputInstruction, components);
			}
		}

		module->assignIDs();

		std::vector<unsigned int> newSPV;
		spvgentwo::BinaryVectorWriter writer(newSPV);
		module->write(writer);

		bool failed = false;
		//(outputInstruction == nullptr || inputInstruction == nullptr);

		delete grammar;
		delete module;
		delete allocator;

		if (failed)
			return nullptr;

		std::string newGLSL = ed::ShaderCompiler::ConvertToGLSL(newSPV, ed::ShaderLanguage::GLSL, ed::ShaderStage::Pixel, passData->TSUsed, passData->GSUsed, nullptr, false);
		
		// input the new shader
		m_renderer->RecompileFromSource(pass->Name, "", newGLSL);
		m_renderer->Render();

		GLuint rendTex = m_renderer->GetTexture();
		float* returnData = (float*)malloc(m_renderer->GetLastRenderSize().x * m_renderer->GetLastRenderSize().y * 4 * sizeof(float));
		glBindTexture(GL_TEXTURE_2D, rendTex);
		glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, returnData);
		glBindTexture(GL_TEXTURE_2D, 0);

		// return old shader
		m_renderer->Recompile(pass->Name);
		m_renderer->Render();

		return returnData;
	}
}