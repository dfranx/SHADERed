#pragma once
#include "PluginData.h"
#include <stddef.h>

namespace ed {
	namespace pluginfn {
		/********** IPlugin1 **********/
		typedef void (*AddObjectFn)(void* objects, const char* name, const char* type, void* data, unsigned int id, void* owner);
		typedef bool (*AddCustomPipelineItemFn)(void* pipeline, void* parent, const char* name, const char* type, void* data, void* owner);

		typedef void (*AddMessageFn)(void* messages, ed::plugin::MessageType mtype, const char* group, const char* txt, int ln);

		typedef bool (*CreateRenderTextureFn)(void* objects, const char* name);
		typedef bool (*CreateImageFn)(void* objects, const char* name, int width, int height);
		typedef void (*ResizeRenderTextureFn)(void* objects, const char* name, int width, int height);
		typedef void (*ResizeImageFn)(void* objects, const char* name, int width, int height);
		typedef bool (*ExistsObjectFn)(void* objects, const char* name);
		typedef void (*RemoveObjectFn)(void* objects, const char* name);

		typedef void (*GetProjectPathFn)(void* project, const char* filename, char* out);
		typedef void (*GetRelativePathFn)(void* project, const char* filename, char* out);
		typedef void (*GetProjectFilenameFn)(void* project, char* out);
		typedef const char* (*GetProjectDirectoryFn)(void* project);
		typedef bool (*IsProjectModifiedFn)(void* project);
		typedef void (*ModifyProjectFn)(void* project);
		typedef void (*OpenProjectFn)(void* ui, const char* filename);
		typedef void (*SaveProjectFn)(void* project);
		typedef void (*SaveAsProjectFn)(void* project, const char* filename, bool copyFiles);

		typedef bool (*IsPausedFn)(void* renderer);
		typedef void (*PauseFn)(void* renderer, bool state);
		typedef unsigned int (*GetWindowColorTextureFn)(void* renderer);
		typedef unsigned int (*GetWindowDepthTextureFn)(void* renderer);
		typedef void (*GetLastRenderSizeFn)(void* renderer, int& w, int& h);
		typedef void (*RenderFn)(void* renderer, int w, int h);

		typedef bool (*ExistsPipelineItemFn)(void* pipeline, const char* name);
		typedef void* (*GetPipelineItemFn)(void* pipeline, const char* name);
		typedef int (*GetPipelineItemCountFn)(void* pipeline);
		typedef void* (*GetPipelineItemByIndexFn)(void* pipeline, int index);
		typedef ed::plugin::PipelineItemType (*GetPipelineItemTypeFn)(void* item);
		typedef const char* (*GetPipelineItemNameFn)(void* item);
		typedef void* (*GetPipelineItemPluginOwnerFn)(void* item);
		typedef int (*GetPipelineItemChildrenCountFn)(void* item);
		typedef void* (*GetPipelineItemChildFn)(void* item, int index);
		typedef void (*SetPipelineItemPositionFn)(void* item, float x, float y, float z);
		typedef void (*SetPipelineItemRotationFn)(void* item, float x, float y, float z);
		typedef void (*SetPipelineItemScaleFn)(void* item, float x, float y, float z);
		typedef void (*GetPipelineItemPositionFn)(void* item, float* pos);
		typedef void (*GetPipelineItemRotationFn)(void* item, float* rot);
		typedef void (*GetPipelineItemScaleFn)(void* item, float* scale);

		typedef int (*GetPipelineItemVariableCountFn)(void* item);
		typedef const char* (*GetPipelineItemVariableNameFn)(void* item, int index);
		typedef char* (*GetPipelineItemVariableValueFn)(void* item, int index);
		typedef ed::plugin::VariableType (*GetPipelineItemVariableTypeFn)(void* item, int index);
		typedef bool (*AddPipelineItemVariableFn)(void* item, const char* name, ed::plugin::VariableType type);

		typedef void (*BindShaderPassVariablesFn)(void* shaderpass, void* item);
		typedef void (*GetViewMatrixFn)(float* out);
		typedef void (*GetProjectionMatrixFn)(float* out);
		typedef void (*GetOrthographicMatrixFn)(float* out);
		typedef void (*GetViewportSizeFn)(float& w, float& h);
		typedef void (*AdvanceTimerFn)(float t);
		typedef void (*GetMousePositionFn)(float& x, float& y);
		typedef int (*GetFrameIndexFn)();
		typedef float (*GetTimeFn)();
		typedef void (*SetTimeFn)(float time);
		typedef void (*SetGeometryTransformFn)(void* item, float scale[3], float rota[3], float pos[3]);
		typedef void (*SetMousePositionFn)(float x, float y);
		typedef void (*SetKeysWASDFn)(bool w, bool a, bool s, bool d);
		typedef void (*SetFrameIndexFn)(int findex);

