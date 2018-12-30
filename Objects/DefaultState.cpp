#include "DefaultState.h"

namespace ed
{
	void DefaultState::Create(ml::Window & wnd)
	{
		// create default blend state
		D3D11_RENDER_TARGET_BLEND_DESC* desc = &Blend.Info.RenderTarget[0];
		Blend.Info.AlphaToCoverageEnable = false;
		Blend.Info.IndependentBlendEnable = false;
		desc->BlendEnable = false;
		desc->SrcBlend = D3D11_BLEND_ONE;
		desc->DestBlend = D3D11_BLEND_ZERO;
		desc->BlendOp = D3D11_BLEND_OP_ADD;
		desc->SrcBlendAlpha = D3D11_BLEND_ONE;
		desc->DestBlendAlpha = D3D11_BLEND_ZERO;
		desc->BlendOpAlpha = D3D11_BLEND_OP_ADD;
		desc->RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		Blend.Create(wnd);

		DepthStencil.Create(wnd);
	}
	void DefaultState::Bind()
	{
		Blend.Bind();
		DepthStencil.Bind();
	}
}
