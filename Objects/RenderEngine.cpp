#include "RenderEngine.h"
#include "Logger.h"
#include "Settings.h"
#include "ShaderTranscompiler.h"
#include "DefaultState.h"
#include "ObjectManager.h"
#include "PipelineManager.h"
#include "SystemVariableManager.h"
#include "../Engine/GeometryFactory.h"
#include "../Engine/GLUtils.h"
#include "../Engine/Ray.h"

#include <algorithm>
#include <ghc/filesystem.hpp>
#include <glm/gtx/intersect.hpp>

static const GLenum fboBuffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4, GL_COLOR_ATTACHMENT5, GL_COLOR_ATTACHMENT6, GL_COLOR_ATTACHMENT7, GL_COLOR_ATTACHMENT8, GL_COLOR_ATTACHMENT9, GL_COLOR_ATTACHMENT10, GL_COLOR_ATTACHMENT11, GL_COLOR_ATTACHMENT12, GL_COLOR_ATTACHMENT13, GL_COLOR_ATTACHMENT14, GL_COLOR_ATTACHMENT15 };

namespace ed
{
	RenderEngine::RenderEngine(PipelineManager * pipeline, ObjectManager* objects, ProjectParser* project, MessageStack* msgs, PluginManager* plugins) :
		m_pipeline(pipeline),
		m_objects(objects),
		m_project(project),
		m_msgs(msgs),
		m_plugins(plugins),
		m_lastSize(0, 0),
		m_pickAwaiting(false),
		m_rtColor(0),
		m_rtDepth(0),
		m_fbosNeedUpdate(false),
		m_computeSupported(true),
		m_wasMultiPick(false)
	{
		m_paused = false;

		glGenTextures(1, &m_rtColor);
		glGenTextures(1, &m_rtDepth);
		glGenTextures(1, &m_rtColorMS);
		glGenTextures(1, &m_rtDepthMS);
	}
	RenderEngine::~RenderEngine()
	{
		glDeleteTextures(1, &m_rtColor);
		glDeleteTextures(1, &m_rtDepth);
		glDeleteTextures(1, &m_rtColorMS);
		glDeleteTextures(1, &m_rtDepthMS);
		FlushCache();
	}
	void RenderEngine::Render(int width, int height)
	{
		bool isMSAA = Settings::Instance().Preview.MSAA != 1;

		if (isMSAA)
			glEnable(GL_MULTISAMPLE);

		// recreate render texture if size has changed
		if (m_lastSize.x != width || m_lastSize.y != height) {
			m_lastSize = glm::vec2(width, height);

			glBindTexture(GL_TEXTURE_2D, m_rtColor);
			glTexImage2D(GL_TEXTURE_2D, 0, Settings::Instance().Project.UseAlphaChannel ? GL_RGBA : GL_RGB, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glBindTexture(GL_TEXTURE_2D, 0);

			glBindTexture(GL_TEXTURE_2D, m_rtDepth);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, width, height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glBindTexture(GL_TEXTURE_2D, 0);

			glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, m_rtColorMS);
			glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, Settings::Instance().Preview.MSAA, Settings::Instance().Project.UseAlphaChannel ? GL_RGBA : GL_RGB, width, height, true);
			
			glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, m_rtDepthMS);
			glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, Settings::Instance().Preview.MSAA, GL_DEPTH24_STENCIL8, width, height, true);
			glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);

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

		// cache elements
		m_cache();

		auto& systemVM = SystemVariableManager::Instance();

		auto& itemVarValues = GetItemVariableValues();
		GLuint previousTexture[MAX_RENDER_TEXTURES] = { 0 }; // dont clear the render target if we use it two times in a row
		GLuint previousDepth = 0;
		bool clearedWindow = false;

		m_plugins->BeginRender();

		for (int i = 0; i < m_items.size(); i++) {
			PipelineItem* it = m_items[i];

			if (it->Type == PipelineItem::ItemType::ShaderPass) {
				pipe::ShaderPass* data = (pipe::ShaderPass*)it->Data;

				if (!data->Active || data->Items.size() <= 0)
					continue;

				const std::vector<GLuint>& srvs = m_objects->GetBindList(m_items[i]);
				const std::vector<GLuint>& ubos = m_objects->GetUniformBindList(m_items[i]);

				if (data->RTCount == 0)
					continue;

				// create/update fbo if necessary
				m_updatePassFBO(data);

				if (m_shaders[i] == 0)
					continue;

				// bind fbo and buffers
				glBindFramebuffer(GL_FRAMEBUFFER, isMSAA ? m_fboMS[data] : data->FBO);
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
					else if (m_objects->IsImage3D(srvs[j]))
						glBindTexture(GL_TEXTURE_3D, srvs[j]);
					else if (m_objects->IsPluginObject(srvs[j])) {
						PluginObject* pobj = m_objects->GetPluginObject(srvs[j]);
						pobj->Owner->BindObject(pobj->Type, pobj->Data, pobj->ID);
					}
					else
						glBindTexture(GL_TEXTURE_2D, srvs[j]);

					if (ShaderTranscompiler::GetShaderTypeFromExtension(data->PSPath) == ShaderLanguage::GLSL) // TODO: or should this be for vulkan glsl too?
						data->Variables.UpdateTexture(m_shaders[i], j);
				}

				for (int j = 0; j < ubos.size(); j++)
					glBindBufferBase(GL_UNIFORM_BUFFER, j, ubos[j]);
				
				// clear messages
				//if (m_msgs->GetGroupWarningMsgCount(it->Name) > 0)
				//	m_msgs->ClearGroup(it->Name, (int)ed::MessageStack::Type::Warning);

				// bind default states for each shader pass
				DefaultState::Bind();

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
						if (geoData->Instanced)
							glDrawArraysInstanced(geoData->Topology, 0, eng::GeometryFactory::VertexCount[geoData->Type], geoData->InstanceCount);
						else
							glDrawArrays(geoData->Topology, 0, eng::GeometryFactory::VertexCount[geoData->Type]);
					}
					else if (item->Type == PipelineItem::ItemType::Model) {
						pipe::Model* objData = reinterpret_cast<pipe::Model*>(item->Data);

						systemVM.SetPicked(std::count(m_pick.begin(), m_pick.end(), item));
						systemVM.SetGeometryTransform(item, objData->Scale, objData->Rotation, objData->Position);

						// bind variables
						data->Variables.Bind(item);

						objData->Data->Draw(objData->Instanced, objData->InstanceCount);
					}
					else if (item->Type == PipelineItem::ItemType::RenderState) {
						pipe::RenderState* state = reinterpret_cast<pipe::RenderState*>(item->Data);
						
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
					else if (item->Type == PipelineItem::ItemType::PluginItem) {
						pipe::PluginItemData* pldata = reinterpret_cast<pipe::PluginItemData*>(item->Data);

						if (m_pickAwaiting && pldata->Owner->IsPipelineItemPickable(pldata->Type))
							m_pickItem(item, m_wasMultiPick);

						if (pldata->Owner->IsPipelineItemPickable(pldata->Type))
							systemVM.SetPicked(std::count(m_pick.begin(), m_pick.end(), item));
						else
							systemVM.SetPicked(false);

						pldata->Owner->ExecutePipelineItem(data, plugin::PipelineItemType::ShaderPass, pldata->Type, pldata->PluginData);
					}

					// set the old value back
					if (item->Type == PipelineItem::ItemType::Geometry || item->Type == PipelineItem::ItemType::Model)
						for (int k = 0; k < itemVarValues.size(); k++)
							if (itemVarValues[k].Item == item)
								itemVarValues[k].Variable->Data = itemVarValues[k].OldValue;

				}

				if (isMSAA) {
					glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fboMS[data]);
					glBindFramebuffer(GL_DRAW_FRAMEBUFFER, data->FBO);
					glDrawBuffer(GL_BACK);
					for (unsigned int i = 0; i < data->RTCount; i++)
					{
						glReadBuffer(GL_COLOR_ATTACHMENT0 + i);
						glDrawBuffer(GL_COLOR_ATTACHMENT0 + i);
						glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
					}
				}
			}
			else if (it->Type == PipelineItem::ItemType::ComputePass && m_computeSupported) {
				pipe::ComputePass *data = (pipe::ComputePass *)it->Data;

				const std::vector<GLuint>& srvs = m_objects->GetBindList(m_items[i]);
				const std::vector<GLuint>& ubos = m_objects->GetUniformBindList(m_items[i]);

				if (m_shaders[i] == 0)
					continue;
				
				// bind shaders
				glUseProgram(m_shaders[i]);

				// bind shader resource views
				for (int j = 0; j < srvs.size(); j++)
				{
					glActiveTexture(GL_TEXTURE0 + j);
					if (m_objects->IsCubeMap(srvs[j]))
						glBindTexture(GL_TEXTURE_CUBE_MAP, srvs[j]);
					else if (m_objects->IsImage3D(srvs[j]))
						glBindTexture(GL_TEXTURE_3D, srvs[j]);
					else
						glBindTexture(GL_TEXTURE_2D, srvs[j]);

					if (ShaderTranscompiler::GetShaderTypeFromExtension(data->Path) == ShaderLanguage::GLSL) // TODO: or should this be for vulkan glsl too?
						data->Variables.UpdateTexture(m_shaders[i], j);
				}

				// bind buffers
				for (int j = 0; j < ubos.size(); j++) {
					if (m_objects->IsImage(ubos[j])) {
						ImageObject* iobj = m_objects->GetImage(m_objects->GetImageNameByID(ubos[j])); // TODO: GetImageByID
						glBindImageTexture(j, ubos[j], 0, GL_FALSE, 0, GL_WRITE_ONLY | GL_READ_ONLY, iobj->Format);
					}
					else if (m_objects->IsImage3D(ubos[j])) {
						Image3DObject* iobj = m_objects->GetImage3D(m_objects->GetImage3DNameByID(ubos[j]));
						glBindImageTexture(j, ubos[j], 0, GL_TRUE, 0, GL_WRITE_ONLY | GL_READ_ONLY, iobj->Format);
					}
					else if (m_objects->IsPluginObject(ubos[j])) {
						PluginObject* pobj = m_objects->GetPluginObject(ubos[j]);
						pobj->Owner->BindObject(pobj->Type, pobj->Data, pobj->ID);
					} else
						glBindBufferBase(GL_SHADER_STORAGE_BUFFER, j, ubos[j]);
				}
				
				// bind variables
				data->Variables.Bind();

				// call compute shader
				glDispatchCompute(data->WorkX, data->WorkY, data->WorkZ);

				// wait until it finishes
				glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_UNIFORM_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
				// or maybe until i implement these as options glMemoryBarrier(GL_ALL_BARRIER_BITS);
			}
			else if (it->Type == PipelineItem::ItemType::AudioPass) {
				pipe::AudioPass *data = (pipe::AudioPass *)it->Data;

				const std::vector<GLuint>& srvs = m_objects->GetBindList(m_items[i]);
				const std::vector<GLuint>& ubos = m_objects->GetUniformBindList(m_items[i]);

				// bind shader resource views
				for (int j = 0; j < srvs.size(); j++)
				{
					glActiveTexture(GL_TEXTURE0 + j);
					if (m_objects->IsCubeMap(srvs[j]))
						glBindTexture(GL_TEXTURE_CUBE_MAP, srvs[j]);
					else if (m_objects->IsImage3D(srvs[j]))
						glBindTexture(GL_TEXTURE_3D, srvs[j]);
					else if (m_objects->IsPluginObject(srvs[j])) {
						PluginObject* pobj = m_objects->GetPluginObject(srvs[j]);
						pobj->Owner->BindObject(pobj->Type, pobj->Data, pobj->ID);
					}
					else
						glBindTexture(GL_TEXTURE_2D, srvs[j]);

					if (ShaderTranscompiler::GetShaderTypeFromExtension(data->Path) == ShaderLanguage::GLSL) // TODO: or should this be for vulkan glsl too?
						data->Variables.UpdateTexture(m_shaders[i], j);
				}

				// bind buffers
				for (int j = 0; j < ubos.size(); j++) {
					if (m_objects->IsBuffer(m_objects->GetBufferNameByID(ubos[j])))
						glBindBufferBase(GL_SHADER_STORAGE_BUFFER, j, ubos[j]);
				}
				
				// bind variables
				data->Variables.Bind();

				data->Stream.renderAudio();
			}
			else if (it->Type == PipelineItem::ItemType::PluginItem) {
				pipe::PluginItemData* pldata = reinterpret_cast<pipe::PluginItemData*>(it->Data);

				pldata->Owner->ExecutePipelineItem(pldata->Type, pldata->PluginData, pldata->Items.data(), pldata->Items.size());
			}
		}

		m_plugins->EndRender();

		// update frame index
		if (!m_paused) {
			systemVM.CopyState();
			systemVM.SetFrameIndex(systemVM.GetFrameIndex() + 1);
		}

		// restore real render target view
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		if (m_pickDist == std::numeric_limits<float>::infinity())
			m_pick.clear();
		if (m_pickAwaiting && m_pickHandle != nullptr)
			m_pickHandle(m_pick.size() == 0 ? nullptr : m_pick[m_pick.size()-1]);
		m_pickAwaiting = false;

		if (isMSAA)
			glDisable(GL_MULTISAMPLE);
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
				if (item->Type == PipelineItem::ItemType::ShaderPass) {
					pipe::ShaderPass* shader = (pipe::ShaderPass*)item->Data;

					m_msgs->ClearGroup(name);

					if (m_shaderSources[i].VS != 0) glDeleteShader(m_shaderSources[i].VS);
					if (m_shaderSources[i].PS != 0) glDeleteShader(m_shaderSources[i].PS);
					if (m_shaderSources[i].GS != 0) glDeleteShader(m_shaderSources[i].GS);

					std::string psContent = "", vsContent = "",
						vsEntry = shader->VSEntry,
						psEntry = shader->PSEntry;
					int lineBias = 0;

					// pixel shader
					m_msgs->CurrentItemType = 1;
					if (ShaderTranscompiler::GetShaderTypeFromExtension(shader->PSPath) == ShaderLanguage::GLSL) {// GLSL
						psContent = m_project->LoadProjectFile(shader->PSPath);
						m_includeCheck(psContent, std::vector<std::string>(), lineBias);
						m_applyMacros(psContent, shader);
					} else { // HLSL / VK
						psContent = ShaderTranscompiler::Transcompile(ShaderTranscompiler::GetShaderTypeFromExtension(shader->PSPath), m_project->GetProjectPath(std::string(shader->PSPath)), 1, shader->PSEntry, shader->Macros, shader->GSUsed, m_msgs, m_project);
						psEntry = "main";
					}

					shader->Variables.UpdateTextureList(psContent);
					GLuint ps = gl::CompileShader(GL_FRAGMENT_SHADER, psContent.c_str());
					bool psCompiled = gl::CheckShaderCompilationStatus(ps, cMsg);

					if (!psCompiled && ShaderTranscompiler::GetShaderTypeFromExtension(shader->PSPath) == ShaderLanguage::GLSL)
						m_msgs->Add(gl::ParseMessages(name, 1, cMsg, lineBias));

					// vertex shader
					m_msgs->CurrentItemType = 0;
					lineBias = 0;
					if (ShaderTranscompiler::GetShaderTypeFromExtension(shader->VSPath) == ShaderLanguage::GLSL) {// GLSL
						vsContent = m_project->LoadProjectFile(shader->VSPath);
						m_includeCheck(vsContent, std::vector<std::string>(), lineBias);
						m_applyMacros(vsContent, shader);
					} else { // HLSL / VK
						vsContent = ShaderTranscompiler::Transcompile(ShaderTranscompiler::GetShaderTypeFromExtension(shader->VSPath), m_project->GetProjectPath(std::string(shader->VSPath)), 0, shader->VSEntry, shader->Macros, shader->GSUsed, m_msgs, m_project);
						vsEntry = "main";
					}

					GLuint vs = gl::CompileShader(GL_VERTEX_SHADER, vsContent.c_str());
					bool vsCompiled = gl::CheckShaderCompilationStatus(vs, cMsg);

					if (!vsCompiled && ShaderTranscompiler::GetShaderTypeFromExtension(shader->PSPath) == ShaderLanguage::GLSL)
						m_msgs->Add(gl::ParseMessages(name, 0, cMsg, lineBias));

					// geometry shader
					bool gsCompiled = true;
					GLuint gs = 0;
					if (shader->GSUsed && strlen(shader->GSPath) > 0 && strlen(shader->GSEntry) > 0) {
						std::string gsContent = "",
							gsEntry = shader->GSEntry;

						m_msgs->CurrentItemType = 2;
						lineBias = 0;
						if (ShaderTranscompiler::GetShaderTypeFromExtension(shader->GSPath) == ShaderLanguage::GLSL) {// GLSL
							gsContent = m_project->LoadProjectFile(shader->GSPath);
							m_includeCheck(gsContent, std::vector<std::string>(), lineBias);
							m_applyMacros(gsContent, shader);
						} else { // HLSL / VK
							gsContent = ShaderTranscompiler::Transcompile(ShaderTranscompiler::GetShaderTypeFromExtension(shader->GSPath), m_project->GetProjectPath(std::string(shader->GSPath)), 2, shader->GSEntry, shader->Macros, shader->GSUsed, m_msgs, m_project);
							gsEntry = "main";

							// TODO: delete this when glslang fixes this https://github.com/KhronosGroup/glslang/issues/1660
							m_msgs->Add(MessageStack::Type::Warning, name, "HLSL geometry shaders are currently not supported by glslang");
						}

						gs = gl::CompileShader(GL_GEOMETRY_SHADER, gsContent.c_str());
						gsCompiled = gl::CheckShaderCompilationStatus(gs, cMsg);

						if (!gsCompiled && ShaderTranscompiler::GetShaderTypeFromExtension(shader->GSPath) == ShaderLanguage::GLSL)
							m_msgs->Add(gl::ParseMessages(name, 2, cMsg, lineBias));
					}

					if (m_shaders[i] != 0)
						glDeleteProgram(m_shaders[i]);

					if (!vsCompiled || !psCompiled || !gsCompiled) {
						Logger::Get().Log("Shaders not compiled", true); 
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

					m_shaderSources[i].VS = vs;
					m_shaderSources[i].PS = ps;
					m_shaderSources[i].GS = gs;
				}
				else if (item->Type == PipelineItem::ItemType::ComputePass && m_computeSupported) {
					pipe::ComputePass *shader = (pipe::ComputePass *)item->Data;

					m_msgs->ClearGroup(name);

					std::string content = "", entry = shader->Entry;
					int lineBias = 0;

					// compute shader
					m_msgs->CurrentItemType = 3;
					if (ShaderTranscompiler::GetShaderTypeFromExtension(shader->Path) == ShaderLanguage::GLSL) {// GLSL
						content = m_project->LoadProjectFile(shader->Path);
						m_includeCheck(content, std::vector<std::string>(), lineBias);
						m_applyMacros(content, shader);
					} else { // HLSL / VK
						content = ShaderTranscompiler::Transcompile(ShaderTranscompiler::GetShaderTypeFromExtension(shader->Path), m_project->GetProjectPath(std::string(shader->Path)), 3, entry, shader->Macros, false, m_msgs, m_project);
						entry = "main";
					}

					// compute shader supported == version 4.3 == not needed: shader->Variables.UpdateTextureList(content);
					GLuint cs = gl::CompileShader(GL_COMPUTE_SHADER, content.c_str());
					bool compiled = gl::CheckShaderCompilationStatus(cs, cMsg);

					if (!compiled && ShaderTranscompiler::GetShaderTypeFromExtension(shader->Path) == ShaderLanguage::GLSL)
						m_msgs->Add(gl::ParseMessages(name, 3, cMsg, lineBias));

					if (m_shaders[i] != 0)
						glDeleteProgram(m_shaders[i]);

					if (!compiled) {
						Logger::Get().Log("Compute shader was not compiled", true);
						m_msgs->Add(MessageStack::Type::Error, name, "Failed to compile the compute shader");
						m_shaders[i] = 0;
					} else {
						m_msgs->Add(MessageStack::Type::Message, name, "Compiled the compute shader.");

						m_shaders[i] = glCreateProgram();
						glAttachShader(m_shaders[i], cs);
						glLinkProgram(m_shaders[i]);
					}

					glDeleteShader(cs);

					if (m_shaders[i] != 0)
						shader->Variables.UpdateUniformInfo(m_shaders[i]);
				}
				else if (item->Type == PipelineItem::ItemType::AudioPass) {
					pipe::AudioPass *shader = (pipe::AudioPass *)item->Data;

					m_msgs->ClearGroup(name);

					std::string content = m_project->LoadProjectFile(shader->Path);

					// compute shader
					m_msgs->CurrentItemType = 1;
					if (ShaderTranscompiler::GetShaderTypeFromExtension(shader->Path) == ShaderLanguage::GLSL)
						m_applyMacros(content, shader);
					
					shader->Stream.compileFromShaderSource(m_project, m_msgs, content, shader->Macros, ShaderTranscompiler::GetShaderTypeFromExtension(shader->Path) == ShaderLanguage::HLSL);
					shader->Variables.UpdateUniformInfo(shader->Stream.getShader());
				}
				else if (item->Type == PipelineItem::ItemType::PluginItem)
				{
					pipe::PluginItemData* idata = (pipe::PluginItemData*)item->Data;
					idata->Owner->HandleRecompile(name);
				}
			}
		}
	}
	void RenderEngine::RecompileFromSource(const char* name, const std::string& vssrc, const std::string& pssrc, const std::string& gssrc)
	{
		m_msgs->BuildOccured = true;
		m_msgs->CurrentItem = name;

		GLchar cMsg[1024];

		int d3dCounter = 0;
		for (int i = 0; i < m_items.size(); i++) {
			PipelineItem* item = m_items[i];
			if (strcmp(item->Name, name) == 0) {
				if (item->Type == PipelineItem::ItemType::ShaderPass) {
					pipe::ShaderPass* shader = (pipe::ShaderPass*)item->Data;
					m_msgs->ClearGroup(name);

					bool vsCompiled = true, psCompiled = true, gsCompiled = true;

					// pixel shader
					if (pssrc.size() > 0) {
						m_msgs->CurrentItemType = 1;
						shader->Variables.UpdateTextureList(pssrc);
						GLuint ps = gl::CompileShader(GL_FRAGMENT_SHADER, pssrc.c_str());
						psCompiled = gl::CheckShaderCompilationStatus(ps, cMsg);

						if (!psCompiled && ShaderTranscompiler::GetShaderTypeFromExtension(shader->PSPath) == ShaderLanguage::GLSL)
							m_msgs->Add(gl::ParseMessages(name, 1, cMsg));

						glDeleteShader(m_shaderSources[i].PS);
						m_shaderSources[i].PS = ps;
					}

					// vertex shader
					if (vssrc.size() > 0) {
						m_msgs->CurrentItemType = 0;
						GLuint vs = gl::CompileShader(GL_VERTEX_SHADER, vssrc.c_str());
						vsCompiled = gl::CheckShaderCompilationStatus(vs, cMsg);

						if (!vsCompiled && ShaderTranscompiler::GetShaderTypeFromExtension(shader->VSPath) == ShaderLanguage::GLSL)
							m_msgs->Add(gl::ParseMessages(name, 0, cMsg));

						glDeleteShader(m_shaderSources[i].VS);
						m_shaderSources[i].VS = vs;
					}

					// geometry shader
					if (gssrc.size() > 0) {
						GLuint gs = 0;
						glDeleteShader(m_shaderSources[i].GS);
						if (shader->GSUsed && strlen(shader->GSPath) > 0 && strlen(shader->GSEntry) > 0) {
							gs = gl::CompileShader(GL_GEOMETRY_SHADER, gssrc.c_str());
							gsCompiled = gl::CheckShaderCompilationStatus(gs, cMsg);

							if (!gsCompiled && ShaderTranscompiler::GetShaderTypeFromExtension(shader->GSPath) == ShaderLanguage::GLSL)
								m_msgs->Add(gl::ParseMessages(name, 2, cMsg));

							// TODO: delete this when glslang fixes this https://github.com/KhronosGroup/glslang/issues/1660
							if (ShaderTranscompiler::GetShaderTypeFromExtension(shader->VSPath) == ShaderLanguage::HLSL)
								m_msgs->Add(MessageStack::Type::Warning, name, "HLSL geometry shaders are currently not supported by glslang");

							m_shaderSources[i].GS = gs;
						}
					}

					if (m_shaders[i] != 0)
						glDeleteProgram(m_shaders[i]);

					if (!vsCompiled || !psCompiled || !gsCompiled) {
						m_msgs->Add(MessageStack::Type::Error, name, "Failed to compile the shader(s)");
						m_shaders[i] = 0;
					}
					else {
						m_msgs->Add(MessageStack::Type::Message, name, "Compiled the shaders.");

						m_shaders[i] = glCreateProgram();
						glAttachShader(m_shaders[i], m_shaderSources[i].VS);
						glAttachShader(m_shaders[i], m_shaderSources[i].PS);
						if (shader->GSUsed) glAttachShader(m_shaders[i], m_shaderSources[i].GS);
						glLinkProgram(m_shaders[i]);
					}

					if (m_shaders[i] != 0)
						shader->Variables.UpdateUniformInfo(m_shaders[i]);
				}
				else if (item->Type == PipelineItem::ItemType::ComputePass && m_computeSupported) {
					pipe::ComputePass *shader = (pipe::ComputePass *)item->Data;
					m_msgs->ClearGroup(name);

					bool compiled = false;
					GLuint cs = 0;

					// compute shader
					if (vssrc.size() > 0)
					{
						m_msgs->CurrentItemType = 3;
						cs = gl::CompileShader(GL_COMPUTE_SHADER, vssrc.c_str());
						compiled = gl::CheckShaderCompilationStatus(cs, cMsg);

						if (!compiled && ShaderTranscompiler::GetShaderTypeFromExtension(shader->Path) == ShaderLanguage::GLSL)
							m_msgs->Add(gl::ParseMessages(name, 3, cMsg));
					}

					if (m_shaders[i] != 0)
						glDeleteProgram(m_shaders[i]);

					if (!compiled)
					{
						m_msgs->Add(MessageStack::Type::Error, name, "Failed to compile the compute shader");
						m_shaders[i] = 0;
					}
					else
					{
						m_msgs->Add(MessageStack::Type::Message, name, "Compiled the compute shader.");

						m_shaders[i] = glCreateProgram();
						glAttachShader(m_shaders[i], cs);
						glLinkProgram(m_shaders[i]);
					}

					if (m_shaders[i] != 0)
						shader->Variables.UpdateUniformInfo(m_shaders[i]);

					glDeleteShader(cs);
				}
				else if (item->Type == PipelineItem::ItemType::AudioPass) {
					pipe::AudioPass *shader = (pipe::AudioPass *)item->Data;
					m_msgs->ClearGroup(name);

					bool compiled = false;
					GLuint ss = 0;

					// audio shader
					if (vssrc.size() > 0)
						shader->Stream.compileFromShaderSource(m_project, m_msgs, vssrc, shader->Macros, true);
					shader->Variables.UpdateUniformInfo(shader->Stream.getShader());
				}
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
			if (geo->Type == pipe::GeometryItem::GeometryType::Rectangle ||
				geo->Type == pipe::GeometryItem::GeometryType::ScreenQuadNDC)
				return;

			world = glm::translate(glm::mat4(1), geo->Position) * glm::yawPitchRoll(geo->Rotation.y, geo->Rotation.x, geo->Rotation.z);
		}
		else if (item->Type == PipelineItem::ItemType::Model) {
			pipe::Model* obj = (pipe::Model*)item->Data;
			
			world = glm::translate(glm::mat4(1), obj->Position) * glm::yawPitchRoll(obj->Rotation.y, obj->Rotation.x, obj->Rotation.z);
		}
		else if (item->Type == PipelineItem::ItemType::PluginItem) {
			pipe::PluginItemData* pldata = (pipe::PluginItemData*)item->Data;

			float plMat[16];
			pldata->Owner->GetPipelineItemWorldMatrix(item->Name, plMat);
			world = glm::make_mat4(plMat);
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
		else if (item->Type == PipelineItem::ItemType::PluginItem) {
			pipe::PluginItemData* obj = (pipe::PluginItemData*)item->Data;

			float hit;
			if (obj->Owner->IntersectPipelineItem(obj->Type, obj->PluginData, glm::value_ptr(vec3Origin), glm::value_ptr(vec3Dir), hit))
				myDist = hit;
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
		for (int i = 0; i < m_shaders.size(); i++) {
			glDeleteProgram(m_shaders[i]);
			glDeleteShader(m_shaderSources[i].VS);
			glDeleteShader(m_shaderSources[i].PS);
			glDeleteShader(m_shaderSources[i].GS);
		}
		
		m_fbos.clear();
		m_fboCount.clear();
		m_items.clear();
		m_shaders.clear();
		m_shaderSources.clear();
		m_fbosNeedUpdate = true;

		// clear textures
		glBindTexture(GL_TEXTURE_2D, m_rtColor);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_lastSize.x, m_lastSize.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		glBindTexture(GL_TEXTURE_2D, m_rtDepth);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, m_lastSize.x, m_lastSize.y, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
		glBindTexture(GL_TEXTURE_2D, 0);
	
		m_lastSize = glm::ivec2(1,1); // recreate window rt!
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

				if (items[i]->Type == PipelineItem::ItemType::ShaderPass) {
					pipe::ShaderPass* data = reinterpret_cast<ed::pipe::ShaderPass*>(items[i]->Data);

					m_items.insert(m_items.begin() + i, items[i]);
					m_shaders.insert(m_shaders.begin() + i, 0);
					m_shaderSources.insert(m_shaderSources.begin() + i, ShaderPack());

					if (strlen(data->VSPath) == 0 || strlen(data->PSPath) == 0) {
						Logger::Get().Log("No shader paths are set", true);
						continue;
					}

					if (m_shaderSources[i].VS != 0) glDeleteShader(m_shaderSources[i].VS);
					if (m_shaderSources[i].PS != 0) glDeleteShader(m_shaderSources[i].PS);
					if (m_shaderSources[i].GS != 0) glDeleteShader(m_shaderSources[i].GS);

					/*
						ITEM CACHING
					*/

					m_fbos[data].resize(MAX_RENDER_TEXTURES);

					GLuint ps = 0, vs = 0, gs = 0;

					m_msgs->CurrentItem = items[i]->Name;

					std::string psContent = "", vsContent = "",
						vsEntry = data->VSEntry,
						psEntry = data->PSEntry;
					int lineBias = 0;

					// vertex shader
					m_msgs->CurrentItemType = 0;
					if (ShaderTranscompiler::GetShaderTypeFromExtension(data->VSPath) == ShaderLanguage::GLSL) { // GLSL
						vsContent = m_project->LoadProjectFile(data->VSPath);
						m_includeCheck(vsContent, std::vector<std::string>(), lineBias);
						m_applyMacros(vsContent, data);
					} else { // HLSL / VK
						vsContent = ShaderTranscompiler::Transcompile(ShaderTranscompiler::GetShaderTypeFromExtension(data->VSPath), m_project->GetProjectPath(std::string(data->VSPath)), 0, data->VSEntry, data->Macros, data->GSUsed, m_msgs, m_project);
						vsEntry = "main";
					}
					
					vs = gl::CompileShader(GL_VERTEX_SHADER, vsContent.c_str());
					bool vsCompiled = gl::CheckShaderCompilationStatus(vs, cMsg);

					if (!vsCompiled && ShaderTranscompiler::GetShaderTypeFromExtension(data->VSPath) == ShaderLanguage::GLSL)
						m_msgs->Add(gl::ParseMessages(m_msgs->CurrentItem, 0, cMsg, lineBias));

					// pixel shader
					m_msgs->CurrentItemType = 1;
					lineBias = 0;
					if (ShaderTranscompiler::GetShaderTypeFromExtension(data->PSPath) == ShaderLanguage::GLSL) { // GLSL
						psContent = m_project->LoadProjectFile(data->PSPath);
						m_includeCheck(psContent, std::vector<std::string>(), lineBias);
						m_applyMacros(psContent, data);
					} else { // HLSL / VK
						psContent = ShaderTranscompiler::Transcompile(ShaderTranscompiler::GetShaderTypeFromExtension(data->PSPath), m_project->GetProjectPath(std::string(data->PSPath)), 1, data->PSEntry, data->Macros, data->GSUsed, m_msgs, m_project);
						psEntry = "main";
					}

					data->Variables.UpdateTextureList(psContent);
					ps = gl::CompileShader(GL_FRAGMENT_SHADER, psContent.c_str());
					bool psCompiled = gl::CheckShaderCompilationStatus(ps, cMsg);

					if (!psCompiled && ShaderTranscompiler::GetShaderTypeFromExtension(data->PSPath) == ShaderLanguage::GLSL)
						m_msgs->Add(gl::ParseMessages(m_msgs->CurrentItem, 1, cMsg, lineBias));

					// geometry shader
					lineBias = 0;
					bool gsCompiled = true;
					if (data->GSUsed && strlen(data->GSEntry) > 0 && strlen(data->GSPath) > 0) {
						std::string gsContent = "", gsEntry = data->GSEntry;
						m_msgs->CurrentItemType = 2;
						if (ShaderTranscompiler::GetShaderTypeFromExtension(data->GSPath) == ShaderLanguage::GLSL) { // GLSL
							gsContent = m_project->LoadProjectFile(data->GSPath);
							m_includeCheck(gsContent, std::vector<std::string>(), lineBias);
							m_applyMacros(gsContent, data);
						} else { // HLSL
							gsContent = ShaderTranscompiler::Transcompile(ShaderTranscompiler::GetShaderTypeFromExtension(data->GSPath), m_project->GetProjectPath(std::string(data->GSPath)), 2, data->GSEntry, data->Macros, data->GSUsed, m_msgs, m_project);
							gsEntry = "main";
							
							m_msgs->Add(MessageStack::Type::Warning, m_msgs->CurrentItem, "Geometry shaders are currently not supported by glslang");
						}

						gs = gl::CompileShader(GL_GEOMETRY_SHADER, gsContent.c_str());
						gsCompiled = gl::CheckShaderCompilationStatus(gs, cMsg);

						if (!gsCompiled && ShaderTranscompiler::GetShaderTypeFromExtension(data->GSPath) == ShaderLanguage::GLSL)
							m_msgs->Add(gl::ParseMessages(m_msgs->CurrentItem, 2, cMsg, lineBias));

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

					m_shaderSources[i].VS = vs;
					m_shaderSources[i].PS = ps;
					m_shaderSources[i].GS = gs;
				} 
				else if (items[i]->Type == PipelineItem::ItemType::ComputePass && m_computeSupported) {
					pipe::ComputePass *data = reinterpret_cast<ed::pipe::ComputePass *>(items[i]->Data);

					m_items.insert(m_items.begin() + i, items[i]);
					m_shaders.insert(m_shaders.begin() + i, 0);
					m_shaderSources.insert(m_shaderSources.begin() + i, ShaderPack());

					if (strlen(data->Path) == 0) {
						Logger::Get().Log("No shader paths are set", true);
						continue;
					}

					/*
						ITEM CACHING
					*/

					GLuint cs = 0;

					m_msgs->CurrentItem = items[i]->Name;

					std::string content = "", entry = data->Entry;
					int lineBias = 0;

					// vertex shader
					m_msgs->CurrentItemType = 3;
					if (ShaderTranscompiler::GetShaderTypeFromExtension(data->Path) == ShaderLanguage::GLSL) { // GLSL
						content = m_project->LoadProjectFile(data->Path);
						m_includeCheck(content, std::vector<std::string>(), lineBias);
						m_applyMacros(content, data);
					} else { // HLSL / VK
						content = ShaderTranscompiler::Transcompile(ShaderTranscompiler::GetShaderTypeFromExtension(data->Path), m_project->GetProjectPath(std::string(data->Path)), 3, entry, data->Macros, false, m_msgs, m_project);
						entry = "main";
					}

					cs = gl::CompileShader(GL_COMPUTE_SHADER, content.c_str());
					bool compiled = gl::CheckShaderCompilationStatus(cs, cMsg);

					if (!compiled && ShaderTranscompiler::GetShaderTypeFromExtension(data->Path) == ShaderLanguage::GLSL)
						m_msgs->Add(gl::ParseMessages(m_msgs->CurrentItem, 3, cMsg, lineBias));

					if (m_shaders[i] != 0)
						glDeleteProgram(m_shaders[i]);

					if (!compiled)
					{
						m_msgs->Add(MessageStack::Type::Error, items[i]->Name, "Failed to compile the compute shader");
						m_shaders[i] = 0;
					}
					else
					{
						m_msgs->ClearGroup(items[i]->Name);

						m_shaders[i] = glCreateProgram();
						glAttachShader(m_shaders[i], cs);
						glLinkProgram(m_shaders[i]);
					}

					if (m_shaders[i] != 0)
						data->Variables.UpdateUniformInfo(m_shaders[i]);

					m_shaderSources[i].VS = 0;
					m_shaderSources[i].PS = 0;
					m_shaderSources[i].GS = 0;
				} 
				else if (items[i]->Type == PipelineItem::ItemType::AudioPass) {
					pipe::AudioPass *data = reinterpret_cast<ed::pipe::AudioPass *>(items[i]->Data);

					m_items.insert(m_items.begin() + i, items[i]);
					m_shaders.insert(m_shaders.begin() + i, 0);
					m_shaderSources.insert(m_shaderSources.begin() + i, ShaderPack());

					/*
						ITEM CACHING
					*/

					m_msgs->CurrentItem = items[i]->Name;
					std::string content = m_project->LoadProjectFile(data->Path);

					// vertex shader
					m_msgs->CurrentItemType = 1;
					if (ShaderTranscompiler::GetShaderTypeFromExtension(data->Path) == ShaderLanguage::GLSL)
						m_applyMacros(content, data);
					data->Stream.compileFromShaderSource(m_project, m_msgs, content, data->Macros, ShaderTranscompiler::GetShaderTypeFromExtension(data->Path) == ShaderLanguage::HLSL);
						
					data->Variables.UpdateUniformInfo(data->Stream.getShader());
				}
				else if (items[i]->Type == PipelineItem::ItemType::PluginItem) {
					pipe::PluginItemData *data = reinterpret_cast<pipe::PluginItemData*>(items[i]->Data);

					m_items.insert(m_items.begin() + i, items[i]);
					m_shaders.insert(m_shaders.begin() + i, 0);
					m_shaderSources.insert(m_shaderSources.begin() + i, ShaderPack());
				}
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

				if (m_items[i]->Type == PipelineItem::ItemType::ShaderPass)
					m_fbos.erase((pipe::ShaderPass*)m_items[i]->Data);
				
				m_items.erase(m_items.begin() + i);
				m_shaders.erase(m_shaders.begin() + i);
				m_shaderSources.erase(m_shaderSources.begin() + i);
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
						ShaderPack ssrcCopy = m_shaderSources[i];
						
						m_shaders.erase(m_shaders.begin() + i);
						m_shaders.insert(m_shaders.begin() + dest, sCopy);
						m_shaderSources.erase(m_shaderSources.begin() + i);
						m_shaderSources.insert(m_shaderSources.begin() + dest, ssrcCopy);
					}
				}
			}
		}
	}
	void RenderEngine::m_applyMacros(std::string &src, pipe::ShaderPass *pass)
	{
		size_t verLoc = src.find_first_of("#version");
		size_t lineLoc = src.find_first_of('\n', verLoc + 1) + 1;
		std::string strMacro = "";

		for (auto &macro : pass->Macros)
		{
			if (!macro.Active)
				continue;

			strMacro += "#define " + std::string(macro.Name) + " " + std::string(macro.Value) + "\n";
		}

		if (strMacro.size() > 0)
			src.insert(lineLoc, strMacro);
	}
	void RenderEngine::m_applyMacros(std::string &src, pipe::ComputePass *pass)
	{
		size_t verLoc = src.find_first_of("#version");
		size_t lineLoc = src.find_first_of('\n', verLoc + 1) + 1;
		std::string strMacro = "";

		for (auto &macro : pass->Macros)
		{
			if (!macro.Active)
				continue;

			strMacro += "#define " + std::string(macro.Name) + " " + std::string(macro.Value) + "\n";
		}

		if (strMacro.size() > 0)
			src.insert(lineLoc, strMacro);
	}
	void RenderEngine::m_applyMacros(std::string &src, pipe::AudioPass *pass)
	{
		size_t verLoc = src.find_first_of("#version");
		size_t lineLoc = src.find_first_of('\n', verLoc + 1) + 1;
		std::string strMacro = "";

		for (auto &macro : pass->Macros)
		{
			if (!macro.Active)
				continue;

			strMacro += "#define " + std::string(macro.Name) + " " + std::string(macro.Value) + "\n";
		}

		if (strMacro.size() > 0)
			src.insert(lineLoc, strMacro);
	}
	void RenderEngine::m_includeCheck(std::string& src, std::vector<std::string> includeStack, int& lineBias)
	{
		size_t incLoc = src.find("#include");
		Settings& settings = Settings::Instance();

		std::vector<std::string> paths = settings.Project.IncludePaths;
		paths.push_back(".");

		while (incLoc != std::string::npos) {
			bool isAfterNewline = true;
			if (incLoc != 0)
				if (src[incLoc - 1] != '\n')
					isAfterNewline = false;

			if (!isAfterNewline) {
				incLoc = src.find("#include", incLoc + 1);
				continue;
			}

			size_t quotePos = src.find_first_of("\"<", incLoc);
			size_t quoteEnd = src.find_first_of("\">", quotePos + 1);
			std::string fileName = src.substr(quotePos+1, quoteEnd - quotePos-1);

			for (int i = 0; i < paths.size(); i++) {
				std::string ipath = paths[i];
				char last = ipath[ipath.size() - 1];
				if (last != '\\' && last != '/')
					ipath += "/";

				ipath += fileName;

				src.erase(incLoc, src.find_first_of('\n', incLoc) - incLoc);

				if (std::count(includeStack.begin(), includeStack.end(), ipath) == 0)
					m_msgs->Add(ed::MessageStack::Type::Error, m_msgs->CurrentItem, "Recursive #include detected");

				if (m_project->FileExists(ipath) && std::count(includeStack.begin(), includeStack.end(), ipath) == 0) {
					includeStack.push_back(ipath);

					std::string incFileSrc = m_project->LoadProjectFile(ipath);
					lineBias = std::count(incFileSrc.begin(), incFileSrc.end(), '\n');

					m_includeCheck(incFileSrc, includeStack, lineBias);

					src.insert(incLoc, incFileSrc);

					break;
				}
			}

			incLoc = src.find("#include", incLoc + 1);
		}
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
		GLuint depthMSID = lastID == m_rtColor ? m_rtDepthMS : m_objects->GetRenderTexture(lastID)->DepthStencilBufferMS;

		pass->DepthTexture = depthID;

		if (pass->FBO != 0) {
			glDeleteFramebuffers(1, &pass->FBO);
			glDeleteFramebuffers(1, &m_fboMS[pass]);
		}

		// normal FBO
		glGenFramebuffers(1, &pass->FBO);
		glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)pass->FBO);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, depthID, 0);
		for (int i = 0; i < pass->RTCount; i++) {
			GLuint texID = pass->RenderTextures[i];

			if (texID == 0) continue;

			// attach
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, texID, 0);
		}
		GLenum retval = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);


		// MSAA fbo
		glGenFramebuffers(1, &m_fboMS[pass]);
		glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)m_fboMS[pass]);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE, depthMSID, 0);
		for (int i = 0; i < pass->RTCount; i++) {
			GLuint texID = pass->RenderTextures[i];

			if (texID == 0) continue;

			if (texID == m_rtColor) texID = m_rtColorMS;
			else texID = m_objects->GetRenderTexture(texID)->BufferMS;

			// attach
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D_MULTISAMPLE, texID, 0);
		}
		retval = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		m_fbosNeedUpdate = false;
	}
}