#pragma once
#include <SHADERed/Engine/GLUtils.h>
#include <SHADERed/Objects/PipelineItem.h>
#include <SHADERed/UI/Tools/CubemapPreview.h>
#include <SHADERed/UI/Tools/Texture3DPreview.h>
#include <SHADERed/UI/Tools/Magnifier.h>
#include <SHADERed/UI/UIView.h>

namespace ed {
	class ObjectPreviewUI : public UIView {
	public:
		ObjectPreviewUI(GUIManager* ui, ed::InterfaceManager* objects, const std::string& name = "", bool visible = true)
				: UIView(ui, objects, name, visible)
		{
			m_cubePrev.Init(256, 192);
			m_tex3DPrev.Init();
			m_curHoveredItem = -1;
			m_initRowSize = false;
			m_saveObject = nullptr;
		}
		~ObjectPreviewUI() { }

		virtual void OnEvent(const SDL_Event& e);
		virtual void Update(float delta);

		void Open(ObjectManagerItem* item);

		inline bool ShouldRun() { return m_items.size() > 0; }
		void CloseAll();
		void Close(const std::string& name);

	private:
		eng::Timer m_bufUpdateClock;
		bool m_drawBufferElement(int row, int col, void* data, ShaderVariable::ValueType type);
		std::vector<ObjectManagerItem*> m_items;
		std::vector<char> m_isOpen; // char since bool is packed
		std::vector<std::vector<ShaderVariable::ValueType>> m_cachedBufFormat;
		std::vector<int> m_cachedBufSize;
		std::vector<glm::ivec2> m_cachedImgSize;
		std::vector<int> m_cachedImgSlice;
		ed::AudioAnalyzer m_audioAnalyzer;
		float m_samples[512], m_fft[512];
		short m_samplesTempBuffer[1024];

		ObjectManagerItem* m_saveObject;

		bool m_initRowSize;

		int m_dialogActionType; // 0 -> from texture (byte), 1 -> from texture (float), 2 -> from model, 3 -> raw data

		// tools
		CubemapPreview m_cubePrev;
		Texture3DPreview m_tex3DPrev;

		// for each item opened
		int m_curHoveredItem;
		std::vector<Magnifier> m_zoom;
		std::vector<glm::vec2> m_lastRTSize;
		std::vector<GLuint> m_zoomColor, m_zoomDepth, m_zoomFBO;
		void m_renderZoom(int ind, glm::vec2 itemSize);
	};
}