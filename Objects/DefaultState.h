#pragma once
#include <MoonLight/Base/BlendState.h>
#include <MoonLight/Base/DepthStencilState.h>

namespace ed
{
	class DefaultState
	{
	public:
		static inline DefaultState& Instance()
		{
			static DefaultState ret;
			return ret;
		}

		ml::BlendState Blend;		

		void Create(ml::Window& wnd);
		void Bind();
	};
}