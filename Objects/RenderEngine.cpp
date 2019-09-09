#include "RenderEngine.h"
#include "Logger.h"
#include "Settings.h"
#include "HLSL2GLSL.h"
#include "DefaultState.h"
#include "ObjectManager.h"
#include "PipelineManager.h"
#include "SystemVariableManager.h"
#include "../Engine/GeometryFactory.h"
#include "../Engine/GLUtils.h"
#include "../Engine/Ray.h"

#include <algorithm>
#include <glm/gtx/intersect.hpp>

static const GLenum fboBuffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4, GL_COLOR_ATTACHMENT5, GL_COLOR_ATTACHMENT6, GL_COLOR_ATTACHMENT7, GL_COLOR_ATTACHMENT8, GL_COLOR_ATTACHMENT9, GL_COLOR_ATTACHMENT10, GL_COLOR_ATTACHMENT11, GL_COLOR_ATTACHMENT12, GL_COLOR_ATTACHMENT13, GL_COLOR_ATTACHMENT14, GL_COLOR_ATTACHMENT15 };

namespace ed
{
	RenderEngine::RenderEngine(PipelineManager * pipeline, ObjectManager* objects, ProjectParser* project, MessageStack* msgs) :
		m_pipeline(pipeline),
		m_objects(objects),
		m_project(project),
		m_msgs(msgs),
		m_lastSize(0, 0),
		m_pickAwaiting(false),
		m_rtColor(0),
		m_rtDepth(0),
		m_fbosNeedUpdate(false),
		m_wasMultiPick(false)
	{
		glGenTextures(1, &m_rtColor);
		glGenTextures(1, &m_rtDepth);
	}
	RenderEngine::~RenderEngine()
	{
		glDeleteTextures(1, &m_rtColor);
		glDeleteTextures(1, &m_rtDepth);
		FlushCache();
	}
	void RenderEngine::Render(int width, int height)
	{
		// recreate render texture if size has changed
		if (m_lastSize.x != width || m_lastSize.y != height) {
			m_lastSize = glm::vec2(width, height);

			glBindTexture(GL_TEXTURE_2D, m_rtColor);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glBindTexture(GL_TEXTURE_2D, 0);

			glBindTexture(GL_TEXTURE_2D, m_rtDepth);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, width, height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glBindTexture(GL_TEXTURE_2D, 0);

			// update
			std::vector<std::string> objs = m_objects->GetObjects();
			for (int i = 0; i < objs.size(); i++) {
				if (m_objects->IsRenderTexture(objs[i])) {
					ed::RenderTextureObject* rtObj = m_objects->GetRenderTexture(m_objects->GetTexture(objs[i]));
					if (rtObj != nullptr && rtObj->FixedSize.x == -1)
						m_objects->ResizeRenderTexture(objs[i], rtObj->CalculateSize(width, height));
				}
			}
		}

		//glEnable(GL_CULL_FACE);

		// cache elements
		m_cache();

		auto& systemVM = SystemVariableManager::Instance();

		auto& itemVarValues = GetItemVariableValues();
		GLuint previousTexture[MAX_RENDER_TEXTURES] = { 0 }; // dont clear the render target if we use it two times in a row
		GLuint previousDepth = 0;
		bool clearedWindow = false;
		bool changedState = false;

		for (int i = 0; i < m_items.size(); i++) {
			PipelineItem* it = m_items[i];
			pipe::ShaderPass* data = (pipe::ShaderPass*)it->Data;

			if (data->Items.size() <= 0)
				continue;

			std::vector<GLuint> srvs = m_objects->GetBindList(m_items[i]);
			std::vector<std::string> ubos = m_objects->GetUniformBindList(m_items[i]);

			if (data->RTCount == 0)
				continue;

			// create/update fbo if necessary
			m_updatePassFBO(data);

			if (m_shaders[i] == 0)
				continue;

			// bind fbo and buffers
			glBindFramebuffer(GL_FRAMEBUFFER, data->FBO);
			glDrawBuffers(data->RTCount, fboBuffers);

			// clear depth texture
			if (data->DepthTexture != previousDepth) {
				if ((data->DepthTexture == m_rtDepth && !clearedWindow) || data->DepthTexture != m_rtDepth) {
					glStencilMask(0xFFFFFFFF);
					glClearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 0);
				}

				previousDepth = data->DepthTexture;
			}

			// bind RTs
			int rtCount = MAX_RENDER_TEXTURES;
			glm::vec2 rtSize(width, height);
			for (int i = 0; i < MAX_RENDER_TEXTURES; i++) {
				if (data->RenderTextures[i] == 0) {
					rtCount = i;
					break;
				}

				GLuint rt = data->RenderTextures[i];

				if (rt != m_rtColor) {
					ed::RenderTextureObject* rtObject = m_objects->GetRenderTexture(rt);

					rtSize = rtObject->CalculateSize(width, height);

					// clear and bind rt (only if not used in last shader pass)
					bool usedPreviously = false;
					for (int j = 0; j < MAX_RENDER_TEXTURES; j++)
						if (previousTexture[j] == rt) {
							usedPreviously = true;
							break;
						}
					if (!usedPreviously && rtObject->Clear)
						glClearBufferfv(GL_COLOR, i, glm::value_ptr(rtObject->ClearColor));

				}
				else if (!clearedWindow) {
					glClearBufferfv(GL_COLOR, i, glm::value_ptr(Settings::Instance().Project.ClearColor));

					clearedWindow = true;
				}
			}
			for (int i = 0; i < data->RTCount; i++)
				previousTexture[i] = data->RenderTextures[i];

			// update viewport value
			systemVM.SetViewportSize(rtSize.x, rtSize.y);
			glViewport(0, 0, rtSize.x, rtSize.y);

			// bind shaders
			glUseProgram(m_shaders[i]);

			// bind shader resource views
			for (int j = 0; j < srvs.size(); j++) {
				glActiveTexture(GL_TEXTURE0 + j);
				if (m_objects->IsCubeMap(srvs[j]))
					glBindTexture(GL_TEXTURE_CUBE_MAP, srvs[j]);
				else
					glBindTexture(GL_TEXTURE_2D, srvs[j]);

				if (!HLSL2GLSL::IsHLSL(data->PSPath))
					data->Variables.UpdateTexture(m_shaders[i], j);
			}

			// TODO: bind ubos
			for (int j = 0; j < ubos.size(); j++)
				glBindBufferBase(GL_UNIFORM_BUFFER, j, m_objects->GetBuffer(ubos[j])->ID);
			
			// clear messages
			if (m_msgs->GetGroupWarningMsgCount(it->Name) > 0)
				m_msgs->ClearGroup(it->Name, (int)ed::MessageStack::Type::Warning);

			// bind default states for each shader pass
			if (changedState) {
				DefaultState::Bind();
				changedState = false;
			}

			// render pipeline items
			for (int j = 0; j < data->Items.size(); j++) {
				PipelineItem* item = data->Items[j];

				systemVM.SetPicked(false);

				// update the value for this element and check if we picked it
				if (item->Type == PipelineItem::ItemType::Geometry || item->Type == PipelineItem::ItemType::Model) {
					if (m_pickAwaiting) m_pickItem(item, m_wasMultiPick);
					for (int k = 0; k < itemVarValues.size(); k++)
						if (itemVarValues[k].Item == item)
							itemVarValues[k].Variable->Data = itemVarValues[k].NewValue->Data;
				}

				if (item->Type == PipelineItem::ItemType::Geometry) {
					pipe::GeometryItem* geoData = reinterpret_cast<pipe::GeometryItem*>(item->Data);

					if (geoData->Type == pipe::GeometryItem::Rectangle) {
						glm::vec3 scaleRect(geoData->Scale.x * width, geoData->Scale.y * height, 1.0f);
						glm::vec3 posRect((geoData->Position.x + 0.5f) * width, (geoData->Position.y + 0.5f) * height, -1000.0f);
						systemVM.SetGeometryTransform(item, scaleRect, geoData->Rotation, posRect);
					} else
						systemVM.SetGeometryTransform(item, geoData->Scale, geoData->Rotation, geoData->Position);

					systemVM.SetPicked(std::count(m_pick.begin(), m_pick.end(), item));

					// bind variables
					data->Variables.Bind(item);

					glBindVertexArray(geoData->VAO);
					glDrawArrays(geoData->Topology, 0, eng::GeometryFactory::VertexCount[geoData->Type]);
				}
				else if (item->Type == PipelineItem::ItemType::Model) {
					pipe::Model* objData = reinterpret_cast<pipe::Model*>(item->Data);

					systemVM.SetPicked(std::count(m_pick.begin(), m_pick.end(), item));
					systemVM.SetGeometryTransform(item, objData->Scale, objData->Rotation, objData->Position);

					// bind variables
					data->Variables.Bind(item);

					objData->Data->Draw();
				}
				else if (item->Type == PipelineItem::ItemType::RenderState) {
					pipe::RenderState* state = reinterpret_cast<pipe::RenderState*>(item->Data);
					changedState = true;

					// depth clamp
					if (state->DepthClamp)
						glEnable(GL_DEPTH_CLAMP);
					else
						glDisable(GL_DEPTH_CLAMP);

					// fill mode
					glPolygonMode(GL_FRONT_AND_BACK, state->PolygonMode);

					// culling and front face
					if (state->CullFace)
						glEnable(GL_CULL_FACE);
					else
						glDisable(GL_CULL_FACE);
					glCullFace(state->CullFaceType);
					glFrontFace(state->FrontFace);

					// disable blending
					if (state->Blend) {
						glEnable(GL_BLEND);
						glBlendEquationSeparate(state->BlendFunctionColor, state->BlendFunctionAlpha);
						glBlendFuncSeparate(state->BlendSourceFactorRGB, state->BlendDestinationFactorRGB, state->BlendSourceFactorAlpha, state->BlendDestinationFactorAlpha);
						glBlendColor(state->BlendFactor.r, state->BlendFactor.g, state->BlendFactor.a, state->BlendFactor.a);
						glSampleCoverage(state->AlphaToCoverage, GL_FALSE);
					}
					else
						glDisable(GL_BLEND);

					// depth state
					if (state->DepthTest)
						glEnable(GL_DEPTH_TEST);
					else
						glDisable(GL_DEPTH_TEST);
					glDepthMask(state->DepthMask);
					glDepthFunc(state->DepthFunction);
					glPolygonOffset(0.0f, state->DepthBias);

					// stencil
					if (state->StencilTest) {
						glEnable(GL_STENCIL_TEST);
						glStencilFuncSeparate(GL_FRONT, state->StencilFrontFaceFunction, 1, state->StencilReference);
						glStencilFuncSeparate(GL_BACK, state->StencilBackFaceFunction, 1, state->StencilReference);
						glStencilMask(state->StencilMask);
						glStencilOpSeparate(GL_FRONT, state->StencilFrontFaceOpStencilFail, state->StencilFrontFaceOpDepthFail, state->StencilFrontFaceOpPass);
						glStencilOpSeparate(GL_BACK, state->StencilBackFaceOpStencilFail, state->StencilBackFaceOpDepthFail, state->StencilBackFaceOpPass);
					}
					else
						glDisable(GL_STENCIL_TEST);
				}

				// set the old value back
				if (item->Type == PipelineItem::ItemType::Geometry || item->Type == PipelineItem::ItemType::Model)
					for (int k = 0; k < itemVarValues.size(); k++)
						if (itemVarValues[k].Item == item)
							itemVarValues[k].Variable->Data = itemVarValues[k].OldValue;

			}
		}

