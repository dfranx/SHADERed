#pragma once
#include <SHADERed/Objects/PipelineItem.h>
#include <SHADERed/Objects/DebugInformation.h>

#define RASTER_BLOCK_SIZE 8
#define RASTER_BLOCK_STEP RASTER_BLOCK_SIZE - 1

namespace ed {
	class FrameAnalysis {
	public:
		FrameAnalysis(DebugInformation* dbgr, RenderEngine* render, ObjectManager* objects, MessageStack* msgs);
		~FrameAnalysis();

		void Init(size_t width, size_t height, const glm::vec4& clearColor);
		void Copy(GLuint tex, int width, int height);
		void SetRegion(int x, int y, int endX, int endY);

		void RenderPass(PipelineItem* pass);
		void RenderPrimitive(PipelineItem* item, unsigned int vertexStart, uint8_t vertexCount, unsigned int topology);
		void RenderTriangle(PipelineItem* item);

		inline uint32_t* GetColorOutput() { return m_color; }
		inline glm::ivec2 GetOutputSize() { return glm::ivec2(m_width, m_height); }

		void BuildHeatmap();
		inline float* GetHeatmap() { return m_heatmap; }
		inline uint32_t GetHeatmapMax() { return m_instCountMax; }
		inline uint32_t GetInstructionCount(int x, int y) { return m_instCount[y * m_width + x]; }

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
		ObjectManager* m_objects;
		MessageStack* m_msgs;

		bool m_isRegion;
		int m_regionX, m_regionY, m_regionEndX, m_regionEndY;

		float* m_depth;
		uint32_t* m_color;
		int m_width, m_height;

		uint32_t m_instCountMax, m_instCountAvg, m_instCountAvgN;
		uint32_t* m_instCount;
		float* m_heatmap;

		PipelineItem* m_pass;
		PixelInformation m_pixel;

		void m_clean();
		void m_copyVBOData(eng::Model::Mesh::Vertex& vertex, GLfloat* vbo, int stride);
		inline uint32_t m_encodeColor(const glm::vec4& color)
		{
			return (uint32_t)(color.r * 255) | (uint32_t)(color.g * 255) << 8 |
				(uint32_t)(color.b * 255) << 16 | (uint32_t)(color.a * 255) << 24;
		}

		void m_renderBlock(DebugInformation* renderer, size_t startX, size_t startY, bool skipChecks, EdgeEquation& e1, EdgeEquation& e2, EdgeEquation& e3);
	};
}