		typedef float (*GetDPIFn)();
		typedef bool (*FileExistsFn)(void* project, const char* path);
		typedef void (*ClearMessageGroupFn)(void* messages, const char* group);
		typedef void (*LogFn)(const char* msg, bool error, const char* file, int line);

		typedef int (*GetObjectCountFn)(void* objects);
		typedef const char* (*GetObjectNameFn)(void* objects, int index);
		typedef bool (*IsTextureFn)(void* objects, const char* name);
		typedef unsigned int (*GetTextureFn)(void* objects, const char* name);
		typedef unsigned int (*GetFlippedTextureFn)(void* objects, const char* name);
		typedef void (*GetTextureSizeFn)(void* objects, const char* name, int& w, int& h);

		typedef bool (*IsRenderTextureFn)(void* objects, const char* name);
		typedef void (*GetRenderTextureSizeFn)(void* objects, const char* name, int& w, int& h);
		typedef unsigned int (*GetDepthTextureFn)(void* objects, const char* name);

		typedef void (*BindDefaultStateFn)();
		typedef void (*OpenInCodeEditorFn)(void* UI, void* item, const char* filename, int id);

		typedef bool (*Deprecated_GetOpenDirectoryDialogFn)(char* out);
		typedef bool (*Deprecated_GetOpenFileDialogFn)(char* out, const char* files);
		typedef bool (*Deprecated_GetSaveFileDialogFn)(char* out, const char* files);

		typedef int (*GetIncludePathCountFn)();
		typedef const char* (*GetIncludePathFn)(void* project, int index);
		typedef const char* (*GetMessagesCurrentItemFn)(void* messages);

		typedef void (*OnEditorContentChangeFn)(void* UI, void* plugin, int langID, int editorID);
		typedef unsigned int* (*GetPipelineItemSPIRVFn)(void* item, ed::plugin::ShaderStage stage, int* dataLen);
		typedef void (*RegisterShortcutFn)(void* plugin, const char* name);

		typedef bool (*GetSettingsBooleanFn)(const char* name);
		typedef int (*GetSettingsIntegerFn)(const char* name);
		typedef const char* (*GetSettingsStringFn)(const char* name);
		typedef float (*GetSettingsFloatFn)(const char* name);
		typedef void (*GetPreviewUIRectFn)(void* ui, float* out);

		typedef void* (*GetPluginFn)(void* pluginManager, const char* name);
		typedef int (*GetPluginListSizeFn)(void* pluginManager);
		typedef const char* (*GetPluginListNameFn)(void* pluginManager, int index);
		typedef void (*SendPluginMessageFn)(void* pluginManager, void* plugin, const char* receiver, char* msg, int msgLen);
		typedef void (*BroadcastPluginMessageFn)(void* pluginManager, void* plugin, char* msg, int msgLen);

		typedef void (*ToggleFullscreenFn)(void* UI);
		typedef bool (*IsFullscreenFn)(void* UI);
		typedef void (*TogglePerformanceModeFn)(void* UI);
		typedef bool (*IsInPerformanceModeFn)(void* UI);

		typedef void (*PushNotificationFn)(void* UI, void* plugin, int id, const char* text, const char* buttonText);

		typedef void (*DebuggerJumpFn)(void* Debugger, void* TextEditor, int line);
		typedef void (*DebuggerContinueFn)(void* Debugger, void* TextEditor);
		typedef void (*DebuggerStepFn)(void* Debugger, void* TextEditor);
		typedef void (*DebuggerStepIntoFn)(void* Debugger, void* TextEditor);
		typedef void (*DebuggerStepOutFn)(void* Debugger, void* TextEditor);
		typedef bool (*DebuggerCheckBreakpointFn)(void* Debugger, void* TextEditor, int line);
		typedef bool (*DebuggerIsDebuggingFn)(void* Debugger, void* TextEditor);
		typedef int (*DebuggerGetCurrentLineFn)(void* Debugger);

		typedef float (*ScaleSizeFn)(float size);