		// update frame index
		if (!m_paused) {
			systemVM.CopyState();
			systemVM.SetFrameIndex(systemVM.GetFrameIndex() + 1);
		}

		// bind default state after rendering everything
		if (changedState)
			DefaultState::Bind();

		// restore real render target view
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		if (m_pickDist == std::numeric_limits<float>::infinity())
			m_pick.clear();
		if (m_pickAwaiting && m_pickHandle != nullptr)
			m_pickHandle(m_pick.size() == 0 ? nullptr : m_pick[m_pick.size()-1]);
		m_pickAwaiting = false;
	}
	void RenderEngine::Pause(bool pause)
	{
		m_paused = pause;

		if (m_paused)
			SystemVariableManager::Instance().GetTimeClock().Pause();
		else 
			SystemVariableManager::Instance().GetTimeClock().Resume();
	}
	void RenderEngine::Recompile(const char * name)
	{
		Logger::Get().Log("Recompiling " + std::string(name)); 
		
		m_msgs->BuildOccured = true;
		m_msgs->CurrentItem = name;

		GLchar cMsg[1024];

		int d3dCounter = 0;
		for (int i = 0; i < m_items.size(); i++) {
			PipelineItem* item = m_items[i];
			if (strcmp(item->Name, name) == 0) {
				pipe::ShaderPass* shader = (pipe::ShaderPass*)item->Data;

				m_msgs->ClearGroup(name);

				std::string psContent = "", vsContent = "",
					vsEntry = shader->VSEntry,
					psEntry = shader->PSEntry;

				// pixel shader
				m_msgs->CurrentItemType = 1;
				if (!HLSL2GLSL::IsHLSL(shader->PSPath)) {// GLSL
					psContent = m_project->LoadProjectFile(shader->PSPath);
					m_applyMacros(psContent, shader);
				} else {// HLSL
					psContent = ed::HLSL2GLSL::Transcompile(m_project->GetProjectPath(std::string(shader->PSPath)), 1, shader->PSEntry, shader->Macros, shader->GSUsed, m_msgs);
					psEntry = "main";
				}

				shader->Variables.UpdateTextureList(psContent);
				GLuint ps = gl::CompileShader(GL_FRAGMENT_SHADER, psContent.c_str());
				bool psCompiled = gl::CheckShaderCompilationStatus(ps, cMsg);

				if (!psCompiled && !HLSL2GLSL::IsHLSL(shader->PSPath))
					m_msgs->Add(gl::ParseMessages(name, 1, cMsg));

				// vertex shader
				m_msgs->CurrentItemType = 0;
				if (!HLSL2GLSL::IsHLSL(shader->VSPath)) {// GLSL
					vsContent = m_project->LoadProjectFile(shader->VSPath);
					m_applyMacros(vsContent, shader);
				} else { // HLSL
					vsContent = ed::HLSL2GLSL::Transcompile(m_project->GetProjectPath(std::string(shader->VSPath)), 0, shader->VSEntry, shader->Macros, shader->GSUsed, m_msgs);
					vsEntry = "main";
				}

				GLuint vs = gl::CompileShader(GL_VERTEX_SHADER, vsContent.c_str());
				bool vsCompiled = gl::CheckShaderCompilationStatus(vs, cMsg);

				if (!vsCompiled && !HLSL2GLSL::IsHLSL(shader->VSPath))
					m_msgs->Add(gl::ParseMessages(name, 0, cMsg));

				// geometry shader
				bool gsCompiled = true;
				GLuint gs = 0;
				if (shader->GSUsed && strlen(shader->GSPath) > 0 && strlen(shader->GSEntry) > 0) {
					std::string gsContent = "",
						gsEntry = shader->GSEntry;

					m_msgs->CurrentItemType = 2;
					if (!HLSL2GLSL::IsHLSL(shader->GSPath)) { // GLSL
						gsContent = m_project->LoadProjectFile(shader->GSPath);
						m_applyMacros(gsContent, shader);
					} else { // HLSL
						gsContent = ed::HLSL2GLSL::Transcompile(m_project->GetProjectPath(std::string(shader->GSPath)), 2, shader->GSEntry, shader->Macros, shader->GSUsed, m_msgs);
						gsEntry = "main";
					}

					gs = gl::CompileShader(GL_GEOMETRY_SHADER, gsContent.c_str());
					gsCompiled = gl::CheckShaderCompilationStatus(gs, cMsg);

					if (!gsCompiled && !HLSL2GLSL::IsHLSL(shader->GSPath))
						m_msgs->Add(gl::ParseMessages(name, 2, cMsg));
				}

				if (m_shaders[i] != 0)
					glDeleteProgram(m_shaders[i]);

				if (!vsCompiled || !psCompiled || !gsCompiled) {
					Logger::Get().Log("Shaders not compilied", true); 
					m_msgs->Add(MessageStack::Type::Error, name, "Failed to compile the shader(s)");
					m_shaders[i] = 0;
				}
				else {
					m_msgs->Add(MessageStack::Type::Message, name, "Compiled the shaders.");

					m_shaders[i] = glCreateProgram();
					glAttachShader(m_shaders[i], vs);
					glAttachShader(m_shaders[i], ps);
					if (shader->GSUsed) glAttachShader(m_shaders[i], gs);
					glLinkProgram(m_shaders[i]);
				}

				if (m_shaders[i] != 0)
					shader->Variables.UpdateUniformInfo(m_shaders[i]);

				if (vs != 0) glDeleteShader(vs);
				if (ps != 0) glDeleteShader(ps);
				if (gs != 0) glDeleteShader(gs);
			}
		}
	}
	void RenderEngine::Pick(float sx, float sy, bool multiPick, std::function<void(PipelineItem*)> func)
	{
		m_pickAwaiting = true;
		m_pickDist = std::numeric_limits<float>::infinity();
		m_pickHandle = func;
		m_wasMultiPick = multiPick;
		
		float mouseX = sx / (m_lastSize.x * 0.5f) - 1.0f;
		float mouseY = sy / (m_lastSize.y * 0.5f) - 1.0f;

		glm::mat4 proj = SystemVariableManager::Instance().GetProjectionMatrix();
		glm::mat4 view = SystemVariableManager::Instance().GetCamera()->GetMatrix();

		glm::mat4 invVP = glm::inverse(proj * view);
		glm::vec4 screenPos(mouseX, mouseY, 1.0f, 1.0f);
		glm::vec4 worldPos = invVP * screenPos;

		m_pickDir = glm::normalize(glm::vec3(worldPos));
		m_pickOrigin = SystemVariableManager::Instance().GetCamera()->GetPosition();
	}
	void RenderEngine::m_pickItem(PipelineItem* item, bool multiPick)
	{
		glm::mat4 world(1);
		if (item->Type == PipelineItem::ItemType::Geometry) {
			pipe::GeometryItem* geo = (pipe::GeometryItem*)item->Data;
			if (geo->Type == pipe::GeometryItem::GeometryType::Rectangle)
				return;

			world = glm::translate(glm::mat4(1), geo->Position) * glm::yawPitchRoll(geo->Rotation.y, geo->Rotation.x, geo->Rotation.z);
		}
		else if (item->Type == PipelineItem::ItemType::Model) {
			pipe::Model* obj = (pipe::Model*)item->Data;
			
			world = glm::translate(glm::mat4(1), obj->Position) * glm::yawPitchRoll(obj->Rotation.y, obj->Rotation.x, obj->Rotation.z);
		}
		glm::mat4 invWorld = glm::inverse(world);
		
		glm::vec4 rayOrigin = invWorld * glm::vec4(m_pickOrigin, 1);
		glm::vec4 rayDir = invWorld * glm::vec4(m_pickDir, 0.0f);
		
		glm::vec3 vec3Origin = glm::vec3(rayOrigin);
		glm::vec3 vec3Dir = glm::vec3(rayDir);

		float myDist = std::numeric_limits<float>::infinity();
		if (item->Type == PipelineItem::ItemType::Geometry) {
			pipe::GeometryItem* geo = (pipe::GeometryItem*)item->Data;
			if (geo->Type == pipe::GeometryItem::GeometryType::Cube) {
				glm::vec3 b1(-geo->Size.x * geo->Scale.x / 2, -geo->Size.y * geo->Scale.y / 2, -geo->Size.z * geo->Scale.z / 2);
				glm::vec3 b2(geo->Size.x * geo->Scale.x / 2, geo->Size.y * geo->Scale.y / 2, geo->Size.z * geo->Scale.z / 2);

				float distHit;
				if (ray::IntersectBox(b1, b2, vec3Origin, vec3Dir, distHit))
					myDist = distHit;
			}
			else if (geo->Type == pipe::GeometryItem::GeometryType::Triangle) {
				float size = geo->Size.x * geo->Scale.x;
				float rightOffs = size / tan(glm::radians(30.0f));
				glm::vec3 v0(0, -size, 0);
				glm::vec3 v1(-rightOffs, size, 0);
				glm::vec3 v2(rightOffs, size, 0);

				float hit;
				if (ray::IntersectTriangle(vec3Origin, glm::normalize(vec3Dir), v0, v1, v2, hit))
					myDist = hit;
			}
			else if (geo->Type == pipe::GeometryItem::GeometryType::Sphere) {
				float r = geo->Size.x * geo->Scale.x;
				r *= r;

				glm::intersectRaySphere(vec3Origin, glm::normalize(vec3Dir), glm::vec3(0), r, myDist);
			}
			else if (geo->Type == pipe::GeometryItem::GeometryType::Plane) {
				glm::vec3 b1(-geo->Size.x * geo->Scale.x / 2, -geo->Size.y * geo->Scale.y / 2, -0.0001f);
				glm::vec3 b2(geo->Size.x * geo->Scale.x / 2, geo->Size.y * geo->Scale.y / 2, 0.0001f);

				float hit;
				if (ray::IntersectBox(b1, b2, vec3Origin, vec3Dir, hit))
					myDist = hit;
			}
			else if (geo->Type == pipe::GeometryItem::GeometryType::Circle) {

				glm::vec3 b1(-geo->Size.x * geo->Scale.x, -geo->Size.y * geo->Scale.y, -0.0001f);
				glm::vec3 b2(geo->Size.x * geo->Scale.x, geo->Size.y * geo->Scale.y, 0.0001f);

				float hit;
				if (ray::IntersectBox(b1, b2, vec3Origin, vec3Dir, hit))
					myDist = hit;
			}
		}
		else if (item->Type == PipelineItem::ItemType::Model) {
			pipe::Model* obj = (pipe::Model*)item->Data;

			glm::vec3 minb = obj->Data->GetMinBound();
			glm::vec3 maxb = obj->Data->GetMaxBound();

			float triDist = std::numeric_limits<float>::infinity();
			if (ray::IntersectBox(minb, maxb, vec3Origin, vec3Dir, triDist)) { // TODO: test this optimization
				if (triDist < m_pickDist) { // optimization: check if bounding box is closer than selected object
					bool donetris = false;
					for (auto& mesh : obj->Data->Meshes) {
						for (int i = 0; i+2 < mesh.Vertices.size(); i+=3) {
							glm::vec3 v0 = mesh.Vertices[i + 0].Position;
							glm::vec3 v1 = mesh.Vertices[i + 1].Position;
							glm::vec3 v2 = mesh.Vertices[i + 2].Position;

							if (ray::IntersectTriangle(vec3Origin, vec3Dir, v0, v1, v2, triDist))
								if (triDist < myDist) {
									myDist = triDist;

									if (triDist < m_pickDist) {
										donetris = true;
										break;
									}
								}
						}

						if (donetris)
							break;
					}
				}
				else myDist = triDist;
			}
		}

		// did we actually pick sth that is closer?
		if (myDist < m_pickDist) {
			m_pickDist = myDist;
			AddPickedItem(item, multiPick);
		}
	}
	void RenderEngine::AddPickedItem(PipelineItem* pipe, bool multiPick)
	{
		// check if it already exists
		bool skipAdd = false;
		for (int i = 0; i < m_pick.size(); i++)
			if (m_pick[i] == pipe) {
				if (!multiPick) {
					m_pick.clear();
					m_pick.push_back(pipe);
				}
				skipAdd = true;
				break;
			}

		// add item
		if (!skipAdd) {
			if (pipe == nullptr)
				m_pick.clear();
			else if (multiPick)
				m_pick.push_back(pipe);
			else {
				m_pick.clear();
				m_pick.push_back(pipe);
			}
		}
	}
	void RenderEngine::FlushCache()
	{
		for (int i = 0; i < m_shaders.size(); i++)
			glDeleteProgram(m_shaders[i]);
		
		m_items.clear();
		m_shaders.clear();
		m_fbosNeedUpdate = true;
	}
	void RenderEngine::m_cache()
	{
		// check for any changes
		std::vector<ed::PipelineItem*>& items = m_pipeline->GetList();

		// check if no major changes were made, if so dont cache for another 0.25s
		if (m_items.size() == items.size()) {
			if (m_cacheTimer.GetElapsedTime() > 0.5f)
				m_cacheTimer.Restart();
			else return;
		}

		GLchar cMsg[1024];

		// check if some item was added
		for (int i = 0; i < items.size(); i++) {
			bool found = false;
			for (int j = 0; j < m_items.size(); j++)
				if (items[i]->Data == m_items[j]->Data) {
					found = true;
					break;
				}

			if (!found) {
				Logger::Get().Log("Caching a new shader pass " + std::string(items[i]->Name));

				pipe::ShaderPass* data = reinterpret_cast<ed::pipe::ShaderPass*>(items[i]->Data);

				m_items.insert(m_items.begin() + i, items[i]);
				m_shaders.insert(m_shaders.begin() + i, 0);

				if (strlen(data->VSPath) == 0 || strlen(data->PSPath) == 0) {
					Logger::Get().Log("No shader paths are set", true);
					continue;
				}

				/*
					ITEM CACHING
				*/

				m_fbos[data].resize(MAX_RENDER_TEXTURES);

				GLuint ps = 0, vs = 0, gs = 0;

				m_msgs->CurrentItem = items[i]->Name;

				std::string psContent = "", vsContent = "",
					vsEntry = data->VSEntry,
					psEntry = data->PSEntry;

				// vertex shader
				m_msgs->CurrentItemType = 0;
				if (!HLSL2GLSL::IsHLSL(data->VSPath)) { // GLSL
					vsContent = m_project->LoadProjectFile(data->VSPath);
					m_applyMacros(vsContent, data);
				} else { // HLSL
					vsContent = ed::HLSL2GLSL::Transcompile(m_project->GetProjectPath(std::string(data->VSPath)), 0, data->VSEntry, data->Macros, data->GSUsed, m_msgs);
					vsEntry = "main";
				}
				
				vs = gl::CompileShader(GL_VERTEX_SHADER, vsContent.c_str());
				bool vsCompiled = gl::CheckShaderCompilationStatus(vs, cMsg);

				if (!vsCompiled && !HLSL2GLSL::IsHLSL(data->VSPath))
					m_msgs->Add(gl::ParseMessages(m_msgs->CurrentItem, 0, cMsg));

				// pixel shader
				m_msgs->CurrentItemType = 1;
				if (!HLSL2GLSL::IsHLSL(data->PSPath)) {// HLSL
					psContent = m_project->LoadProjectFile(data->PSPath);
					m_applyMacros(psContent, data);
				} else { // GLSL
					psContent = ed::HLSL2GLSL::Transcompile(m_project->GetProjectPath(std::string(data->PSPath)), 1, data->PSEntry, data->Macros, data->GSUsed, m_msgs);
					psEntry = "main";
				}

				data->Variables.UpdateTextureList(psContent);
				ps = gl::CompileShader(GL_FRAGMENT_SHADER, psContent.c_str());
				bool psCompiled = gl::CheckShaderCompilationStatus(ps, cMsg);

				if (!psCompiled && !HLSL2GLSL::IsHLSL(data->PSPath))
					m_msgs->Add(gl::ParseMessages(m_msgs->CurrentItem, 1, cMsg)); 

				// geometry shader
				bool gsCompiled = true;
				if (data->GSUsed && strlen(data->GSEntry) > 0 && strlen(data->GSPath) > 0) {
					std::string gsContent = "", gsEntry = data->GSEntry;
					m_msgs->CurrentItemType = 2;
					if (!HLSL2GLSL::IsHLSL(data->GSPath)) { // GLSL
						gsContent = m_project->LoadProjectFile(data->GSPath);
						m_applyMacros(gsContent, data);
					} else { // HLSL
						gsContent = ed::HLSL2GLSL::Transcompile(m_project->GetProjectPath(std::string(data->GSPath)), 2, data->GSEntry, data->Macros, data->GSUsed, m_msgs);
						gsEntry = "main";
					}

					gs = gl::CompileShader(GL_GEOMETRY_SHADER, gsContent.c_str());
					gsCompiled = gl::CheckShaderCompilationStatus(gs, cMsg);

					if (!gsCompiled && !HLSL2GLSL::IsHLSL(data->GSPath))
						m_msgs->Add(gl::ParseMessages(m_msgs->CurrentItem, 2, cMsg));

				}

				if (m_shaders[i] != 0)
					glDeleteProgram(m_shaders[i]);

				if (!vsCompiled || !psCompiled || !gsCompiled) {
					m_msgs->Add(MessageStack::Type::Error, items[i]->Name, "Failed to compile the shader");
					m_shaders[i] = 0;
				}
				else {
					m_msgs->ClearGroup(items[i]->Name);

					m_shaders[i] = glCreateProgram();
					glAttachShader(m_shaders[i], vs);
					glAttachShader(m_shaders[i], ps);
					if (data->GSUsed) glAttachShader(m_shaders[i], gs);
					glLinkProgram(m_shaders[i]);
				}

				if (m_shaders[i] != 0)
					data->Variables.UpdateUniformInfo(m_shaders[i]);

				if (vs != 0) glDeleteShader(vs);
				if (ps != 0) glDeleteShader(ps);
				if (gs != 0) glDeleteShader(gs);
			}
		}

		// check if some item was removed
		for (int i = 0; i < m_items.size(); i++) {
			bool found = false;
			for (int j = 0; j < items.size(); j++)
				if (items[j]->Data == m_items[i]->Data) {
					found = true;
					break;
				}

			if (!found) {
				glDeleteProgram(m_shaders[i]);

				Logger::Get().Log("Removing an item from cache");

				m_fbos.erase((pipe::ShaderPass*)m_items[i]->Data);
				m_items.erase(m_items.begin() + i);
				m_shaders.erase(m_shaders.begin() + i);
			}
		}

		// check if the order of the items changed
		for (int i = 0; i < m_items.size(); i++) {
			// two items at the same position dont match
			if (items[i]->Data != m_items[i]->Data) {
				// find the real position from original list
				for (int j = 0; j < items.size(); j++) {
					// we found the original position so move the item
					if (items[j]->Data == m_items[i]->Data) {
						Logger::Get().Log("Updating cached item " + std::string(items[j]->Name));

						int dest = j > i ? (j - 1) : j;
						m_items.erase(m_items.begin() + i, m_items.begin() + i + 1);
						m_items.insert(m_items.begin() + dest, items[j]);

						GLuint sCopy = m_shaders[i];
						
						m_shaders.erase(m_shaders.begin() + i);
						m_shaders.insert(m_shaders.begin() + dest, sCopy);
					}
				}
			}
		}
	}
	void RenderEngine::m_applyMacros(std::string& src, pipe::ShaderPass* pass)
	{
		size_t verLoc = src.find_first_of("#version");
		size_t lineLoc = src.find_first_of('\n', verLoc+1) + 1;
		std::string strMacro = "";

		for (auto& macro : pass->Macros) {
			if (!macro.Active)
				continue;
			
			strMacro += "#define " + std::string(macro.Name) + " " + std::string(macro.Value) + "\n";
		}

		if (strMacro.size() > 0)
			src.insert(lineLoc, strMacro);
	}
	void RenderEngine::m_updatePassFBO(ed::pipe::ShaderPass* pass)
	{
		bool changed = false;

		for (int i = 0; i < pass->RTCount; i++)
			if (pass->RenderTextures[i] != m_fbos[pass][i]) {
				changed = true;
				break;
			}
		for (int i = 0; i < pass->RTCount; i++)
			m_fbos[pass][i] = pass->RenderTextures[i];

		changed = changed || m_fboCount[pass] != pass->RTCount;
		m_fboCount[pass] = pass->RTCount;

		if (!changed && !m_fbosNeedUpdate)
			return;

		GLuint lastID = pass->RenderTextures[pass->RTCount - 1];
		GLuint depthID = lastID == m_rtColor ? m_rtDepth : m_objects->GetRenderTexture(lastID)->DepthStencilBuffer;

		pass->DepthTexture = depthID;

		if (pass->FBO != 0)
			glDeleteFramebuffers(1, &pass->FBO);

		glGenFramebuffers(1, &pass->FBO);
		glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)pass->FBO);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, depthID, 0);
		for (int i = 0; i < pass->RTCount; i++) {
			GLuint texID = pass->RenderTextures[i];

			if (texID == 0) continue;

			// attach
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, texID, 0);
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		m_fbosNeedUpdate = false;
	}
}