#include "InterfaceManager.h"
#include "GUIManager.h"
#include "Objects/ShaderTranscompiler.h"

namespace ed
{
	void copyFloatData(eng::Model::Mesh::Vertex& out, GLfloat* bufData)
	{
		out.Position = glm::vec3(bufData[0], bufData[1], bufData[2]);
		out.Normal = glm::vec3(bufData[3], bufData[4], bufData[5]);
		out.TexCoords = glm::vec2(bufData[6], bufData[7]);
		out.Tangent = glm::vec3(bufData[8], bufData[9], bufData[10]);
		out.Binormal = glm::vec3(bufData[11], bufData[12], bufData[13]);
		out.Color = glm::vec4(bufData[14], bufData[15], bufData[16], bufData[17]);
	}

	InterfaceManager::InterfaceManager(GUIManager* gui) :
		Renderer(&Pipeline, &Objects, &Parser, &Messages, &Plugins, &Debugger),
		Pipeline(&Parser),
		Objects(&Parser, &Renderer),
		Parser(&Pipeline, &Objects, &Renderer, &Plugins, &Messages, &Debugger, gui),
		Debugger(&Objects, &Renderer)
	{
		m_ui = gui;
	}
	InterfaceManager::~InterfaceManager()
	{
		Objects.Clear();
		Plugins.Destroy();
	}
	bool InterfaceManager::FetchPixel(PixelInformation& pixel)
	{
		bool ret = true;

		int vertID = Renderer.DebugVertexPick(pixel.Owner, pixel.Object, pixel.RelativeCoordinate);
		bool isInstanced = false;

		pixel.VertexID = vertID;

		// getting the vertices
		// TODO: lines, points, etc...
		int vertCount = 3;
		if (pixel.Object->Type == PipelineItem::ItemType::Geometry) {
			pipe::GeometryItem::GeometryType geoType = ((pipe::GeometryItem*)pixel.Object->Data)->Type;
			GLuint vbo = ((pipe::GeometryItem*)pixel.Object->Data)->VBO;

			isInstanced = ((pipe::GeometryItem*)pixel.Object->Data)->Instanced;

			// TODO: don't bother GPU so much, v1.3.*
			glBindBuffer(GL_ARRAY_BUFFER, vbo);
			if (geoType == pipe::GeometryItem::GeometryType::ScreenQuadNDC) {
				GLfloat bufData[6 * 4] = { 0.0f };
				glGetBufferSubData(GL_ARRAY_BUFFER, 0, 6 * 4 * sizeof(float), &bufData[0]);


				int bufferLoc = (pixel.VertexID / vertCount) * vertCount * 4;

				// TODO: change this *PLACEHOLDER*
				pixel.Vertex[0].Position = glm::vec3(bufData[bufferLoc + 0], bufData[bufferLoc + 1], 0.0f);
				pixel.Vertex[1].Position = glm::vec3(bufData[bufferLoc + 4], bufData[bufferLoc + 5], 0.0f);
				pixel.Vertex[2].Position = glm::vec3(bufData[bufferLoc + 8], bufData[bufferLoc + 9], 0.0f);
				pixel.Vertex[0].TexCoords = glm::vec2(bufData[bufferLoc + 2], bufData[bufferLoc + 3]);
				pixel.Vertex[1].TexCoords = glm::vec2(bufData[bufferLoc + 6], bufData[bufferLoc + 7]);
				pixel.Vertex[2].TexCoords = glm::vec2(bufData[bufferLoc + 10], bufData[bufferLoc + 11]);
			}
			else {
				GLfloat bufData[3 * 18] = { 0.0f };
				int vertStart = ((int)(vertID / vertCount)) * vertCount;
				glGetBufferSubData(GL_ARRAY_BUFFER, vertStart * 18 * sizeof(float), vertCount * 18 * sizeof(float), &bufData[0]);

				copyFloatData(pixel.Vertex[0], &bufData[0]);
				copyFloatData(pixel.Vertex[1], &bufData[18]);
				copyFloatData(pixel.Vertex[2], &bufData[36]);
			}
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}
		else {
			int vertStart = ((int)(vertID / vertCount)) * vertCount;

			// TODO: mesh id??
			pipe::Model* mdl = ((pipe::Model*)pixel.Object->Data);
			pixel.Vertex[0] = mdl->Data->Meshes[0].Vertices[vertStart + 0];
			pixel.Vertex[1] = mdl->Data->Meshes[0].Vertices[vertStart + 1];
			pixel.Vertex[2] = mdl->Data->Meshes[0].Vertices[vertStart + 2];

			isInstanced = mdl->Instanced;
		}
		pixel.VertexCount = vertCount;

		// get the instance id if this object uses instancing
		if (isInstanced)
			pixel.InstanceID = Renderer.DebugInstancePick(pixel.Owner, pixel.Object, pixel.RelativeCoordinate);

		pipe::ShaderPass* pass = ((pipe::ShaderPass*)pixel.Owner->Data);

		// getting the debugger's vs output
		ed::ShaderLanguage lang = ShaderTranscompiler::GetShaderTypeFromExtension(pass->VSPath);
		std::string vsSrc = Parser.LoadProjectFile(pass->VSPath);
		bool vsCompiled = Debugger.SetSource(lang, sd::ShaderType::Vertex, lang == ed::ShaderLanguage::GLSL ? "main" : pass->VSEntry, vsSrc);
		if (vsCompiled) {
			for (int i = 0; i < vertCount; i++) {
				Debugger.InitEngine(pixel, i);
				Debugger.Fetch(i); // run the shader
			}
		}
		else ret = false;

		// getting the debugger's ps output
		bool psCompiled = false;
		if (vsCompiled) {
			lang = ShaderTranscompiler::GetShaderTypeFromExtension(pass->PSPath);
			std::string psSrc = Parser.LoadProjectFile(pass->PSPath);
			psCompiled = Debugger.SetSource(lang, sd::ShaderType::Pixel, lang == ed::ShaderLanguage::GLSL ? "main" : pass->PSEntry, psSrc);
			if (psCompiled) {
				Debugger.InitEngine(pixel);
				Debugger.Fetch();
			}
			else ret = false;
		}

		pixel.Discarded = Debugger.Engine.IsDiscarded();
		pixel.Fetched = vsCompiled && psCompiled;

		return ret;
	}
	void InterfaceManager::OnEvent(const SDL_Event& e)
	{}
	void InterfaceManager::Update(float delta)
	{}
}