		/********** IPlugin2 **********/
		typedef int (*GetHostIPluginMaxVersionFn)();
		typedef void (*ImGuiFileDialogOpenFn)(const char* key, const char* title, const char* filter);
		typedef void (*ImGuiDirectoryDialogOpenFn)(const char* key, const char* title);
		typedef bool (*ImGuiFileDialogIsDoneFn)(const char* key);
		typedef void (*ImGuiFileDialogCloseFn)(const char* key);
		typedef bool (*ImGuiFileDialogGetResultFn)();
		typedef void (*ImGuiFileDialogGetPathFn)(char* outPath);
		typedef const char* (*DebuggerImmediateFn)(void* Debugger, const char* expr);

		/********** IPlugin3 **********/
		typedef void (*RegisterPluginFn)(void* pluginManager, void* plugin, const char* pname, int apiVer, int pluginVer, void* procDLL);
		typedef void* (*GetEditorPipelineItemFn)(void* UI, void* plugin, int langID, int editorID);
		typedef void (*SetViewportSizeFn)(float w, float h);
		typedef int (*IsObjectBoundFn)(void* Objects, const char* name, void* pipelineItem);
		typedef void (*DebuggerStepIntoPluginEditorFn)(void* Debugger, void* Code, void* Plugin, int lang, int editor);
		typedef void (*DebuggerGetVariableValueFn)(void* Debugger, const char* name, char* value, int valueLength);
		typedef void (*DebuggerStopPluginEditorFn)(void* Debugger, void* Code, void* Plugin, int lang, int editor);
		typedef bool (*DebuggerIsVMRunningFn)(void* Debugger);
	}

	// CreatePlugin(), DestroyPlugin(ptr), GetPluginAPIVersion(), GetPluginVersion(), GetPluginName()
	class IPlugin1 {
	public:
		virtual int GetVersion() { return 1; }

		virtual bool Init(bool isWeb, int sedVersion) = 0;
		virtual void InitUI(void* ctx) = 0;
		virtual void OnEvent(void* e) = 0; // e is &SDL_Event
		virtual void Update(float delta) = 0;
		virtual void Destroy() = 0;

		virtual bool IsRequired() = 0;
		virtual bool IsVersionCompatible(int version) = 0;

		virtual void BeginRender() = 0;
		virtual void EndRender() = 0;

		virtual void Project_BeginLoad() = 0;
		virtual void Project_EndLoad() = 0;
		virtual void Project_BeginSave() = 0;
		virtual void Project_EndSave() = 0;
		virtual bool Project_HasAdditionalData() = 0;
		virtual const char* Project_ExportAdditionalData() = 0;
		virtual void Project_ImportAdditionalData(const char* xml) = 0;
		virtual void Project_CopyFilesOnSave(const char* dir) = 0;

		/* list: file, newproject, project, createitem, window, custom */
		virtual bool HasCustomMenuItem() = 0;
		virtual bool HasMenuItems(const char* name) = 0;
		virtual void ShowMenuItems(const char* name) = 0;

		/* list: pipeline, shaderpass_add (owner = ShaderPass), pluginitem_add (owner = char* ItemType, extraData = PluginItemData) objects, editcode (owner = char* ItemName) */
		virtual bool HasContextItems(const char* name) = 0;
		virtual void ShowContextItems(const char* name, void* owner = nullptr, void* extraData = nullptr) = 0;

		// system variable methods
		virtual int SystemVariables_GetNameCount(ed::plugin::VariableType varType) = 0;
		virtual const char* SystemVariables_GetName(ed::plugin::VariableType varType, int index) = 0;
		virtual bool SystemVariables_HasLastFrame(char* name, ed::plugin::VariableType varType) = 0;
		virtual void SystemVariables_UpdateValue(char* data, char* name, ed::plugin::VariableType varType, bool isLastFrame) = 0;

		// function variables
		virtual int VariableFunctions_GetNameCount(ed::plugin::VariableType vtype) = 0;
		virtual const char* VariableFunctions_GetName(ed::plugin::VariableType varType, int index) = 0;
		virtual bool VariableFunctions_ShowArgumentEdit(char* fname, char* args, ed::plugin::VariableType vtype) = 0;
		virtual void VariableFunctions_UpdateValue(char* data, char* args, char* fname, ed::plugin::VariableType varType) = 0;
		virtual int VariableFunctions_GetArgsSize(char* fname, ed::plugin::VariableType varType) = 0;
		virtual void VariableFunctions_InitArguments(char* args, char* fname, ed::plugin::VariableType vtype) = 0;
		virtual const char* VariableFunctions_ExportArguments(char* fname, ed::plugin::VariableType vtype, char* args) = 0;
		virtual void VariableFunctions_ImportArguments(char* fname, ed::plugin::VariableType vtype, char* args, const char* argsString) = 0;

