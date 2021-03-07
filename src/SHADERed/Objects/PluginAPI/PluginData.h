#pragma once

namespace ed {
	namespace plugin {
		enum class PipelineItemType {
			ShaderPass,
			Geometry,
			RenderState,
			Model,
			VertexBuffer,
			ComputePass,
			AudioPass,
			PluginItem,
			Count
		};

		enum class VariableType {
			Boolean1,
			Boolean2,
			Boolean3,
			Boolean4,
			Integer1,
			Integer2,
			Integer3,
			Integer4,
			Float1,
			Float2,
			Float3,
			Float4,
			Float2x2,
			Float3x3,
			Float4x4,
			Count
		};

		enum class InputLayoutValue {
			Position,
			Normal,
			Texcoord,
			Tangent,
			Binormal,
			Color,
			BufferFloat,
			BufferFloat2,
			BufferFloat3,
			BufferFloat4,
			BufferInt,
			BufferInt2,
			BufferInt3,
			BufferInt4,
			MaxCount
		};

		enum class MessageType {
			Error,
			Warning,
			Message
		};

		class InputLayoutItem {
		public:
			ed::plugin::InputLayoutValue Value;
			char Semantic[64];
		};

		enum class TextEditorPaletteIndex {
			Default,
			Keyword,
			Number,
			String,
			CharLiteral,
			Punctuation,
			Preprocessor,
			Identifier,
			KnownIdentifier,
			PreprocIdentifier,
			Comment,
			MultiLineComment,
			Background,
			Cursor,
			Selection,
			ErrorMarker,
			Breakpoint,
			BreakpointOutline,
			CurrentLineIndicator,
			CurrentLineIndicatorOutline,
			LineNumber,
			CurrentLineFill,
			CurrentLineFillInactive,
			CurrentLineEdge,
			ErrorMessage,
			BreakpointDisabled,
			UserFunction,
			UserType,
			UniformVariable,
			GlobalVariable,
			LocalVariable,
			FunctionArgument,
			Max
		};

		enum class ShaderStage {
			Vertex,
			Pixel,
			Geometry,
			Compute,
			Audio,
			Plugin,
			TessellationControl,
			TessellationEvaluation,
			Count
		};

		struct ShaderMacro {
			bool Active;
			char Name[32];
			char Value[512];
		};

		enum class ApplicationEvent
		{
			PipelineItemCompiled, /* char* itemName, nullptr */
			PipelineItemAdded,	  /* char* itemName, nullptr */
			PipelineItemDeleted,  /* char* itemName, nullptr */
			PipelineItemRenamed,
			DebuggerStarted,	  /* char* itemName, void* editor */
			DebuggerStepped,	  /* int line, nullptr */
			DebuggerStopped		  /* nullptr, nullptr */
			/* ETC... */
		};
	}
}