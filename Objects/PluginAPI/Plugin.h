#pragma once
#include "PluginData.h"

namespace ed
{
	typedef void (*AddObjectFn)(void* objManager, const char* name, const char* type, void* data, unsigned int id, void* owner);

	// CreatePlugin(), DestroyPlugin(ptr), GetPluginAPIVersion(), GetPluginName()
	class IPlugin
	{
	public:
		virtual bool Init() = 0;
		virtual void OnEvent(void* e) = 0; // e is &SDL_Event, it is void here so that people don't have to link to SDL if they don't want to
		virtual void Update(float delta) = 0;
		virtual void Destroy() = 0;

		virtual bool HasCustomMenu() = 0;

		/* list: file, newproject, project, createitem, window, custom */
		virtual bool HasMenuItems(const char* name) = 0;
		virtual void ShowMenuItems(const char* name) = 0;

		/* list: pipeline, shaderpass_add, objects */
		virtual bool HasContextItems(const char* name) = 0;
		virtual void ShowContextItems(const char* name) = 0;

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
		AddObjectFn AddObject;
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
	};
}