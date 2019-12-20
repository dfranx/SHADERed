#pragma once
#include "PluginData.h"

namespace ed
{
	namespace pluginfn
	{
		typedef void (*AddObjectFn)(void* objects, const char* name, const char* type, void* data, unsigned int id, void* owner);
		typedef bool (*AddCustomPipelineItemFn)(void* pipeline, void* parent, const char* name, const char* type, void* data, void* owner);

		typedef void (*AddMessageFn)(void* messages, plugin::MessageType mtype, const char* group, const char* txt, int ln);

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
		typedef void (*OpenProjectFn)(void* project, const char* filename);
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

		typedef void (*BindShaderPassVariablesFn)(void* shaderpass, void* item);
		typedef void (*GetViewMatrixFn)(float* out);
		typedef void (*GetProjectionMatrixFn)(float* out);
		typedef void (*GetOrthographicMatrixFn)(float* out);
		typedef void (*GetViewportSizeFn)(float& w, float& h);
		typedef void (*AdvanceTimerFn)(float t);
		typedef void (*GetMousePositionFn)(float& x, float& y);
		typedef int  (*GetFrameIndexFn)();
		typedef float (*GetTimeFn)();
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
		
		typedef void (*BindDefaultStateFn)();
		typedef void (*OpenInCodeEditorFn)(void* CodeEditorUI, void* item, const char* filename, int id);
	}

	// CreatePlugin(), DestroyPlugin(ptr), GetPluginAPIVersion(), GetPluginVersion(), GetPluginName()
	class IPlugin
	{
	public:
		virtual bool Init() = 0;
		virtual void OnEvent(void* e) = 0; // e is &SDL_Event, it is void here so that people don't have to link to SDL if they don't want to
		virtual void Update(float delta) = 0;
		virtual void Destroy() = 0;

		virtual void BeginRender() = 0;
		virtual void EndRender() = 0;

		virtual void BeginProjectLoading() = 0;
		virtual void EndProjectLoading() = 0;
		virtual void BeginProjectSaving() = 0;
		virtual void EndProjectSaving() = 0;
		virtual void CopyFilesOnSave(const char* dir) = 0;
		virtual bool HasCustomMenu() = 0;

		/* list: file, newproject, project, createitem, window, custom */
		virtual bool HasMenuItems(const char* name) = 0;
		virtual void ShowMenuItems(const char* name) = 0;

		/* list: pipeline, shaderpass_add (owner = ShaderPass), pluginitem_add (owner = char* ItemType, extraData = PluginItemData) objects, editcode (owner = char* ItemName) */
		virtual bool HasContextItems(const char* name) = 0;
		virtual void ShowContextItems(const char* name, void* owner = nullptr, void* extraData = nullptr) = 0;

		// system variable methods
		virtual bool HasSystemVariables(plugin::VariableType varType) = 0;
		virtual int GetSystemVariableNameCount(plugin::VariableType varType) = 0;
		virtual const char* GetSystemVariableName(plugin::VariableType varType, int index) = 0;
		virtual bool HasLastFrame(char* name, plugin::VariableType varType) = 0;
		virtual void UpdateSystemVariableValue(char* data, char* name, plugin::VariableType varType, bool isLastFrame) = 0;

		// function variables
		virtual bool HasVariableFunctions(plugin::VariableType vtype) = 0;
		virtual int GetVariableFunctionNameCount(plugin::VariableType vtype) = 0;
		virtual const char* GetVariableFunctionName(plugin::VariableType varType, int index) = 0;
		virtual bool ShowFunctionArgumentEdit(char* fname, char* args, plugin::VariableType vtype) = 0;
		virtual void UpdateVariableFunctionValue(char* data, char* args, char* fname, plugin::VariableType varType) = 0;
		virtual int GetVariableFunctionArgSpaceSize(char* fname, plugin::VariableType varType) = 0;
		virtual void InitVariableFunctionArguments(char* args, char* fname, plugin::VariableType vtype) = 0;
		virtual const char* ExportFunctionArguments(char* fname, plugin::VariableType vtype, char* args) = 0;
		virtual void ImportFunctionArguments(char* fname, plugin::VariableType vtype, char* args, const char* argsString) = 0;

		// object manager stuff
		void* ObjectManager;
		pluginfn::AddObjectFn AddObject;
		virtual bool HasObjectPreview(const char* type) = 0;
		virtual void ShowObjectPreview(const char* type, void* data, unsigned int id) = 0;
		virtual bool IsObjectBindable(const char* type) = 0;
		virtual bool IsObjectBindableUAV(const char* type) = 0;
		virtual void RemoveObject(const char* name, const char* type, void* data, unsigned int id) = 0;
		virtual bool HasObjectExtendedPreview(const char* type) = 0;
		virtual void ShowObjectExtendedPreview(const char* type, void* data, unsigned int id) = 0;
		virtual bool HasObjectProperties(const char* type) = 0;
		virtual void ShowObjectProperties(const char* type, void* data, unsigned int id) = 0;
		virtual void BindObject(const char* type, void* data, unsigned int id) = 0;
		virtual const char* ExportObject(char* type, void* data, unsigned int id) = 0;
		virtual void ImportObject(const char* name, const char* type, const char* argsString) = 0;
		virtual bool HasObjectContext(const char* type) = 0;
		virtual void ShowObjectContext(const char* type, void* data) = 0;

