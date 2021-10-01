#include <SHADERed/Engine/GLUtils.h>
#include <SHADERed/Engine/GeometryFactory.h>
#include <SHADERed/Engine/Ray.h>
#include <SHADERed/Objects/DefaultState.h>
#include <SHADERed/Objects/Logger.h>
#include <SHADERed/Objects/Names.h>
#include <SHADERed/Objects/ObjectManager.h>
#include <SHADERed/Objects/PipelineManager.h>
#include <SHADERed/Objects/RenderEngine.h>
#include <SHADERed/Objects/Settings.h>
#include <SHADERed/Objects/ShaderCompiler.h>
#include <SHADERed/Objects/SystemVariableManager.h>

#include <algorithm>
#include <glm/gtx/intersect.hpp>

static const GLenum fboBuffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4, GL_COLOR_ATTACHMENT5, GL_COLOR_ATTACHMENT6, GL_COLOR_ATTACHMENT7, GL_COLOR_ATTACHMENT8, GL_COLOR_ATTACHMENT9, GL_COLOR_ATTACHMENT10, GL_COLOR_ATTACHMENT11, GL_COLOR_ATTACHMENT12, GL_COLOR_ATTACHMENT13, GL_COLOR_ATTACHMENT14, GL_COLOR_ATTACHMENT15 };
static const char* GeneralDebugShaderCode = R"(
#version 330

uniform vec4 _sed_dbg_pixel_color;
out vec4 outColor;

void main()
{
	outColor = _sed_dbg_pixel_color;
}
)";


namespace ed {
	void DebugDrawPrimitives(int& vertexStart, int vertexCount, int maxVertexCount, int vertexStrip, GLuint topology, GLuint varLoc, bool instanced, int instanceCount, bool useIndices = false, int vbase = 0)
	{
		int actualVertexCount = vertexCount;
		while (vertexStart < maxVertexCount) {
			float r = ((vertexStart + vbase) & 0x000000FF) / 255.0f;
			float g = (((vertexStart + vbase) & 0x0000FF00) >> 8) / 255.0f;
			float b = (((vertexStart + vbase) & 0x00FF0000) >> 16) / 255.0f;
			glUniform4f(varLoc, r, g, b, 0.0f);

			actualVertexCount = std::min<int>(vertexCount, maxVertexCount - vertexStart);
			if (actualVertexCount <= 0) break;

			if (useIndices) {
				if (instanced)
					glDrawElementsInstanced(GL_TRIANGLES, actualVertexCount, GL_UNSIGNED_INT, (void*)(vertexStart * sizeof(GLuint)), instanceCount);
				else
					glDrawElements(GL_TRIANGLES, actualVertexCount, GL_UNSIGNED_INT, (void*)(vertexStart * sizeof(GLuint)));
			} else {
				if (instanced)
					glDrawArraysInstanced(topology, vertexStart, actualVertexCount, instanceCount);
				else
					glDrawArrays(topology, vertexStart, actualVertexCount);
			}
			vertexStart += vertexCount - vertexStrip;
		}
	}
	void DebugDrawInstanced(int& iStart, int iStep, int iCount, int vertexCount, GLuint topology, GLuint varLoc, bool useIndices = false)
	{
		while (iStart < iCount) {
			float r = (iStart & 0x000000FF) / 255.0f;
			float g = ((iStart & 0x0000FF00) >> 8) / 255.0f;
			float b = ((iStart & 0x00FF0000) >> 16) / 255.0f;
			glUniform4f(varLoc, r, g, b, 0.0f);

			if (useIndices)
				glDrawElementsInstanced(GL_TRIANGLES, vertexCount, GL_UNSIGNED_INT, nullptr, std::min<int>(iStart + iStep, iCount));
			else
				glDrawArraysInstanced(topology, 0, vertexCount, std::min<int>(iStart + iStep, iCount)); // hax since baseInstance is 4.2 feature
			iStart += iStep;
		}
	}

