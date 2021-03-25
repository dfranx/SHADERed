#include <SHADERed/UI/BrowseOnlineUI.h>
#include <SHADERed/UI/OptionsUI.h>
#include <SHADERed/UI/CodeEditorUI.h>
#include <SHADERed/UI/UIHelper.h>
#include <SHADERed/Objects/Logger.h>
#include <SHADERed/Objects/Settings.h>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <misc/stb_image.h>

namespace ed {
	BrowseOnlineUI::BrowseOnlineUI(GUIManager* ui, ed::InterfaceManager* objects, const std::string& name, bool visible)
			: UIView(ui, objects, name, visible)
	{
		memset(&m_onlineQuery, 0, sizeof(m_onlineQuery));
		memset(&m_onlineUsername, 0, sizeof(m_onlineUsername));
		m_onlinePage = 0;
		m_onlineShaderPage = 0;
		m_onlinePluginPage = 0;
		m_onlineThemePage = 0;
		m_onlineIsShader = true;
		m_onlineIsPlugin = false;
		m_onlineExcludeGodot = false;
		m_onlineIncludeCPP = false;
		m_onlineIncludeRust = false;
		m_refreshPluginThumbnails = false;
		m_refreshShaderThumbnails = false;
		m_refreshThemeThumbnails = false;
		m_queueThumbnailPixelData = nullptr;
		m_queueThumbnailWidth = m_queueThumbnailHeight = 0;
		m_queueThumbnailNext = false;
		m_queueThumbnailProcess = false;
		m_queueThumbnailID = 0;
	}

	void BrowseOnlineUI::OnEvent(const SDL_Event& e) {
		// WARNING: this method is never called
	}
	void BrowseOnlineUI::Update(float delta) {
		// load thumbnails 1 by 1 in a seperate thread
		if (m_refreshShaderThumbnails) {
			if (m_queueThumbnailProcess) {
				GLuint thumbnailTex = 0;

				if (m_queueThumbnailPixelData) {

					glGenTextures(1, &thumbnailTex);
					glBindTexture(GL_TEXTURE_2D, thumbnailTex);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
					glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_queueThumbnailWidth, m_queueThumbnailHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, m_queueThumbnailPixelData);
					glBindTexture(GL_TEXTURE_2D, 0);

					free(m_queueThumbnailPixelData);
				}

				m_onlineShaderThumbnail.push_back(thumbnailTex);

				if (m_onlineShaderThumbnail.size() < std::min<int>(12, m_onlineShaders.size()))
					m_queueThumbnailNext = true;
				else
					m_refreshShaderThumbnails = false;
				m_queueThumbnailID = std::min<int>(12, m_queueThumbnailID + 1);
				m_queueThumbnailProcess = false;
			}

			if (m_queueThumbnailNext) {
				m_data->API.AllocateShaderThumbnail(m_onlineShaders[m_queueThumbnailID].ID, [&](unsigned char* data, int width, int height) {
					m_queueThumbnailHeight = height;
					m_queueThumbnailWidth = width;
					m_queueThumbnailPixelData = data;
					m_queueThumbnailProcess = true;
				});
				m_queueThumbnailNext = false;
			}
		}

		int startY = ImGui::GetCursorPosY();
		ImGui::Text("Query:");
		ImGui::SameLine();
		ImGui::SetCursorPosX(Settings::Instance().CalculateSize(75));
		ImGui::PushItemWidth(-Settings::Instance().CalculateSize(110));
		ImGui::InputText("##online_query", m_onlineQuery, sizeof(m_onlineQuery));
		ImGui::PopItemWidth();
		int startX = ImGui::GetCursorPosX();