		// object manager stuff
		virtual bool Object_HasPreview(const char* type) = 0;
		virtual void Object_ShowPreview(const char* type, void* data, unsigned int id) = 0;
		virtual bool Object_IsBindable(const char* type) = 0;
		virtual bool Object_IsBindableUAV(const char* type) = 0;
		virtual void Object_Remove(const char* name, const char* type, void* data, unsigned int id) = 0;
		virtual bool Object_HasExtendedPreview(const char* type) = 0;
		virtual void Object_ShowExtendedPreview(const char* type, void* data, unsigned int id) = 0;
		virtual bool Object_HasProperties(const char* type) = 0;
		virtual void Object_ShowProperties(const char* type, void* data, unsigned int id) = 0;
		virtual void Object_Bind(const char* type, void* data, unsigned int id) = 0;
		virtual const char* Object_Export(char* type, void* data, unsigned int id) = 0;
		virtual void Object_Import(const char* name, const char* type, const char* argsString) = 0;
		virtual bool Object_HasContext(const char* type) = 0;
		virtual void Object_ShowContext(const char* type, void* data) = 0;

		// pipeline item stuff
		virtual bool PipelineItem_HasProperties(const char* type, void* data) = 0;
		virtual void PipelineItem_ShowProperties(const char* type, void* data) = 0;
		virtual bool PipelineItem_IsPickable(const char* type, void* data) = 0;
		virtual bool PipelineItem_HasShaders(const char* type, void* data) = 0; // so that they can be opened in the shader editor
		virtual void PipelineItem_OpenInEditor(const char* type, void* data) = 0;
		virtual bool PipelineItem_CanHaveChild(const char* type, void* data, ed::plugin::PipelineItemType itemType) = 0;
		virtual int PipelineItem_GetInputLayoutSize(const char* type, void* data) = 0; // this must be supported if this item can have geometry as child..
		virtual void PipelineItem_GetInputLayoutItem(const char* type, void* data, int index, ed::plugin::InputLayoutItem& out) = 0;
		virtual void PipelineItem_Remove(const char* itemName, const char* type, void* data) = 0;
		virtual void PipelineItem_Rename(const char* oldName, const char* newName) = 0;
		virtual void PipelineItem_AddChild(const char* owner, const char* name, ed::plugin::PipelineItemType type, void* data) = 0;
		virtual bool PipelineItem_CanHaveChildren(const char* type, void* data) = 0;
		virtual void* PipelineItem_CopyData(const char* type, void* data) = 0;
		virtual void PipelineItem_Execute(void* Owner, ed::plugin::PipelineItemType OwnerType, const char* type, void* data) = 0;
		virtual void PipelineItem_Execute(const char* type, void* data, void* children, int count) = 0;
		virtual void PipelineItem_GetWorldMatrix(const char* type, void* data, float (&pMat)[16]) = 0; //must be implemented if item is pickable
		virtual bool PipelineItem_Intersect(const char* type, void* data, const float* rayOrigin, const float* rayDir, float& hitDist) = 0;
		virtual void PipelineItem_GetBoundingBox(const char* type, void* data, float (&minPos)[3], float (&maxPos)[3]) = 0;
		virtual bool PipelineItem_HasContext(const char* type, void* data) = 0;
		virtual void PipelineItem_ShowContext(const char* type, void* data) = 0;
		virtual const char* PipelineItem_Export(const char* type, void* data) = 0;
		virtual void* PipelineItem_Import(const char* ownerName, const char* name, const char* type, const char* argsString) = 0;
		virtual void PipelineItem_MoveDown(void* ownerData, const char* ownerType, const char* itemName) = 0;
		virtual void PipelineItem_MoveUp(void* ownerData, const char* ownerType, const char* itemName) = 0;
		virtual void PipelineItem_ApplyGizmoTransform(const char* type, void* data, float* transl, float* scale, float* rota) = 0;
		virtual void PipelineItem_GetTransform(const char* type, void* data, float* transl, float* scale, float* rota) = 0;
		virtual void PipelineItem_DebugVertexExecute(void* Owner, ed::plugin::PipelineItemType OwnerType, const char* type, void* data, unsigned int colorVarLoc) = 0;
		virtual int PipelineItem_DebugVertexExecute(const char* type, void* data, const char* childName, float rx, float ry, int vertexGroup) = 0;
		virtual void PipelineItem_DebugInstanceExecute(void* Owner, ed::plugin::PipelineItemType OwnerType, const char* type, void* data, unsigned int colorVarLoc) = 0;
		virtual int PipelineItem_DebugInstanceExecute(const char* type, void* data, const char* childName, float rx, float ry, int vertexGroup) = 0;
		virtual unsigned int PipelineItem_GetVBO(const char* type, void* data) = 0;
		virtual unsigned int PipelineItem_GetVBOStride(const char* type, void* data) = 0;
		virtual bool PipelineItem_CanChangeVariables(const char* type, void* data) = 0;
		virtual bool PipelineItem_IsDebuggable(const char* type, void* data) = 0;
		virtual bool PipelineItem_IsStageDebuggable(const char* type, void* data, ed::plugin::ShaderStage stage) = 0;
		virtual void PipelineItem_DebugExecute(const char* type, void* data, void* children, int count, int* debugID) = 0;
		virtual unsigned int PipelineItem_GetTopology(const char* type, void* data) = 0;
		virtual unsigned int PipelineItem_GetVariableCount(const char* type, void* data) = 0;
		virtual const char* PipelineItem_GetVariableName(const char* type, void* data, unsigned int variable) = 0;
		virtual ed::plugin::VariableType PipelineItem_GetVariableType(const char* type, void* data, unsigned int variable) = 0;
		virtual float PipelineItem_GetVariableValueFloat(const char* type, void* data, unsigned int variable, int col, int row) = 0;
		virtual int PipelineItem_GetVariableValueInteger(const char* type, void* data, unsigned int variable, int col) = 0;
		virtual bool PipelineItem_GetVariableValueBoolean(const char* type, void* data, unsigned int variable, int col) = 0;
		virtual unsigned int PipelineItem_GetSPIRVSize(const char* type, void* data, ed::plugin::ShaderStage stage) = 0;
		virtual unsigned int* PipelineItem_GetSPIRV(const char* type, void* data, ed::plugin::ShaderStage stage) = 0;
		virtual void PipelineItem_DebugPrepareVariables(const char* type, void* data, const char* name) = 0;
		virtual bool PipelineItem_DebugUsesCustomTextures(const char* type, void* data) = 0;
		virtual unsigned int PipelineItem_DebugGetTexture(const char* type, void* data, int loc, const char* variableName) = 0;
		virtual void PipelineItem_DebugGetTextureSize(const char* type, void* data, int loc, const char* variableName, int& x, int& y, int& z) = 0;

