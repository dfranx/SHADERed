#pragma once

namespace ed {
	struct ShaderMacro {
		bool Active;
		char Name[32];
		char Value[512];
	};
}