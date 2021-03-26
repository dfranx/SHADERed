#pragma once
#include <SHADERed/Objects/PipelineItem.h>
#include <SHADERed/Objects/DebugInformation.h>

#define RASTER_BLOCK_SIZE 8
#define RASTER_BLOCK_STEP RASTER_BLOCK_SIZE - 1

namespace ed {
	class FrameAnalysis {
	public:
		FrameAnalysis(DebugInformation* dbgr, RenderEngine* render, PipelineManager* pipeline, ObjectManager* objects, MessageStack* msgs);
		~FrameAnalysis();

		void Init(size_t width, size_t height, const glm::vec4& clearColor);
		void Copy(GLuint tex, int width, int height);
		void SetRegion(int x, int y, int endX, int endY);

		void SetBreakpoints(const std::vector<const dbg::Breakpoint*>& breakpoints, const std::vector<glm::vec3>& bkptColors, const std::vector<const char*>& bkptPaths);

		void RenderPass(PipelineItem* pass);
		void RenderPrimitive(PipelineItem* item, unsigned int vertexStart, uint8_t vertexCount, unsigned int topology);
		void RenderTriangle(PipelineItem* item);

		inline uint32_t* GetColorOutput() { return m_color; }
		inline glm::ivec2 GetOutputSize() { return glm::ivec2(m_width, m_height); }

		float* AllocateHeatmap();
		inline uint32_t GetHeatmapMax() { return m_instCountMax; }
		inline uint32_t GetInstructionCount(int x, int y) { return m_instCount[y * m_width + x]; }
		inline uint32_t GetInstructionCountAverage() { return m_instCountAvg; }

		uint32_t* AllocateUndefinedBehaviorMap();
		inline uint32_t GetUndefinedBehaviorLastLine(int x, int y) { return (m_ub[y * m_width + x] & 0xFFFFF000) >> 12; }
		inline uint32_t GetUndefinedBehaviorCount(int x, int y) { return (m_ub[y * m_width + x] & 0x00000F00) >> 8; }
		inline uint32_t GetUndefinedBehaviorLastType(int x, int y) { return (m_ub[y * m_width + x] & 0x000000FF); }

		inline uint32_t GetPixelCount() { return m_pixelCount; }
		inline uint32_t GetPixelsDiscarded() { return m_pixelsDiscarded; }
		inline uint32_t GetPixelsUndefinedBehavior() { return m_pixelsUB; }
		inline uint32_t GetPixelsFailedDepthTest() { return m_pixelsFailedDepthTest; }

		inline uint32_t GetTriangleCount() { return m_triangleCount; }
		inline uint32_t GetTrianglesDiscarded() { return m_trianglesDiscarded; }

		uint32_t* AllocateGlobalBreakpointsMap();
		inline bool HasGlobalBreakpoints() { return m_hasBreakpoints; }

		float* AllocateVariableValueMap(PipelineItem* pass, const std::string& variableName, unsigned int line, uint8_t& components);

	private:
		class EdgeEquation {
		public:
			float a;
			float b;
			float c;
			bool tie;

			EdgeEquation(const glm::ivec2& v0, const glm::ivec2& v1);
			
			inline bool Test(int x, int y) {
				return m_test(m_evaluate(x, y));
			}

		private:
			inline float m_evaluate(int x, int y) {
				return a * x + b * y + c;
			}
			inline bool m_test(float v) {
				return (v > 0 || v == 0 && tie);
			}
		};

		DebugInformation* m_debugger;
		RenderEngine* m_renderer;
		PipelineManager* m_pipeline;
		ObjectManager* m_objects;
		MessageStack* m_msgs;

		bool m_isRegion;
		int m_regionX, m_regionY, m_regionEndX, m_regionEndY;

		float* m_depth;
		uint32_t* m_color;
		int m_width, m_height;

		glm::ivec2 m_pixelHistoryLocation;

		uint32_t m_pixelCount, m_pixelsDiscarded, m_pixelsUB, m_pixelsFailedDepthTest;
		uint32_t m_triangleCount, m_trianglesDiscarded;

		int m_instCountMax, m_instCountAvg, m_instCountAvgN;
		uint32_t* m_instCount;

		uint32_t* m_ub;

		bool m_hasBreakpoints;
		struct BreakpointData {
			const dbg::Breakpoint* Breakpoint;
			glm::vec3 Color;
			const char* PSPath;

			// VM stuff
			bool Cached;
			int ResultID;
			std::vector<std::string> VariableList;
			std::vector<unsigned int> SPIRV;

