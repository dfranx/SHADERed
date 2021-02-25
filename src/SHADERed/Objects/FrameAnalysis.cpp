#include <SHADERed/Objects/FrameAnalysis.h>
#include <SHADERed/Objects/Debug/PixelInformation.h>
#include <SHADERed/Objects/Names.h>
#include <SHADERed/Engine/GeometryFactory.h>

#include <thread>
#include <algorithm>
#include <glm/gtc/type_ptr.hpp>

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

		return (color[id2] - color[id1]) * value + color[id1];
	}

	FrameAnalysis::FrameAnalysis(DebugInformation* dbgr, RenderEngine* renderer, ObjectManager* objects, MessageStack* msgs)
	{
		m_depth = nullptr;
		m_color = nullptr;
		m_instCount = nullptr;
		m_ub = nullptr;
		m_pass = nullptr;

		m_width = 0;
		m_height = 0;

		m_instCountAvg = m_instCountAvgN = m_instCountMax = 0;

		m_debugger = dbgr;
		m_renderer = renderer;
		m_objects = objects;
		m_msgs = msgs;
	}
	FrameAnalysis::~FrameAnalysis()
	{
		m_clean();
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

	void FrameAnalysis::Init(size_t width, size_t height, const glm::vec4& clearColor)
	{
		m_clean();

		m_depth = (float*)malloc(width * height * sizeof(float));
		m_color = (uint32_t*)malloc(width * height * sizeof(uint32_t));
		m_instCount = (uint32_t*)calloc(width * height, sizeof(uint32_t));
		m_ub = (uint32_t*)calloc(width * height, sizeof(uint32_t));

		uint32_t clearColorU32 = m_encodeColor(clearColor);

		for (size_t x = 0; x < width; x++)
			for (size_t y = 0; y < height; y++) {
				m_color[y * width + x] = clearColorU32;
				m_depth[y * width + x] = FLT_MAX;
			}

		m_width = width;
		m_height = height;

		m_instCountAvg = m_instCountAvgN = m_instCountMax = 0;

		m_isRegion = false;
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

	void FrameAnalysis::RenderPass(PipelineItem* pass)
	{
		m_pass = pass;

		if (pass->Type == PipelineItem::ItemType::ShaderPass) {
			pipe::ShaderPass* data = (pipe::ShaderPass*)pass->Data;

			m_pixel.GeometryShaderUsed = data->GSUsed;

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
		int minX = std::max(0, std::min(std::min(vert[0].x, vert[1].x), vert[2].x));
		int maxX = std::min(m_width - 1, std::max(std::max(vert[0].x, vert[1].x), vert[2].x));
		int minY = std::max(0, std::min(std::min(vert[0].y, vert[1].y), vert[2].y));
		int maxY = std::min(m_height - 1, std::max(std::max(vert[0].y, vert[1].y), vert[2].y));
		if (minX < 0 || maxX < 0 || minY < 0 || maxY < 0)
			return;

		// edge equations
		EdgeEquation edge1(vert[0], vert[1]);
		EdgeEquation edge2(vert[1], vert[2]);
		EdgeEquation edge3(vert[2], vert[0]);

		// check if backfacing
		if (edge1.c + edge2.c + edge3.c < 0.0f)
			return;

		// clip to region limits
		if (m_isRegion) {
			minX = std::max(minX, m_regionX);
			minY = std::max(minY, m_regionY);
			maxX = std::min(maxX, m_regionEndX);
			maxY = std::min(maxY, m_regionEndY);
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

				if (result == 4)
					m_renderBlock(m_debugger, x, y, true, edge1, edge2, edge3);
				else {
					// TODO: check if triangle and and rectangle don't intersect - skip the m_renderBlock
					m_renderBlock(m_debugger, x, y, false, edge1, edge2, edge3);
				}
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
	void FrameAnalysis::m_renderBlock(DebugInformation* renderer, size_t startX, size_t startY, bool skipChecks, EdgeEquation& e1, EdgeEquation& e2, EdgeEquation& e3)
	{
		for (size_t x = startX; x < startX + RASTER_BLOCK_SIZE; x++) {
			for (size_t y = startY; y < startY + RASTER_BLOCK_SIZE; y++) {
				if (skipChecks || (e1.Test(x, y) && e2.Test(x, y) && e3.Test(x, y))) {
					m_pixel.Coordinate = glm::ivec2(x, y);
					m_pixel.RelativeCoordinate = glm::vec2(x, y) / glm::vec2(m_pixel.RenderTextureSize);

					float depth = renderer->SetPixelShaderInput(m_pixel);

					if (depth <= m_depth[y * m_width + x]) { // TODO: OpExecutionMode DepthReplacing -> execute pixel shader, then go through depth test
						m_pixel.DebuggerColor = renderer->ExecutePixelShader(x, y, m_pixel.RenderTextureIndex);

						if (renderer->GetVM()->discarded)
							continue;
						
						// actual color and depth
						m_color[y * m_width + x] = m_encodeColor(m_pixel.DebuggerColor);
						m_depth[y * m_width + x] = depth;

						// instruction count / heatmap stuff
						uint32_t instCount = renderer->GetVM()->instruction_count;
						m_instCount[y * m_width + x] = instCount;
						m_instCountMax = std::max(m_instCountMax, instCount);
						m_instCountAvgN++;
						m_instCountAvg = m_instCountAvg + (instCount - m_instCountAvg) / m_instCountAvgN;
					
						// undefined behavior
						spvm_word ubType = renderer->GetLastUndefinedBehaviorType();
						spvm_word ubLine = renderer->GetLastUndefinedBehaviorLine();
						spvm_word ubCount = renderer->GetUndefinedBehaviorCount();
						m_ub[y * m_width + x] = (ubType & 0x000000FF) | ((ubLine << 8) & 0x00000F00) | ((ubLine << 12) & 0xFFFFF000);
					}
				}
			}
		}
	}
}