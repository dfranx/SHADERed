#include <SHADERed/Engine/GLUtils.h>
#include <SHADERed/Engine/GeometryFactory.h>
#include <SHADERed/Objects/DefaultState.h>
#include <SHADERed/Objects/KeyboardShortcuts.h>
#include <SHADERed/Objects/Logger.h>
#include <SHADERed/Objects/Settings.h>
#include <SHADERed/Objects/SystemVariableManager.h>
#include <SHADERed/Objects/ThemeContainer.h>
#include <SHADERed/UI/Icons.h>
#include <SHADERed/UI/PipelineUI.h>
#include <SHADERed/UI/PreviewUI.h>
#include <SHADERed/UI/PropertyUI.h>
#include <SHADERed/UI/FrameAnalysisUI.h>
#include <SHADERed/UI/Tools/DebuggerOutline.h>

#include <imgui/imgui_internal.h>
#include <chrono>
#include <thread>

#define STATUSBAR_HEIGHT Settings::Instance().CalculateSize(32) + ImGui::GetStyle().FramePadding.y
#define BUTTON_SIZE Settings::Instance().CalculateSize(20)
#define ICON_BUTTON_WIDTH Settings::Instance().CalculateSize(25)
#define BUTTON_INDENT Settings::Instance().CalculateSize(5)
#define FPS_UPDATE_RATE 0.3f
#define BOUNDING_BOX_PADDING 0.01f
#define MAX_PICKED_ITEM_LIST_SIZE 4

/* bounding box shaders */
const char* BOX_VS_CODE = R"(
#version 330

layout (location = 0) in vec3 iPos;

uniform mat4 uMatWVP;

void main() {
	gl_Position = uMatWVP * vec4(iPos, 1.0f);
}
)";
const char* BOX_PS_CODE = R"(
#version 330

uniform vec4 uColor;
out vec4 fragColor;

void main()
{
	fragColor = uColor;
}
)";

namespace ed {
	const char* getViewName(PreviewUI::PreviewView view)
	{
		switch (view) {
		case PreviewUI::PreviewView::Debugger: return "Debugger view";
		case PreviewUI::PreviewView::Heatmap: return "Heatmap";
		case PreviewUI::PreviewView::UndefinedBehavior: return "Undefined behavior";
		case PreviewUI::PreviewView::GlobalBreakpoints: return "Global breakpoints";
		case PreviewUI::PreviewView::VariableValue: return "Variable value";
		default: return "Preview";
		}
		return "Preview";
	}
	const char* getUndefinedBehavior(uint32_t type)
	{
		switch (type) {
		case spvm_undefined_behavior_div_by_zero:
			return "division by zero";
		case spvm_undefined_behavior_mod_by_zero:
			return "mod by zero";
		case spvm_undefined_behavior_image_read_out_of_bounds:
			return "image read out of bounds";
		case spvm_undefined_behavior_image_write_out_of_bounds:
			return "image write out of bounds";
		case spvm_undefined_behavior_vector_extract_dynamic:
			return "vector component out of bounds";
		case spvm_undefined_behavior_vector_insert_dynamic:
			return "vector component out of bounds";
		case spvm_undefined_behavior_asin:
			return "asin(x), |x| > 1";
		case spvm_undefined_behavior_acos:
			return "acos(x), |x| > 1";
		case spvm_undefined_behavior_acosh:
			return "acosh(x), x < 1";
		case spvm_undefined_behavior_atanh:
			return "atanh(x), |x| >= 1";
		case spvm_undefined_behavior_atan2:
			return "atan2(y, x), x = y = 0";
		case spvm_undefined_behavior_pow:
			return "pow(x, y), x < 0 or (x = 0 and y <= 0)";
		case spvm_undefined_behavior_log:
			return "log(x), x <= 0";
		case spvm_undefined_behavior_log2:
			return "log2(x), x <= 0";
		case spvm_undefined_behavior_sqrt:
			return "sqrt(x), x < 0";
		case spvm_undefined_behavior_inverse_sqrt:
			return "invsqrt(x), x <= 0";
		case spvm_undefined_behavior_fmin:
			return "min(x, y), x or y is NaN";
		case spvm_undefined_behavior_fmax:
			return "max(x, y), x or y is NaN";
		case spvm_undefined_behavior_clamp:
			return "clamp(x, minVal, maxVal), minVal > maxVal";
		case spvm_undefined_behavior_smoothstep:
			return "smoothstep(edge0, edge1, x), edge0 >= edge1";
		case spvm_undefined_behavior_frexp:
			return "frexp(x, out exp), x is NaN or inf";
		case spvm_undefined_behavior_ldexp:
			return "ldexp(x, exp), exp > 128 (float) or exp > 1024 (double)";
		default: return "unknown";
		}
		return "unknown";
	}