		ImGui::Text("By:");
		ImGui::SameLine();
		ImGui::SetCursorPosX(Settings::Instance().CalculateSize(75));
		ImGui::PushItemWidth(-Settings::Instance().CalculateSize(110));
		ImGui::InputText("##online_username", m_onlineUsername, sizeof(m_onlineUsername));
		ImGui::PopItemWidth();
		int endY = ImGui::GetCursorPosY();

		ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth() - Settings::Instance().CalculateSize(90), startY));
		if (ImGui::Button("SEARCH", ImVec2(Settings::Instance().CalculateSize(70), endY - startY))) {
			if (m_onlineIsShader)
				m_onlineShaderPage = 0;
			else if (m_onlineIsPlugin)
				m_onlinePluginPage = 0;
			else
				m_onlineThemePage = 0;

			if (m_onlineIsShader)
				m_onlineSearchShaders();
			else if (m_onlineIsPlugin)
				m_onlineSearchPlugins();
			else
				m_onlineSearchThemes();
		}

		bool hasNext = true;
		bool hasPrevious = m_onlinePage > 0;

		if (ImGui::BeginTabBar("BrowseOnlineTabBar")) {
			if (ImGui::BeginTabItem("Shaders")) {
				m_onlineIsShader = true;
				m_onlineIsPlugin = false;
				m_onlinePage = m_onlineShaderPage;
				hasNext = m_onlineShaders.size() > 12;

				ImGui::BeginChild("##online_shader_container", ImVec2(0, Settings::Instance().CalculateSize(-60)));

				if (ImGui::BeginTable("##shaders_table", 2, ImGuiTableFlags_None)) {
					ImGui::TableSetupColumn("Thumbnail", ImGuiTableColumnFlags_WidthAlwaysAutoResize);
					ImGui::TableSetupColumn("Info", ImGuiTableColumnFlags_WidthStretch);
					ImGui::TableAutoHeaders();

					for (int i = 0; i < std::min<int>(12, m_onlineShaders.size()); i++) {
						const ed::WebAPI::ShaderResult& shaderInfo = m_onlineShaders[i];

						ImGui::TableNextRow();

						ImGui::TableSetColumnIndex(0);
						if (i < m_onlineShaderThumbnail.size())
							ImGui::Image((ImTextureID)m_onlineShaderThumbnail[i], ImVec2(256, 144), ImVec2(0, 1), ImVec2(1, 0));
						else {
							ImVec2 spinnerPos = ImGui::GetCursorPos();
							ImGui::Dummy(ImVec2(256, 144));
							ImGui::SetCursorPos(ImVec2(spinnerPos.x + 256 / 2 - 32, spinnerPos.y + 144 / 2 - 32));
							UIHelper::Spinner("##spinner", 32.0f, 6, ImGui::GetColorU32(ImGuiCol_Text));
						}

						ImGui::TableSetColumnIndex(1);
						ImGui::Text("%s", shaderInfo.Title.c_str()); // just in case someone has a %s or sth in the title, so that the app doesn't crash :'D
						ImGui::Text("Language: %s", shaderInfo.Language.c_str());
						ImGui::Text("%d view(s)", shaderInfo.Views);
						ImGui::Text("by: %s", shaderInfo.Owner.c_str());

						ImGui::PushID(i);
						if (ImGui::Button("OPEN")) {
							CodeEditorUI* codeUI = ((CodeEditorUI*)m_ui->Get(ViewID::Code));
							codeUI->SetTrackFileChanges(false);

							bool ret = m_data->API.DownloadShaderProject(shaderInfo.ID);
							if (ret) {
								std::string outputPath = Settings::Instance().ConvertPath("temp/");

								m_ui->Open(outputPath + "project.sprj");
								m_data->Parser.SetOpenedFile("");
								ImGui::CloseCurrentPopup();
							}

							codeUI->SetTrackFileChanges(Settings::Instance().General.RecompileOnFileChange);
						}
						ImGui::PopID();
					}

					ImGui::EndTable();
				}

				ImGui::EndChild();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Plugins")) {
				// load plugin thumbnails
				if (m_refreshPluginThumbnails) {
					int minSize = std::min<int>(12, m_onlinePlugins.size());
					m_onlinePluginThumbnail.resize(minSize);

					size_t dataLen = 0;
					for (int i = 0; i < minSize; i++) {
						char* data = m_data->API.DecodeThumbnail(m_onlinePlugins[i].Thumbnail, dataLen);
						if (data == nullptr) {
							Logger::Get().Log("Failed to load a texture thumbnail for " + m_onlinePlugins[i].ID, true);
							continue;
						}
						m_onlinePlugins[i].Thumbnail.clear(); // since they are pretty large, no need to keep them in memory

						int width, height, nrChannels;
						unsigned char* pixelData = stbi_load_from_memory((stbi_uc*)data, dataLen, &width, &height, &nrChannels, STBI_rgb_alpha);

						if (pixelData == nullptr) {
							Logger::Get().Log("Failed to load a texture thumbnail for " + m_onlinePlugins[i].ID, true);
							free(data);
							continue;
						}

						// normal texture
						glGenTextures(1, &m_onlinePluginThumbnail[i]);
						glBindTexture(GL_TEXTURE_2D, m_onlinePluginThumbnail[i]);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
						glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixelData);
						glBindTexture(GL_TEXTURE_2D, 0);

						free(data);
					}

					m_refreshPluginThumbnails = false;
				}

				m_onlineIsShader = false;
				m_onlineIsPlugin = true;
				m_onlinePage = m_onlinePluginPage;
				hasNext = m_onlinePlugins.size() > 12;

				ImGui::BeginChild("##online_plugin_container", ImVec2(0, Settings::Instance().CalculateSize(-60)));

				if (ImGui::BeginTable("##plugins_table", 2, ImGuiTableFlags_None)) {
					ImGui::TableSetupColumn("Thumbnail", ImGuiTableColumnFlags_WidthAlwaysAutoResize);
					ImGui::TableSetupColumn("Info", ImGuiTableColumnFlags_WidthStretch);
					ImGui::TableAutoHeaders();

					for (int i = 0; i < std::min<int>(12, m_onlinePlugins.size()); i++) {
						const ed::WebAPI::PluginResult& pluginInfo = m_onlinePlugins[i];

						ImGui::TableNextRow();

						ImGui::TableSetColumnIndex(0);
						ImGui::Image((ImTextureID)m_onlinePluginThumbnail[i], ImVec2(256, 144), ImVec2(0, 1), ImVec2(1, 0));

						ImGui::TableSetColumnIndex(1);
						ImGui::Text("%s", pluginInfo.Title.c_str()); // just in case someone has a %s or sth in the title, so that the app doesn't crash :'D
						ImGui::TextWrapped("%s", pluginInfo.Description.c_str());
						ImGui::Text("%d download(s)", pluginInfo.Downloads);
						ImGui::Text("by: %s", pluginInfo.Owner.c_str());

						bool isInstalled = !(m_data->Plugins.GetPlugin(pluginInfo.ID) == nullptr && std::count(m_onlineInstalledPlugins.begin(), m_onlineInstalledPlugins.end(), pluginInfo.ID) == 0);

						if (!isInstalled) {
							std::string pluginLower = pluginInfo.ID;
							std::transform(pluginLower.begin(), pluginLower.end(), pluginLower.begin(), ::tolower);

							const std::vector<std::string>& notLoaded = Settings::Instance().Plugins.NotLoaded;

							for (int j = 0; j < notLoaded.size(); j++) {
								std::string nameLower = notLoaded[j];
								std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);

								if (pluginLower == nameLower) {
									isInstalled = true;
									break;
								}
							}
						}

						if (isInstalled) {
							if (std::count(m_onlineInstalledPlugins.begin(), m_onlineInstalledPlugins.end(), pluginInfo.ID) > 0)
								ImGui::TextWrapped("Plugin installed! Restart SHADERed.");
							else
								ImGui::TextWrapped("Plugin already installed!");
						} else {
							ImGui::PushID(i);
							if (ImGui::Button("DOWNLOAD")) {
								m_onlineInstalledPlugins.push_back(pluginInfo.ID); // since PluginManager's GetPlugin won't register it immediately
								m_data->API.DownloadPlugin(pluginInfo.ID);
							}
							ImGui::PopID();
						}
					}

					ImGui::EndTable();
				}

				ImGui::EndChild();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Themes")) {
				// load theme thumbnails
				if (m_refreshThemeThumbnails) {
					int minSize = std::min<int>(12, m_onlineThemes.size());

					m_onlineThemeThumbnail.resize(minSize);

					size_t dataLen = 0;
					for (int i = 0; i < minSize; i++) {
						char* data = m_data->API.DecodeThumbnail(m_onlineThemes[i].Thumbnail, dataLen);
						if (data == nullptr) {
							Logger::Get().Log("Failed to load a texture thumbnail for " + m_onlineThemes[i].ID, true);
							continue;
						}
						m_onlineThemes[i].Thumbnail.clear(); // since they are pretty large, no need to keep them in memory

						int width, height, nrChannels;
						unsigned char* pixelData = stbi_load_from_memory((stbi_uc*)data, dataLen, &width, &height, &nrChannels, STBI_rgb_alpha);

						if (pixelData == nullptr) {
							Logger::Get().Log("Failed to load a texture thumbnail for " + m_onlineThemes[i].ID, true);
							free(data);
							continue;
						}

						// normal texture
						glGenTextures(1, &m_onlineThemeThumbnail[i]);
						glBindTexture(GL_TEXTURE_2D, m_onlineThemeThumbnail[i]);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
						glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixelData);
						glBindTexture(GL_TEXTURE_2D, 0);

						free(data);
					}

					m_refreshThemeThumbnails = false;
				}

				m_onlineIsShader = false;
				m_onlineIsPlugin = false;
				m_onlinePage = m_onlineThemePage;

				hasNext = m_onlineThemes.size() > 12;

				ImGui::BeginChild("##online_theme_container", ImVec2(0, Settings::Instance().CalculateSize(-60)));

				if (ImGui::BeginTable("##themes_table", 2, ImGuiTableFlags_None)) {
					ImGui::TableSetupColumn("Thumbnail", ImGuiTableColumnFlags_WidthAlwaysAutoResize);
					ImGui::TableSetupColumn("Info", ImGuiTableColumnFlags_WidthStretch);
					ImGui::TableAutoHeaders();

					for (int i = 0; i < std::min<int>(12, m_onlineThemes.size()); i++) {
						const ed::WebAPI::ThemeResult& themeInfo = m_onlineThemes[i];

						ImGui::TableNextRow();

						ImGui::TableSetColumnIndex(0);
						ImGui::Image((ImTextureID)m_onlineThemeThumbnail[i], ImVec2(256, 144), ImVec2(0, 1), ImVec2(1, 0));

						ImGui::TableSetColumnIndex(1);
						ImGui::TextUnformatted(themeInfo.Title.c_str());
						ImGui::TextWrapped("%s", themeInfo.Description.c_str());
						ImGui::Text("%d download(s)", themeInfo.Downloads);
						ImGui::Text("by: %s", themeInfo.Owner.c_str());

						const auto& tList = ((ed::OptionsUI*)m_ui->Get(ViewID::Options))->GetThemeList();

						if (std::count(tList.begin(), tList.end(), themeInfo.Title) == 0) {
							ImGui::PushID(i);
							if (ImGui::Button("DOWNLOAD")) {
								m_data->API.DownloadTheme(themeInfo.ID);
								((ed::OptionsUI*)m_ui->Get(ViewID::Options))->RefreshThemeList();
							}
							ImGui::PopID();
						} else {
							ImGui::PushID(i);
							if (ImGui::Button("APPLY")) {
								ed::Settings::Instance().Theme = themeInfo.Title;
								((ed::OptionsUI*)m_ui->Get(ViewID::Options))->ApplyTheme();
							}
							ImGui::PopID();
						}
					}

					ImGui::EndTable();
				}

				ImGui::EndChild();
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}

		ImGui::SetCursorPosX(ImGui::GetWindowWidth() - Settings::Instance().CalculateSize(180));
		if (!hasPrevious) {
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
		}
		if (ImGui::Button("<<", ImVec2(Settings::Instance().CalculateSize(70), 0))) {
			m_onlinePage = std::max<int>(0, m_onlinePage - 1);

			if (m_onlineIsShader)
				m_onlineShaderPage = m_onlinePage;
			else if (m_onlineIsPlugin)
				m_onlinePluginPage = m_onlinePage;
			else
				m_onlineThemePage = m_onlinePage;

			m_onlineSearchShaders();
		}
		if (!hasPrevious) {
			ImGui::PopStyleVar();
			ImGui::PopItemFlag();
		}
		ImGui::SameLine();
		ImGui::Text("%d", m_onlinePage + 1);
		ImGui::SameLine();
		ImGui::SetCursorPosX(ImGui::GetWindowWidth() - Settings::Instance().CalculateSize(90));

		if (!hasNext) {
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
		}
		if (ImGui::Button(">>", ImVec2(Settings::Instance().CalculateSize(70), 0))) {
			m_onlinePage++;

			if (m_onlineIsShader)
				m_onlineShaderPage = m_onlinePage;
			else if (m_onlineIsPlugin)
				m_onlinePluginPage = m_onlinePage;
			else
				m_onlineThemePage = m_onlinePage;

			m_onlineSearchShaders();
		}
		if (!hasNext) {
			ImGui::PopStyleVar();
			ImGui::PopItemFlag();
		}
	}

	void BrowseOnlineUI::Open()
	{
		// check if godot shaders should be excluded from the "Browse online" window
		m_onlineExcludeGodot = m_data->Plugins.GetPlugin("GodotShaders") == nullptr;
		m_onlineIncludeCPP = m_data->Plugins.GetPlugin("CPP") != nullptr;
		m_onlineIncludeRust = m_data->Plugins.GetPlugin("Rust") != nullptr;

		memset(&m_onlineQuery, 0, sizeof(m_onlineQuery));
		memset(&m_onlineUsername, 0, sizeof(m_onlineUsername));

		if (m_onlineShaders.size() == 0)
			m_onlineSearchShaders();
		if (m_onlinePlugins.size() == 0)
			m_onlineSearchPlugins();
		if (m_onlineThemes.size() == 0)
			m_onlineSearchThemes();				
	}
	void BrowseOnlineUI::FreeMemory()
	{
		for (int i = 0; i < m_onlineShaderThumbnail.size(); i++)
			glDeleteTextures(1, &m_onlineShaderThumbnail[i]);
		m_onlineShaderThumbnail.clear();

		for (int i = 0; i < m_onlinePluginThumbnail.size(); i++)
			glDeleteTextures(1, &m_onlinePluginThumbnail[i]);
		m_onlinePluginThumbnail.clear();
	}

	void BrowseOnlineUI::m_onlineSearchShaders()
	{
		m_queueThumbnailProcess = false;
		m_queueThumbnailNext = false;
		m_onlineShaders.clear();
		for (int i = 0; i < m_onlineShaderThumbnail.size(); i++)
			glDeleteTextures(1, &m_onlineShaderThumbnail[i]);
		m_onlineShaderThumbnail.clear();
		m_queueThumbnailID = 0;
		m_refreshShaderThumbnails = false;

		m_data->API.SearchShaders(
			m_onlineQuery, m_onlinePage, "hot", "", m_onlineUsername, m_onlineExcludeGodot, m_onlineIncludeCPP, m_onlineIncludeRust, [&](const std::vector<WebAPI::ShaderResult>& shaders) {
				m_onlineShaders = shaders;
				m_refreshShaderThumbnails = true;
				m_queueThumbnailProcess = false;
				m_queueThumbnailNext = true;
			});
	}
	void BrowseOnlineUI::m_onlineSearchPlugins()
	{
		m_onlinePlugins.clear();
		for (int i = 0; i < m_onlinePluginThumbnail.size(); i++)
			glDeleteTextures(1, &m_onlinePluginThumbnail[i]);
		m_onlinePluginThumbnail.clear();
		m_refreshPluginThumbnails = false;

		m_data->API.SearchPlugins(m_onlineQuery, m_onlinePage, "popular", m_onlineUsername, [&](const std::vector<WebAPI::PluginResult>& plugins) {
			m_onlinePlugins = plugins;
			m_refreshPluginThumbnails = true;
		});
	}
	void BrowseOnlineUI::m_onlineSearchThemes()
	{
		m_onlineThemes.clear();
		for (int i = 0; i < m_onlineThemeThumbnail.size(); i++)
			glDeleteTextures(1, &m_onlineThemeThumbnail[i]);
		m_onlineThemeThumbnail.clear();
		m_refreshThemeThumbnails = false;

		m_data->API.SearchThemes(m_onlineQuery, m_onlinePage, "popular", m_onlineUsername, [&](const std::vector<WebAPI::ThemeResult>& themes) {
			m_onlineThemes = themes;
			m_refreshThemeThumbnails = true;
		});
	}
}