		// options
		virtual bool Options_HasSection() = 0;
		virtual void Options_RenderSection() = 0;
		virtual void Options_Parse(const char* key, const char* val) = 0;
		virtual int Options_GetCount() = 0;
		virtual const char* Options_GetKey(int index) = 0;
		virtual const char* Options_GetValue(int index) = 0;

		// languages
		virtual int CustomLanguage_GetCount() = 0;
		virtual const char* CustomLanguage_GetName(int langID) = 0;
		virtual const unsigned int* CustomLanguage_CompileToSPIRV(int langID, const char* src, size_t src_len, ed::plugin::ShaderStage stage, const char* entry, plugin::ShaderMacro* macros, size_t macroCount, size_t* spv_length, bool* compiled) = 0;
		virtual const char* CustomLanguage_ProcessGeneratedGLSL(int langID, const char* src) = 0;
		virtual bool CustomLanguage_SupportsAutoUniforms(int langID) = 0;
		virtual bool CustomLanguage_IsDebuggable(int langID) = 0;
		virtual const char* CustomLanguage_GetDefaultExtension(int langID) = 0;

		// language text editor
		virtual bool ShaderEditor_Supports(int langID) = 0;
		virtual void ShaderEditor_Open(int langID, int editorID, const char* data, int dataLen) = 0;
		virtual void ShaderEditor_Render(int langID, int editorID) = 0;
		virtual void ShaderEditor_Close(int langID, int editorID) = 0;
		virtual const char* ShaderEditor_GetContent(int langID, int editorID, size_t* dataLength) = 0;
		virtual bool ShaderEditor_IsChanged(int langID, int editorID) = 0;
		virtual void ShaderEditor_ResetChangeState(int langID, int editorID) = 0;
		virtual bool ShaderEditor_CanUndo(int langID, int editorID) = 0;
		virtual bool ShaderEditor_CanRedo(int langID, int editorID) = 0;
		virtual void ShaderEditor_Undo(int langID, int editorID) = 0;
		virtual void ShaderEditor_Redo(int langID, int editorID) = 0;
		virtual void ShaderEditor_Cut(int langID, int editorID) = 0;
		virtual void ShaderEditor_Paste(int langID, int editorID) = 0;
		virtual void ShaderEditor_Copy(int langID, int editorID) = 0;
		virtual void ShaderEditor_SelectAll(int langID, int editorID) = 0;
		virtual bool ShaderEditor_HasStats(int langID, int editorID) = 0;

