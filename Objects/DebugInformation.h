#pragma once
#include <ShaderDebugger/ShaderDebugger.h>
#include "Debug/PixelInformation.h"
#include "ShaderLanguage.h"
#include "ObjectManager.h"

namespace ed
{
	class DebugInformation
	{
	public:
		DebugInformation(ObjectManager* objs);

		inline void ClearPixelList() { m_pixels.clear(); }
		inline void AddPixel(const PixelInformation& px) { m_pixels.push_back(px); }
		inline std::vector<PixelInformation>& GetPixelList() { return m_pixels; }

		bool SetSource(ed::ShaderLanguage lang, sd::ShaderType stage, const std::string& entry, const std::string& src);
		void InitEngine(PixelInformation& pass, int id = 0); // set up input variables
		void Fetch(int id = 0);

		sd::ShaderDebugger DebugEngine;
	
	private:
		ObjectManager* m_objs;

		bv_stack m_args;
		bv_library* m_libHLSL, *m_libGLSL;
		std::string m_entry;
		PixelInformation* m_pixel;
		ed::ShaderLanguage m_lang;
		sd::ShaderType m_stage;
		std::vector<PixelInformation> m_pixels;

		void m_cleanTextures(sd::ShaderType stage);
		std::unordered_map<sd::ShaderType, std::vector<sd::Texture*>> m_textures;
	};
}