	void PreviewUI::m_setupShortcuts()
	{
		KeyboardShortcuts::Instance().SetCallback("Gizmo.Position", [=]() {
			m_pickMode = 0;
			m_gizmo.SetMode(m_pickMode);
		});
		KeyboardShortcuts::Instance().SetCallback("Gizmo.Scale", [=]() {
			m_pickMode = 1;
			m_gizmo.SetMode(m_pickMode);
		});
		KeyboardShortcuts::Instance().SetCallback("Gizmo.Rotation", [=]() {
			m_pickMode = 2;
			m_gizmo.SetMode(m_pickMode);
		});
		KeyboardShortcuts::Instance().SetCallback("Preview.Delete", [=]() {
			if (m_picks.size() != 0 && m_hasFocus) {
				PropertyUI* props = (PropertyUI*)m_ui->Get(ViewID::Properties);
				for (int i = 0; i < m_picks.size(); i++) {
					if (props->CurrentItemName() == m_picks[i]->Name)
						props->Close();
					m_data->Pipeline.Remove(m_picks[i]->Name);
				}
				m_picks.clear();
			}
		});
		KeyboardShortcuts::Instance().SetCallback("Preview.DecreaseTime", [=]() {
			if (!m_data->Renderer.IsPaused())
				return;

			float deltaTime = SystemVariableManager::Instance().GetTimeDelta();

			SystemVariableManager::Instance().AdvanceTimer(-deltaTime);
			SystemVariableManager::Instance().SetFrameIndex(SystemVariableManager::Instance().GetFrameIndex() - 1);

			m_data->Renderer.Render(m_imgSize.x, m_imgSize.y);
		});
		KeyboardShortcuts::Instance().SetCallback("Preview.DecreaseTimeFast", [=]() {
			if (!m_data->Renderer.IsPaused())
				return;

			float deltaTime = SystemVariableManager::Instance().GetTimeDelta();

			SystemVariableManager::Instance().AdvanceTimer(-0.1f);
			SystemVariableManager::Instance().SetFrameIndex(SystemVariableManager::Instance().GetFrameIndex() - 0.1f / deltaTime);

			m_data->Renderer.Render(m_imgSize.x, m_imgSize.y);
		});
		KeyboardShortcuts::Instance().SetCallback("Preview.IncreaseTime", [=]() {
			if (!m_data->Renderer.IsPaused())
				return;

			float deltaTime = SystemVariableManager::Instance().GetTimeDelta();

			SystemVariableManager::Instance().AdvanceTimer(deltaTime); // add one second to timer
			SystemVariableManager::Instance().SetFrameIndex(SystemVariableManager::Instance().GetFrameIndex() + 1);

			m_data->Renderer.Render(m_imgSize.x, m_imgSize.y);
		});
		KeyboardShortcuts::Instance().SetCallback("Preview.IncreaseTimeFast", [=]() {
			if (!m_data->Renderer.IsPaused())
				return;

			float deltaTime = SystemVariableManager::Instance().GetTimeDelta();

			SystemVariableManager::Instance().AdvanceTimer(0.1f);																   // add one second to timer
			SystemVariableManager::Instance().SetFrameIndex(SystemVariableManager::Instance().GetFrameIndex() + 0.1f / deltaTime); // add estimated number of frames

			m_data->Renderer.Render(m_imgSize.x, m_imgSize.y);
		});
		KeyboardShortcuts::Instance().SetCallback("Preview.ResetTime", [=]() {
			SystemVariableManager::Instance().Reset();

			if (m_data->Renderer.IsPaused())
				m_data->Renderer.Render(m_imgSize.x, m_imgSize.y);
		});
		KeyboardShortcuts::Instance().SetCallback("Preview.TogglePause", [=]() {
			m_pause();
		});
		KeyboardShortcuts::Instance().SetCallback("Preview.Unselect", [=]() {
			m_picks.clear();
		});
		KeyboardShortcuts::Instance().SetCallback("Preview.Duplicate", [=]() {
			Duplicate();
		});
		KeyboardShortcuts::Instance().SetCallback("Preview.SelectAll", [=]() {
			// clear the list
			Pick(nullptr);

			// select all geometry and mesh items
			std::vector<PipelineItem*>& pass = m_data->Pipeline.GetList();
			for (int i = 0; i < pass.size(); i++) {
				if (pass[i]->Type != PipelineItem::ItemType::ShaderPass)
					continue;

				ed::pipe::ShaderPass* pdata = (ed::pipe::ShaderPass*)pass[i]->Data;
				for (int j = 0; j < pdata->Items.size(); j++) {
					if (pdata->Items[j]->Type == PipelineItem::ItemType::Geometry || pdata->Items[j]->Type == PipelineItem::ItemType::Model || pdata->Items[j]->Type == PipelineItem::ItemType::VertexBuffer)
						Pick(pdata->Items[j], true);
				}
			}
		});
		KeyboardShortcuts::Instance().SetCallback("Preview.ToggleCursorVisibility", [=]() {
			m_mouseVisible = !m_mouseVisible;
		});
		KeyboardShortcuts::Instance().SetCallback("Preview.ToggleMouseLock", [=]() {
			m_mouseLock = !m_mouseLock;
		});
		KeyboardShortcuts::Instance().SetCallback("Preview.ToggleTimePause", [=]() {
			m_pauseTime = !m_pauseTime;
			if (m_pauseTime)
				SystemVariableManager::Instance().GetTimeClock().Pause();
			else
				SystemVariableManager::Instance().GetTimeClock().Resume();
		});

	}
	void PreviewUI::OnEvent(const SDL_Event& e)
	{
		if (e.type == SDL_MOUSEBUTTONDOWN) {
			const Uint8* keyState = SDL_GetKeyboardState(NULL);
			bool isAltDown = keyState[SDL_SCANCODE_LALT] || keyState[SDL_SCANCODE_RALT];

			m_mouseContact = ImVec2(e.button.x, e.button.y);

			if (!m_data->Renderer.IsPaused() && (e.button.button == SDL_BUTTON_LEFT) && !isAltDown) {
				m_lastButton = glm::vec2(e.button.x, e.button.y);
				m_lastButtonUpdate = true;
			}

			if (isAltDown && m_mouseHovers) {
				if (e.button.button == SDL_BUTTON_LEFT)
					m_zoom.StartMouseAction(true);
				if (e.button.button == SDL_BUTTON_RIGHT)
					m_zoom.StartMouseAction(false);
			}
		} else if (e.type == SDL_MOUSEMOTION) {
			if (Settings::Instance().Preview.Gizmo && m_picks.size() != 0) {
				const glm::vec2& zPos = m_zoom.GetZoomPosition();
				const glm::vec2& zSize = m_zoom.GetZoomSize();

				// screen space position
				glm::vec2 s(zPos.x + zSize.x * m_mousePos.x, zPos.y + zSize.y * m_mousePos.y);

				s.x *= m_lastSize.x;
				s.y *= m_lastSize.y;

				m_gizmo.HandleMouseMove(s.x, s.y, m_lastSize.x, m_lastSize.y);
			}

			m_zoom.Drag();

			// mouse visibility
			if (!m_mouseVisible && m_fullWindowFocus) {
				if (m_mousePos.x >= 0.0f && m_mousePos.x <= 1.0f && m_mousePos.y >= 0.0f && m_mousePos.y <= 1.0f)
					SDL_ShowCursor(0);
				else
					SDL_ShowCursor(1);
			}
		} else if (e.type == SDL_MOUSEBUTTONUP) {
			SDL_CaptureMouse(SDL_FALSE);
			m_startWrap = false;
			if (Settings::Instance().Preview.Gizmo)
				m_gizmo.UnselectAxis();

			if (!m_data->Renderer.IsPaused() && (e.button.button == SDL_BUTTON_LEFT)) {
				glm::vec4 mbtn = SystemVariableManager::Instance().GetMouseButton();
				SystemVariableManager::Instance().SetMouseButton(mbtn.x, mbtn.y, -std::max<float>(0.0f, mbtn.z), -std::max<float>(0.0f, mbtn.w));
			}

			m_zoom.EndMouseAction();
		} else if (e.type == SDL_KEYDOWN) {
			if (e.key.keysym.sym == SDLK_ESCAPE) {
				m_mouseVisible = true;
				m_mouseLock = false;
			}
		} else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
			m_fullWindowFocus = true;
		} else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
			m_fullWindowFocus = false;
		}
	}
	void PreviewUI::Pick(PipelineItem* item, bool add)
	{
		// reset variables
		m_prevScale = m_tempScale = glm::vec3(1, 1, 1);
		m_prevRota = m_tempRota = glm::vec3(0, 0, 0);

		// check if it already exists
		bool skipAdd = false;
		for (int i = 0; i < m_picks.size(); i++)
			if (m_picks[i] == item) {
				if (!add) {
					m_picks.clear();
					m_picks.push_back(item);
				}
				skipAdd = true;
				break;
			}

		// add item
		if (!skipAdd) {
			if (item == nullptr)
				m_picks.clear();
			else if (add)
				m_picks.push_back(item);
			else {
				m_picks.clear();
				m_picks.push_back(item);
			}

			m_data->Renderer.Pick(item, add);
		}

		// calculate position
		m_prevTrans = glm::vec3(0, 0, 0);
		for (int i = 0; i < m_picks.size(); i++) {
			glm::vec3 pos(0, 0, 0);
			if (m_picks[i]->Type == PipelineItem::ItemType::Geometry) {
				pipe::GeometryItem* geo = (pipe::GeometryItem*)m_picks[i]->Data;
				pos = geo->Position;
			} else if (m_picks[i]->Type == PipelineItem::ItemType::Model) {
				pipe::Model* obj = (pipe::Model*)m_picks[i]->Data;
				pos = obj->Position;
			} else if (m_picks[i]->Type == PipelineItem::ItemType::VertexBuffer) {
				pipe::VertexBuffer* obj = (pipe::VertexBuffer*)m_picks[i]->Data;
				pos = obj->Position;
			}
			m_prevTrans.x += pos.x;
			m_prevTrans.y += pos.y;
			m_prevTrans.z += pos.z;
		}
		if (m_picks.size() != 0) {
			m_prevTrans.x /= m_picks.size();
			m_prevTrans.y /= m_picks.size();
			m_prevTrans.z /= m_picks.size();
		}
		m_tempTrans = m_prevTrans;

		if (m_picks.size() != 0) {
			if (Settings::Instance().Preview.BoundingBox)
				m_buildBoundingBox();

			m_gizmo.SetTransform(&m_tempTrans, &m_tempScale, &m_tempRota);
		}
	}
	void PreviewUI::SetVariableValue(PipelineItem* item, const std::string& varName, int line)
	{
		m_varValueItem = item;
		m_varValueName = varName;
		m_varValueLine = line;

		if (m_varValue != nullptr) {
			free(m_varValue);
			m_varValue = nullptr;
		}

		if (m_frameAnalyzed) {
			// variable value viewer
			m_varValue = m_data->Analysis.AllocateVariableValueMap(item, varName, line, m_varValueComponents);
			glDeleteTextures(1, &m_viewVariableValue);
			glGenTextures(1, &m_viewVariableValue);
			glBindTexture(GL_TEXTURE_2D, m_viewVariableValue);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_imgSize.x, m_imgSize.y, 0, GL_RGBA, GL_FLOAT, m_varValue);
		}
	}
	void PreviewUI::Update(float delta)
	{
		if (!m_data->Messages.CanRenderPreview()) {
			ImVec4 errorMsgColor = ThemeContainer::Instance().GetCustomStyle(Settings::Instance().Theme).ErrorMessage;

			if (m_data->DAP.IsStarted() || !m_ui->Get(ViewID::Output)->Visible) {
				for (const auto& msg : m_data->Messages.GetMessages()) {
					if (msg.Line < 0)
						continue;

					std::string errorFilename = "";
					if (msg.Shader != ShaderStage::Count) {
						ed::PipelineItem* msgItem = m_data->Pipeline.Get(msg.Group.c_str());

						if (msgItem != nullptr) {
							if (msgItem->Type == PipelineItem::ItemType::ShaderPass) {
								pipe::ShaderPass* msgData = (pipe::ShaderPass*)msgItem->Data;

								if (msg.Shader == ed::ShaderStage::Pixel)
									errorFilename = std::filesystem::path(msgData->PSPath).filename().u8string();
								else if (msg.Shader == ed::ShaderStage::Vertex)
									errorFilename = std::filesystem::path(msgData->VSPath).filename().u8string();
								else if (msg.Shader == ed::ShaderStage::Geometry)
									errorFilename = std::filesystem::path(msgData->GSPath).filename().u8string();
								else if (msg.Shader == ed::ShaderStage::TessellationControl)
									errorFilename = std::filesystem::path(msgData->TCSPath).filename().u8string();
								else if (msg.Shader == ed::ShaderStage::TessellationEvaluation)
									errorFilename = std::filesystem::path(msgData->TESPath).filename().u8string();
							} else if (msgItem->Type == PipelineItem::ItemType::ComputePass) {
								pipe::ComputePass* msgData = (pipe::ComputePass*)msgItem->Data;
								errorFilename = std::filesystem::path(msgData->Path).filename().u8string();
							} else if (msgItem->Type == PipelineItem::ItemType::AudioPass) {
								pipe::AudioPass* msgData = (pipe::AudioPass*)msgItem->Data;
								errorFilename = std::filesystem::path(msgData->Path).filename().u8string();
							}
						}
					}

					ImGui::TextColored(errorMsgColor, "%s (L%d): %s", errorFilename.c_str(), msg.Line, msg.Text.c_str());
				}
			}

			ImGui::NewLine();

			ImGui::TextColored(errorMsgColor, "Can not display preview - there are some errors you should fix.");
			return;
		}

		auto& pixelList = m_data->Debugger.GetPixelList();

		ed::Settings& settings = Settings::Instance();

		bool paused = m_data->Renderer.IsPaused();
		bool capWholeApp = settings.Preview.ApplyFPSLimitToApp;
		bool isNotMinimalMode = !m_ui->IsMinimalMode();
		bool statusbar = settings.Preview.StatusBar && isNotMinimalMode;
		float fpsLimit = settings.Preview.FPSLimit;
		if (fpsLimit != m_fpsLimit) {
			m_elapsedTime = 0;
			m_fpsLimit = fpsLimit;
		}

		ImVec2 imageSize = m_imgSize = ImVec2(ImGui::GetWindowContentRegionWidth(), abs(ImGui::GetWindowContentRegionMax().y - ImGui::GetWindowContentRegionMin().y - (STATUSBAR_HEIGHT - ImGui::GetStyle().FramePadding.y) * statusbar));
		ed::RenderEngine* renderer = &m_data->Renderer;

		if (m_zoomLastSize.x != (int)imageSize.x || m_zoomLastSize.y != (int)imageSize.y) {
			m_zoomLastSize.x = imageSize.x;
			m_zoomLastSize.y = imageSize.y;
			SystemVariableManager::Instance().SetViewportSize(imageSize.x, imageSize.y);

			m_zoom.RebuildVBO(imageSize.x, imageSize.y);
		}

		m_fpsUpdateTime += delta;
		m_elapsedTime += delta;

		bool useFpsLimit = !capWholeApp && m_fpsLimit > 0 && m_elapsedTime >= 1.0f / m_fpsLimit;
		if (capWholeApp || m_fpsLimit <= 0 || useFpsLimit) {
			if (!paused) {
				renderer->Render(imageSize.x, imageSize.y);
				m_data->Objects.Update(delta);
			}

			float fps = m_fpsTimer.Restart();
			if (m_fpsUpdateTime > FPS_UPDATE_RATE) {
				m_fpsDelta = fps;
				m_fpsUpdateTime -= FPS_UPDATE_RATE;
			}

			m_elapsedTime -= (1 / m_fpsLimit) * useFpsLimit + (delta * !useFpsLimit);
		}

		if (capWholeApp && m_fpsLimit > 0 && 1000 / delta > m_fpsLimit)
			std::this_thread::sleep_for(std::chrono::milliseconds(1000 / (int)m_fpsLimit - (int)(1000 * delta)));

		m_imgPosition = ImGui::GetCursorScreenPos();

		// display the image on the imgui window
		const glm::vec2& zPos = m_zoom.GetZoomPosition();
		const glm::vec2& zSize = m_zoom.GetZoomSize();
		GLuint displayImagePtr = 0;
		if (m_view == PreviewView::Normal)
			displayImagePtr = m_data->Renderer.GetTexture();
		else if (m_view == PreviewView::Debugger)
			displayImagePtr = m_viewDebugger;
		else if (m_view == PreviewView::Heatmap)
			displayImagePtr = m_viewHeatmap;
		else if (m_view == PreviewView::UndefinedBehavior)
			displayImagePtr = m_viewUB;
		else if (m_view == PreviewView::GlobalBreakpoints)
			displayImagePtr = m_viewBreakpoints;
		else if (m_view == PreviewView::VariableValue)
			displayImagePtr = m_viewVariableValue;
		ImGui::Image((void*)displayImagePtr, imageSize, ImVec2(zPos.x, zPos.y + zSize.y), ImVec2(zPos.x + zSize.x, zPos.y));
		m_hasFocus = ImGui::IsWindowFocused();

		// analyzer tooltip
		if ((m_view == PreviewView::UndefinedBehavior || m_view == PreviewView::Heatmap || m_view == PreviewView::VariableValue) && ImGui::IsItemHovered()) {
			// heatmap tooltip
			if (m_view == PreviewView::Heatmap || m_view == PreviewView::VariableValue) {
				glm::ivec2 outputSize = m_data->Analysis.GetOutputSize();
				if (m_view == PreviewView::VariableValue)
					outputSize = m_data->Renderer.GetLastRenderSize();

				glm::vec2 pixelSize = 1.0f / glm::vec2(outputSize);
				const float pixelMult = 30.0f;
				const int pixelCount = 7;

				glm::vec2 selectionPos(zPos.x + zSize.x * m_mousePos.x, zPos.y + zSize.y * m_mousePos.y);
				selectionPos.x = glm::floor(selectionPos.x / pixelSize.x) * pixelSize.x;
				selectionPos.y = glm::floor(selectionPos.y / pixelSize.y) * pixelSize.y;
				
				glm::ivec2 pixelPos(selectionPos.x * outputSize.x, selectionPos.y * outputSize.y);

				if (pixelPos.x >= 0 && pixelPos.y >= 0 && pixelPos.x < outputSize.x && pixelPos.y < outputSize.y) {
					ImGui::BeginTooltip();
					ImVec2 selectorPos = ImGui::GetCursorScreenPos();
					ImDrawList* drawList = ImGui::GetWindowDrawList();
					ImGui::Image(m_view == PreviewView::VariableValue ? (ImTextureID)m_viewVariableValue : (ImTextureID)m_viewHeatmap, ImVec2(pixelCount * pixelMult, pixelCount * pixelMult), ImVec2(selectionPos.x - (pixelCount / 2) * pixelSize.x, selectionPos.y + (pixelCount / 2 + 1) * pixelSize.y), ImVec2(selectionPos.x + (pixelCount / 2 + 1) * pixelSize.x, selectionPos.y - (pixelCount / 2) * pixelSize.y));
					drawList->AddRect(ImVec2(selectorPos.x + (pixelCount / 2) * pixelMult, selectorPos.y + (pixelCount / 2) * pixelMult), ImVec2(selectorPos.x + (pixelCount / 2 + 1) * pixelMult, selectorPos.y + (pixelCount / 2 + 1) * pixelMult), 0xFFFFFFFF);
					if (m_view == PreviewView::VariableValue) {
						if (m_varValue) {
							float* varVal = &m_varValue[(pixelPos.y * m_data->Renderer.GetLastRenderSize().x + pixelPos.x) * 4];
							if (m_varValueComponents == 1)
								ImGui::Text("value: %.6f", varVal[0]);
							else if (m_varValueComponents == 2)
								ImGui::Text("x: %.6f\ny: %.6f", varVal[0], varVal[1]);
							else if (m_varValueComponents == 3)
								ImGui::Text("x: %.6f\ny: %.6f\nz: %.6f", varVal[0], varVal[1], varVal[2]);
							else if (m_varValueComponents == 4)
								ImGui::Text("x: %.6f\ny: %.6f\nz: %.6f\nw: %.6f", varVal[0], varVal[1], varVal[2], varVal[3]);
						}
					} else 
						ImGui::Text("Instruction count: %u", m_data->Analysis.GetInstructionCount(pixelPos.x, pixelPos.y));
					ImGui::EndTooltip();
				}
			}
			// ub tooltip
			else if (m_view == PreviewView::UndefinedBehavior) {
				glm::vec2 outputSize = m_data->Analysis.GetOutputSize();
				glm::vec2 pixelPos = (zPos + zSize * m_mousePos) * outputSize;

				
				if (pixelPos.x >= 0 && pixelPos.y >= 0 && pixelPos.x < outputSize.x && pixelPos.y < outputSize.y) {
					uint32_t ubType = m_data->Analysis.GetUndefinedBehaviorLastType(pixelPos.x, pixelPos.y);
					uint32_t ubLine = m_data->Analysis.GetUndefinedBehaviorLastLine(pixelPos.x, pixelPos.y);
					uint32_t ubCount = m_data->Analysis.GetUndefinedBehaviorCount(pixelPos.x, pixelPos.y);

					if (ubType) {
						ImGui::BeginTooltip();
						ImGui::Text("Undefined behavior");
						ImGui::Separator();
						ImGui::Text("%s\nLine: %d", getUndefinedBehavior(ubType), ubLine); // todo: ubType
						if (ubCount <= 10)
							ImGui::Text("Total UB count: %d", ubCount);
						else
							ImGui::Text("Total UB count is over 10");
						ImGui::EndTooltip();
					}
				}
			}
		}

		// rerender when paused and resized
		if (paused && m_zoomLastSize != renderer->GetLastRenderSize() && !m_data->Debugger.IsDebugging()) {
			renderer->Render(imageSize.x, imageSize.y);
			m_data->Debugger.ClearPixelList();
		}

		// render the gizmo/bounding box/zoom area if necessary
		if (isNotMinimalMode && (m_zoom.IsSelecting() || (m_picks.size() != 0 && (settings.Preview.Gizmo || settings.Preview.BoundingBox)))) {
			// recreate render texture if size is changed
			if (m_lastSize.x != (int)imageSize.x || m_lastSize.y != (int)imageSize.y) {
				m_lastSize.x = imageSize.x;
				m_lastSize.y = imageSize.y;

				gl::FreeSimpleFramebuffer(m_overlayFBO, m_overlayColor, m_overlayDepth);
				m_overlayFBO = gl::CreateSimpleFramebuffer(imageSize.x, imageSize.y, m_overlayColor, m_overlayDepth);
			}
			glBindFramebuffer(GL_FRAMEBUFFER, m_overlayFBO);
			glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
			glViewport(0.0f, 0.0f, imageSize.x, imageSize.y);

			if (!paused && settings.Preview.Gizmo && m_picks.size() != 0 && pixelList.size() == 0) {
				m_gizmo.SetProjectionMatrix(SystemVariableManager::Instance().GetProjectionMatrix());
				m_gizmo.SetViewMatrix(SystemVariableManager::Instance().GetCamera()->GetMatrix());
				m_gizmo.Render();
			}

			if (!paused && settings.Preview.BoundingBox && m_picks.size() != 0 && pixelList.size() == 0)
				m_renderBoundingBox();

			if (m_zoom.IsSelecting())
				m_zoom.Render();

			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			ImGui::SetCursorPosY(ImGui::GetWindowContentRegionMin().y);
			ImGui::Image((void*)m_overlayColor, imageSize, ImVec2(zPos.x, zPos.y + zSize.y), ImVec2(zPos.x + zSize.x, zPos.y));
		}

		m_mousePos = glm::vec2((ImGui::GetMousePos().x - ImGui::GetCursorScreenPos().x - ImGui::GetScrollX()) / imageSize.x,
			1.0f - (imageSize.y + (ImGui::GetMousePos().y - ImGui::GetCursorScreenPos().y - ImGui::GetScrollY())) / imageSize.y);
		m_zoom.SetCurrentMousePosition(m_mousePos);

		if (m_lastButtonUpdate) {
			m_lastButton = glm::vec2((m_lastButton.x - ImGui::GetCursorScreenPos().x - ImGui::GetScrollX()) / imageSize.x,
				1.0f - (imageSize.y + (m_lastButton.y - ImGui::GetCursorScreenPos().y - ImGui::GetScrollY())) / imageSize.y);
			m_lastButtonUpdate = false;
		}

		if (!paused) {
			// update wasd key state
			SystemVariableManager::Instance().SetKeysWASD(ImGui::IsKeyDown(SDL_SCANCODE_W), ImGui::IsKeyDown(SDL_SCANCODE_A), ImGui::IsKeyDown(SDL_SCANCODE_S), ImGui::IsKeyDown(SDL_SCANCODE_D));

			// update system variable mouse position value
			if (ImGui::IsMouseDown(0)) {
				glm::vec4 mbtnlast = SystemVariableManager::Instance().GetMouseButton();
				SystemVariableManager::Instance().SetMouseButton(std::max<float>(0.0f, m_mousePos.x * imageSize.x),
					std::max<float>(0.0f, m_mousePos.y * imageSize.y),
					std::max<float>(0.0f, m_lastButton.x * imageSize.x),
					std::max<float>(0.0f, m_lastButton.y * imageSize.y));
			}

			SystemVariableManager::Instance().SetMousePosition(m_mousePos.x, m_mousePos.y);
			SystemVariableManager::Instance().SetMouse(m_mousePos.x, m_mousePos.y, ImGui::IsMouseDown(0), ImGui::IsMouseDown(1));
		}

		// apply transformations to picked items
		if (settings.Preview.Gizmo && m_picks.size() != 0) {
			if (m_tempTrans != m_prevTrans || m_tempScale != m_prevScale || m_tempRota != m_prevRota) {
				for (int i = 0; i < m_picks.size(); i++) {
					glm::vec3 t = m_tempTrans - m_prevTrans;
					glm::vec3 s = m_tempScale - m_prevScale;
					glm::vec3 r = m_tempRota - m_prevRota;
					glm::vec3 *ot = nullptr, *os = nullptr, *orot = nullptr;

					if (m_picks[i]->Type == PipelineItem::ItemType::Geometry) {
						pipe::GeometryItem* geo = (pipe::GeometryItem*)m_picks[i]->Data;
						ot = &geo->Position;
						os = &geo->Scale;
						orot = &geo->Rotation;
					} else if (m_picks[i]->Type == PipelineItem::ItemType::Model) {
						pipe::Model* obj = (pipe::Model*)m_picks[i]->Data;
						ot = &obj->Position;
						os = &obj->Scale;
						orot = &obj->Rotation;
					} else if (m_picks[i]->Type == PipelineItem::ItemType::VertexBuffer) {
						pipe::VertexBuffer* obj = (pipe::VertexBuffer*)m_picks[i]->Data;
						ot = &obj->Position;
						os = &obj->Scale;
						orot = &obj->Rotation;
					} else if (m_picks[i]->Type == PipelineItem::ItemType::PluginItem) {
						pipe::PluginItemData* obj = (pipe::PluginItemData*)m_picks[i]->Data;
						obj->Owner->PipelineItem_ApplyGizmoTransform(obj->Type, obj->PluginData, glm::value_ptr(t), glm::value_ptr(s), glm::value_ptr(r));
					}

					if (ot != nullptr)
						*ot += t;
					if (os != nullptr)
						*os += s;
					if (orot != nullptr)
						*orot += r;
				}

				m_prevTrans = m_tempTrans;
				m_prevRota = m_tempRota;
				m_prevScale = m_tempScale;

				m_buildBoundingBox();
			} else if (m_picks.size() == 1) {
				if (m_picks[0]->Type == PipelineItem::ItemType::Geometry) {
					pipe::GeometryItem* obj = (pipe::GeometryItem*)m_picks[0]->Data;
					if (obj->Position != m_tempTrans) {
						m_prevTrans = m_tempTrans = obj->Position;
						m_buildBoundingBox();
					} else if (obj->Scale != m_tempScale) {
						m_prevScale = m_tempScale = obj->Scale;
						m_buildBoundingBox();
					} else if (obj->Rotation != m_tempRota) {
						m_prevRota = m_tempRota = obj->Rotation;
						m_buildBoundingBox();
					}
				} else if (m_picks[0]->Type == PipelineItem::ItemType::Model) {
					pipe::Model* obj = (pipe::Model*)m_picks[0]->Data;
					if (obj->Position != m_tempTrans) {
						m_prevTrans = m_tempTrans = obj->Position;
						m_buildBoundingBox();
					} else if (obj->Scale != m_tempScale) {
						m_prevScale = m_tempScale = obj->Scale;
						m_buildBoundingBox();
					} else if (obj->Rotation != m_tempRota) {
						m_prevRota = m_tempRota = obj->Rotation;
						m_buildBoundingBox();
					}
				} else if (m_picks[0]->Type == PipelineItem::ItemType::VertexBuffer) {
					pipe::VertexBuffer* obj = (pipe::VertexBuffer*)m_picks[0]->Data;
					if (obj->Position != m_tempTrans) {
						m_prevTrans = m_tempTrans = obj->Position;
						m_buildBoundingBox();
					} else if (obj->Scale != m_tempScale) {
						m_prevScale = m_tempScale = obj->Scale;
						m_buildBoundingBox();
					} else if (obj->Rotation != m_tempRota) {
						m_prevRota = m_tempRota = obj->Rotation;
						m_buildBoundingBox();
					}
				} else if (m_picks[0]->Type == PipelineItem::ItemType::PluginItem) {
					pipe::PluginItemData* obj = (pipe::PluginItemData*)m_picks[0]->Data;
					
					float objPositionPtr[3], objRotationPtr[3], objScalePtr[3];
					obj->Owner->PipelineItem_GetTransform(obj->Type, obj->PluginData, objPositionPtr, objRotationPtr, objScalePtr);

					glm::vec3 objPosition = glm::make_vec3(objPositionPtr),
							  objRotation = glm::make_vec3(objRotationPtr),
							  objScale = glm::make_vec3(objScalePtr);

					if (objPosition != m_tempTrans) {
						m_prevTrans = m_tempTrans = objPosition;
						m_buildBoundingBox();
					} else if (objScale != m_tempScale) {
						m_prevScale = m_tempScale = objScale;
						m_buildBoundingBox();
					} else if (objRotation != m_tempRota) {
						m_prevRota = m_tempRota = objRotation;
						m_buildBoundingBox();
					}
				}
			}
		}

		// reset zoom on double click
		if (m_mouseHovers && ImGui::GetIO().KeyAlt && ImGui::IsMouseDoubleClicked(0))
			m_zoom.Reset();

		// auto pause in DAP mode
		m_mouseHovers = ImGui::IsItemHovered();
		if (m_data->DAP.IsStarted() && m_mouseHovers) {
			if (!paused) {
				if (((ImGui::IsMouseClicked(0) && !settings.Preview.SwitchLeftRightClick) || (ImGui::IsMouseClicked(1) && settings.Preview.SwitchLeftRightClick)) && !ImGui::GetIO().KeyAlt) {
					m_pause();
					paused = true;
				}
			} else {
				if (((ImGui::IsMouseClicked(1) && !settings.Preview.SwitchLeftRightClick) || (ImGui::IsMouseClicked(0) && settings.Preview.SwitchLeftRightClick))) {
					m_pause();
					paused = false;
				}
			}
		}

		// mouse controls for preview window
		// if not paused - various controls
		if (m_mouseHovers && !paused) {
			bool fp = settings.Project.FPCamera;

			// rt zoom in/out
			if (ImGui::GetIO().KeyCtrl && ImGui::GetIO().KeyAlt) {
				float wheelDelta = ImGui::GetIO().MouseWheel;
				if (wheelDelta != 0.0f) {
					if (wheelDelta < 0)
						wheelDelta = -wheelDelta * 2;
					else
						wheelDelta = wheelDelta / 2; // TODO: does this work if wheelDelta > 2 ?

					m_zoom.Zoom(wheelDelta);
				}
			}

			// arc ball zoom in/out if needed
			if (!fp && !(ImGui::GetIO().KeyAlt && ImGui::GetIO().KeyCtrl))
				((ed::ArcBallCamera*)SystemVariableManager::Instance().GetCamera())->Move(-ImGui::GetIO().MouseWheel);

			// handle left click - selection
			if ((settings.Preview.Gizmo || settings.Preview.BoundingBox) && !ImGui::GetIO().KeyAlt && 
				isNotMinimalMode && 
				((ImGui::IsMouseClicked(0) && !settings.Preview.SwitchLeftRightClick) || (ImGui::IsMouseClicked(1) && settings.Preview.SwitchLeftRightClick)))
			{
				// screen space position
				glm::vec2 s(zPos.x + zSize.x * m_mousePos.x, zPos.y + zSize.y * m_mousePos.y);

				s.x *= imageSize.x;
				s.y *= imageSize.y;

				bool shiftPickBegan = ImGui::GetIO().KeyShift;

				if ((settings.Preview.BoundingBox && !settings.Preview.Gizmo) || (m_picks.size() != 0 && m_gizmo.Click(s.x, s.y, imageSize.x, imageSize.y) == -1) || m_picks.size() == 0) {
					renderer->Pick(s.x, s.y, shiftPickBegan, [&](PipelineItem* item) {
						if (settings.Preview.PropertyPick)
							((PropertyUI*)m_ui->Get(ViewID::Properties))->Open(item);

						bool shift = ImGui::GetIO().KeyShift;

						Pick(item, shift);
					});
				}
			}

			// handle right mouse dragging - camera
			if (((ImGui::IsMouseDown(0) && settings.Preview.SwitchLeftRightClick) || (ImGui::IsMouseDown(1) && !settings.Preview.SwitchLeftRightClick))
				&& !m_zoom.IsDragging()) {
				m_startWrap = true;
				SDL_CaptureMouse(SDL_TRUE);

				int ptX, ptY;
				SDL_GetMouseState(&ptX, &ptY);

				// get the delta from the last position
				int dX = ptX - m_mouseContact.x;
				int dY = ptY - m_mouseContact.y;

				// save the last position
				m_mouseContact = ImVec2(ptX, ptY);

				// rotate the camera according to the delta
				if (!fp) {
					ed::ArcBallCamera* cam = ((ed::ArcBallCamera*)SystemVariableManager::Instance().GetCamera());
					cam->Yaw(dX);
					cam->Pitch(dY);
				} else {
					ed::FirstPersonCamera* cam = ((ed::FirstPersonCamera*)SystemVariableManager::Instance().GetCamera());
					cam->Yaw(dX * 0.05f);
					cam->Pitch(dY * 0.05f);
				}
			}

			// handle left mouse dragging - moving objects if selected
			else if (((ImGui::IsMouseDown(0) && !settings.Preview.SwitchLeftRightClick) || (ImGui::IsMouseDown(1) && settings.Preview.SwitchLeftRightClick)) && settings.Preview.Gizmo && !m_zoom.IsSelecting()) {
				// screen space position
				glm::vec2 s = m_mousePos;
				s.x *= imageSize.x;
				s.y *= imageSize.y;

				if (m_gizmo.Transform(s.x, s.y, ImGui::GetIO().KeyShift))
					m_data->Parser.ModifyProject();
			}

			// WASD key press - first person camera
			if (fp) {
				float mult = ImGui::GetIO().KeyShift ? 100.0f : 1.0f;

				ed::FirstPersonCamera* cam = ((ed::FirstPersonCamera*)SystemVariableManager::Instance().GetCamera());
				cam->MoveUpDown(((ImGui::IsKeyDown(SDL_SCANCODE_S) - ImGui::IsKeyDown(SDL_SCANCODE_W)) / 70.0f) * mult);
				cam->MoveLeftRight(((ImGui::IsKeyDown(SDL_SCANCODE_D) - ImGui::IsKeyDown(SDL_SCANCODE_A)) / 70.0f) * mult);
			}
		}
		// else if paused - pixel inspection
		else if (m_mouseHovers) {
			// handle left click - pixel selection
			if (((ImGui::IsMouseClicked(0) && !settings.Preview.SwitchLeftRightClick) || (ImGui::IsMouseClicked(1) && settings.Preview.SwitchLeftRightClick)) && !ImGui::GetIO().KeyAlt) {
				m_ui->StopDebugging();

				// screen space position
				glm::vec2 s(zPos.x + zSize.x * m_mousePos.x, zPos.y + zSize.y * m_mousePos.y);

				m_data->DebugClick(s);
			}
		}

		// mouse wrapping
		if (m_startWrap || (m_mouseLock && m_fullWindowFocus)) {
			int ptX, ptY;
			SDL_GetMouseState(&ptX, &ptY);

			// screen space position
			bool needsXAdjust = m_ui->IsPerformanceMode();
			bool needsYAdjust = needsXAdjust && !statusbar;

			ImVec2 mouseStart = ImGui::GetWindowContentRegionMin();
			mouseStart.x += ImGui::GetWindowPos().x;
			mouseStart.y += ImGui::GetWindowPos().y + ImGui::GetScrollY() + !needsXAdjust * ImGui::GetStyle().FramePadding.y;

			bool wrappedMouse = false;
			const float mPercentX = needsXAdjust ? 0.005f : 0.00f;
			const float mPercentYTop = 0.00f;
			const float mPercentYBottom = needsYAdjust ? 0.005f : 0.00f;
			if (m_mousePos.x < mPercentX) {
				ptX = mouseStart.x + imageSize.x * (1 - mPercentX) - 2;
				wrappedMouse = true;
			} else if (m_mousePos.x > 1 - mPercentX) {
				ptX = mouseStart.x + imageSize.x * mPercentX + 2;
				wrappedMouse = true;
			} else if (m_mousePos.y > 1 - mPercentYTop) {
				ptY = mouseStart.y + imageSize.y * (1 - mPercentYTop) - 2;
				wrappedMouse = true;
			} else if (m_mousePos.y < mPercentYBottom) {
				ptY = mouseStart.y + imageSize.y * mPercentYBottom + 2;
				wrappedMouse = true;
			}

			if (wrappedMouse) {
				m_mouseContact = ImVec2(ptX, ptY);
				SDL_WarpMouseInWindow(m_ui->GetSDLWindow(), ptX, ptY);
			}
		}

		// status bar
		if (statusbar)
			m_renderStatusbar(imageSize.x, imageSize.y);

		// debugger vertex outline
		if (paused && pixelList.size() > 0 && (settings.Debug.PixelOutline || settings.Debug.PrimitiveOutline)) {
			unsigned int outlineColor = 0xffffffff;
			auto drawList = ImGui::GetWindowDrawList();
			static char pxCoord[32] = { 0 };

			for (int i = 0; i < pixelList.size(); i++) {
				if (pixelList[i].RenderTexture == nullptr && pixelList[i].Fetched) { // we only care about window's pixel info here

					if (settings.Debug.PrimitiveOutline) {
						// render the lines
						ImGui::SetCursorPosY(ImGui::GetWindowContentRegionMin().y);
						ImVec2 uiPos = ImGui::GetCursorScreenPos();

						DebuggerOutline::RenderPrimitiveOutline(pixelList[i], glm::vec2(uiPos.x, uiPos.y), glm::vec2(imageSize.x, imageSize.y), zPos, zSize);
					}
					if (settings.Debug.PixelOutline) {
						// render the lines
						ImGui::SetCursorPosY(ImGui::GetWindowContentRegionMin().y);
						ImVec2 uiPos = ImGui::GetCursorScreenPos();

						DebuggerOutline::RenderPixelOutline(pixelList[i], glm::vec2(uiPos.x, uiPos.y), glm::vec2(imageSize.x, imageSize.y), zPos, zSize);
					}
				}
			}
		}
	
		// frame analyzer popup
		ImGui::SetNextWindowSize(ImVec2(m_imgSize.x * 0.75f + Settings::Instance().CalculateSize(300), m_imgSize.y * 0.75f + Settings::Instance().CalculateSize(55)), ImGuiCond_Once);
		if (ImGui::BeginPopupModal("Analyzer##analyzer")) {
			m_renderAnalyzerPopup();
			ImGui::EndPopup();
		}
	}
	void PreviewUI::Duplicate()
	{
		if (m_picks.size() == 0)
			return;

		// store pointers to these objects as we will select them after duplication
		std::vector<PipelineItem*> duplicated;

		// duplicate each item
		for (int i = 0; i < m_picks.size(); i++) {
			ed::PipelineItem* item = m_picks[i];

			// first find a name that is not used
			std::string name = std::string(item->Name);

			// remove numbers at the end of the string
			size_t lastOfLetter = std::string::npos;
			for (size_t j = name.size() - 1; j > 0; j--)
				if (!std::isdigit(name[j])) {
					lastOfLetter = j + 1;
					break;
				}
			if (lastOfLetter != std::string::npos)
				name = name.substr(0, lastOfLetter);

			// add number to the string and check if it already exists
			for (size_t j = 2; /*WE WILL BREAK FROM INSIDE ONCE WE FIND THE NAME*/; j++) {
				std::string newName = name + std::to_string(j);
				bool has = m_data->Pipeline.Has(newName.c_str());

				if (!has) {
					name = newName;
					break;
				}
			}

			// get item owner
			char* owner = m_data->Pipeline.GetItemOwner(item->Name);

			// once we found a name, duplicate the properties:
			// duplicate geometry object:
			if (item->Type == PipelineItem::ItemType::Geometry) {
				pipe::GeometryItem* data = new pipe::GeometryItem();
				pipe::GeometryItem* origData = (pipe::GeometryItem*)item->Data;

				pipe::ShaderPass* ownerData = (pipe::ShaderPass*)(m_data->Pipeline.Get(owner)->Data);

				data->Position = origData->Position;
				data->Rotation = origData->Rotation;
				data->Scale = origData->Scale;
				data->Size = origData->Size;
				data->Topology = origData->Topology;
				data->Type = origData->Type;

				if (data->Type == pipe::GeometryItem::GeometryType::Cube)
					data->VAO = eng::GeometryFactory::CreateCube(data->VBO, data->Size.x, data->Size.y, data->Size.z, ownerData->InputLayout);
				else if (data->Type == pipe::GeometryItem::Circle) {
					data->VAO = eng::GeometryFactory::CreateCircle(data->VBO, data->Size.x, data->Size.y, ownerData->InputLayout);
					data->Topology = GL_TRIANGLE_STRIP;
				} else if (data->Type == pipe::GeometryItem::Plane)
					data->VAO = eng::GeometryFactory::CreatePlane(data->VBO, data->Size.x, data->Size.y, ownerData->InputLayout);
				else if (data->Type == pipe::GeometryItem::Rectangle)
					data->VAO = eng::GeometryFactory::CreatePlane(data->VBO, 1, 1, ownerData->InputLayout);
				else if (data->Type == pipe::GeometryItem::Sphere)
					data->VAO = eng::GeometryFactory::CreateSphere(data->VBO, data->Size.x, ownerData->InputLayout);
				else if (data->Type == pipe::GeometryItem::Triangle)
					data->VAO = eng::GeometryFactory::CreateTriangle(data->VBO, data->Size.x, ownerData->InputLayout);
				else if (data->Type == pipe::GeometryItem::ScreenQuadNDC)
					data->VAO = eng::GeometryFactory::CreateScreenQuadNDC(data->VBO, ownerData->InputLayout);
				m_data->Pipeline.AddItem(owner, name.c_str(), item->Type, data);
			}

			// duplicate Model:
			else if (item->Type == PipelineItem::ItemType::Model) {
				pipe::Model* data = new pipe::Model();
				pipe::Model* origData = (pipe::Model*)item->Data;

				strcpy(data->Filename, origData->Filename);
				strcpy(data->GroupName, origData->GroupName);
				data->OnlyGroup = origData->OnlyGroup;
				data->Scale = origData->Scale;
				data->Position = origData->Position;
				data->Rotation = origData->Rotation;

				if (strlen(data->Filename) > 0) {
					std::string objMem = m_data->Parser.LoadProjectFile(data->Filename);
					eng::Model* mdl = m_data->Parser.LoadModel(data->Filename);

					bool loaded = mdl != nullptr;
					if (loaded)
						data->Data = mdl;
					else
						m_data->Messages.Add(ed::MessageStack::Type::Error, owner, "Failed to create .obj model " + std::string(item->Name));
				}

				m_data->Pipeline.AddItem(owner, name.c_str(), item->Type, data);
			}

			// duplicate VertexBuffer:
			else if (item->Type == PipelineItem::ItemType::VertexBuffer) {
				pipe::VertexBuffer* newData = new pipe::VertexBuffer();
				pipe::VertexBuffer* origData = (pipe::VertexBuffer*)item->Data;

				pipe::ShaderPass* ownerData = (pipe::ShaderPass*)(m_data->Pipeline.Get(owner)->Data);

				newData->Scale = origData->Scale;
				newData->Position = origData->Position;
				newData->Rotation = origData->Rotation;
				newData->Topology = origData->Topology;
				newData->Buffer = origData->Buffer;

				if (newData->Buffer != 0)
					gl::CreateBufferVAO(newData->VAO, ((ed::BufferObject*)newData->Buffer)->ID, m_data->Objects.ParseBufferFormat(((ed::BufferObject*)newData->Buffer)->ViewFormat));

				m_data->Pipeline.AddItem(owner, name.c_str(), item->Type, newData);
			}

			duplicated.push_back(m_data->Pipeline.Get(name.c_str()));
		}

		// select the newly created items
		m_picks = duplicated;

		// open in properties if needed
		if (Settings::Instance().Preview.PropertyPick && m_picks.size() > 0)
			((PropertyUI*)m_ui->Get(ViewID::Properties))->Open(m_picks[m_picks.size() - 1]);
	}

	void PreviewUI::m_renderStatusbar(float width, float height)
	{
		bool isItemListVisible = width >= Settings::Instance().CalculateSize(675);
		bool isZoomVisible = width >= Settings::Instance().CalculateSize(565);
		bool isTimeVisible = width >= Settings::Instance().CalculateSize(445);
		bool areControlsVisible = width >= Settings::Instance().CalculateSize(330);

		int offset = (-100 * !isZoomVisible) + (-100 * !isTimeVisible);

		float FPS = 1.0f / m_fpsDelta;
		ImGui::Separator();
		ImGui::Text("FPS: %.2f", FPS);

		if (isTimeVisible) {
			ImGui::SameLine(Settings::Instance().CalculateSize(120));
			ImGui::Text("Time: %.2f", SystemVariableManager::Instance().GetTime());
		}

		if (isZoomVisible) {
			ImGui::SameLine(Settings::Instance().CalculateSize(240));
			ImGui::Text("Zoom: %d%%", (int)((1.0f / m_zoom.GetZoomSize().x) * 100.0f));
		}

		if (areControlsVisible) {
			if (!m_data->Renderer.IsPaused()) {
				ImGui::SameLine(Settings::Instance().CalculateSize(340 + offset));
				if (m_pickMode == 0) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
				if (ImGui::Button("P##pickModePos", ImVec2(BUTTON_SIZE, BUTTON_SIZE)) && m_pickMode != 0) {
					m_pickMode = 0;
					m_gizmo.SetMode(m_pickMode);
				} else if (m_pickMode == 0)
					ImGui::PopStyleColor();

				ImGui::SameLine();
				if (m_pickMode == 1) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
				if (ImGui::Button("S##pickModeScl", ImVec2(BUTTON_SIZE, BUTTON_SIZE)) && m_pickMode != 1) {
					m_pickMode = 1;
					m_gizmo.SetMode(m_pickMode);
				} else if (m_pickMode == 1)
					ImGui::PopStyleColor();

				ImGui::SameLine();
				if (m_pickMode == 2) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
				if (ImGui::Button("R##pickModeRot", ImVec2(BUTTON_SIZE, BUTTON_SIZE)) && m_pickMode != 2) {
					m_pickMode = 2;
					m_gizmo.SetMode(m_pickMode);
				} else if (m_pickMode == 2)
					ImGui::PopStyleColor();
			} else {
				ImGui::SameLine(Settings::Instance().CalculateSize(340 + offset));
				if (m_frameAnalyzed) {
					ImGui::PushItemWidth(150.0f);
					if (ImGui::BeginCombo("##fa_preview", getViewName(m_view))) {
						// normal preview
						if (ImGui::Selectable(getViewName(PreviewView::Normal)))
							m_view = PreviewView::Normal;

						// SPIRV-VM view
						if (ImGui::Selectable(getViewName(PreviewView::Debugger)))
							m_view = PreviewView::Debugger;

						// opcode heatmap view
						if (ImGui::Selectable(getViewName(PreviewView::Heatmap)))
							m_view = PreviewView::Heatmap;

						// undefined behavior map
						if (ImGui::Selectable(getViewName(PreviewView::UndefinedBehavior)))
							m_view = PreviewView::UndefinedBehavior;

						// global breakpoints
						if (m_data->Analysis.HasGlobalBreakpoints() && ImGui::Selectable(getViewName(PreviewView::GlobalBreakpoints)))
							m_view = PreviewView::GlobalBreakpoints;

						// variable value viewer
						if (ImGui::Selectable(getViewName(PreviewView::VariableValue)))
							m_view = PreviewView::VariableValue;

						ImGui::EndCombo();
					}
					ImGui::PopItemWidth();
				} else {
					if (ImGui::Button("Analyze", ImVec2(150.0f, 0.0f))) {
						ImGui::OpenPopup("Analyzer##analyzer");
						m_buildBreakpointList();
					}
				}
			}
		}

		ImGui::SameLine();
		if (m_picks.size() != 0 && isItemListVisible) {
			ImGui::SameLine(0, Settings::Instance().CalculateSize(20));
			ImGui::Text("Picked: ");

			for (int i = 0; i < m_picks.size(); i++) {
				ImGui::SameLine();

				if (i != m_picks.size() - 1)
					ImGui::Text("%s,", m_picks[i]->Name);
				else
					ImGui::Text("%s", m_picks[i]->Name);

				if (i >= MAX_PICKED_ITEM_LIST_SIZE - 1) {
					ImGui::SameLine();
					ImGui::Text("...");
					break;
				}
			}
			ImGui::SameLine();
		}

		float controlsStartX = (width - ((ICON_BUTTON_WIDTH * 4) + (BUTTON_INDENT * 3))) / 2;
		if (ImGui::GetCursorPosX() >= controlsStartX - 100)
			ImGui::SameLine();
		else
			ImGui::SetCursorPosX(controlsStartX);

		
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

		if (ImGui::Button(UI_ICON_PRESS, ImVec2(ICON_BUTTON_WIDTH, BUTTON_SIZE)) && m_data->Renderer.IsPaused()) {
			float deltaTime = SystemVariableManager::Instance().GetTimeDelta();

			if (ImGui::GetIO().KeyCtrl) {
				SystemVariableManager::Instance().AdvanceTimer(-deltaTime);
				SystemVariableManager::Instance().SetFrameIndex(SystemVariableManager::Instance().GetFrameIndex() - 1);
			} else {
				SystemVariableManager::Instance().AdvanceTimer(-0.1f);
				SystemVariableManager::Instance().SetFrameIndex(SystemVariableManager::Instance().GetFrameIndex() - 0.1f / deltaTime);
			}

			m_data->Renderer.Render(m_imgSize.x, m_imgSize.y);
		}

		/* RESET TIME BUTTON */
		ImGui::SameLine();
		if (ImGui::Button(UI_ICON_UNDO, ImVec2(ICON_BUTTON_WIDTH, BUTTON_SIZE))) {
			SystemVariableManager::Instance().Reset();

			if (m_data->Renderer.IsPaused())
				m_data->Renderer.Render(m_imgSize.x, m_imgSize.y);
		}

		/* PAUSE BUTTON */
		ImGui::SameLine();
		if (ImGui::Button(m_data->Renderer.IsPaused() ? UI_ICON_PLAY : UI_ICON_PAUSE, ImVec2(ICON_BUTTON_WIDTH, BUTTON_SIZE)))
			m_pause();

		ImGui::SameLine(0, BUTTON_INDENT);
		if (ImGui::Button(UI_ICON_NEXT, ImVec2(ICON_BUTTON_WIDTH, BUTTON_SIZE)) && m_data->Renderer.IsPaused()) {
			float deltaTime = SystemVariableManager::Instance().GetTimeDelta();

			if (ImGui::GetIO().KeyCtrl) {
				SystemVariableManager::Instance().AdvanceTimer(deltaTime);												// add one second to timer
				SystemVariableManager::Instance().SetFrameIndex(SystemVariableManager::Instance().GetFrameIndex() + 1); // increase by 1 frame
			} else {
				SystemVariableManager::Instance().AdvanceTimer(0.1f);																   // add one second to timer
				SystemVariableManager::Instance().SetFrameIndex(SystemVariableManager::Instance().GetFrameIndex() + 0.1f / deltaTime); // add estimated number of frames
			}

			m_data->Renderer.Render(m_imgSize.x, m_imgSize.y);
		}
		ImGui::SameLine();

		float zoomStartX = width - (ICON_BUTTON_WIDTH * 3) - BUTTON_INDENT * 3;
		ImGui::SetCursorPosX(zoomStartX);
		if (ImGui::Button(UI_ICON_ZOOM_IN, ImVec2(ICON_BUTTON_WIDTH, BUTTON_SIZE)))
			m_zoom.Zoom(0.5f, false);
		ImGui::SameLine(0, BUTTON_INDENT);
		if (ImGui::Button(UI_ICON_ZOOM_OUT, ImVec2(ICON_BUTTON_WIDTH, BUTTON_SIZE)))
			m_zoom.Zoom(2, false);
		ImGui::SameLine(0, BUTTON_INDENT);
		if (m_ui->IsPerformanceMode())
			ImGui::PopStyleColor();
		if (ImGui::Button(UI_ICON_MAXIMIZE, ImVec2(ICON_BUTTON_WIDTH, BUTTON_SIZE)))
			m_ui->SetPerformanceMode(!m_ui->IsPerformanceMode());

		if (!m_ui->IsPerformanceMode())
			ImGui::PopStyleColor();
	}

	void PreviewUI::m_setupBoundingBox()
	{
		Logger::Get().Log("Setting up the bounding box");

		// create a shader program for gizmo
		m_boxShader = gl::CreateShader(&BOX_VS_CODE, &BOX_PS_CODE, "bounding box");

		m_uMatWVPLoc = glGetUniformLocation(m_boxShader, "uMatWVP");
		m_uColorLoc = glGetUniformLocation(m_boxShader, "uColor");
	}
	void PreviewUI::m_buildBoundingBox()
	{
		glm::vec3 minPos(std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity()),
			maxPos(-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity());

		// find min and max pos
		for (int i = 0; i < m_picks.size(); i++) {
			ed::PipelineItem* item = m_picks[i];

			bool rotatePoints = true;
			glm::vec3 minPosItem(0, 0, 0), maxPosItem(0, 0, 0);
			glm::vec3 rota(0, 0, 0);
			glm::vec3 pos(0, 0, 0);

			if (item->Type == ed::PipelineItem::ItemType::Geometry) {
				pipe::GeometryItem* data = (pipe::GeometryItem*)item->Data;
				glm::vec3 size(data->Size.x * data->Scale.x, data->Size.y * data->Scale.y, data->Size.z * data->Scale.z);

				if (data->Type == pipe::GeometryItem::Sphere) {
					size = glm::vec3(size.x * 2, size.x * 2, size.x * 2);
					rotatePoints = false;
				} else if (data->Type == pipe::GeometryItem::Circle)
					size = glm::vec3(size.x * 2, size.y * 2, 0.0001f);
				else if (data->Type == pipe::GeometryItem::Triangle) {
					float rightOffs = data->Size.x / tan(glm::radians(30.0f));
					size = glm::vec3(rightOffs * 2 * data->Scale.x, data->Size.x * 2 * data->Scale.y, 0.0001f);
				} else if (data->Type == pipe::GeometryItem::Plane)
					size.z = 0.0001f;

				minPosItem = glm::vec3(-size.x / 2, -size.y / 2, -size.z / 2);
				maxPosItem = glm::vec3(+size.x / 2, +size.y / 2, +size.z / 2);

				rota = data->Rotation;
				pos = data->Position;
			} else if (item->Type == ed::PipelineItem::ItemType::Model) {
				pipe::Model* model = (pipe::Model*)item->Data;

				minPosItem = model->Data->GetMinBound() * model->Scale;
				maxPosItem = model->Data->GetMaxBound() * model->Scale;

				rota = model->Rotation;
				pos = model->Position;
			} else if (item->Type == ed::PipelineItem::ItemType::VertexBuffer) {
				pipe::VertexBuffer* vertBuffer = (pipe::VertexBuffer*)item->Data;
				gl::GetVertexBufferBounds(&m_data->Objects, vertBuffer, minPosItem, maxPosItem);
			
				minPosItem *= vertBuffer->Scale;
				maxPosItem *= vertBuffer->Scale;

				rota = vertBuffer->Rotation;
				pos = vertBuffer->Position;
			} else if (item->Type == ed::PipelineItem::ItemType::PluginItem) {
				pipe::PluginItemData* pldata = (pipe::PluginItemData*)item->Data;

				float minPos[3], maxPos[3];
				pldata->Owner->PipelineItem_GetBoundingBox(pldata->Type, pldata->PluginData, minPos, maxPos);

				minPosItem = glm::make_vec3(minPos);
				maxPosItem = glm::make_vec3(maxPos);
			}

			// 8 points
			float pointsX[8] = { minPosItem.x, minPosItem.x, minPosItem.x, minPosItem.x, maxPosItem.x, maxPosItem.x, maxPosItem.x, maxPosItem.x };
			float pointsY[8] = { minPosItem.y, minPosItem.y, maxPosItem.y, maxPosItem.y, minPosItem.y, minPosItem.y, maxPosItem.y, maxPosItem.y };
			float pointsZ[8] = { minPosItem.z, maxPosItem.z, minPosItem.z, maxPosItem.z, minPosItem.z, maxPosItem.z, minPosItem.z, maxPosItem.z };

			// apply rotation and translation to those 8 points and check for min and max pos
			for (int j = 0; j < 8; j++) {
				glm::vec4 point(pointsX[j], pointsY[j], pointsZ[j], 1);

				if (rotatePoints)
					point = glm::yawPitchRoll(rota.y, rota.x, rota.z) * point;

				point = glm::translate(glm::mat4(1), pos) * point;

				minPos.x = std::min<float>(point.x, minPos.x);
				minPos.y = std::min<float>(point.y, minPos.y);
				minPos.z = std::min<float>(point.z, minPos.z);
				maxPos.x = std::max<float>(point.x, maxPos.x);
				maxPos.y = std::max<float>(point.y, maxPos.y);
				maxPos.z = std::max<float>(point.z, maxPos.z);
			}
		}

		minPos.x -= BOUNDING_BOX_PADDING;
		minPos.y -= BOUNDING_BOX_PADDING;
		minPos.z -= BOUNDING_BOX_PADDING;
		maxPos.x += BOUNDING_BOX_PADDING;
		maxPos.y += BOUNDING_BOX_PADDING;
		maxPos.z += BOUNDING_BOX_PADDING;

		// lines
		std::vector<glm::vec3> verts = {
			// back face
			{ glm::vec3(minPos.x, minPos.y, minPos.z) },
			{ glm::vec3(maxPos.x, minPos.y, minPos.z) },
			{ glm::vec3(maxPos.x, minPos.y, minPos.z) },
			{ glm::vec3(maxPos.x, maxPos.y, minPos.z) },
			{ glm::vec3(minPos.x, minPos.y, minPos.z) },
			{ glm::vec3(minPos.x, maxPos.y, minPos.z) },
			{ glm::vec3(minPos.x, maxPos.y, minPos.z) },
			{ glm::vec3(maxPos.x, maxPos.y, minPos.z) },

			// front face
			{ glm::vec3(minPos.x, minPos.y, maxPos.z) },
			{ glm::vec3(maxPos.x, minPos.y, maxPos.z) },
			{ glm::vec3(maxPos.x, minPos.y, maxPos.z) },
			{ glm::vec3(maxPos.x, maxPos.y, maxPos.z) },
			{ glm::vec3(minPos.x, minPos.y, maxPos.z) },
			{ glm::vec3(minPos.x, maxPos.y, maxPos.z) },
			{ glm::vec3(minPos.x, maxPos.y, maxPos.z) },
			{ glm::vec3(maxPos.x, maxPos.y, maxPos.z) },

			// sides
			{ glm::vec3(minPos.x, minPos.y, minPos.z) },
			{ glm::vec3(minPos.x, minPos.y, maxPos.z) },
			{ glm::vec3(maxPos.x, minPos.y, minPos.z) },
			{ glm::vec3(maxPos.x, minPos.y, maxPos.z) },
			{ glm::vec3(minPos.x, maxPos.y, minPos.z) },
			{ glm::vec3(minPos.x, maxPos.y, maxPos.z) },
			{ glm::vec3(maxPos.x, maxPos.y, minPos.z) },
			{ glm::vec3(maxPos.x, maxPos.y, maxPos.z) },
		};

		if (m_boxVBO != 0)
			glDeleteBuffers(1, &m_boxVBO);
		if (m_boxVAO != 0)
			glDeleteVertexArrays(1, &m_boxVAO);

		// create vbo
		glGenBuffers(1, &m_boxVBO);
		glBindBuffer(GL_ARRAY_BUFFER, m_boxVBO);
		glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(glm::vec3), verts.data(), GL_STATIC_DRAW);

		// create vao
		glGenVertexArrays(1, &m_boxVAO);
		glBindVertexArray(m_boxVAO);

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);

		// unbind
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		// calculate color for the bounding box
		glm::vec4 clearClr = Settings::Instance().Project.ClearColor;
		float avgClear = ((float)clearClr.r + clearClr.g + clearClr.b) / 3.0f;
		ImVec4 wndBg = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
		float avgWndBg = (wndBg.x + wndBg.y + wndBg.z) / 3;
		float clearAlpha = Settings::Instance().Project.UseAlphaChannel ? (clearClr.a / 255.0f) : 1;
		float color = avgClear * clearAlpha + avgWndBg * (1 - clearAlpha);
		if (color >= 0.5f)
			m_boxColor = glm::vec4(0, 0, 0, 1);
		else
			m_boxColor = glm::vec4(1, 1, 1, 1);
	}
	void PreviewUI::m_renderBoundingBox()
	{
		glUseProgram(m_boxShader);

		glm::mat4 matWorld(1);
		glm::mat4 matProj = SystemVariableManager::Instance().GetProjectionMatrix() * SystemVariableManager::Instance().GetViewMatrix();

		glUniformMatrix4fv(m_uMatWVPLoc, 1, GL_FALSE, glm::value_ptr(matProj * matWorld));
		glUniform4fv(m_uColorLoc, 1, glm::value_ptr(m_boxColor));

		glBindVertexArray(m_boxVAO);
		glDrawArrays(GL_LINES, 0, 24);
	}
	void PreviewUI::m_pause()
	{
		m_data->Renderer.Pause(!m_data->Renderer.IsPaused());
		m_data->Objects.Pause(m_data->Renderer.IsPaused());
		m_ui->StopDebugging();
		m_view = PreviewView::Normal;
		m_frameAnalyzed = false;
	}
	void PreviewUI::m_renderAnalyzerPopup()
	{
		bool isFullFrame = m_isAnalyzingFullFrame;
		if (ImGui::BeginTabBar("AnalyzerTabs")) {
			if (ImGui::BeginTabItem("Region")) {
				if (ImGui::BeginTable("##analyzer_layout", 2, ImGuiTableFlags_BordersVInner, ImVec2(0, -Settings::Instance().CalculateSize(45)))) {
					ImGui::TableSetupColumn("##preview", ImGuiTableColumnFlags_WidthStretch, ImGui::GetWindowWidth() - Settings::Instance().CalculateSize(250));
					ImGui::TableSetupColumn("##controls", ImGuiTableColumnFlags_WidthStretch, Settings::Instance().CalculateSize(250));
					ImGui::TableNextRow();

					// image
					ImGui::TableSetColumnIndex(0);
					ImVec2 iSize = ImGui::GetContentRegionAvail();
					ImVec2 contentSize = iSize;
					float scale = std::min<float>(iSize.x / m_imgSize.x, (iSize.y - Settings::Instance().CalculateSize(45.0f)) / m_imgSize.y);
					iSize.x = m_imgSize.x * scale;
					iSize.y = m_imgSize.y * scale;
					ImVec2 cursorPos = ImGui::GetCursorPos();
					cursorPos = ImVec2((contentSize.x - iSize.x) / 2.0f, cursorPos.y);
					ImGui::SetCursorPos(cursorPos);
					ImGui::Image((ImTextureID)m_data->Renderer.GetTexture(), iSize, ImVec2(0, 1), ImVec2(1, 0), ImVec4(1, 1, 1, isFullFrame ? 1.0f : 0.5f));

					// handle region selection
					if (!isFullFrame) {
						ImGui::SetCursorPos(cursorPos);
						ImVec2 screenCursorPos = ImGui::GetCursorScreenPos();
						ImGui::InvisibleButton("##analyzer_image_btn", iSize);
						if (ImGui::IsItemHovered()) {
							if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
								ImVec2 mousePos = ImGui::GetMousePos();
								m_regionStart = glm::vec2((mousePos.x - screenCursorPos.x) / iSize.x, (mousePos.y - screenCursorPos.y) / iSize.y);

								glm::vec2 blockCount = glm::floor(m_regionStart * glm::vec2(m_imgSize.x / RASTER_BLOCK_SIZE, m_imgSize.y / RASTER_BLOCK_SIZE));
								m_regionStart = ((blockCount * glm::vec2(RASTER_BLOCK_SIZE)) / glm::vec2(m_imgSize.x, m_imgSize.y));

								m_isSelectingRegion = true;
							}
						}
						if (m_isSelectingRegion) {
							if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
								ImVec2 mousePos = ImGui::GetMousePos();
								m_regionEnd = glm::min(glm::vec2(1.0f, 1.0f), glm::max(glm::vec2(0.0f, 0.0f), glm::vec2((mousePos.x - screenCursorPos.x) / iSize.x, (mousePos.y - screenCursorPos.y) / iSize.y)));
								m_isSelectingRegion = false;

								// round the result to blocks
								glm::vec2 blockCount = glm::ceil((m_regionEnd - m_regionStart) * glm::vec2(m_imgSize.x / RASTER_BLOCK_SIZE, m_imgSize.y / RASTER_BLOCK_SIZE));
								m_regionEnd = m_regionStart + ((blockCount * glm::vec2(RASTER_BLOCK_SIZE)) / glm::vec2(m_imgSize.x, m_imgSize.y));
							}
							if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
								ImVec2 mousePos = ImGui::GetMousePos();
								m_regionEnd = glm::min(glm::vec2(1.0f, 1.0f), glm::max(glm::vec2(0.0f, 0.0f), glm::vec2((mousePos.x - screenCursorPos.x) / iSize.x, (mousePos.y - screenCursorPos.y) / iSize.y)));

								// round the result to blocks
								glm::vec2 blockCount = glm::ceil((m_regionEnd - m_regionStart) * glm::vec2(m_imgSize.x / RASTER_BLOCK_SIZE, m_imgSize.y / RASTER_BLOCK_SIZE));
								m_regionEnd = m_regionStart + ((blockCount * glm::vec2(RASTER_BLOCK_SIZE)) / glm::vec2(m_imgSize.x, m_imgSize.y));
							}
						}

						// render the selected region
						if (m_regionEnd != m_regionStart) {
							ImGui::SetCursorPos(ImVec2(cursorPos.x + iSize.x * m_regionStart.x, cursorPos.y + iSize.y * m_regionStart.y));
							ImGui::Image((ImTextureID)m_data->Renderer.GetTexture(), ImVec2((m_regionEnd.x - m_regionStart.x) * iSize.x, (m_regionEnd.y - m_regionStart.y) * iSize.y), ImVec2(m_regionStart.x, 1.0f - m_regionStart.y), ImVec2(m_regionEnd.x, 1.0f - m_regionEnd.y));
						}
					}

					// controls
					ImGui::TableSetColumnIndex(1);
					if (!isFullFrame) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
					if (ImGui::Button("Region", ImVec2(-FLT_MIN, 0)))
						m_isAnalyzingFullFrame = false;

					if (!isFullFrame)
						ImGui::PopStyleColor();
					else
						ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
					if (ImGui::Button("Full frame", ImVec2(-FLT_MIN, 0)))
						m_isAnalyzingFullFrame = true;
					if (isFullFrame) ImGui::PopStyleColor();

					ImGui::NewLine();
					if (isFullFrame)
						ImGui::TextWrapped("WARNING: Running the analyzer on the whole frame might be very slow. Please save your project before starting the analyzer.");
					else
						ImGui::TextWrapped("Please select the area that you want to analyze.");

					ImGui::EndTable();
				}
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Breakpoints")) {
				ImGui::Text("Here's a list of relevant breakpoints. Select up to 8 breakpoints");
				if (ImGui::BeginTable("##breakpoints_table", 5, ImGuiTableFlags_BordersHInner, ImVec2(0, -Settings::Instance().CalculateSize(45)))) {
					ImGui::TableSetupColumn("Global##bkpt_isglob", ImGuiTableColumnFlags_WidthAlwaysAutoResize);
					ImGui::TableSetupColumn("Color##bkpt_globclr", ImGuiTableColumnFlags_WidthAlwaysAutoResize);
					ImGui::TableSetupColumn("Condition##bkpt_cond", ImGuiTableColumnFlags_WidthStretch);
					ImGui::TableSetupColumn("File##bkpt_file", ImGuiTableColumnFlags_WidthStretch);
					ImGui::TableSetupColumn("Line##bkpt_line", ImGuiTableColumnFlags_WidthAlwaysAutoResize);
					ImGui::TableAutoHeaders();

					for (int i = 0; i < m_analyzerBreakpoint.size(); i++) {
						ImGui::TableNextRow();
						ImGui::PushID(i);

						// checkbox
						ImGui::TableSetColumnIndex(0);
						bool isGlobal = m_analyzerBreakpointGlobal[i];
						if (ImGui::Checkbox("##is_global", &isGlobal)) {
							// limit to 8 global breakpoints (uint8_t)
							int globalBkpts = 0;
							for (int j = 0; j < m_analyzerBreakpointGlobal.size(); j++) {
								if (j != i && m_analyzerBreakpointGlobal[j])
									globalBkpts++;
							}
							if (globalBkpts >= 8)
								isGlobal = false;

							m_analyzerBreakpointGlobal[i] = isGlobal;
						}

						// color
						ImGui::TableSetColumnIndex(1);
						ImGui::ColorEdit3("##global_color", glm::value_ptr(m_analyzerBreakpointColor[i]), ImGuiColorEditFlags_NoInputs);

						// condition
						ImGui::TableSetColumnIndex(2);
						const std::string& cond = m_analyzerBreakpoint[i]->Condition;
						if (cond.size() == 0) ImGui::Text("-- N/A --");
						else ImGui::TextUnformatted(cond.c_str());

						// file
						ImGui::TableSetColumnIndex(3);
						ImGui::TextUnformatted(m_analyzerBreakpointPath[i]);

						// line
						ImGui::TableSetColumnIndex(4);
						ImGui::Text("%d", m_analyzerBreakpoint[i]->Line);

						ImGui::PopID();
					}

					ImGui::EndTable();
				}
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}

		// Analyze | Close buttons
		ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 260);
		bool canContinue = isFullFrame || ((std::abs(m_regionEnd.x - m_regionStart.x) * std::abs(m_regionEnd.y - m_regionStart.y) * m_imgSize.x * m_imgSize.y) > 100); // don't allow small regions
		if (!canContinue) {
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
		}
		if (ImGui::Button("Analyze", ImVec2(120, 0))) {
			m_runFrameAnalysis();
			ImGui::CloseCurrentPopup();
		}
		if (!canContinue) {
			ImGui::PopStyleVar();
			ImGui::PopItemFlag();
		}
		ImGui::SameLine();
		if (ImGui::Button("Close", ImVec2(120, 0)))
			ImGui::CloseCurrentPopup();
	}
	void PreviewUI::m_runFrameAnalysis()
	{
		if (m_frameAnalyzed)
			return;

		m_frameAnalyzed = true;
		m_view = PreviewView::Debugger;

		// pass breakpoints
		std::vector<const dbg::Breakpoint*> bkpts;
		std::vector<glm::vec3> bkptColors;
		std::vector<const char*> bkptPaths;
		for (int i = 0; i < m_analyzerBreakpoint.size(); i++) {
			if (m_analyzerBreakpointGlobal[i]) {
				bkpts.push_back(m_analyzerBreakpoint[i]);
				bkptColors.push_back(m_analyzerBreakpointColor[i]);
				bkptPaths.push_back(m_analyzerBreakpointPath[i]);
			}
		}
		m_data->Analysis.SetBreakpoints(bkpts, bkptColors, bkptPaths);

		// initialize buffers
		glm::vec4 clearColor = Settings::Instance().Project.ClearColor;
		clearColor.a = Settings::Instance().Project.UseAlphaChannel ? clearColor.a : 1.0f;
		m_data->Analysis.Init(m_imgSize.x, m_imgSize.y, clearColor);

		// turn on "region based" rendering
		if (!m_isAnalyzingFullFrame) {
			m_data->Analysis.Copy(m_data->Renderer.GetTexture(), m_imgSize.x, m_imgSize.y);
			float minX = std::min<float>(m_regionEnd.x, m_regionStart.x);
			float minY = std::min<float>(m_regionStart.y, m_regionEnd.y);
			float maxX = std::max<float>(m_regionEnd.x, m_regionStart.x);
			float maxY = std::max<float>(m_regionEnd.y, m_regionStart.y);	
			m_data->Analysis.SetRegion(minX * m_imgSize.x, (1.0f - maxY) * m_imgSize.y, maxX * m_imgSize.x, (1.0f - minY) * m_imgSize.y);
		}
		
		// find the range of passes to render
		int passStartPos = -1;
		int passEndPos = -1;
		auto& passes = m_data->Pipeline.GetList();
		for (int i = 0; i < passes.size(); i++) {
			if (passes[i]->Type == PipelineItem::ItemType::ShaderPass) {
				pipe::ShaderPass* pass = (pipe::ShaderPass*)passes[i]->Data;
				bool isWindowUsed = false;

				for (int j = 0; j < pass->RTCount; j++)
					if (pass->RenderTextures[j] == m_data->Renderer.GetTexture()) {
						isWindowUsed = true;
						break;
					}

				if (isWindowUsed) {
					if (passStartPos == -1 || i - 1 != passStartPos)
						passStartPos = passEndPos = i;
					else if (i - 1 == passStartPos)
						passEndPos = i;
				}
			}
		}

		// render the pass
		if (passStartPos != -1 && passEndPos != -1) {
			for (int i = passStartPos; i <= passEndPos; i++)
				m_data->Analysis.RenderPass(passes[i]);
		}

		// build a histogram and other stuff
		((FrameAnalysisUI*)m_ui->Get(ViewID::FrameAnalysis))->Process();

		// TODO: ayo, why didn't I create gl::CreateTexture() ???

		// normal texture
		glDeleteTextures(1, &m_viewDebugger);
		glGenTextures(1, &m_viewDebugger);
		glBindTexture(GL_TEXTURE_2D, m_viewDebugger);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_imgSize.x, m_imgSize.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, m_data->Analysis.GetColorOutput());
	
		// heatmap
		float* heatmap = m_data->Analysis.AllocateHeatmap();
		glDeleteTextures(1, &m_viewHeatmap);
		glGenTextures(1, &m_viewHeatmap);
		glBindTexture(GL_TEXTURE_2D, m_viewHeatmap);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, m_imgSize.x, m_imgSize.y, 0, GL_RGB, GL_FLOAT, heatmap);
		free(heatmap);

		// undefined behavior
		uint32_t* ubMap = m_data->Analysis.AllocateUndefinedBehaviorMap();
		glDeleteTextures(1, &m_viewUB);
		glGenTextures(1, &m_viewUB);
		glBindTexture(GL_TEXTURE_2D, m_viewUB);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_imgSize.x, m_imgSize.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, ubMap);
		free(ubMap);

		// global breakpoints
		if (m_data->Analysis.HasGlobalBreakpoints()) {
			uint32_t* globBkptMap = m_data->Analysis.AllocateGlobalBreakpointsMap();
			glDeleteTextures(1, &m_viewBreakpoints);
			glGenTextures(1, &m_viewBreakpoints);
			glBindTexture(GL_TEXTURE_2D, m_viewBreakpoints);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_imgSize.x, m_imgSize.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, globBkptMap);
			free(globBkptMap);
		}

		// refresh variable value
		SetVariableValue(m_varValueItem, m_varValueName, m_varValueLine);
	}
	void PreviewUI::m_buildBreakpointList()
	{
		m_analyzerBreakpoint.clear();
		m_analyzerBreakpointGlobal.clear();
		m_analyzerBreakpointColor.clear();

		// find the passes that contain relevant breakpoints
		int passStartPos = -1;
		int passEndPos = -1;
		auto& passes = m_data->Pipeline.GetList();
		for (int i = 0; i < passes.size(); i++) {
			if (passes[i]->Type == PipelineItem::ItemType::ShaderPass) {
				pipe::ShaderPass* pass = (pipe::ShaderPass*)passes[i]->Data;
				bool isWindowUsed = false;

				for (int j = 0; j < pass->RTCount; j++)
					if (pass->RenderTextures[j] == m_data->Renderer.GetTexture()) {
						isWindowUsed = true;
						break;
					}

				if (isWindowUsed) {
					if (passStartPos == -1 || i - 1 != passStartPos)
						passStartPos = passEndPos = i;
					else if (i - 1 == passStartPos)
						passEndPos = i;
				}
			}
		}

		// process the breakpoints
		const auto& bkptFiles = m_data->Debugger.GetBreakpointList();
		if (passStartPos != -1 && passEndPos != -1) {
			for (int i = passStartPos; i <= passEndPos; i++) {
				const char* psPath = ((pipe::ShaderPass*)passes[i]->Data)->PSPath;
				std::string psPathAbsolute = std::filesystem::absolute(m_data->Parser.GetProjectPath(psPath)).generic_u8string();
				
				if (bkptFiles.count(psPathAbsolute) == 0)
					continue;

				const auto& bkpts = m_data->Debugger.GetBreakpointList(psPathAbsolute);
				for (const auto& bkpt : bkpts) {
					m_analyzerBreakpoint.push_back(&bkpt);
					m_analyzerBreakpointPath.push_back(psPath);
				}
			}
		}

		m_analyzerBreakpointColor.resize(m_analyzerBreakpoint.size(), glm::vec3(1.0f, 0.0f, 0.0f));
		m_analyzerBreakpointGlobal.resize(m_analyzerBreakpoint.size(), false);
	}
}