		// code editor
		virtual void CodeEditor_SaveItem(const char* src, int srcLen, const char* path) = 0;
		virtual void CodeEditor_CloseItem(const char* path) = 0;
		virtual bool LanguageDefinition_Exists(int id) = 0;
		virtual int LanguageDefinition_GetKeywordCount(int id) = 0;
		virtual const char** LanguageDefinition_GetKeywords(int id) = 0;
		virtual int LanguageDefinition_GetTokenRegexCount(int id) = 0;
		virtual const char* LanguageDefinition_GetTokenRegex(int index, ed::plugin::TextEditorPaletteIndex& palIndex, int id) = 0;
		virtual int LanguageDefinition_GetIdentifierCount(int id) = 0;
		virtual const char* LanguageDefinition_GetIdentifier(int index, int id) = 0;
		virtual const char* LanguageDefinition_GetIdentifierDesc(int index, int id) = 0;
		virtual const char* LanguageDefinition_GetCommentStart(int id) = 0;
		virtual const char* LanguageDefinition_GetCommentEnd(int id) = 0;
		virtual const char* LanguageDefinition_GetLineComment(int id) = 0;
		virtual bool LanguageDefinition_IsCaseSensitive(int id) = 0;
		virtual bool LanguageDefinition_GetAutoIndent(int id) = 0;
		virtual const char* LanguageDefinition_GetName(int id) = 0;
		virtual const char* LanguageDefinition_GetNameAbbreviation(int id) = 0;

		// autocomplete
		virtual int Autocomplete_GetCount(ed::plugin::ShaderStage stage) = 0;
		virtual const char* Autocomplete_GetDisplayString(ed::plugin::ShaderStage stage, int index) = 0;
		virtual const char* Autocomplete_GetSearchString(ed::plugin::ShaderStage stage, int index) = 0;
		virtual const char* Autocomplete_GetValue(ed::plugin::ShaderStage stage, int index) = 0;

		// file change checks
		virtual int ShaderFilePath_GetCount() = 0;
		virtual const char* ShaderFilePath_Get(int index) = 0;
		virtual bool ShaderFilePath_HasChanged() = 0;
		virtual void ShaderFilePath_Update() = 0;

		// misc
		virtual bool HandleDropFile(const char* filename) = 0;
		virtual void HandleRecompile(const char* itemName) = 0;
		virtual void HandleRecompileFromSource(const char* itemName, int sid, const char* shaderCode, int shaderSize) = 0;
		virtual void HandleShortcut(const char* name) = 0;
		virtual void HandlePluginMessage(const char* sender, char* msg, int msgLen) = 0;
		virtual void HandleApplicationEvent(ed::plugin::ApplicationEvent event, void* data1, void* data2) = 0;
		virtual void HandleNotification(int id) = 0;

