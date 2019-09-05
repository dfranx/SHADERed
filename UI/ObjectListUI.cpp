#include "ObjectListUI.h"
#include "PropertyUI.h"
#include "../Objects/Settings.h"
#include "../Objects/Logger.h"
#include "../Engine/GeometryFactory.h"
#include "../Engine/GLUtils.h"
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#define IMAGE_CONTEXT_WIDTH 150 * Settings::Instance().DPIScale

#define CUBEMAP_TEX_WIDTH 152.0f
#define CUBEMAP_TEX_HEIGHT 114.0f

const char* CUBEMAP_VS_CODE = R"(
#version 330

layout (location = 0) in vec3 iPos;
layout (location = 1) in vec3 iNormal;
layout (location = 2) in vec2 iUV;

uniform mat4 uMatWVP;
out vec2 uv;

void main() {
	gl_Position = uMatWVP * vec4(iPos, 1.0f);
	uv = iUV;
}
)";

/* https://stackoverflow.com/questions/54101329/project-cubemap-to-2d-texture */
const char* CUBEMAP_PS_CODE = R"(
#version 330

uniform samplerCube cubemap;

in vec2 uv;
out vec4 fragColor;

void main()
{
    vec2 localUV = uv;
	
	localUV.y = mod(localUV.y*3,1);
    localUV.x = mod(localUV.x*4,1);

	vec3 dir = vec3(1.0, 0.0, 0.0);

    if (uv.x*4>1 && uv.x*4<2)
    {
        if (uv.y * 3.f < 1)		// -y
            dir = vec3(localUV.x*2-1,1,localUV.y*2-1);
        else if (uv.y * 3.f > 2)// +y
            dir = vec3(localUV.x*2-1,-1,-localUV.y*2+1);
        else					// -z
            dir = vec3(localUV.x*2-1,-localUV.y*2+1,1);
    }
    else if (uv.y * 3.f > 1 && uv.y * 3.f < 2)
    {
        if (uv.x*4.f < 1)		// -x
            dir=vec3(-1,-localUV.y*2+1,localUV.x*2-1);
        else if (uv.x*4.f < 3)	// +x
            dir=vec3(1,-localUV.y*2+1,-localUV.x*2+1);
        else 					// +z
            dir=vec3(-localUV.x*2+1,-localUV.y*2+1,-1);
    }
    else discard;

    fragColor = texture(cubemap, dir);
}
)";

namespace ed
{
	void ObjectListUI::m_setupCubemapPreview()
	{
		Logger::Get().Log("Setting up cubemap preview system...");

		GLint success = 0;
		char infoLog[512];

		// create vertex shader
		unsigned int cubemapVS = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(cubemapVS, 1, &CUBEMAP_VS_CODE, nullptr);
		glCompileShader(cubemapVS);
		glGetShaderiv(cubemapVS, GL_COMPILE_STATUS, &success);
		if(!success) {
			glGetShaderInfoLog(cubemapVS, 512, NULL, infoLog);
			ed::Logger::Get().Log("Failed to compile cubemap projection vertex shader", true);
			ed::Logger::Get().Log(infoLog, true);
		}

		// create pixel shader
		unsigned int cubemapPS = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(cubemapPS, 1, &CUBEMAP_PS_CODE, nullptr);
		glCompileShader(cubemapPS);
		glGetShaderiv(cubemapPS, GL_COMPILE_STATUS, &success);
		if(!success) {
			glGetShaderInfoLog(cubemapPS, 512, NULL, infoLog);
			ed::Logger::Get().Log("Failed to compile cubemap projection pixel shader", true);
			ed::Logger::Get().Log(infoLog, true);
		}

		// create a shader program for cubemap preview
		m_cubeShader = glCreateProgram();
		glAttachShader(m_cubeShader, cubemapVS);
		glAttachShader(m_cubeShader, cubemapPS);
		glLinkProgram(m_cubeShader);
		glGetProgramiv(m_cubeShader, GL_LINK_STATUS, &success);
		if(!success) {
			glGetProgramInfoLog(m_cubeShader, 512, NULL, infoLog);
			ed::Logger::Get().Log("Failed to create a cubemap projection shader program", true);
			ed::Logger::Get().Log(infoLog, true);
		}

		glDeleteShader(cubemapVS);
		glDeleteShader(cubemapPS);

		m_uMatWVPLoc = glGetUniformLocation(m_cubeShader, "uMatWVP");
		glUniform1i(glGetUniformLocation(m_cubeShader, "cubemap"), 0);

		m_fsVAO = ed::eng::GeometryFactory::CreatePlane(m_fsVBO, CUBEMAP_TEX_WIDTH, CUBEMAP_TEX_HEIGHT);
		m_cubeFBO = gl::CreateSimpleFramebuffer(CUBEMAP_TEX_WIDTH, CUBEMAP_TEX_HEIGHT, m_cubeTex, m_cubeDepth);
		
	}
	void ObjectListUI::m_renderCubemap(GLuint tex)
	{
		// bind fbo and buffers
		glBindFramebuffer(GL_FRAMEBUFFER, m_cubeFBO);
		static const GLuint fboBuffers[] = { GL_COLOR_ATTACHMENT0 };
		glDrawBuffers(1, fboBuffers);
		glClearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 0);
		glClearBufferfv(GL_COLOR, 0, glm::value_ptr(glm::vec4(0.0f, 0.0f, 0.0f, 0.0f)));
		glViewport(0, 0, CUBEMAP_TEX_WIDTH, CUBEMAP_TEX_HEIGHT);
		glUseProgram(m_cubeShader);

