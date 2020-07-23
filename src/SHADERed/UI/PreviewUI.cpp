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
						props->Open(nullptr);
					m_data->Pipeline.Remove(m_picks[i]->Name);
				}
				m_picks.clear();
			}
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
		KeyboardShortcuts::Instance().SetCallback("Preview.TogglePause", [=]() {
			m_data->Renderer.Pause(!m_data->Renderer.IsPaused());
			m_data->Objects.Pause(m_data->Renderer.IsPaused());
			m_ui->StopDebugging();
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
	void PreviewUI::Update(float delta)
	{
		if (!m_data->Messages.CanRenderPreview()) {
			ImGui::TextColored(ThemeContainer::Instance().GetCustomStyle(Settings::Instance().Theme).ErrorMessage, "Can not display preview - there are some errors you should fix.");
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

		GLuint rtView = renderer->GetTexture();
		
		m_imgPosition = ImGui::GetCursorScreenPos();

		// display the image on the imgui window
		const glm::vec2& zPos = m_zoom.GetZoomPosition();
		const glm::vec2& zSize = m_zoom.GetZoomSize();
		ImGui::Image((void*)rtView, imageSize, ImVec2(zPos.x, zPos.y + zSize.y), ImVec2(zPos.x + zSize.x, zPos.y));

		m_hasFocus = ImGui::IsWindowFocused();

		if (paused && m_zoomLastSize != renderer->GetLastRenderSize() &&!m_data->Debugger.IsDebugging()) {
			renderer->Render(imageSize.x, imageSize.y);
			pixelList.clear();
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

			if (settings.Preview.Gizmo && m_picks.size() != 0 && pixelList.size() == 0) {
				m_gizmo.SetProjectionMatrix(SystemVariableManager::Instance().GetProjectionMatrix());
				m_gizmo.SetViewMatrix(SystemVariableManager::Instance().GetCamera()->GetMatrix());
				m_gizmo.Render();
			}

			if (settings.Preview.BoundingBox && m_picks.size() != 0 && pixelList.size() == 0)
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

		// mouse controls for preview window
		m_mouseHovers = ImGui::IsItemHovered();
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
			if (isNotMinimalMode && ((ImGui::IsMouseClicked(0) && !settings.Preview.SwitchLeftRightClick) || (ImGui::IsMouseClicked(1) && settings.Preview.SwitchLeftRightClick)) && (settings.Preview.Gizmo || settings.Preview.BoundingBox) && !ImGui::GetIO().KeyAlt) {
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
				ed::FirstPersonCamera* cam = ((ed::FirstPersonCamera*)SystemVariableManager::Instance().GetCamera());
				cam->MoveUpDown((ImGui::IsKeyDown(SDL_SCANCODE_S) - ImGui::IsKeyDown(SDL_SCANCODE_W)) / 70.0f);
				cam->MoveLeftRight((ImGui::IsKeyDown(SDL_SCANCODE_D) - ImGui::IsKeyDown(SDL_SCANCODE_A)) / 70.0f);
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
				if (pixelList[i].RenderTexture.empty() && pixelList[i].Fetched) { // we only care about window's pixel info here

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

		float pauseStartX = (width - ((ICON_BUTTON_WIDTH * 2) + (BUTTON_INDENT * 1))) / 2;
		if (ImGui::GetCursorPosX() >= pauseStartX - 100)
			ImGui::SameLine();
		else
			ImGui::SetCursorPosX(pauseStartX);


		/* PAUSE BUTTON */
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		if (ImGui::Button(m_data->Renderer.IsPaused() ? UI_ICON_PLAY : UI_ICON_PAUSE, ImVec2(ICON_BUTTON_WIDTH, BUTTON_SIZE))) {
			m_data->Renderer.Pause(!m_data->Renderer.IsPaused());
			m_data->Objects.Pause(m_data->Renderer.IsPaused());
			m_ui->StopDebugging();
		}

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
		Logger::Get().Log("Setting up bounding box...");

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
}