		// host functions
		void *Renderer, *Messages, *Project, *UI, *ObjectManager, *PipelineManager, *Plugins, *Debugger;
		pluginfn::AddObjectFn AddObject;
		pluginfn::AddCustomPipelineItemFn AddCustomPipelineItem;
		pluginfn::AddMessageFn AddMessage;
		pluginfn::CreateRenderTextureFn CreateRenderTexture;
		pluginfn::CreateImageFn CreateImage;
		pluginfn::ResizeRenderTextureFn ResizeRenderTexture;
		pluginfn::ResizeImageFn ResizeImage;
		pluginfn::ExistsObjectFn ExistsObject;
		pluginfn::RemoveObjectFn RemoveGlobalObject;
		pluginfn::GetProjectPathFn GetProjectPath;
		pluginfn::GetRelativePathFn GetRelativePath;
		pluginfn::GetProjectFilenameFn GetProjectFilename;
		pluginfn::GetProjectDirectoryFn GetProjectDirectory;
		pluginfn::IsProjectModifiedFn IsProjectModified;
		pluginfn::ModifyProjectFn ModifyProject;
		pluginfn::OpenProjectFn OpenProject;
		pluginfn::SaveProjectFn SaveProject;
		pluginfn::SaveAsProjectFn SaveAsProject;
		pluginfn::IsPausedFn IsPaused;
		pluginfn::PauseFn Pause;
		pluginfn::GetWindowColorTextureFn GetWindowColorTexture;
		pluginfn::GetWindowDepthTextureFn GetWindowDepthTexture;
		pluginfn::GetLastRenderSizeFn GetLastRenderSize;
		pluginfn::RenderFn Render;
		pluginfn::ExistsPipelineItemFn ExistsPipelineItem;
		pluginfn::GetPipelineItemFn GetPipelineItem;
		pluginfn::BindShaderPassVariablesFn BindShaderPassVariables;
		pluginfn::GetViewMatrixFn GetViewMatrix;
		pluginfn::GetProjectionMatrixFn GetProjectionMatrix;
		pluginfn::GetOrthographicMatrixFn GetOrthographicMatrix;
		pluginfn::GetViewportSizeFn GetViewportSize;
		pluginfn::AdvanceTimerFn AdvanceTimer;
		pluginfn::GetMousePositionFn GetMousePosition;
		pluginfn::GetFrameIndexFn GetFrameIndex;
		pluginfn::GetTimeFn GetTime;
		pluginfn::SetTimeFn SetTime;
		pluginfn::SetGeometryTransformFn SetGeometryTransform;
		pluginfn::SetMousePositionFn SetMousePosition;
		pluginfn::SetKeysWASDFn SetKeysWASD;
		pluginfn::SetFrameIndexFn SetFrameIndex;
		pluginfn::GetDPIFn GetDPI;
		pluginfn::FileExistsFn FileExists;
		pluginfn::ClearMessageGroupFn ClearMessageGroup;
		pluginfn::LogFn Log;
		pluginfn::GetObjectCountFn GetObjectCount;
		pluginfn::GetObjectNameFn GetObjectName;
		pluginfn::IsTextureFn IsTexture;
		pluginfn::GetTextureFn GetTexture;
		pluginfn::GetFlippedTextureFn GetFlippedTexture;
		pluginfn::GetTextureSizeFn GetTextureSize;
		pluginfn::BindDefaultStateFn BindDefaultState;
		pluginfn::OpenInCodeEditorFn OpenInCodeEditor;
		pluginfn::GetPipelineItemCountFn GetPipelineItemCount;
		pluginfn::GetPipelineItemTypeFn GetPipelineItemType;
		pluginfn::GetPipelineItemByIndexFn GetPipelineItemByIndex;
		pluginfn::GetPipelineItemNameFn GetPipelineItemName;
		pluginfn::GetPipelineItemPluginOwnerFn GetPipelineItemPluginOwner;
		pluginfn::GetPipelineItemVariableCountFn GetPipelineItemVariableCount;
		pluginfn::GetPipelineItemVariableNameFn GetPipelineItemVariableName;
		pluginfn::GetPipelineItemVariableValueFn GetPipelineItemVariableValue;
		pluginfn::GetPipelineItemVariableTypeFn GetPipelineItemVariableType;
		pluginfn::AddPipelineItemVariableFn AddPipelineItemVariable;
		pluginfn::GetPipelineItemChildrenCountFn GetPipelineItemChildrenCount;
		pluginfn::GetPipelineItemChildFn GetPipelineItemChild;
		pluginfn::SetPipelineItemPositionFn SetPipelineItemPosition;
		pluginfn::SetPipelineItemRotationFn SetPipelineItemRotation;
		pluginfn::SetPipelineItemScaleFn SetPipelineItemScale;
		pluginfn::GetPipelineItemPositionFn GetPipelineItemPosition;
		pluginfn::GetPipelineItemRotationFn GetPipelineItemRotation;
		pluginfn::GetPipelineItemScaleFn GetPipelineItemScale;
		pluginfn::Deprecated_GetOpenDirectoryDialogFn DEPRECATED_GetOpenDirectoryDialog;
		pluginfn::Deprecated_GetOpenFileDialogFn DEPRECATED_GetOpenFileDialog;
		pluginfn::Deprecated_GetSaveFileDialogFn DEPRECATED_GetSaveFileDialog;
		pluginfn::GetIncludePathCountFn GetIncludePathCount;
		pluginfn::GetIncludePathFn GetIncludePath;
		pluginfn::GetMessagesCurrentItemFn GetMessagesCurrentItem;
		pluginfn::OnEditorContentChangeFn OnEditorContentChange;
		pluginfn::GetPipelineItemSPIRVFn GetPipelineItemSPIRV;
		pluginfn::RegisterShortcutFn RegisterShortcut;
		pluginfn::GetSettingsBooleanFn GetSettingsBoolean;
		pluginfn::GetSettingsStringFn GetSettingsString;
		pluginfn::GetSettingsIntegerFn GetSettingsInteger;
		pluginfn::GetSettingsFloatFn GetSettingsFloat;
		pluginfn::GetPreviewUIRectFn GetPreviewUIRect;
		pluginfn::GetPluginFn GetPlugin;
		pluginfn::GetPluginListSizeFn GetPluginListSize;
		pluginfn::GetPluginListNameFn GetPluginName;
		pluginfn::SendPluginMessageFn SendPluginMessage;
		pluginfn::BroadcastPluginMessageFn BroadcastPluginMessage;
		pluginfn::ToggleFullscreenFn ToggleFullscreen;
		pluginfn::IsFullscreenFn IsFullscreen;
		pluginfn::TogglePerformanceModeFn TogglePerformanceMode;
		pluginfn::IsInPerformanceModeFn IsInPerformanceMode;
		pluginfn::PushNotificationFn PushNotification;
		pluginfn::DebuggerJumpFn DebuggerJump;
		pluginfn::DebuggerContinueFn DebuggerContinue;
		pluginfn::DebuggerStepFn DebuggerStep;
		pluginfn::DebuggerStepIntoFn DebuggerStepInto;
		pluginfn::DebuggerStepOutFn DebuggerStepOut;
		pluginfn::DebuggerCheckBreakpointFn DebuggerCheckBreakpoint;
		pluginfn::DebuggerIsDebuggingFn DebuggerIsDebugging;
		pluginfn::DebuggerGetCurrentLineFn DebuggerGetCurrentLine;
		pluginfn::IsRenderTextureFn IsRenderTexture;
		pluginfn::GetRenderTextureSizeFn GetRenderTextureSize;
		pluginfn::GetDepthTextureFn GetDepthTexture;
		pluginfn::ScaleSizeFn ScaleSize;
	};