		glm::mat4 matVP = glm::ortho(0.0f, CUBEMAP_TEX_WIDTH, CUBEMAP_TEX_HEIGHT, 0.0f);
		glm::mat4 matW = glm::translate(glm::mat4(1.0f), glm::vec3(CUBEMAP_TEX_WIDTH/2, CUBEMAP_TEX_HEIGHT/2, 0.0f));
		glUniformMatrix4fv(m_uMatWVPLoc, 1, GL_FALSE, glm::value_ptr(matVP * matW));

		// bind shader resource views
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_CUBE_MAP, tex);

		glBindVertexArray(m_fsVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);
	}
	void ObjectListUI::OnEvent(const SDL_Event & e)
	{
	}
	void ObjectListUI::Update(float delta)
	{
		ImVec2 containerSize = ImVec2(ImGui::GetWindowContentRegionWidth(), abs(ImGui::GetWindowContentRegionMax().y - ImGui::GetWindowContentRegionMin().y));
		bool itemMenuOpened = false;
		std::vector<std::string> items = m_data->Objects.GetObjects();
		std::vector<PipelineItem*> passes = m_data->Pipeline.GetList();

		ImGui::BeginChild("##object_scroll_container", containerSize);

		if (items.size() == 0)
			ImGui::TextWrapped("Right click on this window or go to Create menu in the menu bar to create an item.");

		for (int i = 0; i < items.size(); i++) {
			GLuint tex = m_data->Objects.GetTexture(items[i]);

			float imgWH = 0.0f;
			if (m_data->Objects.IsRenderTexture(items[i])) {
				glm::ivec2 rtSize = m_data->Objects.GetRenderTextureSize(items[i]);
				imgWH = (float)rtSize.y / rtSize.x;
			}
			else if (m_data->Objects.IsAudio(items[i]))
				imgWH = 2.0f / 512.0f;
			else if (m_data->Objects.IsCubeMap(items[i]))
				imgWH = 375.0f / 512.0f;
			else {
				auto img = m_data->Objects.GetImageSize(items[i]);
				imgWH = (float)img.second / img.first;
			}

			size_t lastSlash = items[i].find_last_of("/\\");
			std::string itemText = items[i];

			if (lastSlash != std::string::npos)
				itemText = itemText.substr(lastSlash + 1);

			ImGui::Selectable(itemText.c_str());

			if (ImGui::BeginPopupContextItem(std::string("##context" + items[i]).c_str())) {
				itemMenuOpened = true;
				ImGui::Text("Preview");
				if (m_data->Objects.IsCubeMap(items[i])) {
					m_renderCubemap(tex);
					ImGui::Image((void*)(intptr_t)m_cubeTex, ImVec2(IMAGE_CONTEXT_WIDTH, ((float)imgWH)* IMAGE_CONTEXT_WIDTH), ImVec2(0,1), ImVec2(1,0));
				} else
					ImGui::Image((void*)(intptr_t)tex, ImVec2(IMAGE_CONTEXT_WIDTH, ((float)imgWH)* IMAGE_CONTEXT_WIDTH), ImVec2(0,1), ImVec2(1,0));

				ImGui::Separator();


				if (ImGui::BeginMenu("Bind")) {
					for (int j = 0; j < passes.size(); j++) {
						int boundID = m_data->Objects.IsBound(items[i], passes[j]);
						size_t boundItemCount = m_data->Objects.GetBindList(passes[j]).size();
						bool isBound = boundID != -1;
						if (ImGui::MenuItem(passes[j]->Name, ("(" + std::to_string(boundID == -1 ? boundItemCount : boundID) + ")").c_str(), isBound)) {
							if (!isBound)
								m_data->Objects.Bind(items[i], passes[j]);
							else
								m_data->Objects.Unbind(items[i], passes[j]);
						}
					}
					ImGui::EndMenu();
				}

				if (m_data->Objects.IsRenderTexture(items[i])) {
					if (ImGui::Selectable("Properties")) {
						((ed::PropertyUI*)m_ui->Get(ViewID::Properties))->Open(items[i], m_data->Objects.GetRenderTexture(tex));
					}
				}

				if (m_data->Objects.IsAudio(items[i])) {
					bool isMuted = m_data->Objects.IsAudioMuted(items[i]);
					if (ImGui::MenuItem("Mute", (const char*)0, &isMuted)) {
						if (isMuted)
							m_data->Objects.Mute(items[i]);
						else
							m_data->Objects.Unmute(items[i]);
					}
				}

				if (ImGui::Selectable("Delete")) {
					if (m_data->Objects.IsRenderTexture(items[i])) {
						auto& passes = m_data->Pipeline.GetList();
						for (int j = 0; j < passes.size(); j++) {
							pipe::ShaderPass* sData = (pipe::ShaderPass*)passes[j]->Data;

							// check if shader pass uses this rt
							for (int k = 0; k < MAX_RENDER_TEXTURES; k++) {
								if (tex == sData->RenderTextures[k]) {
									// TODO: maybe implement better logic for what to replace the deleted RT with
									bool alreadyUsesWindow = false;
									for (int l = 0; l < MAX_RENDER_TEXTURES; l++)
										if (sData->RenderTextures[l] == m_data->Renderer.GetTexture()) {
											alreadyUsesWindow = true;
											break;
										}

									if (!alreadyUsesWindow)
										sData->RenderTextures[k] = m_data->Renderer.GetTexture();
									else if (k != 0) {
										for (int l = j; l < MAX_RENDER_TEXTURES; l++)
											sData->RenderTextures[l] = 0;
									}
									else {
										for (int l = 0; l < MAX_RENDER_TEXTURES; l++)
											sData->RenderTextures[l] = 0;
										sData->RenderTextures[0] = m_data->Renderer.GetTexture();
									}
								}
							}
						}
					}

					((PropertyUI*)m_ui->Get(ViewID::Properties))->Open(nullptr); // TODO: test this, deleting RT while having sth opened in properties
					m_data->Objects.Remove(items[i]);
				}

				ImGui::EndPopup();
			}
		}

		ImGui::EndChild();

		if (!itemMenuOpened && ImGui::BeginPopupContextItem("##context_main_objects")) {
			if (ImGui::Selectable("Create Texture")) { m_ui->CreateNewTexture(); }
			if (ImGui::Selectable("Create Cubemap")) { m_ui->CreateNewCubemap(); }
			if (ImGui::Selectable("Create Render Texture")) { m_ui->CreateNewRenderTexture(); }
			if (ImGui::Selectable("Create Audio")) { m_ui->CreateNewAudio(); }

			ImGui::EndPopup();
		}
	}
}