	RenderEngine::RenderEngine(PipelineManager* pipeline, ObjectManager* objects, ProjectParser* project, MessageStack* msgs, PluginManager* plugins, DebugInformation* debugger)
			: m_pipeline(pipeline)
			, m_objects(objects)
			, m_project(project)
			, m_msgs(msgs)
			, m_plugins(plugins)
			, m_debug(debugger)
			, m_lastSize(0, 0)
			, m_pickAwaiting(false)
			, m_rtColor(0)
			, m_rtDepth(0)
			, m_fbosNeedUpdate(false)
			, m_computeSupported(true)
			, m_tessellationSupported(true)
			, m_tessMaxPatchVertices(0)
			, m_wasMultiPick(false)
	{
		m_paused = false;

		glGenTextures(1, &m_rtColor);
		glGenTextures(1, &m_rtDepth);
		glGenTextures(1, &m_rtColorMS);
		glGenTextures(1, &m_rtDepthMS);

		GLchar msg[1024];
		m_generalDebugShader = gl::CompileShader(GL_FRAGMENT_SHADER, GeneralDebugShaderCode);
		bool isDebugShaderCompiled = gl::CheckShaderCompilationStatus(m_generalDebugShader, msg);
		if (!isDebugShaderCompiled)
			Logger::Get().Log("Failed to compile the debug pixel shader.", true);
	}
	RenderEngine::~RenderEngine()
	{
		glDeleteTextures(1, &m_rtColor);
		glDeleteTextures(1, &m_rtDepth);
		glDeleteTextures(1, &m_rtColorMS);
		glDeleteTextures(1, &m_rtDepthMS);
		glDeleteShader(m_generalDebugShader);
		FlushCache();
	}
	void RenderEngine::Render(int width, int height, bool isDebug, PipelineItem* breakItem)
	{
		bool isMSAA = (Settings::Instance().Preview.MSAA != 1) && !isDebug;

		if (isMSAA)
			glEnable(GL_MULTISAMPLE);

		// recreate render texture if size has changed
		if (m_lastSize.x != width || m_lastSize.y != height) {
			m_lastSize = glm::vec2(width, height);

			glBindTexture(GL_TEXTURE_2D, m_rtColor);
			glTexImage2D(GL_TEXTURE_2D, 0, Settings::Instance().Project.UseAlphaChannel ? GL_RGBA32F : GL_RGB32F, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
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
			std::vector<ObjectManagerItem*>& objs = m_objects->GetObjects();
			for (int i = 0; i < objs.size(); i++) {
				if (objs[i]->Type == ObjectType::RenderTexture) {
					ed::RenderTextureObject* rtObj = objs[i]->RT;
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
		int debugID = DEBUG_ID_START;

		m_plugins->BeginRender();

		// check if we need to perform performance measurement
		bool performPerfMeasure = Settings::Instance().General.Profiler && m_lastPerfMeasure.GetElapsedTime() > 0.4f;
		if (performPerfMeasure) {
			bool isAllDone = true;
			unsigned long long totalTime = 0;

			for (int i = 0; i < m_perfTimers.size(); i++) {
				if (!m_perfTimers[i].IsDone) {
					isAllDone = false;

					int queryDone = 0;
					glGetQueryObjectiv(m_perfTimers[i].Object, GL_QUERY_RESULT_AVAILABLE, &queryDone);

					if (queryDone) {
						glGetQueryObjectui64v(m_perfTimers[i].Object, GL_QUERY_RESULT, &m_perfTimers[i].LastTime);
						m_perfTimers[i].IsDone = true;
					}
				}
				totalTime += m_perfTimers[i].LastTime;
			}
			
			m_totalPerfTime = totalTime;
			performPerfMeasure &= isAllDone;

			if (performPerfMeasure)
				m_lastPerfMeasure.Restart();
		}

		for (int i = 0; i < m_items.size(); i++) {
			PipelineItem* it = m_items[i];

			if (performPerfMeasure) {
				glBeginQuery(GL_TIME_ELAPSED, m_perfTimers[i].Object);
				m_perfTimers[i].IsDone = false;
			}

			if (it->Type == PipelineItem::ItemType::ShaderPass) {
				pipe::ShaderPass* data = (pipe::ShaderPass*)it->Data;

				if (!data->Active || data->Items.size() <= 0 || data->RTCount == 0)
					continue;

				const std::vector<GLuint>& srvs = m_objects->GetBindList(m_items[i]);
				const std::vector<GLuint>& ubos = m_objects->GetUniformBindList(m_items[i]);

				// create/update fbo if necessary
				m_updatePassFBO(data);

				if (m_shaders[i] == 0)
					continue;

				if (data->TSUsed && m_tessellationSupported) 
					glPatchParameteri(GL_PATCH_VERTICES, data->TSPatchVertices);

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
						ed::RenderTextureObject* rtObject = m_objects->GetByTextureID(rt)->RT;

						rtSize = rtObject->CalculateSize(width, height);

						// clear and bind rt (only if not used in last shader pass)
						bool usedPreviously = false;
						for (int j = 0; j < MAX_RENDER_TEXTURES; j++)
							if (previousTexture[j] == rt) {
								usedPreviously = true;
								break;
							}
						if (!usedPreviously && rtObject->Clear)
							glClearBufferfv(GL_COLOR, i, isDebug ? glm::value_ptr(glm::vec4(0.0f)) : glm::value_ptr(rtObject->ClearColor));

					} else if (!clearedWindow) {
						glClearBufferfv(GL_COLOR, i, isDebug ? glm::value_ptr(glm::vec4(0.0f)) : glm::value_ptr(Settings::Instance().Project.ClearColor));

						clearedWindow = true;
					}
				}
				for (int i = 0; i < data->RTCount; i++)
					previousTexture[i] = data->RenderTextures[i];

				// update viewport value
				systemVM.SetViewportSize(rtSize.x, rtSize.y);
				glViewport(0, 0, rtSize.x, rtSize.y);

				// bind shaders
				if (isDebug) {
					data->Variables.UpdateUniformInfo(m_debugShaders[i]);
					glUseProgram(m_debugShaders[i]);
				} else
					glUseProgram(m_shaders[i]);

				// bind shader resource views
				for (int j = 0; j < srvs.size(); j++) {
					ObjectManagerItem* srvData = m_objects->GetByTextureID(srvs[j]);

					glActiveTexture(GL_TEXTURE0 + j);
					if (srvData->Type == ObjectType::CubeMap)
						glBindTexture(GL_TEXTURE_CUBE_MAP, srvs[j]);
					else if (srvData->Type == ObjectType::Image3D || srvData->Type == ObjectType::Texture3D)
						glBindTexture(GL_TEXTURE_3D, srvs[j]);
					else if (srvData->Type == ObjectType::PluginObject) {
						PluginObject* pobj = srvData->Plugin;
						pobj->Owner->Object_Bind(pobj->Type, pobj->Data, pobj->ID);
					} else
						glBindTexture(GL_TEXTURE_2D, srvs[j]);

					
					if (ShaderCompiler::GetShaderLanguageFromExtension(data->PSPath) == ShaderLanguage::GLSL) // TODO: or should this be for vulkan glsl too?
						data->Variables.UpdateTexture(m_shaders[i], j);
				}

				for (int j = 0; j < ubos.size(); j++)
					glBindBufferBase(GL_SHADER_STORAGE_BUFFER, j, ubos[j]);

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
					if (item->Type == PipelineItem::ItemType::Geometry || item->Type == PipelineItem::ItemType::Model || item->Type == PipelineItem::ItemType::VertexBuffer || item->Type == PipelineItem::ItemType::PluginItem) {
						if (m_pickAwaiting) m_pickItem(item, m_wasMultiPick);
						for (int k = 0; k < itemVarValues.size(); k++)
							if (itemVarValues[k].Item == item)
								itemVarValues[k].Variable->Data = itemVarValues[k].NewValue->Data;

						if (isDebug) {
							float r = (debugID & 0x000000FF) / 255.0f;
							float g = ((debugID & 0x0000FF00) >> 8) / 255.0f;
							float b = ((debugID & 0x00FF0000) >> 16) / 255.0f;
							float a = 0.0f; // Maybe pack additional data here?
							glUniform4f(glGetUniformLocation(m_debugShaders[i], "_sed_dbg_pixel_color"), r, g, b, a);
							debugID++;
						}
					}

					if (item->Type == PipelineItem::ItemType::Geometry) {
						pipe::GeometryItem* geoData = reinterpret_cast<pipe::GeometryItem*>(item->Data);

						if (geoData->Type == pipe::GeometryItem::Rectangle) {
							// TODO: don't multiply with m_renderer->GetLastRenderSize() but rather with actual RT size
							glm::vec3 scaleRect(geoData->Scale.x * rtSize.x, geoData->Scale.y * rtSize.y, 1.0f);
							glm::vec3 posRect((geoData->Position.x + 0.5f) * rtSize.x, (geoData->Position.y + 0.5f) * rtSize.y, -1000.0f);
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
					} else if (item->Type == PipelineItem::ItemType::Model) {
						pipe::Model* objData = reinterpret_cast<pipe::Model*>(item->Data);

						systemVM.SetPicked(std::count(m_pick.begin(), m_pick.end(), item));
						systemVM.SetGeometryTransform(item, objData->Scale, objData->Rotation, objData->Position);

						// bind variables
						data->Variables.Bind(item);

						objData->Data->Draw(objData->Instanced, objData->InstanceCount);
					} else if (item->Type == PipelineItem::ItemType::VertexBuffer) {
						pipe::VertexBuffer* vbData = reinterpret_cast<pipe::VertexBuffer*>(item->Data);
						ed::BufferObject* bobj = (ed::BufferObject*)vbData->Buffer;

						if (bobj != 0) {
							auto bobjFmt = m_objects->ParseBufferFormat(bobj->ViewFormat);
							int stride = 0;
							for (const auto& f : bobjFmt)
								stride += ShaderVariable::GetSize(f, true);

							if (stride != 0) {
								int vertCount = bobj->Size / stride;

								systemVM.SetGeometryTransform(item, vbData->Scale, vbData->Rotation, vbData->Position);
								systemVM.SetPicked(std::count(m_pick.begin(), m_pick.end(), item));

								// bind variables
								data->Variables.Bind(item);

								
								glBindVertexArray(vbData->VAO);
								if (vbData->Instanced)
									glDrawArraysInstanced(vbData->Topology, 0, vertCount, vbData->InstanceCount);
								else
									glDrawArrays(vbData->Topology, 0, vertCount);
							}
						}
					} else if (item->Type == PipelineItem::ItemType::RenderState) {
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
						if (state->Blend && !isDebug) {
							glEnable(GL_BLEND);
							glBlendEquationSeparate(state->BlendFunctionColor, state->BlendFunctionAlpha);
							glBlendFuncSeparate(state->BlendSourceFactorRGB, state->BlendDestinationFactorRGB, state->BlendSourceFactorAlpha, state->BlendDestinationFactorAlpha);
							glBlendColor(state->BlendFactor.r, state->BlendFactor.g, state->BlendFactor.a, state->BlendFactor.a);
							glSampleCoverage(state->AlphaToCoverage, GL_FALSE);
						} else
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
						} else
							glDisable(GL_STENCIL_TEST);
					} else if (item->Type == PipelineItem::ItemType::PluginItem) {
						pipe::PluginItemData* pldata = reinterpret_cast<pipe::PluginItemData*>(item->Data);

						if (m_pickAwaiting && pldata->Owner->PipelineItem_IsPickable(pldata->Type, pldata->PluginData))
							m_pickItem(item, m_wasMultiPick);

						if (pldata->Owner->PipelineItem_IsPickable(pldata->Type, pldata->PluginData))
							systemVM.SetPicked(std::count(m_pick.begin(), m_pick.end(), item));
						else
							systemVM.SetPicked(false);

						pldata->Owner->PipelineItem_Execute(data, plugin::PipelineItemType::ShaderPass, pldata->Type, pldata->PluginData);
					}

					// set the old value back
					if (item->Type == PipelineItem::ItemType::Geometry || item->Type == PipelineItem::ItemType::Model || item->Type == PipelineItem::ItemType::VertexBuffer || item->Type == PipelineItem::ItemType::PluginItem)
						for (int k = 0; k < itemVarValues.size(); k++)
							if (itemVarValues[k].Item == item)
								itemVarValues[k].Variable->Data = itemVarValues[k].OldValue;
				}

				if (isDebug)
					data->Variables.UpdateUniformInfo(m_shaders[i]); // return old variable data

				if (isMSAA) {
					glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fboMS[data]);
					glBindFramebuffer(GL_DRAW_FRAMEBUFFER, data->FBO);
					glDrawBuffer(GL_BACK);
					for (unsigned int i = 0; i < data->RTCount; i++) {
						glReadBuffer(GL_COLOR_ATTACHMENT0 + i);
						glDrawBuffer(GL_COLOR_ATTACHMENT0 + i);
						glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
					}
				}
			}
			else if (it->Type == PipelineItem::ItemType::ComputePass && !isDebug && (!m_paused || SystemVariableManager::Instance().IsSavingToFile()) && m_computeSupported) {
				pipe::ComputePass* data = (pipe::ComputePass*)it->Data;

				if (!data->Active)
					continue;

				const std::vector<GLuint>& srvs = m_objects->GetBindList(m_items[i]);
				const std::vector<GLuint>& ubos = m_objects->GetUniformBindList(m_items[i]);

				if (m_shaders[i] == 0)
					continue;

				// bind shaders
				glUseProgram(m_shaders[i]);
				
				// bind shader resource views
				for (int j = 0; j < srvs.size(); j++) {
					ObjectManagerItem* srvData = m_objects->GetByTextureID(srvs[j]);

					glActiveTexture(GL_TEXTURE0 + j);
					if (srvData->Type == ObjectType::CubeMap)
						glBindTexture(GL_TEXTURE_CUBE_MAP, srvs[j]);
					else if (srvData->Type == ObjectType::Image3D || srvData->Type == ObjectType::Texture3D)
						glBindTexture(GL_TEXTURE_3D, srvs[j]);
					else if (srvData->Type == ObjectType::PluginObject) {
						PluginObject* pobj = srvData->Plugin;
						pobj->Owner->Object_Bind(pobj->Type, pobj->Data, pobj->ID);
					} else
						glBindTexture(GL_TEXTURE_2D, srvs[j]);

					if (ShaderCompiler::GetShaderLanguageFromExtension(data->Path) == ShaderLanguage::GLSL)
						data->Variables.UpdateTexture(m_shaders[i], j);
				}

				// bind buffers
				int cMax = (m_uboMax[data] = std::max<int>(ubos.size(), m_uboMax[data]));
				for (int j = ubos.size(); j < cMax; j++)
					glBindBufferBase(GL_SHADER_STORAGE_BUFFER, j, 0);

				for (int j = 0; j < ubos.size(); j++) {
					ObjectManagerItem* uboData = m_objects->GetByTextureID(ubos[j]);
					if (uboData == nullptr)
						uboData = m_objects->GetByBufferID(ubos[j]);

					if (uboData->Type == ObjectType::Image) {
						ImageObject* iobj = uboData->Image;
						glBindImageTexture(j, ubos[j], 0, GL_FALSE, 0, GL_WRITE_ONLY | GL_READ_ONLY, iobj->Format);
					} else if (uboData->Type == ObjectType::Image3D) {
						Image3DObject* iobj = uboData->Image3D;
						glBindImageTexture(j, ubos[j], 0, GL_TRUE, 0, GL_WRITE_ONLY | GL_READ_ONLY, iobj->Format);
					} else if (uboData->Type == ObjectType::PluginObject) {
						PluginObject* pobj = uboData->Plugin;
						pobj->Owner->Object_Bind(pobj->Type, pobj->Data, pobj->ID);
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
			else if (it->Type == PipelineItem::ItemType::AudioPass && !isDebug) {
				pipe::AudioPass* data = (pipe::AudioPass*)it->Data;

				const std::vector<GLuint>& srvs = m_objects->GetBindList(m_items[i]);
				const std::vector<GLuint>& ubos = m_objects->GetUniformBindList(m_items[i]);

				// bind shader resource views
				for (int j = 0; j < srvs.size(); j++) {
					ObjectManagerItem* srvData = m_objects->GetByTextureID(srvs[j]);

					glActiveTexture(GL_TEXTURE0 + j);
					if (srvData->Type == ObjectType::CubeMap)
						glBindTexture(GL_TEXTURE_CUBE_MAP, srvs[j]);
					else if (srvData->Type == ObjectType::Image3D || srvData->Type == ObjectType::Texture3D)
						glBindTexture(GL_TEXTURE_3D, srvs[j]);
					else if (srvData->Type == ObjectType::PluginObject) {
						PluginObject* pobj = srvData->Plugin;
						pobj->Owner->Object_Bind(pobj->Type, pobj->Data, pobj->ID);
					} else
						glBindTexture(GL_TEXTURE_2D, srvs[j]);

					if (ShaderCompiler::GetShaderLanguageFromExtension(data->Path) == ShaderLanguage::GLSL) // TODO: or should this be for vulkan glsl too?
						data->Variables.UpdateTexture(m_shaders[i], j);
				}

				// bind buffers
				for (int j = 0; j < ubos.size(); j++)
					glBindBufferBase(GL_SHADER_STORAGE_BUFFER, j, ubos[j]);

				// bind variables
				data->Variables.Bind();

				data->Stream.RenderAudio();
			}
			else if (it->Type == PipelineItem::ItemType::PluginItem) {
				pipe::PluginItemData* pldata = reinterpret_cast<pipe::PluginItemData*>(it->Data);

				if (!isDebug)
					pldata->Owner->PipelineItem_Execute(pldata->Type, pldata->PluginData, pldata->Items.data(), pldata->Items.size());
				else if (pldata->Owner->PipelineItem_IsDebuggable(pldata->Type, pldata->PluginData))
					pldata->Owner->PipelineItem_DebugExecute(pldata->Type, pldata->PluginData, pldata->Items.data(), pldata->Items.size(), &debugID);
			}

			if (performPerfMeasure)
				glEndQuery(GL_TIME_ELAPSED);

			if (it == breakItem && breakItem != nullptr)
				break;
		}

		m_plugins->EndRender();

		// update frame index
		if (!m_paused) {
			systemVM.CopyState();
			systemVM.SetFrameIndex(systemVM.GetFrameIndex() + 1);
		}

		// restore real render target view
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		if (m_pickAwaiting) {
			if (m_pickDist == std::numeric_limits<float>::infinity())
				m_pick.clear();
			if (m_pickHandle != nullptr)
				m_pickHandle(m_pick.size() == 0 ? nullptr : m_pick[m_pick.size() - 1]);
			m_pickAwaiting = false;
		}

		if (isMSAA)
			glDisable(GL_MULTISAMPLE);
	}
	int RenderEngine::DebugVertexPick(PipelineItem* vertexData, PipelineItem* vertexItem, glm::vec2 r, int group)
	{
		if (vertexData->Type == PipelineItem::ItemType::ShaderPass) {
			pipe::ShaderPass* vertexPass = (pipe::ShaderPass*)vertexData->Data;

			int vertexPassID = 0;
			for (int i = 0; i < m_items.size(); i++)
				if (m_items[i] == vertexData)
					vertexPassID = i;

			// _sed_dbg_pixel_color
			GLuint sedVarLoc = glGetUniformLocation(m_debugShaders[vertexPassID], "_sed_dbg_pixel_color");

			// update info
			vertexPass->Variables.UpdateUniformInfo(m_debugShaders[vertexPassID]);

			// get resources
			const std::vector<GLuint>& srvs = m_objects->GetBindList(vertexData);
			const std::vector<GLuint>& ubos = m_objects->GetUniformBindList(vertexData);

			// item variable values
			auto& itemVarValues = GetItemVariableValues();

			// bind fbo and buffers
			glBindFramebuffer(GL_FRAMEBUFFER, vertexPass->FBO);
			glDrawBuffers(vertexPass->RTCount, fboBuffers);

			glStencilMask(0xFFFFFFFF);
			glClearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 0);

			// bind RTs
			int rtCount = MAX_RENDER_TEXTURES;
			glm::vec2 rtSize(m_lastSize.x, m_lastSize.y);
			for (int i = 0; i < MAX_RENDER_TEXTURES; i++) {
				if (vertexPass->RenderTextures[i] == 0) {
					rtCount = i;
					break;
				}

				GLuint rt = vertexPass->RenderTextures[i];

				if (rt != m_rtColor) {
					ed::RenderTextureObject* rtObject = m_objects->GetByTextureID(rt)->RT;
					rtSize = rtObject->CalculateSize(m_lastSize.x, m_lastSize.y);
				}

				glClearBufferfv(GL_COLOR, i, glm::value_ptr(glm::vec4(0.0f)));
			}

			// update viewport value
			glViewport(0, 0, rtSize.x, rtSize.y);

			// bind shaders
			glUseProgram(m_debugShaders[vertexPassID]);

			// bind shader resource views
			for (int j = 0; j < srvs.size(); j++) {
				ObjectManagerItem* srvData = m_objects->GetByTextureID(srvs[j]);

				glActiveTexture(GL_TEXTURE0 + j);
				if (srvData->Type == ObjectType::CubeMap)
					glBindTexture(GL_TEXTURE_CUBE_MAP, srvs[j]);
				else if (srvData->Type == ObjectType::Image3D || srvData->Type == ObjectType::Texture3D)
					glBindTexture(GL_TEXTURE_3D, srvs[j]);
				else if (srvData->Type == ObjectType::PluginObject) {
					PluginObject* pobj = srvData->Plugin;
					pobj->Owner->Object_Bind(pobj->Type, pobj->Data, pobj->ID);
				} else
					glBindTexture(GL_TEXTURE_2D, srvs[j]);

				if (ShaderCompiler::GetShaderLanguageFromExtension(vertexPass->PSPath) == ShaderLanguage::GLSL) // TODO: or should this be for vulkan glsl too?
					vertexPass->Variables.UpdateTexture(m_debugShaders[vertexPassID], j);
			}
			for (int j = 0; j < ubos.size(); j++)
				glBindBufferBase(GL_UNIFORM_BUFFER, j, ubos[j]);

			// bind default states for each shader pass
			SystemVariableManager& systemVM = SystemVariableManager::Instance();

			// data
			int x = r.x * rtSize.x;
			int y = r.y * rtSize.y;
			uint8_t* mainPixelData = new uint8_t[(int)(rtSize.x * rtSize.y) * 4];

			// render pipeline items
			DefaultState::Bind();
			for (int j = 0; j < vertexPass->Items.size(); j++) {
				PipelineItem* item = vertexPass->Items[j];

				// update the value for this element and check if we picked it
				if (item->Type == PipelineItem::ItemType::Geometry || item->Type == PipelineItem::ItemType::Model || item->Type == PipelineItem::ItemType::VertexBuffer || item->Type == PipelineItem::ItemType::PluginItem) {
					if (item != vertexItem)
						continue;
					for (int k = 0; k < itemVarValues.size(); k++)
						if (itemVarValues[k].Item == item)
							itemVarValues[k].Variable->Data = itemVarValues[k].NewValue->Data;
				}

				if (item->Type == PipelineItem::ItemType::Geometry) {
					pipe::GeometryItem* geoData = reinterpret_cast<pipe::GeometryItem*>(item->Data);

					if (geoData->Type == pipe::GeometryItem::Rectangle) {
						glm::vec3 scaleRect(geoData->Scale.x * rtSize.x, geoData->Scale.y * rtSize.y, 1.0f);
						glm::vec3 posRect((geoData->Position.x + 0.5f) * rtSize.x, (geoData->Position.y + 0.5f) * rtSize.y, -1000.0f);
						systemVM.SetGeometryTransform(item, scaleRect, geoData->Rotation, posRect);
					} else
						systemVM.SetGeometryTransform(item, geoData->Scale, geoData->Rotation, geoData->Position);

					systemVM.SetPicked(std::count(m_pick.begin(), m_pick.end(), item));

					// bind variables
					vertexPass->Variables.Bind(item);

					int topologySelection = 0;
					for (; topologySelection < (sizeof(TOPOLOGY_ITEM_VALUES) / sizeof(*TOPOLOGY_ITEM_VALUES)); topologySelection++)
						if (TOPOLOGY_ITEM_VALUES[topologySelection] == geoData->Topology)
							break;

					int singlePrimitiveVCount = TOPOLOGY_SINGLE_VERTEX_COUNT[topologySelection];
					int vertexStart = (group >= 0) * group;
					int maxVertexCount = (group < 0 ? eng::GeometryFactory::VertexCount[geoData->Type] : (vertexStart + DEBUG_PRIMITIVE_GROUP * singlePrimitiveVCount));
					int vertexCount = (group < 0 ? DEBUG_PRIMITIVE_GROUP : 1) * singlePrimitiveVCount;
					int vertexStrip = (group >= 0) * TOPOLOGY_IS_STRIP[topologySelection];

					maxVertexCount = std::min<int>(maxVertexCount, eng::GeometryFactory::VertexCount[geoData->Type]);

					glBindVertexArray(geoData->VAO);
					DebugDrawPrimitives(vertexStart, vertexCount, maxVertexCount, vertexStrip, geoData->Topology, sedVarLoc, geoData->Instanced, geoData->InstanceCount);
				} else if (item->Type == PipelineItem::ItemType::Model) {
					pipe::Model* objData = reinterpret_cast<pipe::Model*>(item->Data);

					systemVM.SetPicked(std::count(m_pick.begin(), m_pick.end(), item));
					systemVM.SetGeometryTransform(item, objData->Scale, objData->Rotation, objData->Position);

					// bind variables
					vertexPass->Variables.Bind(item);

					int vbase = 0;
					for (const auto& mesh : objData->Data->Meshes) {
						int vertexStart = (group >= 0) * group;
						int maxVertexCount = (group < 0 ? mesh.Indices.size() : (vertexStart + DEBUG_PRIMITIVE_GROUP * 3));
						int vertexCount = (group < 0 ? DEBUG_PRIMITIVE_GROUP : 1) * 3;

						maxVertexCount = std::min<int>(maxVertexCount, mesh.Indices.size());

						glBindVertexArray(mesh.VAO);
						DebugDrawPrimitives(vertexStart, vertexCount, maxVertexCount, 0, GL_TRIANGLES, sedVarLoc, objData->Instanced, objData->InstanceCount, true, vbase);

						vbase += mesh.Indices.size();
					}
				} else if (item->Type == PipelineItem::ItemType::PluginItem) {
					pipe::PluginItemData* plData = reinterpret_cast<pipe::PluginItemData*>(item->Data);

					if (plData->Owner->PipelineItem_IsPickable(plData->Type, plData->PluginData))
						systemVM.SetPicked(std::count(m_pick.begin(), m_pick.end(), item));
					else
						systemVM.SetPicked(false);

					plData->Owner->PipelineItem_DebugVertexExecute(vertexPass, plugin::PipelineItemType::ShaderPass, plData->Type, plData->PluginData, sedVarLoc);
				} else if (item->Type == PipelineItem::ItemType::VertexBuffer) {
					pipe::VertexBuffer* vbData = reinterpret_cast<pipe::VertexBuffer*>(item->Data);
					ed::BufferObject* bobj = (ed::BufferObject*)vbData->Buffer;

					auto bobjFmt = m_objects->ParseBufferFormat(bobj->ViewFormat);
					int stride = 0;
					for (const auto& f : bobjFmt)
						stride += ShaderVariable::GetSize(f, true);

					if (stride != 0) {
						int actualMaxVertexCount = bobj->Size / stride;

						systemVM.SetGeometryTransform(item, vbData->Scale, vbData->Rotation, vbData->Position);
						systemVM.SetPicked(std::count(m_pick.begin(), m_pick.end(), item));

						// bind variables
						vertexPass->Variables.Bind(item);

						int topologySelection = 0;
						for (; topologySelection < (sizeof(TOPOLOGY_ITEM_VALUES) / sizeof(*TOPOLOGY_ITEM_VALUES)); topologySelection++)
							if (TOPOLOGY_ITEM_VALUES[topologySelection] == vbData->Topology)
								break;

						int singlePrimitiveVCount = TOPOLOGY_SINGLE_VERTEX_COUNT[topologySelection];
						int vertexStart = (group >= 0) * group;
						int maxVertexCount = (group < 0 ? actualMaxVertexCount : (vertexStart + DEBUG_PRIMITIVE_GROUP * singlePrimitiveVCount));
						int vertexCount = (group < 0 ? DEBUG_PRIMITIVE_GROUP : 1) * singlePrimitiveVCount;
						int vertexStrip = (group >= 0) * TOPOLOGY_IS_STRIP[topologySelection];

						maxVertexCount = std::min<int>(maxVertexCount, actualMaxVertexCount);

						glBindVertexArray(vbData->VAO);
						DebugDrawPrimitives(vertexStart, vertexCount, maxVertexCount, vertexStrip, vbData->Topology, sedVarLoc, vbData->Instanced, vbData->InstanceCount);
					}
				} else if (item->Type == PipelineItem::ItemType::RenderState) {
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
				}

				// set the old value back
				if (item->Type == PipelineItem::ItemType::Geometry || item->Type == PipelineItem::ItemType::Model || item->Type == PipelineItem::ItemType::VertexBuffer || item->Type == PipelineItem::ItemType::PluginItem)
					for (int k = 0; k < itemVarValues.size(); k++)
						if (itemVarValues[k].Item == item)
							itemVarValues[k].Variable->Data = itemVarValues[k].OldValue;
			}

			// window pixel color
			int vertexGroup = 0x00ffffff & getPixelID(vertexPass->RenderTextures[0], mainPixelData, x, y, rtSize.x);

			// return old info
			vertexPass->Variables.UpdateUniformInfo(m_shaders[vertexPassID]);
			delete[] mainPixelData;

			return vertexGroup;
		}
		else if (vertexData->Type == PipelineItem::ItemType::PluginItem) {
			pipe::PluginItemData* plData = (pipe::PluginItemData*)vertexData->Data;
			plData->Owner->BeginRender();
			int ret = plData->Owner->PipelineItem_DebugVertexExecute(plData->Type, plData->PluginData, vertexItem->Name, r.x, r.y, group);
			plData->Owner->EndRender();
			return ret;
		}
		
		return 0;
	}
	int RenderEngine::DebugInstancePick(PipelineItem* vertexData, PipelineItem* vertexItem, glm::vec2 r, int group)
	{
		if (vertexItem->Type == PipelineItem::ItemType::Geometry) {
			pipe::GeometryItem* geoData = reinterpret_cast<pipe::GeometryItem*>(vertexItem->Data);
			if (!geoData->Instanced)
				return 0;
		} else if (vertexItem->Type == PipelineItem::ItemType::Model) {
			pipe::Model* objData = reinterpret_cast<pipe::Model*>(vertexItem->Data);
			if (!objData->Instanced)
				return 0;
		} else if (vertexItem->Type == PipelineItem::ItemType::VertexBuffer) {
			pipe::VertexBuffer* objData = reinterpret_cast<pipe::VertexBuffer*>(vertexItem->Data);
			if (!objData->Instanced)
				return 0;
		} else
			return 0;

		if (vertexData->Type == PipelineItem::ItemType::ShaderPass) {
			pipe::ShaderPass* vertexPass = (pipe::ShaderPass*)vertexData->Data;

			int vertexPassID = 0;
			for (int i = 0; i < m_items.size(); i++)
				if (m_items[i] == vertexData)
					vertexPassID = i;

			// _sed_dbg_pixel_color
			GLuint sedVarLoc = glGetUniformLocation(m_debugShaders[vertexPassID], "_sed_dbg_pixel_color");

			// update info
			vertexPass->Variables.UpdateUniformInfo(m_debugShaders[vertexPassID]);

			// get resources
			const std::vector<GLuint>& srvs = m_objects->GetBindList(vertexData);
			const std::vector<GLuint>& ubos = m_objects->GetUniformBindList(vertexData);

			// item variable values
			auto& itemVarValues = GetItemVariableValues();

			// bind fbo and buffers
			glBindFramebuffer(GL_FRAMEBUFFER, vertexPass->FBO);
			glDrawBuffers(vertexPass->RTCount, fboBuffers);

			glStencilMask(0xFFFFFFFF);
			glClearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 0);

			// bind RTs
			int rtCount = MAX_RENDER_TEXTURES;
			glm::vec2 rtSize(m_lastSize.x, m_lastSize.y);
			for (int i = 0; i < MAX_RENDER_TEXTURES; i++) {
				if (vertexPass->RenderTextures[i] == 0) {
					rtCount = i;
					break;
				}

				GLuint rt = vertexPass->RenderTextures[i];

				if (rt != m_rtColor) {
					ObjectManagerItem* rtObject = m_objects->GetByTextureID(rt);
					rtSize = rtObject->RT->CalculateSize(m_lastSize.x, m_lastSize.y);
				}

				glClearBufferfv(GL_COLOR, i, glm::value_ptr(glm::vec4(0.0f)));
			}

			// update viewport value
			glViewport(0, 0, rtSize.x, rtSize.y);

			// bind shaders
			glUseProgram(m_debugShaders[vertexPassID]);

			// bind shader resource views
			for (int j = 0; j < srvs.size(); j++) {
				ObjectManagerItem* srvData = m_objects->GetByTextureID(srvs[j]);

				glActiveTexture(GL_TEXTURE0 + j);
				if (srvData->Type == ObjectType::CubeMap)
					glBindTexture(GL_TEXTURE_CUBE_MAP, srvs[j]);
				else if (srvData->Type == ObjectType::Image3D || srvData->Type == ObjectType::Texture3D)
					glBindTexture(GL_TEXTURE_3D, srvs[j]);
				else if (srvData->Type == ObjectType::PluginObject) {
					PluginObject* pobj = srvData->Plugin;
					pobj->Owner->Object_Bind(pobj->Type, pobj->Data, pobj->ID);
				} else
					glBindTexture(GL_TEXTURE_2D, srvs[j]);

				if (ShaderCompiler::GetShaderLanguageFromExtension(vertexPass->PSPath) == ShaderLanguage::GLSL) // TODO: or should this be for vulkan glsl too?
					vertexPass->Variables.UpdateTexture(m_debugShaders[vertexPassID], j);
			}
			for (int j = 0; j < ubos.size(); j++)
				glBindBufferBase(GL_UNIFORM_BUFFER, j, ubos[j]);

			// bind default states for each shader pass
			SystemVariableManager& systemVM = SystemVariableManager::Instance();

			// data
			int x = r.x * rtSize.x;
			int y = r.y * rtSize.y;
			uint8_t* mainPixelData = new uint8_t[(int)(rtSize.x * rtSize.y) * 4];

			// render pipeline items
			DefaultState::Bind();
			for (int j = 0; j < vertexPass->Items.size(); j++) {
				PipelineItem* item = vertexPass->Items[j];

				// update the value for this element and check if we picked it
				if (item->Type == PipelineItem::ItemType::Geometry || item->Type == PipelineItem::ItemType::Model || item->Type == PipelineItem::ItemType::VertexBuffer || item->Type == PipelineItem::ItemType::PluginItem) {
					if (item != vertexItem)
						continue;
					for (int k = 0; k < itemVarValues.size(); k++)
						if (itemVarValues[k].Item == item)
							itemVarValues[k].Variable->Data = itemVarValues[k].NewValue->Data;
				}

				if (item->Type == PipelineItem::ItemType::Geometry) {
					pipe::GeometryItem* geoData = reinterpret_cast<pipe::GeometryItem*>(item->Data);

					if (geoData->Type == pipe::GeometryItem::Rectangle) {
						glm::vec3 scaleRect(geoData->Scale.x * rtSize.x, geoData->Scale.y * rtSize.y, 1.0f);
						glm::vec3 posRect((geoData->Position.x + 0.5f) * rtSize.x, (geoData->Position.y + 0.5f) * rtSize.y, -1000.0f);
						systemVM.SetGeometryTransform(item, scaleRect, geoData->Rotation, posRect);
					} else
						systemVM.SetGeometryTransform(item, geoData->Scale, geoData->Rotation, geoData->Position);

					systemVM.SetPicked(std::count(m_pick.begin(), m_pick.end(), item));

					// bind variables
					vertexPass->Variables.Bind(item);

					int vertexCount = eng::GeometryFactory::VertexCount[geoData->Type];
					int iStart = (group >= 0) * group;
					int iCount = group < 0 ? geoData->InstanceCount : (geoData->InstanceCount + DEBUG_INSTANCE_GROUP);
					int iStep = group < 0 ? DEBUG_INSTANCE_GROUP : 1;

					iCount = std::min<int>(iCount, geoData->InstanceCount);

					glBindVertexArray(geoData->VAO);
					DebugDrawInstanced(iStart, iStep, iCount, vertexCount, geoData->Topology, sedVarLoc);
				} else if (item->Type == PipelineItem::ItemType::Model) {
					pipe::Model* objData = reinterpret_cast<pipe::Model*>(item->Data);

					systemVM.SetPicked(std::count(m_pick.begin(), m_pick.end(), item));
					systemVM.SetGeometryTransform(item, objData->Scale, objData->Rotation, objData->Position);

					// bind variables
					vertexPass->Variables.Bind(item);

					for (const auto& mesh : objData->Data->Meshes) {
						int vertexCount = mesh.Indices.size();
						int iStart = (group >= 0) * group;
						int iCount = group < 0 ? objData->InstanceCount : (objData->InstanceCount + DEBUG_INSTANCE_GROUP);
						int iStep = group < 0 ? DEBUG_INSTANCE_GROUP : 1;

						iCount = std::min<int>(iCount, objData->InstanceCount);

						glBindVertexArray(mesh.VAO);
						DebugDrawInstanced(iStart, iStep, iCount, vertexCount, GL_TRIANGLES, sedVarLoc);
					}
				} else if (item->Type == PipelineItem::ItemType::VertexBuffer) {
					pipe::VertexBuffer* vbData = reinterpret_cast<pipe::VertexBuffer*>(item->Data);
					ed::BufferObject* bobj = (ed::BufferObject*)vbData->Buffer;

					auto bobjFmt = m_objects->ParseBufferFormat(bobj->ViewFormat);
					int stride = 0;
					for (const auto& f : bobjFmt)
						stride += ShaderVariable::GetSize(f, true);

					if (stride != 0) {
						int vertexCount = bobj->Size / stride;

						systemVM.SetGeometryTransform(item, vbData->Scale, vbData->Rotation, vbData->Position);
						systemVM.SetPicked(std::count(m_pick.begin(), m_pick.end(), item));

						// bind variables
						vertexPass->Variables.Bind(item);

						int iStart = (group >= 0) * group;
						int iCount = group < 0 ? vbData->InstanceCount : (vbData->InstanceCount + DEBUG_INSTANCE_GROUP);
						int iStep = group < 0 ? DEBUG_INSTANCE_GROUP : 1;

						iCount = std::min<int>(iCount, vbData->InstanceCount);

						glBindVertexArray(vbData->VAO);
						DebugDrawInstanced(iStart, iStep, iCount, vertexCount, vbData->Topology, sedVarLoc);
					}
				} else if (item->Type == PipelineItem::ItemType::RenderState) {
					pipe::RenderState* state = reinterpret_cast<pipe::RenderState*>(item->Data);

					// culling and front face (only thing we care about when picking a vertex, i think)
					if (state->CullFace)
						glEnable(GL_CULL_FACE);
					else
						glDisable(GL_CULL_FACE);
					glCullFace(state->CullFaceType);
					glFrontFace(state->FrontFace);
				} else if (item->Type == PipelineItem::ItemType::PluginItem) {
					pipe::PluginItemData* plData = reinterpret_cast<pipe::PluginItemData*>(item->Data);

					if (plData->Owner->PipelineItem_IsPickable(plData->Type, plData->PluginData))
						systemVM.SetPicked(std::count(m_pick.begin(), m_pick.end(), item));
					else
						systemVM.SetPicked(false);

					plData->Owner->PipelineItem_DebugInstanceExecute(vertexPass, plugin::PipelineItemType::ShaderPass, plData->Type, plData->PluginData, sedVarLoc);
				}

				// set the old value back
				if (item->Type == PipelineItem::ItemType::Geometry || item->Type == PipelineItem::ItemType::Model || item->Type == PipelineItem::ItemType::VertexBuffer || item->Type == PipelineItem::ItemType::PluginItem)
					for (int k = 0; k < itemVarValues.size(); k++)
						if (itemVarValues[k].Item == item)
							itemVarValues[k].Variable->Data = itemVarValues[k].OldValue;
			}

			// window pixel color
			int vertexGroup = 0x00ffffff & getPixelID(vertexPass->RenderTextures[0], mainPixelData, x, y, rtSize.x);

			// return old info
			vertexPass->Variables.UpdateUniformInfo(m_shaders[vertexPassID]);
			delete[] mainPixelData;

			return vertexGroup;
		}
		else if (vertexData->Type == PipelineItem::ItemType::PluginItem) {
			pipe::PluginItemData* plData = (pipe::PluginItemData*)vertexData->Data;
			plData->Owner->BeginRender();
			int ret = plData->Owner->PipelineItem_DebugInstanceExecute(plData->Type, plData->PluginData, vertexItem->Name, r.x, r.y, group);
			plData->Owner->EndRender();
			return ret;
		}

		return 0;
	}
	void RenderEngine::Pause(bool pause)
	{
		m_paused = pause;

		if (m_paused)
			SystemVariableManager::Instance().GetTimeClock().Pause();
		else
			SystemVariableManager::Instance().GetTimeClock().Resume();

		auto& itemList = m_pipeline->GetList();
		for (int i = 0; i < itemList.size(); i++) {
			if (itemList[i]->Type == PipelineItem::ItemType::AudioPass) {
				pipe::AudioPass* pass = (pipe::AudioPass*)itemList[i]->Data;

				if (m_paused)
					pass->Stream.Stop();
				else
					pass->Stream.Start();
			}
		}

		m_debug->ClearPixelList();
	}
	void RenderEngine::Recompile(const char* name)
	{
		Logger::Get().Log("Recompiling " + std::string(name));

		m_msgs->BuildOccured = true;
		m_msgs->CurrentItem = name;

		m_plugins->HandleApplicationEvent(plugin::ApplicationEvent::PipelineItemCompiled, (void*)name, nullptr);

		GLchar shaderMessage[1024] = { 0 };
		for (int i = 0; i < m_items.size(); i++) {
			PipelineItem* item = m_items[i];
			if (strcmp(item->Name, name) == 0) {
				if (item->Type == PipelineItem::ItemType::ShaderPass) {
					pipe::ShaderPass* shader = (pipe::ShaderPass*)item->Data;
					int shaderMessagesBefore = m_msgs->GetGroupErrorAndWarningMsgCount(name);

					SPIRVQueue.push_back(item);

					m_msgs->ClearGroup(name);

					glDeleteShader(m_shaderSources[i].VS);
					glDeleteShader(m_shaderSources[i].PS);
					glDeleteShader(m_shaderSources[i].GS);
					glDeleteShader(m_shaderSources[i].TCS);
					glDeleteShader(m_shaderSources[i].TES);

					std::string psContent = "", vsContent = "",
								vsEntry = shader->VSEntry,
								psEntry = shader->PSEntry;
					int lineBias = 0;
					ShaderLanguage psLang = ShaderCompiler::GetShaderLanguageFromExtension(shader->PSPath);
					ShaderLanguage vsLang = ShaderCompiler::GetShaderLanguageFromExtension(shader->VSPath);
					ShaderLanguage gsLang = ShaderCompiler::GetShaderLanguageFromExtension(shader->GSPath);
					ShaderLanguage tcsLang = ShaderCompiler::GetShaderLanguageFromExtension(shader->TCSPath);
					ShaderLanguage tesLang = ShaderCompiler::GetShaderLanguageFromExtension(shader->TESPath);

					// pixel shader
					bool psCompiled = false;

					if (psLang == ShaderLanguage::Plugin)
						psCompiled = m_pluginCompileToSpirv(item, shader->PSSPV, shader->PSPath, psEntry, plugin::ShaderStage::Pixel, shader->Macros.data(), shader->Macros.size());
					else
						psCompiled = ShaderCompiler::CompileToSPIRV(shader->PSSPV, psLang, shader->PSPath, ShaderStage::Pixel, psEntry, shader->Macros, m_msgs, m_project);

					if (psLang == ShaderLanguage::GLSL) { // GLSL
						psContent = m_project->LoadProjectFile(shader->PSPath);
						m_includeCheck(psContent, std::vector<std::string>(), lineBias);
						m_applyMacros(psContent, shader);
					} else { // HLSL / VK
						psContent = ShaderCompiler::ConvertToGLSL(shader->PSSPV, psLang, ShaderStage::Pixel, shader->TSUsed, shader->GSUsed, m_msgs);
						psEntry = "main";

						if (psLang == ShaderLanguage::Plugin)
							psContent = m_pluginProcessGLSL(shader->PSPath, psContent.c_str());		
					}

					shader->Variables.UpdateTextureList(psContent);
					GLuint ps = gl::CompileShader(GL_FRAGMENT_SHADER, psContent.c_str());
					psCompiled &= gl::CheckShaderCompilationStatus(ps, shaderMessage);

					// vertex shader
					lineBias = 0;
					bool vsCompiled = false;

					if (vsLang == ShaderLanguage::Plugin)
						vsCompiled = m_pluginCompileToSpirv(item, shader->VSSPV, shader->VSPath, vsEntry, plugin::ShaderStage::Vertex, shader->Macros.data(), shader->Macros.size());
					else
						vsCompiled = ShaderCompiler::CompileToSPIRV(shader->VSSPV, vsLang, shader->VSPath, ShaderStage::Vertex, vsEntry, shader->Macros, m_msgs, m_project);
					
					// generate glsl
					if (vsLang == ShaderLanguage::GLSL) { // GLSL
						vsContent = m_project->LoadProjectFile(shader->VSPath);
						m_includeCheck(vsContent, std::vector<std::string>(), lineBias);
						m_applyMacros(vsContent, shader);
					} else { // HLSL / VK
						vsContent = ShaderCompiler::ConvertToGLSL(shader->VSSPV, vsLang, ShaderStage::Vertex, shader->TSUsed, shader->GSUsed, m_msgs);
						vsEntry = "main";

						if (vsLang == ShaderLanguage::Plugin)
							vsContent = m_pluginProcessGLSL(shader->VSPath, vsContent.c_str());
					}

					GLuint vs = gl::CompileShader(GL_VERTEX_SHADER, vsContent.c_str());
					vsCompiled &= gl::CheckShaderCompilationStatus(vs, shaderMessage);

					// geometry shader
					bool gsCompiled = true;
					GLuint gs = 0;
					if (shader->GSUsed && strlen(shader->GSPath) > 0 && strlen(shader->GSEntry) > 0) {
						std::string gsContent = "",
									gsEntry = shader->GSEntry;

						lineBias = 0;
						
						if (gsLang == ShaderLanguage::Plugin)
							gsCompiled = m_pluginCompileToSpirv(item, shader->GSSPV, shader->GSPath, gsEntry, plugin::ShaderStage::Geometry, shader->Macros.data(), shader->Macros.size());
						else
							gsCompiled = ShaderCompiler::CompileToSPIRV(shader->GSSPV, gsLang, shader->GSPath, ShaderStage::Geometry, gsEntry, shader->Macros, m_msgs, m_project);
						
						if (gsLang == ShaderLanguage::GLSL) { // GLSL
							gsContent = m_project->LoadProjectFile(shader->GSPath);
							m_includeCheck(gsContent, std::vector<std::string>(), lineBias);
							m_applyMacros(gsContent, shader);
						} else { // HLSL / VK
							gsContent = ShaderCompiler::ConvertToGLSL(shader->GSSPV, gsLang, ShaderStage::Geometry, shader->TSUsed, shader->GSUsed, m_msgs);
							gsEntry = "main";

							if (gsLang == ShaderLanguage::Plugin)
								gsContent = m_pluginProcessGLSL(shader->GSPath, gsContent.c_str());
						}

						gs = gl::CompileShader(GL_GEOMETRY_SHADER, gsContent.c_str());
						gsCompiled &= gl::CheckShaderCompilationStatus(gs, shaderMessage);

						if (gsContent.empty())
							gsCompiled = false;
					}

					// tessellation shader
					bool tsCompiled = ((shader->TSUsed && m_tessellationSupported) || !shader->TSUsed);
					GLuint tcs = 0, tes = 0;
					if (shader->TSUsed && m_tessellationSupported) {
						// tess control
						if (strlen(shader->TCSPath) > 0 && strlen(shader->TCSEntry) > 0) {
							std::string tcsContent = "",
										tcsEntry = shader->TCSEntry;

							lineBias = 0;

							if (gsLang == ShaderLanguage::Plugin)
								tsCompiled &= m_pluginCompileToSpirv(item, shader->TCSSPV, shader->TCSPath, tcsEntry, plugin::ShaderStage::TessellationControl, shader->Macros.data(), shader->Macros.size());
							else
								tsCompiled &= ShaderCompiler::CompileToSPIRV(shader->TCSSPV, tcsLang, shader->TCSPath, ShaderStage::TessellationControl, tcsEntry, shader->Macros, m_msgs, m_project);

							if (tcsLang == ShaderLanguage::GLSL) { // GLSL
								tcsContent = m_project->LoadProjectFile(shader->TCSPath);
								m_includeCheck(tcsContent, std::vector<std::string>(), lineBias);
								m_applyMacros(tcsContent, shader);
							} else { // HLSL / VK
								tcsContent = ShaderCompiler::ConvertToGLSL(shader->TCSSPV, gsLang, ShaderStage::TessellationControl, shader->TSUsed, shader->GSUsed, m_msgs);
								tcsEntry = "main";

								if (tcsLang == ShaderLanguage::Plugin)
									tcsContent = m_pluginProcessGLSL(shader->TCSPath, tcsContent.c_str());
							}

							tcs = gl::CompileShader(GL_TESS_CONTROL_SHADER, tcsContent.c_str());
							tsCompiled &= gl::CheckShaderCompilationStatus(tcs, shaderMessage);

							if (tcsContent.empty())
								tsCompiled = false;
						}
						
						// tess evaluation
						if (strlen(shader->TESPath) > 0 && strlen(shader->TESEntry) > 0) {
							std::string tesContent = "",
										tesEntry = shader->TESEntry;

							lineBias = 0;

							if (gsLang == ShaderLanguage::Plugin)
								tsCompiled &= m_pluginCompileToSpirv(item, shader->TESSPV, shader->TESPath, tesEntry, plugin::ShaderStage::TessellationEvaluation, shader->Macros.data(), shader->Macros.size());
							else
								tsCompiled &= ShaderCompiler::CompileToSPIRV(shader->TESSPV, tcsLang, shader->TESPath, ShaderStage::TessellationEvaluation, tesEntry, shader->Macros, m_msgs, m_project);

							if (tesLang == ShaderLanguage::GLSL) { // GLSL
								tesContent = m_project->LoadProjectFile(shader->TESPath);
								m_includeCheck(tesContent, std::vector<std::string>(), lineBias);
								m_applyMacros(tesContent, shader);
							} else { // HLSL / VK
								tesContent = ShaderCompiler::ConvertToGLSL(shader->TESSPV, gsLang, ShaderStage::TessellationEvaluation, shader->TSUsed, shader->GSUsed, m_msgs);
								tesEntry = "main";

								if (tesLang == ShaderLanguage::Plugin)
									tesContent = m_pluginProcessGLSL(shader->TESPath, tesContent.c_str());
							}

							tes = gl::CompileShader(GL_TESS_EVALUATION_SHADER, tesContent.c_str());
							tsCompiled &= gl::CheckShaderCompilationStatus(tes, shaderMessage);

							if (tesContent.empty())
								tsCompiled = false;
						}
					}


					if (m_shaders[i] != 0)
						glDeleteProgram(m_shaders[i]);

					if (!vsCompiled || !psCompiled || !gsCompiled || !tsCompiled || vsContent.empty() || psContent.empty()) {
						Logger::Get().Log("Shaders not compiled", true);
						if (vsContent.empty() || psContent.empty())
							m_msgs->Add(MessageStack::Type::Error, name, "Shader source empty - try recompiling");
						else {
							if (shaderMessage[0] != 0 && shaderMessagesBefore == m_msgs->GetGroupErrorAndWarningMsgCount(name))
								m_msgs->Add(MessageStack::Type::Error, name, shaderMessage);
							m_msgs->Add(MessageStack::Type::Error, name, "Failed to compile the shader(s)");
						}

						m_shaders[i] = 0;
					} else {
						m_msgs->Add(MessageStack::Type::Message, name, "Compiled the shaders.");

						m_shaders[i] = glCreateProgram();
						glAttachShader(m_shaders[i], vs);
						if (shader->TSUsed) glAttachShader(m_shaders[i], tcs);
						if (shader->TSUsed) glAttachShader(m_shaders[i], tes);
						if (shader->GSUsed) glAttachShader(m_shaders[i], gs);
						glAttachShader(m_shaders[i], ps);
						glLinkProgram(m_shaders[i]);
					}

					if (m_shaders[i] != 0)
						shader->Variables.UpdateUniformInfo(m_shaders[i]);

					m_shaderSources[i].VS = vs;
					m_shaderSources[i].PS = ps;
					m_shaderSources[i].GS = gs;
					m_shaderSources[i].TCS = tcs;
					m_shaderSources[i].TES = tes;
				} 
				else if (item->Type == PipelineItem::ItemType::ComputePass && m_computeSupported) {
					pipe::ComputePass* shader = (pipe::ComputePass*)item->Data;
					int shaderMessagesBefore = m_msgs->GetGroupErrorAndWarningMsgCount(name);

					SPIRVQueue.push_back(item);

					m_msgs->ClearGroup(name);

					std::string content = "", entry = shader->Entry;
					int lineBias = 0;
					ShaderLanguage lang = ShaderCompiler::GetShaderLanguageFromExtension(shader->Path);

					// compute shader
					bool compiled = false;
						
					if (lang == ShaderLanguage::Plugin)
						compiled = m_pluginCompileToSpirv(item, shader->SPV, shader->Path, entry, plugin::ShaderStage::Compute, shader->Macros.data(), shader->Macros.size());
					else
						compiled = ShaderCompiler::CompileToSPIRV(shader->SPV, lang, shader->Path, ShaderStage::Compute, entry, shader->Macros, m_msgs, m_project);
					
					if (lang == ShaderLanguage::GLSL) { // GLSL
						content = m_project->LoadProjectFile(shader->Path);
						m_includeCheck(content, std::vector<std::string>(), lineBias);
						m_applyMacros(content, shader);
					} else { // HLSL / VK
						content = ShaderCompiler::ConvertToGLSL(shader->SPV, lang, ShaderStage::Compute, false, false, m_msgs);
						entry = "main";

						if (lang == ShaderLanguage::Plugin)
							content = m_pluginProcessGLSL(shader->Path, content.c_str());		
					}

					// compute shader supported == version 4.3 == not needed: shader->Variables.UpdateTextureList(content);
					GLuint cs = gl::CompileShader(GL_COMPUTE_SHADER, content.c_str());
					compiled &= gl::CheckShaderCompilationStatus(cs, shaderMessage);

					if (m_shaders[i] != 0)
						glDeleteProgram(m_shaders[i]);

					if (!compiled || content.empty()) {
						Logger::Get().Log("Compute shader was not compiled", true);
						if (content.empty())
							m_msgs->Add(MessageStack::Type::Error, name, "Shader source empty - try recompiling");
						else {
							if (shaderMessage[0] != 0 && shaderMessagesBefore == m_msgs->GetGroupErrorAndWarningMsgCount(name))
								m_msgs->Add(MessageStack::Type::Error, name, shaderMessage);
							m_msgs->Add(MessageStack::Type::Error, name, "Failed to compile the compute shader");
						}

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
					pipe::AudioPass* shader = (pipe::AudioPass*)item->Data;

					m_msgs->ClearGroup(name);

					std::string content = m_project->LoadProjectFile(shader->Path);

					// audio shader
					if (ShaderCompiler::GetShaderLanguageFromExtension(shader->Path) == ShaderLanguage::GLSL)
						m_applyMacros(content, shader);

					shader->Stream.CompileFromShaderSource(m_project, m_msgs, content, shader->Macros, ShaderCompiler::GetShaderLanguageFromExtension(shader->Path) == ShaderLanguage::HLSL);
					shader->Variables.UpdateUniformInfo(shader->Stream.GetShader());
				} 
				else if (item->Type == PipelineItem::ItemType::PluginItem) {
					pipe::PluginItemData* idata = (pipe::PluginItemData*)item->Data;
					idata->Owner->HandleRecompile(name);
				}
			}
		}

		Render();
	}
	void RenderEngine::RecompileFile(const char* fname)
	{
		for (int i = 0; i < m_items.size(); i++) {
			PipelineItem* item = m_items[i];
			if (item->Type == PipelineItem::ItemType::ShaderPass) {
				pipe::ShaderPass* shader = (pipe::ShaderPass*)item->Data;
				if (strcmp(shader->VSPath, fname) == 0 || strcmp(shader->TESPath, fname) == 0 || strcmp(shader->TCSPath, fname) == 0 || strcmp(shader->PSPath, fname) == 0 || strcmp(shader->GSPath, fname) == 0) {
					Recompile(item->Name);
				}
			} else if (item->Type == PipelineItem::ItemType::ComputePass && m_computeSupported) {
				pipe::ComputePass* shader = (pipe::ComputePass*)item->Data;
				if (strcmp(shader->Path, fname) == 0)
					Recompile(item->Name);
			} else if (item->Type == PipelineItem::ItemType::AudioPass) {
				pipe::AudioPass* shader = (pipe::AudioPass*)item->Data;
				if (strcmp(shader->Path, fname) == 0)
					Recompile(item->Name);
			}
		}
	}
	void RenderEngine::RecompileFromSource(const char* name, const std::string& vssrc, const std::string& pssrc, const std::string& gssrc, const std::string& tcssrc, const std::string& tessrc)
	{
		m_msgs->BuildOccured = true;
		m_msgs->CurrentItem = name;

		m_plugins->HandleApplicationEvent(plugin::ApplicationEvent::PipelineItemCompiled, (void*)name, nullptr);

		GLchar shaderMessage[1024] = { 0 };
		for (int i = 0; i < m_items.size(); i++) {
			PipelineItem* item = m_items[i];
			if (strcmp(item->Name, name) == 0) {
				if (item->Type == PipelineItem::ItemType::ShaderPass) {
					pipe::ShaderPass* shader = (pipe::ShaderPass*)item->Data;
					int shaderMessagesBefore = m_msgs->GetGroupErrorAndWarningMsgCount(name);

					m_msgs->ClearGroup(name);

					SPIRVQueue.push_back(item);

					bool vsCompiled = true, psCompiled = true, gsCompiled = true;
					bool tsCompiled = ((shader->TSUsed && m_tessellationSupported) || !shader->TSUsed);
					int lineBias = 0;

					// pixel shader
					if (pssrc.size() > 0) {
						ShaderLanguage psLang = ShaderCompiler::GetShaderLanguageFromExtension(shader->PSPath);
						if (psLang == ShaderLanguage::Plugin)
							psCompiled = m_pluginCompileToSpirv(item, shader->PSSPV, shader->PSPath, shader->PSEntry, plugin::ShaderStage::Pixel, shader->Macros.data(), shader->Macros.size(), pssrc);
						else
							psCompiled = ShaderCompiler::CompileSourceToSPIRV(shader->PSSPV, psLang, shader->PSPath, pssrc, ShaderStage::Pixel, shader->PSEntry, shader->Macros, m_msgs, m_project);

						std::string psContent = pssrc;
						if (psLang == ShaderLanguage::GLSL) { // GLSL
							m_includeCheck(psContent, std::vector<std::string>(), lineBias);
							m_applyMacros(psContent, shader);
						} else { // HLSL / VK
							psContent = ShaderCompiler::ConvertToGLSL(shader->PSSPV, psLang, ShaderStage::Pixel, shader->TSUsed, shader->GSUsed, m_msgs);
							
							if (psLang == ShaderLanguage::Plugin)
								psContent = m_pluginProcessGLSL(shader->PSPath, psContent.c_str());
						}



						shader->Variables.UpdateTextureList(psContent);
						GLuint ps = gl::CompileShader(GL_FRAGMENT_SHADER, psContent.c_str());
						psCompiled &= gl::CheckShaderCompilationStatus(ps, shaderMessage);

						glDeleteShader(m_shaderSources[i].PS);
						m_shaderSources[i].PS = ps;
					}

					// vertex shader
					if (vssrc.size() > 0) {
						lineBias = 0;
						
						ShaderLanguage vsLang = ShaderCompiler::GetShaderLanguageFromExtension(shader->VSPath);
						if (vsLang == ShaderLanguage::Plugin)
							vsCompiled = m_pluginCompileToSpirv(item, shader->VSSPV, shader->VSPath, shader->VSEntry, plugin::ShaderStage::Vertex, shader->Macros.data(), shader->Macros.size(), vssrc);
						else
							vsCompiled = ShaderCompiler::CompileSourceToSPIRV(shader->VSSPV, vsLang, shader->VSPath, vssrc, ShaderStage::Vertex, shader->VSEntry, shader->Macros, m_msgs, m_project);

						std::string vsContent = vssrc;
						if (vsLang == ShaderLanguage::GLSL) { // GLSL
							m_includeCheck(vsContent, std::vector<std::string>(), lineBias);
							m_applyMacros(vsContent, shader);
						} else { // HLSL / VK
							vsContent = ShaderCompiler::ConvertToGLSL(shader->VSSPV, vsLang, ShaderStage::Vertex, shader->TSUsed, shader->GSUsed, m_msgs);

							if (vsLang == ShaderLanguage::Plugin)
								vsContent = m_pluginProcessGLSL(shader->VSPath, vsContent.c_str());
						}


						GLuint vs = gl::CompileShader(GL_VERTEX_SHADER, vsContent.c_str());
						vsCompiled &= gl::CheckShaderCompilationStatus(vs, shaderMessage);

						glDeleteShader(m_shaderSources[i].VS);
						m_shaderSources[i].VS = vs;
					}

					// geometry shader
					if (gssrc.size() > 0) {
						lineBias = 0;

						ShaderLanguage gsLang = ShaderCompiler::GetShaderLanguageFromExtension(shader->GSPath);
						if (gsLang == ShaderLanguage::Plugin)
							gsCompiled = m_pluginCompileToSpirv(item, shader->GSSPV, shader->GSPath, shader->GSEntry, plugin::ShaderStage::Geometry, shader->Macros.data(), shader->Macros.size(), gssrc);
						else
							gsCompiled = ShaderCompiler::CompileSourceToSPIRV(shader->GSSPV, gsLang, shader->GSPath, gssrc, ShaderStage::Geometry, shader->GSEntry, shader->Macros, m_msgs, m_project);

						std::string gsContent = gssrc;
						if (gsLang == ShaderLanguage::GLSL) { // GLSL
							m_includeCheck(gsContent, std::vector<std::string>(), lineBias);
							m_applyMacros(gsContent, shader);
						} else { // HLSL / VK
							gsContent = ShaderCompiler::ConvertToGLSL(shader->GSSPV, gsLang, ShaderStage::Geometry, shader->TSUsed, shader->GSUsed, m_msgs);

							if (gsLang == ShaderLanguage::Plugin)
								gsContent = m_pluginProcessGLSL(shader->GSPath, gsContent.c_str());
						}


						GLuint gs = 0;
						glDeleteShader(m_shaderSources[i].GS);
						if (shader->GSUsed && strlen(shader->GSPath) > 0 && strlen(shader->GSEntry) > 0) {
							gs = gl::CompileShader(GL_GEOMETRY_SHADER, gsContent.c_str());
							gsCompiled &= gl::CheckShaderCompilationStatus(gs, shaderMessage);

							m_shaderSources[i].GS = gs;
						}
					}

					// tessellation control shader
					if (tcssrc.size() > 0 && m_tessellationSupported) {
						lineBias = 0;

						ShaderLanguage tcsLang = ShaderCompiler::GetShaderLanguageFromExtension(shader->TCSPath);
						if (tcsLang == ShaderLanguage::Plugin)
							tsCompiled &= m_pluginCompileToSpirv(item, shader->TCSSPV, shader->TCSPath, shader->TCSEntry, plugin::ShaderStage::TessellationControl, shader->Macros.data(), shader->Macros.size(), tcssrc);
						else
							tsCompiled &= ShaderCompiler::CompileSourceToSPIRV(shader->TCSSPV, tcsLang, shader->TCSPath, tcssrc, ShaderStage::TessellationControl, shader->TCSEntry, shader->Macros, m_msgs, m_project);

						std::string tcsContent = tcssrc;
						if (tcsLang == ShaderLanguage::GLSL) { // GLSL
							m_includeCheck(tcsContent, std::vector<std::string>(), lineBias);
							m_applyMacros(tcsContent, shader);
						} else { // HLSL / VK
							tcsContent = ShaderCompiler::ConvertToGLSL(shader->TCSSPV, tcsLang, ShaderStage::TessellationControl, shader->TSUsed, shader->GSUsed, m_msgs);

							if (tcsLang == ShaderLanguage::Plugin)
								tcsContent = m_pluginProcessGLSL(shader->TCSPath, tcsContent.c_str());
						}

						GLuint tcs = 0;
						glDeleteShader(m_shaderSources[i].TCS);
						if (shader->TSUsed && strlen(shader->TCSPath) > 0 && strlen(shader->TCSEntry) > 0) {
							tcs = gl::CompileShader(GL_TESS_CONTROL_SHADER, tcsContent.c_str());
							tsCompiled &= gl::CheckShaderCompilationStatus(tcs, shaderMessage);

							m_shaderSources[i].TCS = tcs;
						}
					}

					// tessellation control shader
					if (tessrc.size() > 0 && m_tessellationSupported) {
						lineBias = 0;

						ShaderLanguage tesLang = ShaderCompiler::GetShaderLanguageFromExtension(shader->TESPath);
						if (tesLang == ShaderLanguage::Plugin)
							tsCompiled &= m_pluginCompileToSpirv(item, shader->TESSPV, shader->TESPath, shader->TESEntry, plugin::ShaderStage::TessellationEvaluation, shader->Macros.data(), shader->Macros.size(), tessrc);
						else
							tsCompiled &= ShaderCompiler::CompileSourceToSPIRV(shader->TESSPV, tesLang, shader->TESPath, tessrc, ShaderStage::TessellationEvaluation, shader->TESEntry, shader->Macros, m_msgs, m_project);

						std::string tesContent = tessrc;
						if (tesLang == ShaderLanguage::GLSL) { // GLSL
							m_includeCheck(tesContent, std::vector<std::string>(), lineBias);
							m_applyMacros(tesContent, shader);
						} else { // HLSL / VK
							tesContent = ShaderCompiler::ConvertToGLSL(shader->TESSPV, tesLang, ShaderStage::TessellationEvaluation, shader->TSUsed, shader->GSUsed, m_msgs);

							if (tesLang == ShaderLanguage::Plugin)
								tesContent = m_pluginProcessGLSL(shader->TESPath, tesContent.c_str());
						}

						GLuint tes = 0;
						glDeleteShader(m_shaderSources[i].TES);
						if (shader->TSUsed && strlen(shader->TESPath) > 0 && strlen(shader->TESEntry) > 0) {
							tes = gl::CompileShader(GL_TESS_EVALUATION_SHADER, tesContent.c_str());
							tsCompiled &= gl::CheckShaderCompilationStatus(tes, shaderMessage);

							m_shaderSources[i].TES = tes;
						}
					}

					if (m_shaders[i] != 0)
						glDeleteProgram(m_shaders[i]);

					if (!vsCompiled || !psCompiled || !gsCompiled || !tsCompiled) {
						if (shaderMessage[0] != 0 && shaderMessagesBefore == m_msgs->GetGroupErrorAndWarningMsgCount(name))
							m_msgs->Add(MessageStack::Type::Error, name, shaderMessage);
						m_msgs->Add(MessageStack::Type::Error, name, "Failed to compile the shader(s)");
						m_shaders[i] = 0;
					} else {
						m_msgs->Add(MessageStack::Type::Message, name, "Compiled the shaders.");

						m_shaders[i] = glCreateProgram();
						glAttachShader(m_shaders[i], m_shaderSources[i].VS);
						glAttachShader(m_shaders[i], m_shaderSources[i].PS);
						if (shader->GSUsed) glAttachShader(m_shaders[i], m_shaderSources[i].GS);
						if (shader->TSUsed) glAttachShader(m_shaders[i], m_shaderSources[i].TCS);
						if (shader->TSUsed) glAttachShader(m_shaders[i], m_shaderSources[i].TES);
						glLinkProgram(m_shaders[i]);
					}

					if (m_shaders[i] != 0)
						shader->Variables.UpdateUniformInfo(m_shaders[i]);
				} 
				else if (item->Type == PipelineItem::ItemType::ComputePass && m_computeSupported) {
					pipe::ComputePass* shader = (pipe::ComputePass*)item->Data;
					int shaderMessagesBefore = m_msgs->GetGroupErrorAndWarningMsgCount(name);

					m_msgs->ClearGroup(name);

					SPIRVQueue.push_back(item);

					bool compiled = false;
					GLuint cs = 0;

					// compute shader
					if (vssrc.size() > 0) {
						int lineBias = 0;

						ShaderLanguage lang = ShaderCompiler::GetShaderLanguageFromExtension(shader->Path);
						if (lang == ShaderLanguage::Plugin)
							compiled = m_pluginCompileToSpirv(item, shader->SPV, shader->Path, shader->Entry, plugin::ShaderStage::Compute, shader->Macros.data(), shader->Macros.size(), vssrc);
						else
							compiled = ShaderCompiler::CompileSourceToSPIRV(shader->SPV, lang, shader->Path, vssrc, ShaderStage::Compute, shader->Entry, shader->Macros, m_msgs, m_project);

						std::string content = vssrc;
						if (lang == ShaderLanguage::GLSL) { // GLSL
							m_includeCheck(content, std::vector<std::string>(), lineBias);
							m_applyMacros(content, shader);
						} else { // HLSL / VK
							content = ShaderCompiler::ConvertToGLSL(shader->SPV, lang, ShaderStage::Compute, false, false, m_msgs);

							if (lang == ShaderLanguage::Plugin)
								content = m_pluginProcessGLSL(shader->Path, content.c_str());
						}


						cs = gl::CompileShader(GL_COMPUTE_SHADER, content.c_str());
						compiled &= gl::CheckShaderCompilationStatus(cs);
					}

					if (m_shaders[i] != 0)
						glDeleteProgram(m_shaders[i]);

					if (m_shaders[i] != 0 && shaderMessagesBefore == m_msgs->GetGroupErrorAndWarningMsgCount(name))
						shader->Variables.UpdateUniformInfo(m_shaders[i]);

					if (!compiled) {
						m_msgs->Add(MessageStack::Type::Error, name, "Failed to compile the compute shader");
						m_shaders[i] = 0;
					} else {
						m_msgs->Add(MessageStack::Type::Message, name, "Compiled the compute shader.");

						m_shaders[i] = glCreateProgram();
						glAttachShader(m_shaders[i], cs);
						glLinkProgram(m_shaders[i]);
					}

					glDeleteShader(cs);
				} 
				else if (item->Type == PipelineItem::ItemType::AudioPass) {
					pipe::AudioPass* shader = (pipe::AudioPass*)item->Data;
					m_msgs->ClearGroup(name);

					bool compiled = false;
					GLuint ss = 0;

					// audio shader
					if (vssrc.size() > 0)
						shader->Stream.CompileFromShaderSource(m_project, m_msgs, vssrc, shader->Macros, true);
					shader->Variables.UpdateUniformInfo(shader->Stream.GetShader());
				}
			}
		}

		Render();
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
	void RenderEngine::Pick(PipelineItem* item, bool add)
	{
		// check if it already exists
		bool skipAdd = false;
		for (int i = 0; i < m_pick.size(); i++)
			if (m_pick[i] == item) {
				if (!add) {
					m_pick.clear();
					m_pick.push_back(item);
				}
				skipAdd = true;
				break;
			}

		if (!skipAdd) {
			if (item == nullptr)
				m_pick.clear();
			else if (add)
				m_pick.push_back(item);
			else {
				m_pick.clear();
				m_pick.push_back(item);
			}
		}
	}
	void RenderEngine::m_pickItem(PipelineItem* item, bool multiPick)
	{
		glm::mat4 world(1);
		if (item->Type == PipelineItem::ItemType::Geometry) {
			pipe::GeometryItem* geo = (pipe::GeometryItem*)item->Data;
			if (geo->Type == pipe::GeometryItem::GeometryType::Rectangle || geo->Type == pipe::GeometryItem::GeometryType::ScreenQuadNDC)
				return;

			world = glm::translate(glm::mat4(1), geo->Position) * glm::yawPitchRoll(geo->Rotation.y, geo->Rotation.x, geo->Rotation.z);
		} else if (item->Type == PipelineItem::ItemType::Model) {
			pipe::Model* obj = (pipe::Model*)item->Data;

			world = glm::translate(glm::mat4(1), obj->Position) * glm::scale(glm::mat4(1.0f), obj->Scale) * glm::yawPitchRoll(obj->Rotation.y, obj->Rotation.x, obj->Rotation.z);
		} else if (item->Type == PipelineItem::ItemType::PluginItem) {
			pipe::PluginItemData* pldata = (pipe::PluginItemData*)item->Data;

			float plMat[16];
			pldata->Owner->PipelineItem_GetWorldMatrix(pldata->Type, pldata->PluginData, plMat);
			world = glm::make_mat4(plMat);
		} else if (item->Type == PipelineItem::ItemType::VertexBuffer) {
			pipe::VertexBuffer* obj = (pipe::VertexBuffer*)item->Data;

			world = glm::translate(glm::mat4(1), obj->Position) * glm::scale(glm::mat4(1.0f), obj->Scale) * glm::yawPitchRoll(obj->Rotation.y, obj->Rotation.x, obj->Rotation.z);
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
				if (ray::IntersectBox(vec3Origin, vec3Dir, b1, b2, distHit))
					myDist = distHit;
			} else if (geo->Type == pipe::GeometryItem::GeometryType::Triangle) {
				float size = geo->Size.x * geo->Scale.x;
				float rightOffs = size / tan(glm::radians(30.0f));
				glm::vec3 v0(0, -size, 0);
				glm::vec3 v1(-rightOffs, size, 0);
				glm::vec3 v2(rightOffs, size, 0);

				float hit;
				if (ray::IntersectTriangle(vec3Origin, glm::normalize(vec3Dir), v0, v1, v2, hit))
					myDist = hit;
			} else if (geo->Type == pipe::GeometryItem::GeometryType::Sphere) {
				float r = geo->Size.x * geo->Scale.x;
				r *= r;

				glm::intersectRaySphere(vec3Origin, glm::normalize(vec3Dir), glm::vec3(0), r, myDist);
			} else if (geo->Type == pipe::GeometryItem::GeometryType::Plane) {
				glm::vec3 b1(-geo->Size.x * geo->Scale.x / 2, -geo->Size.y * geo->Scale.y / 2, -0.0001f);
				glm::vec3 b2(geo->Size.x * geo->Scale.x / 2, geo->Size.y * geo->Scale.y / 2, 0.0001f);

				float hit;
				if (ray::IntersectBox(vec3Origin, vec3Dir, b1, b2, hit))
					myDist = hit;
			} else if (geo->Type == pipe::GeometryItem::GeometryType::Circle) {
				glm::vec3 b1(-geo->Size.x * geo->Scale.x, -geo->Size.y * geo->Scale.y, -0.0001f);
				glm::vec3 b2(geo->Size.x * geo->Scale.x, geo->Size.y * geo->Scale.y, 0.0001f);

				float hit;
				if (ray::IntersectBox(vec3Origin, vec3Dir, b1, b2, hit))
					myDist = hit;
			}
		} else if (item->Type == PipelineItem::ItemType::Model) {
			pipe::Model* obj = (pipe::Model*)item->Data;

			glm::vec3 minb = obj->Data->GetMinBound();
			glm::vec3 maxb = obj->Data->GetMaxBound();

			float triDist = std::numeric_limits<float>::infinity();
			if (ray::IntersectBox(vec3Origin, vec3Dir, minb, maxb, triDist)) {
				bool donetris = false;
				for (auto& mesh : obj->Data->Meshes) {
					for (int i = 0; i + 2 < mesh.Vertices.size(); i += 3) {
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
		} else if (item->Type == PipelineItem::ItemType::VertexBuffer) {
			pipe::VertexBuffer* obj = (pipe::VertexBuffer*)item->Data;

			glm::vec3 b1(0.0f);
			glm::vec3 b2(0.0f);
			gl::GetVertexBufferBounds(m_objects, obj, b1, b2);

			// TODO: use ray-triangle intersection for more precision

			float distHit;
			if (ray::IntersectBox(vec3Origin, vec3Dir, b1, b2, distHit))
				myDist = distHit;
		} else if (item->Type == PipelineItem::ItemType::PluginItem) {
			pipe::PluginItemData* obj = (pipe::PluginItemData*)item->Data;

			float hit;
			if (obj->Owner->PipelineItem_Intersect(obj->Type, obj->PluginData, glm::value_ptr(vec3Origin), glm::value_ptr(vec3Dir), hit))
				myDist = hit;
		}

		// did we actually pick sth that is closer?
		if (myDist < m_pickDist) {
			SystemVariableManager::Instance().SetPickPosition(m_pickOrigin + m_pickDir * myDist);

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
	std::pair<PipelineItem*, PipelineItem*> RenderEngine::GetPipelineItemByDebugID(int id)
	{
		int debugID = DEBUG_ID_START;
		for (int i = 0; i < m_items.size(); i++) {
			PipelineItem* it = m_items[i];

			if (it->Type == PipelineItem::ItemType::ShaderPass) {
				pipe::ShaderPass* data = (pipe::ShaderPass*)it->Data;

				if (!data->Active || data->Items.size() <= 0 || data->RTCount == 0 || m_shaders[i] == 0)
					continue;

				// render pipeline items
				for (int j = 0; j < data->Items.size(); j++) {
					PipelineItem* item = data->Items[j];

					// update the value for this element and check if we picked it
					if (item->Type == PipelineItem::ItemType::Geometry || item->Type == PipelineItem::ItemType::Model || item->Type == PipelineItem::ItemType::VertexBuffer || item->Type == PipelineItem::ItemType::PluginItem) {
						if (debugID == id)
							return std::make_pair(it, item);

						debugID++;
					}
				}
			} else if (it->Type == PipelineItem::ItemType::PluginItem) {
				pipe::PluginItemData* data = (pipe::PluginItemData*)it->Data;
				PipelineItem* item = nullptr;
				if (data->Owner->PipelineItem_IsDebuggable(data->Type, data->PluginData))
					for (int j = 0; j < data->Items.size(); j++) {
						if (debugID == id)
							return std::make_pair(it, data->Items[j]);
						debugID++;
					}
			}
		}

		return std::make_pair(nullptr, nullptr);
	}
	void RenderEngine::FlushCache()
	{
		for (int i = 0; i < m_shaders.size(); i++) {
			glDeleteShader(m_shaderSources[i].VS);
			glDeleteShader(m_shaderSources[i].PS);
			glDeleteShader(m_shaderSources[i].GS);
			glDeleteShader(m_shaderSources[i].TCS);
			glDeleteShader(m_shaderSources[i].TES);
			glDeleteProgram(m_shaders[i]);
			glDeleteQueries(1, &m_perfTimers[i].Object);
		}

		m_fbos.clear();
		m_fboCount.clear();
		m_items.clear();
		m_shaders.clear();
		m_perfTimers.clear();
		m_shaderSources.clear();
		m_uboMax.clear();
		m_fbosNeedUpdate = true;

		// clear textures
		glBindTexture(GL_TEXTURE_2D, m_rtColor);
		glTexImage2D(GL_TEXTURE_2D, 0, Settings::Instance().Project.UseAlphaChannel ? GL_RGBA32F : GL_RGB32F, m_lastSize.x, m_lastSize.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		glBindTexture(GL_TEXTURE_2D, m_rtDepth);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, m_lastSize.x, m_lastSize.y, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
		glBindTexture(GL_TEXTURE_2D, 0);

		m_lastSize = glm::ivec2(1, 1); // recreate window rt!
	}
	void RenderEngine::m_cache()
	{
		// check for any changes
		std::vector<ed::PipelineItem*>& items = m_pipeline->GetList();

		// check if no major changes were made, if so dont cache for another 0.25s
		if (m_items.size() == items.size()) {
			if (m_cacheTimer.GetElapsedTime() > 0.5f)
				m_cacheTimer.Restart();
			else
				return;
		}

		// check if some item was added
		GLchar shaderMessage[1024] = { 0 };
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
					int shaderMessagesBefore = m_msgs->GetGroupErrorAndWarningMsgCount(items[i]->Name);

					m_items.insert(m_items.begin() + i, items[i]);
					m_shaders.insert(m_shaders.begin() + i, 0);
					m_debugShaders.insert(m_debugShaders.begin() + i, 0);
					m_shaderSources.insert(m_shaderSources.begin() + i, ShaderPack());
					
					SPIRVQueue.push_back(items[i]);

					// cache performance timer
					m_perfTimers.insert(m_perfTimers.begin() + i, PerformanceTimer(items[i]));
					glGenQueries(1, &m_perfTimers[i].Object);
					m_perfTimers[i].IsCreated = true;

					if (strlen(data->VSPath) == 0 || strlen(data->PSPath) == 0) {
						Logger::Get().Log("No shader paths are set", true);
						continue;
					}

					glDeleteShader(m_shaderSources[i].VS);
					glDeleteShader(m_shaderSources[i].PS);
					glDeleteShader(m_shaderSources[i].GS);
					glDeleteShader(m_shaderSources[i].TCS);
					glDeleteShader(m_shaderSources[i].TES);

					/*
						ITEM CACHING
					*/

					m_fbos[data].resize(MAX_RENDER_TEXTURES);

					GLuint ps = 0, vs = 0, gs = 0, tcs = 0, tes = 0;

					m_msgs->CurrentItem = items[i]->Name;

					int lineBias = 0;
					std::string psContent = "", vsContent = "",
								vsEntry = data->VSEntry,
								psEntry = data->PSEntry;
					ShaderLanguage vsLang = ShaderCompiler::GetShaderLanguageFromExtension(data->VSPath);
					ShaderLanguage psLang = ShaderCompiler::GetShaderLanguageFromExtension(data->PSPath);

					// vertex shader
					bool vsCompiled = false;

					if (vsLang == ShaderLanguage::Plugin)
						vsCompiled = m_pluginCompileToSpirv(items[i], data->VSSPV, data->VSPath, vsEntry, plugin::ShaderStage::Vertex, data->Macros.data(), data->Macros.size());
					else
						vsCompiled = ShaderCompiler::CompileToSPIRV(data->VSSPV, vsLang, data->VSPath, ShaderStage::Vertex, vsEntry, data->Macros, m_msgs, m_project);
					
					// generate glsl
					if (vsLang == ShaderLanguage::GLSL) { // GLSL
						vsContent = m_project->LoadProjectFile(data->VSPath);
						m_includeCheck(vsContent, std::vector<std::string>(), lineBias);
						m_applyMacros(vsContent, data);
					} else if (vsCompiled) {
						vsContent = ShaderCompiler::ConvertToGLSL(data->VSSPV, vsLang, ShaderStage::Vertex, data->TSUsed, data->GSUsed, m_msgs);
						vsEntry = "main";

						if (vsLang == ShaderLanguage::Plugin)
							vsContent = std::string(m_pluginProcessGLSL(data->VSPath, vsContent.c_str()));
					}

					vs = gl::CompileShader(GL_VERTEX_SHADER, vsContent.c_str());
					vsCompiled &= gl::CheckShaderCompilationStatus(vs, shaderMessage);
					
					// pixel shader
					lineBias = 0;
					bool psCompiled = false;

					if (psLang == ShaderLanguage::Plugin)
						psCompiled = m_pluginCompileToSpirv(items[i], data->PSSPV, data->PSPath, psEntry, plugin::ShaderStage::Pixel, data->Macros.data(), data->Macros.size());
					else
						psCompiled = ShaderCompiler::CompileToSPIRV(data->PSSPV, psLang, data->PSPath, ShaderStage::Pixel, psEntry, data->Macros, m_msgs, m_project);
					
					if (psLang == ShaderLanguage::GLSL) { // GLSL
						psContent = m_project->LoadProjectFile(data->PSPath);
						m_includeCheck(psContent, std::vector<std::string>(), lineBias);
						m_applyMacros(psContent, data);
					} else if (psCompiled) { // HLSL / VK
						psContent = ShaderCompiler::ConvertToGLSL(data->PSSPV, psLang, ShaderStage::Pixel, data->TSUsed, data->GSUsed, m_msgs);
						psEntry = "main";

						if (psLang == ShaderLanguage::Plugin)
							psContent = m_pluginProcessGLSL(data->PSPath, psContent.c_str());
					}

					data->Variables.UpdateTextureList(psContent);
					ps = gl::CompileShader(GL_FRAGMENT_SHADER, psContent.c_str());
					psCompiled &= gl::CheckShaderCompilationStatus(ps, shaderMessage);

					// geometry shader
					lineBias = 0;
					bool gsCompiled = true;
					if (data->GSUsed && strlen(data->GSEntry) > 0 && strlen(data->GSPath) > 0) {
						std::string gsContent = "", gsEntry = data->GSEntry;
						ShaderLanguage gsLang = ShaderCompiler::GetShaderLanguageFromExtension(data->GSPath);
						
						if (gsLang == ShaderLanguage::Plugin)
							gsCompiled = m_pluginCompileToSpirv(items[i], data->GSSPV, data->GSPath, gsEntry, plugin::ShaderStage::Geometry, data->Macros.data(), data->Macros.size());
						else
							gsCompiled = ShaderCompiler::CompileToSPIRV(data->GSSPV, gsLang, data->GSPath, ShaderStage::Geometry, gsEntry, data->Macros, m_msgs, m_project);
						
						if (gsLang == ShaderLanguage::GLSL) { // GLSL
							gsContent = m_project->LoadProjectFile(data->GSPath);
							m_includeCheck(gsContent, std::vector<std::string>(), lineBias);
							m_applyMacros(gsContent, data);
						} else if (gsCompiled) { // HLSL
							gsContent = ShaderCompiler::ConvertToGLSL(data->GSSPV, gsLang, ShaderStage::Geometry, data->TSUsed, data->GSUsed, m_msgs);
							gsEntry = "main";

							if (gsLang == ShaderLanguage::Plugin)
								gsContent = m_pluginProcessGLSL(data->GSPath, gsContent.c_str());
						}

						gs = gl::CompileShader(GL_GEOMETRY_SHADER, gsContent.c_str());
						gsCompiled &= gl::CheckShaderCompilationStatus(gs, shaderMessage);
					}

					// tessellation shader
					lineBias = 0;
					bool tsCompiled = ((data->TSUsed && m_tessellationSupported) || !data->TSUsed);
					if (data->TSUsed && m_tessellationSupported) {
						// tessellation control shader
						if (strlen(data->TCSEntry) > 0 && strlen(data->TCSPath) > 0) {
							std::string tcsContent = "", tcsEntry = data->TCSEntry;
							ShaderLanguage tcsLang = ShaderCompiler::GetShaderLanguageFromExtension(data->TCSPath);

							if (tcsLang == ShaderLanguage::Plugin)
								tsCompiled &= m_pluginCompileToSpirv(items[i], data->TCSSPV, data->TCSPath, tcsEntry, plugin::ShaderStage::TessellationControl, data->Macros.data(), data->Macros.size());
							else
								tsCompiled &= ShaderCompiler::CompileToSPIRV(data->TCSSPV, tcsLang, data->TCSPath, ShaderStage::TessellationControl, tcsEntry, data->Macros, m_msgs, m_project);

							if (tcsLang == ShaderLanguage::GLSL) { // GLSL
								tcsContent = m_project->LoadProjectFile(data->TCSPath);
								m_includeCheck(tcsContent, std::vector<std::string>(), lineBias);
								m_applyMacros(tcsContent, data);
							} else if (gsCompiled) { // HLSL
								tcsContent = ShaderCompiler::ConvertToGLSL(data->TCSSPV, tcsLang, ShaderStage::TessellationControl, data->TSUsed, data->GSUsed, m_msgs);
								tcsEntry = "main";

								if (tcsLang == ShaderLanguage::Plugin)
									tcsContent = m_pluginProcessGLSL(data->TCSPath, tcsContent.c_str());
							}

							tcs = gl::CompileShader(GL_TESS_CONTROL_SHADER, tcsContent.c_str());
							tsCompiled &= gl::CheckShaderCompilationStatus(tcs, shaderMessage);
						}

						// tessellation evauluation shader
						if (strlen(data->TESEntry) > 0 && strlen(data->TESPath) > 0) {
							std::string tesContent = "", tesEntry = data->TESEntry;
							ShaderLanguage tesLang = ShaderCompiler::GetShaderLanguageFromExtension(data->TESPath);

							if (tesLang == ShaderLanguage::Plugin)
								tsCompiled &= m_pluginCompileToSpirv(items[i], data->TESSPV, data->TESPath, tesEntry, plugin::ShaderStage::TessellationEvaluation, data->Macros.data(), data->Macros.size());
							else
								tsCompiled &= ShaderCompiler::CompileToSPIRV(data->TESSPV, tesLang, data->TESPath, ShaderStage::TessellationEvaluation, tesEntry, data->Macros, m_msgs, m_project);

							if (tesLang == ShaderLanguage::GLSL) { // GLSL
								tesContent = m_project->LoadProjectFile(data->TESPath);
								m_includeCheck(tesContent, std::vector<std::string>(), lineBias);
								m_applyMacros(tesContent, data);
							} else if (gsCompiled) { // HLSL
								tesContent = ShaderCompiler::ConvertToGLSL(data->TESSPV, tesLang, ShaderStage::TessellationEvaluation, data->TSUsed, data->GSUsed, m_msgs);
								tesEntry = "main";

								if (tesLang == ShaderLanguage::Plugin)
									tesContent = m_pluginProcessGLSL(data->TESPath, tesContent.c_str());
							}

							tes = gl::CompileShader(GL_TESS_EVALUATION_SHADER, tesContent.c_str());
							tsCompiled &= gl::CheckShaderCompilationStatus(tes, shaderMessage);
						}
					}


					if (m_shaders[i] != 0)
						glDeleteProgram(m_shaders[i]);

					if (m_debugShaders[i] != 0)
						glDeleteProgram(m_debugShaders[i]);

					if (!vsCompiled || !psCompiled || !gsCompiled || !tsCompiled) {
						if (shaderMessage[0] != 0 && shaderMessagesBefore == m_msgs->GetGroupErrorAndWarningMsgCount(items[i]->Name))
							m_msgs->Add(MessageStack::Type::Error, items[i]->Name, shaderMessage);
						m_msgs->Add(MessageStack::Type::Error, items[i]->Name, "Failed to compile the shader");
						m_shaders[i] = 0;
					} else {
						m_msgs->ClearGroup(items[i]->Name);

						m_shaders[i] = glCreateProgram();
						glAttachShader(m_shaders[i], vs);
						glAttachShader(m_shaders[i], ps);
						if (data->GSUsed) glAttachShader(m_shaders[i], gs);
						if (data->TSUsed) glAttachShader(m_shaders[i], tcs);
						if (data->TSUsed) glAttachShader(m_shaders[i], tes);
						glLinkProgram(m_shaders[i]);
						// XXX TODO check link status

						m_debugShaders[i] = glCreateProgram();
						glAttachShader(m_debugShaders[i], m_generalDebugShader);
						glAttachShader(m_debugShaders[i], vs);
						if (data->GSUsed) glAttachShader(m_debugShaders[i], gs);
						if (data->TSUsed) glAttachShader(m_debugShaders[i], tcs);
						if (data->TSUsed) glAttachShader(m_debugShaders[i], tes);
						glLinkProgram(m_debugShaders[i]);
					}

					if (m_shaders[i] != 0)
						data->Variables.UpdateUniformInfo(m_shaders[i]);

					m_shaderSources[i].VS = vs;
					m_shaderSources[i].PS = ps;
					m_shaderSources[i].GS = gs;
					m_shaderSources[i].TCS = tcs;
					m_shaderSources[i].TES = tes;
				}
				else if (items[i]->Type == PipelineItem::ItemType::ComputePass && m_computeSupported) {
					pipe::ComputePass* data = reinterpret_cast<ed::pipe::ComputePass*>(items[i]->Data);
					int shaderMessagesBefore = m_msgs->GetGroupErrorAndWarningMsgCount(items[i]->Name);

					m_items.insert(m_items.begin() + i, items[i]);
					m_shaders.insert(m_shaders.begin() + i, 0);
					m_debugShaders.insert(m_debugShaders.begin() + i, 0);
					m_shaderSources.insert(m_shaderSources.begin() + i, ShaderPack());

					SPIRVQueue.push_back(items[i]);

					// cache performance timer
					m_perfTimers.insert(m_perfTimers.begin() + i, PerformanceTimer(items[i]));
					glGenQueries(1, &m_perfTimers[i].Object);
					m_perfTimers[i].IsCreated = true;

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
					ShaderLanguage lang = ShaderCompiler::GetShaderLanguageFromExtension(data->Path);

					// compute shader
					bool compiled = false;

					if (lang == ShaderLanguage::Plugin)
						compiled = m_pluginCompileToSpirv(items[i], data->SPV, data->Path, entry, plugin::ShaderStage::Compute, data->Macros.data(), data->Macros.size());
					else
						compiled = ShaderCompiler::CompileToSPIRV(data->SPV, lang, data->Path, ShaderStage::Compute, entry, data->Macros, m_msgs, m_project);
					
					if (lang == ShaderLanguage::GLSL) { // GLSL
						content = m_project->LoadProjectFile(data->Path);
						m_includeCheck(content, std::vector<std::string>(), lineBias);
						m_applyMacros(content, data);
					} else if (compiled) { // HLSL / VK
						content = ShaderCompiler::ConvertToGLSL(data->SPV, lang, ShaderStage::Compute, false, false, m_msgs);
						entry = "main";

						if (lang == ShaderLanguage::Plugin)
							content = m_pluginProcessGLSL(data->Path, content.c_str());
					}

					cs = gl::CompileShader(GL_COMPUTE_SHADER, content.c_str());
					compiled &= gl::CheckShaderCompilationStatus(cs, shaderMessage);

					if (m_shaders[i] != 0)
						glDeleteProgram(m_shaders[i]);

					if (!compiled) {
						if (shaderMessage[0] != 0 && shaderMessagesBefore == m_msgs->GetGroupErrorAndWarningMsgCount(items[i]->Name))
							m_msgs->Add(MessageStack::Type::Error, items[i]->Name, shaderMessage);
						m_msgs->Add(MessageStack::Type::Error, items[i]->Name, "Failed to compile the compute shader");
						m_shaders[i] = 0;
					} else {
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
					m_shaderSources[i].TCS = 0;
					m_shaderSources[i].TES = 0;
				} 
				else if (items[i]->Type == PipelineItem::ItemType::AudioPass) {
					pipe::AudioPass* data = reinterpret_cast<ed::pipe::AudioPass*>(items[i]->Data);

					m_items.insert(m_items.begin() + i, items[i]);
					m_shaders.insert(m_shaders.begin() + i, 0);
					m_debugShaders.insert(m_debugShaders.begin() + i, 0);
					m_shaderSources.insert(m_shaderSources.begin() + i, ShaderPack());

					// cache performance timer
					m_perfTimers.insert(m_perfTimers.begin() + i, PerformanceTimer(items[i]));
					glGenQueries(1, &m_perfTimers[i].Object);
					m_perfTimers[i].IsCreated = true;

					/*
						ITEM CACHING
					*/

					m_msgs->CurrentItem = items[i]->Name;
					std::string content = m_project->LoadProjectFile(data->Path);

					// vertex shader
					if (ShaderCompiler::GetShaderLanguageFromExtension(data->Path) == ShaderLanguage::GLSL)
						m_applyMacros(content, data);
					data->Stream.CompileFromShaderSource(m_project, m_msgs, content, data->Macros, ShaderCompiler::GetShaderLanguageFromExtension(data->Path) == ShaderLanguage::HLSL);

					data->Variables.UpdateUniformInfo(data->Stream.GetShader());
				} 
				else if (items[i]->Type == PipelineItem::ItemType::PluginItem) {
					pipe::PluginItemData* data = reinterpret_cast<pipe::PluginItemData*>(items[i]->Data);

					m_items.insert(m_items.begin() + i, items[i]);
					m_shaders.insert(m_shaders.begin() + i, 0);
					m_debugShaders.insert(m_debugShaders.begin() + i, 0);
					m_shaderSources.insert(m_shaderSources.begin() + i, ShaderPack());

					// cache performance timer
					m_perfTimers.insert(m_perfTimers.begin() + i, PerformanceTimer(items[i]));
					glGenQueries(1, &m_perfTimers[i].Object);
					m_perfTimers[i].IsCreated = true;
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
				glDeleteProgram(m_debugShaders[i]);
				glDeleteQueries(1, &m_perfTimers[i].Object);

				Logger::Get().Log("Removing an item from cache");

				if (m_items[i]->Type == PipelineItem::ItemType::ShaderPass)
					m_fbos.erase((pipe::ShaderPass*)m_items[i]->Data);

				m_items.erase(m_items.begin() + i);
				m_shaders.erase(m_shaders.begin() + i);
				m_debugShaders.erase(m_debugShaders.begin() + i);
				m_shaderSources.erase(m_shaderSources.begin() + i);
				m_perfTimers.erase(m_perfTimers.begin() + i);
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
						GLuint sdbgCopy = m_debugShaders[i];
						ShaderPack ssrcCopy = m_shaderSources[i];
						PerformanceTimer perfTimerCopy = m_perfTimers[i];

						m_shaders.erase(m_shaders.begin() + i);
						m_shaders.insert(m_shaders.begin() + dest, sCopy);

						m_debugShaders.erase(m_debugShaders.begin() + i);
						m_debugShaders.insert(m_debugShaders.begin() + dest, sdbgCopy);

						m_shaderSources.erase(m_shaderSources.begin() + i);
						m_shaderSources.insert(m_shaderSources.begin() + dest, ssrcCopy);

						m_perfTimers.erase(m_perfTimers.begin() + i);
						m_perfTimers.insert(m_perfTimers.begin() + dest, perfTimerCopy);
					}
				}
			}
		}
	}
	void RenderEngine::m_applyMacros(std::string& src, pipe::ShaderPass* pass)
	{
		size_t verLoc = src.find_first_of("#version");
		size_t lineLoc = src.find_first_of('\n', verLoc + 1) + 1;
		std::string strMacro = "";

#ifdef SHADERED_WEB
		strMacro += "#define SHADERED_WEB\n";
#else
		strMacro += "#define SHADERED_DESKTOP\n";
#endif
		strMacro += "#define SHADERED_VERSION " + std::to_string(SHADERED_VERSION) + "\n";

		for (auto& macro : pass->Macros) {
			if (!macro.Active)
				continue;

			strMacro += "#define " + std::string(macro.Name) + " " + std::string(macro.Value) + "\n";
		}

		if (strMacro.size() > 0)
			src.insert(lineLoc, strMacro);
	}
	void RenderEngine::m_applyMacros(std::string& src, pipe::ComputePass* pass)
	{
		size_t verLoc = src.find_first_of("#version");
		size_t lineLoc = src.find_first_of('\n', verLoc + 1) + 1;
		std::string strMacro = "";

#ifdef SHADERED_WEB
		strMacro += "#define SHADERED_WEB\n";
#else
		strMacro += "#define SHADERED_DESKTOP\n";
#endif
		strMacro += "#define SHADERED_VERSION " + std::to_string(SHADERED_VERSION) + "\n";

		for (auto& macro : pass->Macros) {
			if (!macro.Active)
				continue;

			strMacro += "#define " + std::string(macro.Name) + " " + std::string(macro.Value) + "\n";
		}

		if (strMacro.size() > 0)
			src.insert(lineLoc, strMacro);
	}
	void RenderEngine::m_applyMacros(std::string& src, pipe::AudioPass* pass)
	{
		size_t verLoc = src.find_first_of("#version");
		size_t lineLoc = src.find_first_of('\n', verLoc + 1) + 1;
		std::string strMacro = "";

#ifdef SHADERED_WEB
		strMacro += "#define SHADERED_WEB\n";
#else
		strMacro += "#define SHADERED_DESKTOP\n";
#endif
		strMacro += "#define SHADERED_VERSION " + std::to_string(SHADERED_VERSION) + "\n";

		for (auto& macro : pass->Macros) {
			if (!macro.Active)
				continue;

			strMacro += "#define " + std::string(macro.Name) + " " + std::string(macro.Value) + "\n";
		}

		if (strMacro.size() > 0)
			src.insert(lineLoc, strMacro);
	}
	
	
	const char* RenderEngine::m_pluginProcessGLSL(const char* path, const char* src)
	{
		Logger::Get().Log("Plugin is processing GLSL");

		bool ret = false;

		int plLang = 0;
		IPlugin1* plugin = ShaderCompiler::GetPluginLanguageFromExtension(&plLang, path, m_plugins->Plugins());

		return plugin->CustomLanguage_ProcessGeneratedGLSL(plLang, src);
	}
	bool RenderEngine::m_pluginCompileToSpirv(PipelineItem* owner, std::vector<GLuint>& spvvec, const std::string& path, const std::string& entry, plugin::ShaderStage stage, ed::ShaderMacro* macros, size_t macroCount, const std::string& actualSource)
	{
		Logger::Get().Log("Plugin is compiling the shader to SPIR-V");

		bool ret = false;

		int plLang = 0;
		IPlugin1* plugin = ShaderCompiler::GetPluginLanguageFromExtension(&plLang, path, m_plugins->Plugins());
		if (plugin == nullptr) {
			spvvec.clear();
			return false;
		}

		std::string source = actualSource;
		if (actualSource.empty())
			source = m_project->LoadProjectFile(path);

		size_t spv_length = 0;
		const unsigned int* spv = plugin->CustomLanguage_CompileToSPIRV(plLang, source.c_str(), source.size(), stage, entry.c_str(), (plugin::ShaderMacro*)macros, macroCount, &spv_length, &ret);

		if (spv == nullptr && ret && plugin->GetVersion() >= 3) {
			IPlugin3* plugin3 = (IPlugin3*)plugin;
			spv_length = 0;
			spv = plugin3->CustomLanguage_CompileToSPIRV2(owner, plLang, source.c_str(), source.size(), stage, entry.c_str(), (plugin::ShaderMacro*)macros, macroCount, &spv_length, &ret);
		}

		spvvec = std::vector<GLuint>(spv, spv + spv_length);
		
		return ret;
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
			std::string fileName = src.substr(quotePos + 1, quoteEnd - quotePos - 1);

			for (int i = 0; i < paths.size(); i++) {
				std::string ipath = paths[i];
				char last = ipath[ipath.size() - 1];
				if (last != '\\' && last != '/')
					ipath += "/";

				ipath += fileName;

				src.erase(incLoc, src.find_first_of('\n', incLoc) - incLoc);

				if (std::count(includeStack.begin(), includeStack.end(), ipath) > 0)
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
		ObjectManagerItem* lastData = m_objects->GetByTextureID(lastID);

		GLuint depthID = lastID == m_rtColor ? m_rtDepth : lastData->RT->DepthStencilBuffer;
		GLuint depthMSID = lastID == m_rtColor ? m_rtDepthMS : lastData->RT->DepthStencilBufferMS;

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

			if (texID == m_rtColor)
				texID = m_rtColorMS;
			else
				texID = m_objects->GetByTextureID(texID)->RT->BufferMS;

			// attach
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D_MULTISAMPLE, texID, 0);
		}
		retval = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		m_fbosNeedUpdate = false;
	}
}