			spvm_program_t Shader;
			spvm_state_t VM;
		};
		std::vector<BreakpointData> m_breakpoint;
		uint8_t* m_bkpt;

		std::vector<unsigned int>* m_getPixelShaderSPV(const char* path);
		void m_cacheBreakpoint(int index);
		spvm_result_t m_executeBreakpoint(int index, spvm_result_t& retType);
		void m_cleanBreakpoints();

		PipelineItem* m_pass;
		PixelInformation m_pixel;

		void m_variableViewerProcess(spvgentwo::Module* module, const spvgentwo::Function& func, const std::string& variableName, unsigned int line, spvgentwo::Instruction* outputInstruction, spvgentwo::Instruction*& inputInstruction, uint8_t& components);

		void m_clean();
		void m_copyVBOData(eng::Model::Mesh::Vertex& vertex, GLfloat* vbo, int stride);
		inline uint32_t m_encodeColor(const glm::vec4& color)
		{
			return (uint32_t)(color.r * 255) | (uint32_t)(color.g * 255) << 8 |
				(uint32_t)(color.b * 255) << 16 | (uint32_t)(color.a * 255) << 24;
		}

		glm::vec4 m_executePixelShaderWithBreakpoints(int x, int y, uint8_t& res, int loc = 0);

		template <bool hasBreakpoints>
		void m_renderBlock(DebugInformation* renderer, size_t startX, size_t startY, bool skipChecks, EdgeEquation& e1, EdgeEquation& e2, EdgeEquation& e3)
		{
			for (size_t x = startX; x < std::min<size_t>(m_width, startX + RASTER_BLOCK_SIZE); x++) {
				for (size_t y = startY; y < std::min<size_t>(m_height, startY + RASTER_BLOCK_SIZE); y++) {
					if (skipChecks || (e1.Test(x, y) && e2.Test(x, y) && e3.Test(x, y))) {
						m_pixel.Coordinate = glm::ivec2(x, y);
						m_pixel.RelativeCoordinate = glm::vec2(x, y) / glm::vec2(m_pixel.RenderTextureSize);

						// prepare inputs & calculate
						float depth = renderer->SetPixelShaderInput(m_pixel);

						if (depth <= m_depth[y * m_width + x]) { // TODO: OpExecutionMode DepthReplacing -> execute pixel shader, then go through depth test
							if constexpr (!hasBreakpoints)
								m_pixel.DebuggerColor = renderer->ExecutePixelShader(x, y, m_pixel.RenderTextureIndex);
							else
								m_pixel.DebuggerColor = m_executePixelShaderWithBreakpoints(x, y, m_bkpt[y * m_width + x], m_pixel.RenderTextureIndex);

							if (renderer->GetVM()->discarded) {
								m_pixelsDiscarded++;
								continue;
							}

							// actual color and depth
							m_color[y * m_width + x] = m_encodeColor(m_pixel.DebuggerColor);
							m_depth[y * m_width + x] = depth;
							m_pixelCount++;

							// instruction count / heatmap stuff
							int instCount = renderer->GetVM()->instruction_count;
							m_instCount[y * m_width + x] = instCount;
							m_instCountMax = std::max<int>(m_instCountMax, instCount);
							m_instCountAvgN++;
							m_instCountAvg = m_instCountAvg + (instCount - m_instCountAvg) / m_instCountAvgN;

							// undefined behavior
							spvm_word ubType = renderer->GetLastUndefinedBehaviorType();
							spvm_word ubLine = renderer->GetLastUndefinedBehaviorLine();
							spvm_word ubCount = renderer->GetUndefinedBehaviorCount();
							m_ub[y * m_width + x] = (ubType & 0x000000FF) | ((ubCount << 8) & 0x00000F00) | ((ubLine << 12) & 0xFFFFF000);
							m_pixelsUB += (ubType > 0);

							// pixel history
							if (m_pixelHistoryLocation == m_pixel.Coordinate) {
								bool exists = false;
								for (const auto& pixel : m_debugger->GetPixelList())
									if (pixel.Object == m_pixel.Object && pixel.VertexID == m_pixel.VertexID) {
										exists = true;
										break;
									}

								if (!exists) {
									m_pixel.Color = m_pixel.DebuggerColor;
									m_pixel.History = true;
									m_debugger->AddPixel(m_pixel);
								}
							}

						} else
							m_pixelsFailedDepthTest++;
					}
				}
			}
		}
	};
}