	class IPlugin2 : public IPlugin1 {
	public:
		virtual int GetVersion() { return 2; }

		virtual bool PipelineItem_SupportsImmediateMode(const char* type, void* data, ed::plugin::ShaderStage stage) = 0;
		virtual bool PipelineItem_HasCustomImmediateModeCompiler(const char* type, void* data, ed::plugin::ShaderStage stage) = 0;
		virtual bool PipelineItem_ImmediateModeCompile(const char* type, void* data, ed::plugin::ShaderStage stage, const char* expression) = 0;

		virtual unsigned int ImmediateMode_GetSPIRVSize() = 0;
		virtual unsigned int* ImmediateMode_GetSPIRV() = 0;
		virtual unsigned int ImmediateMode_GetVariableCount() = 0;
		virtual const char* ImmediateMode_GetVariableName(unsigned int index) = 0;
		virtual int ImmediateMode_GetResultID() = 0;

		pluginfn::GetHostIPluginMaxVersionFn GetHostIPluginMaxVersion;
		pluginfn::ImGuiFileDialogOpenFn ImGuiFileDialogOpen;
		pluginfn::ImGuiDirectoryDialogOpenFn ImGuiDirectoryDialogOpen;
		pluginfn::ImGuiFileDialogIsDoneFn ImGuiFileDialogIsDone;
		pluginfn::ImGuiFileDialogCloseFn ImGuiFileDialogClose;
		pluginfn::ImGuiFileDialogGetResultFn ImGuiFileDialogGetResult;
		pluginfn::ImGuiFileDialogGetPathFn ImGuiFileDialogGetPath;
		pluginfn::DebuggerImmediateFn DebuggerImmediate;
	};

	class IPlugin3 : public IPlugin2 {
	public:
		virtual int GetVersion() { return 3; }

		virtual void PluginManager_RegisterPlugins() = 0;
		virtual const unsigned int* CustomLanguage_CompileToSPIRV2(void* item, int langID, const char* src, size_t src_len, ed::plugin::ShaderStage stage, const char* entry, ed::plugin::ShaderMacro* macros, size_t macroCount, size_t* spv_length, bool* compiled) = 0;
		virtual void ShaderEditor_SetLineIndicator(int langID, int editorID, int line) = 0;

		pluginfn::RegisterPluginFn RegisterPlugin;
		pluginfn::GetEditorPipelineItemFn GetEditorPipelineItem;
		pluginfn::SetViewportSizeFn SetViewportSize;
		pluginfn::IsObjectBoundFn IsObjectBound;
		pluginfn::DebuggerStepIntoPluginEditorFn DebuggerStepIntoPluginEditor;
		pluginfn::DebuggerGetVariableValueFn DebuggerGetVariableValue;
		pluginfn::DebuggerStopPluginEditorFn DebuggerStopPluginEditor;
		pluginfn::DebuggerIsVMRunningFn DebuggerIsVMRunning;
	};
}