		// pipeline item stuff
		void* PipelineManager;
		pluginfn::AddCustomPipelineItemFn AddCustomPipelineItem;
		virtual bool HasPipelineItemProperties(const char* type) = 0;
		virtual void ShowPipelineItemProperties(const char* type, void* data) = 0;
		virtual bool IsPipelineItemPickable(const char* type) = 0;
		virtual bool HasPipelineItemShaders(const char* type) = 0; // so that they can be opened in the shader editor
		virtual void OpenPipelineItemInEditor(void* CodeEditor, const char* type, void* data) = 0;
		virtual bool CanPipelineItemHaveChild(const char* type, plugin::PipelineItemType itemType) = 0;
		virtual int GetPipelineItemInputLayoutSize(const char* itemName) = 0; // this must be supported if this item can have geometry as child..
		virtual void GetPipelineItemInputLayoutItem(const char* itemName, int index, plugin::InputLayoutItem& out) = 0;
		virtual void RemovePipelineItem(const char* itemName, const char* type, void* data) = 0;
		virtual void RenamePipelineItem(const char* oldName, const char* newName) = 0;
		virtual void AddPipelineItemChild(const char* owner, const char* name, plugin::PipelineItemType type, void* data) = 0;
		virtual bool CanPipelineItemHaveChildren(const char* type) = 0;
		virtual void* CopyPipelineItemData(const char* type, void* data) = 0;
		virtual void ExecutePipelineItem(void* Owner, plugin::PipelineItemType OwnerType, const char* type, void* data) = 0;
		virtual void ExecutePipelineItem(const char* type, void* data, void* children, int count) = 0;
		virtual void GetPipelineItemWorldMatrix(const char* name, float(&pMat)[16]) = 0; //must be implemented if item is pickable
		virtual bool IntersectPipelineItem(const char* type, void* data, const float* rayOrigin, const float* rayDir, float& hitDist) = 0;
		virtual void GetPipelineItemBoundingBox(const char* name, float(&minPos)[3], float(&maxPos)[3]) = 0;
		virtual bool HasPipelineItemContext(const char* type) = 0;
		virtual void ShowPipelineItemContext(const char* type, void* data) = 0;
		virtual const char* ExportPipelineItem(const char* type, void* data) = 0;
		virtual void* ImportPipelineItem(const char* ownerName, const char* name, const char* type, const char* argsString) = 0;

		// options
		virtual bool HasSectionInOptions() = 0;
		virtual void ShowOptions() = 0;

		// code editor
		virtual void SaveCodeEditorItem(const char* src, int srcLen, int id) = 0;
		virtual void CloseCodeEditorItem(int id) = 0;
		virtual int GetLanguageDefinitionKeywordCount(int id) = 0;
		virtual const char** GetLanguageDefinitionKeywords(int id) = 0;
		virtual int GetLanguageDefinitionTokenRegexCount(int id) = 0;
		virtual const char* GetLanguageDefinitionTokenRegex(int index, plugin::TextEditorPaletteIndex& palIndex, int id) = 0;
		virtual int GetLanguageDefinitionIdentifierCount(int id) = 0;
		virtual const char* GetLanguageDefinitionIdentifier(int index, int id) = 0;
		virtual const char* GetLanguageDefinitionIdentifierDesc(int index, int id) = 0;
		virtual const char* GetLanguageDefinitionCommentStart(int id) = 0;
		virtual const char* GetLanguageDefinitionCommentEnd(int id) = 0;
		virtual const char* GetLanguageDefinitionLineComment(int id) = 0;
		virtual bool IsLanguageDefinitionCaseSensitive(int id) = 0;
		virtual bool GetLanguageDefinitionAutoIndent(int id) = 0;
		virtual const char* GetLanguageDefinitionName(int id) = 0;

		// misc
		virtual bool HandleDropFile(const char* filename) = 0;
		virtual void HandleRecompile(const char* itemName) = 0;
		virtual void HandleRecompileFromSource(const char* itemName, int sid, const char* shaderCode, int shaderSize) = 0;
		virtual int GetShaderFilePathCount() = 0; // for file change checks
		virtual const char* GetShaderFilePath(int index) = 0;
		virtual bool HasShaderFilePathChanged() = 0;
		virtual void UpdateShaderFilePath() = 0;

		// some functions exported from SHADERed
		void* Renderer, * Messages, * Project, * CodeEditor;
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
	};
}