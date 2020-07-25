#pragma once
#include <SHADERed/Engine/GLUtils.h>
#include <SHADERed/Objects/PipelineItem.h>
#include <SHADERed/UI/Tools/CubemapPreview.h>
#include <SHADERed/UI/Tools/Magnifier.h>
#include <SHADERed/UI/UIView.h>

namespace ed {
	class ObjectPreviewUI : public UIView {
	public:
		ObjectPreviewUI(GUIManager* ui, ed::InterfaceManager* objects, const std::string& name = "", bool visible = true)
				: UIView(ui, objects, name, visible)
		{
			m_cubePrev.Init(256, 192);
			m_curHoveredItem = -1;
			m_initRowSize = false;
		}
		~ObjectPreviewUI() { }

		virtual void OnEvent(const SDL_Event& e);
		virtual void Update(float delta);

		void Open(const std::string& name, float w, float h, unsigned int item, bool isCube = false, void* rt = nullptr, void* audio = nullptr, void* buffer = nullptr, void* plugin = nullptr);

		inline bool ShouldRun() { return m_items.size() > 0; }
		inline void CloseAll() { m_items.clear(); }
		void Close(const std::string& name);

	protected:
		struct mItem {
			std::string Name;
			float Width, Height;
			unsigned int Texture;
			bool IsCube;
			bool IsOpen;

			void* RT;

			void* Audio;

			void* Buffer;
			std::vector<ShaderVariable::ValueType> CachedFormat;
			int CachedSize;

			void* Plugin;
		};

	private:
		eng::Timer m_bufUpdateClock;
		bool m_drawBufferElement(int row, int col, void* data, ShaderVariable::ValueType type);
		std::vector<mItem> m_items;
		ed::AudioAnalyzer m_audioAnalyzer;
		float m_samples[512], m_fft[512];

		std::string m_saveObject;

		bool m_initRowSize;

		int m_dialogActionType; // 0 -> from texture (byte), 1 -> from texture (float), 2 -> from model, 3 -> raw data

		// tools
		CubemapPreview m_cubePrev;

		// for each item opened
		int m_curHoveredItem;
		std::vector<Magnifier> m_zoom;
		std::vector<glm::vec2> m_lastRTSize;
		std::vector<GLuint> m_zoomColor, m_zoomDepth, m_zoomFBO;
		void m_renderZoom(int ind, glm::vec2 itemSize);
	};
}