#include <SHADERed/UI/FrameAnalysisUI.h>
#include <SHADERed/UI/PixelInspectUI.h>

namespace ed {
	void FrameAnalysisUI::Process()
	{
		for (int i = 0; i < 256; i++) {
			m_histogramR[i] = m_histogramG[i] = m_histogramB[i] = 0.0f;
			m_histogramRGB[i] = 0.0f;
		}

		// get texture data
		glm::ivec2 rendererSize = m_data->Renderer.GetLastRenderSize();
		uint8_t* pixels = (uint8_t*)malloc(rendererSize.x * rendererSize.y * 4);
		glBindTexture(GL_TEXTURE_2D, m_data->Renderer.GetTexture());
		glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
		glBindTexture(GL_TEXTURE_2D, 0);

		// build histogram
		for (int x = 0; x < rendererSize.x; x++) {
			for (int y = 0; y < rendererSize.y; y++) {
				uint8_t* px = &pixels[(y * rendererSize.x + x) * 4];

				m_histogramR[px[0]]++;
				m_histogramG[px[1]]++;
				m_histogramB[px[2]]++;

				// TODO: is this even how RGB histogram works? I just did this by my logic idk tho, compare with Photoshop 
				m_histogramRGB[px[0]]++;
				m_histogramRGB[px[1]]++;
				m_histogramRGB[px[2]]++;
			}
		}

		// free memory
		free(pixels);
	}
	void FrameAnalysisUI::OnEvent(const SDL_Event& e)
	{
	}
	void FrameAnalysisUI::Update(float delta)
	{
		ImGui::TextWrapped("Color histogram");
		ImGui::Separator();
		ImGui::PushItemWidth(-1);
		ImGui::Combo("##histogram_combo", &m_histogramType, "RGB\0R\0G\0B\0");
		ImGui::PopItemWidth();

		const float* histogramData = m_histogramRGB;
		if (m_histogramType == 1)
			histogramData = m_histogramR;
		else if (m_histogramType == 2)
			histogramData = m_histogramG;
		else if (m_histogramType == 3)
			histogramData = m_histogramB;

		ImGui::PlotHistogram("##testhistogram", histogramData, 256, 0, 0, 0.0f, FLT_MAX, ImVec2(ImGui::GetContentRegionAvailWidth(), 256));
		ImGui::NewLine();

		ImGui::TextWrapped("Miscellaneous");
		ImGui::Separator();
		ImGui::TextWrapped("%u pixels rendered", m_data->Analysis.GetPixelCount());
		ImGui::TextWrapped("%u pixels discarded with \"discard;\"", m_data->Analysis.GetPixelsDiscarded());
		ImGui::TextWrapped("%u pixels discarded due to depth test", m_data->Analysis.GetPixelsFailedDepthTest());
		ImGui::TextWrapped("%u pixels have undefined behavior", m_data->Analysis.GetPixelsUndefinedBehavior());
		ImGui::TextWrapped("Average instruction count per pixel: %u", m_data->Analysis.GetInstructionCountAverage());
		ImGui::NewLine();
		ImGui::TextWrapped("%u triangles", m_data->Analysis.GetTriangleCount());
		ImGui::TextWrapped("%u triangles discarded", m_data->Analysis.GetTrianglesDiscarded());
		ImGui::NewLine();

		ImGui::TextWrapped("Pixel history");
		ImGui::Separator();
		std::vector<PixelInformation>& pixels = m_data->Debugger.GetPixelList();
		if (pixels.size() != m_pixelHeights.size())
			m_pixelHeights.resize(pixels.size());
		int pixelHistoryCount = 0;
		for (auto& pixel : pixels)
			pixelHistoryCount += pixel.History;
		if (pixelHistoryCount > 0) {
			int pxId = 0;
			for (auto& pixel : pixels) {
				if (!pixel.History)
					continue;

				ImGui::PushID(pxId);

				((PixelInspectUI*)m_ui->Get(ViewID::PixelInspect))->RenderPixelInfo(pixel, m_pixelHeights[pxId]);

				ImGui::PopID();
				pxId++;
			}
		} else
			ImGui::TextWrapped("No pixel history");
		ImGui::NewLine();
	}
}