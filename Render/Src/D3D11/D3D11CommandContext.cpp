#include "D3D11/D3D11CommandContext.h"
#include "RHIPrivate/D3D11RHIPrivate.h"
#include "RHIPrivate/D3D11StateCachePrivate.h"
#include "D3D11/D3D11RHI.h"
#include "D3D11/D3D11ReourceTraits.h"
#include "core/logger.h"

namespace RenderCore
{
	// Primitive drawing.

	static D3D11_PRIMITIVE_TOPOLOGY GetD3D11PrimitiveType(EPrimitiveType PrimitiveType, bool bUsingTessellation)
	{
		if (bUsingTessellation)
		{
			switch (PrimitiveType)
			{
			case PT_1_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST;
			case PT_2_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_2_CONTROL_POINT_PATCHLIST;

				// This is the case for tessellation without AEN or other buffers, so just flip to 3 CPs
			case PT_TriangleList: return D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST;

			case PT_LineList:
			case PT_TriangleStrip:
			case PT_QuadList:
			case PT_PointList:
			case PT_RectList:
				core::logger::err() << L"Invalid type specified for tessellated render, probably missing a case in FStaticMeshSceneProxy::GetMeshElement";
				break;
			default:
				// Other cases are valid.
				break;
			};
		}

		switch (PrimitiveType)
		{
		case PT_TriangleList: return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		case PT_TriangleStrip: return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
		case PT_LineList: return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
		case PT_PointList: return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;

			// ControlPointPatchList types will pretend to be TRIANGLELISTS with a stride of N 
			// (where N is the number of control points specified), so we can return them for
			// tessellation and non-tessellation. This functionality is only used when rendering a 
			// default material with something that claims to be tessellated, generally because the 
			// tessellation material failed to compile for some reason.
		case PT_3_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST;
		case PT_4_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST;
		case PT_5_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_5_CONTROL_POINT_PATCHLIST;
		case PT_6_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_6_CONTROL_POINT_PATCHLIST;
		case PT_7_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_7_CONTROL_POINT_PATCHLIST;
		case PT_8_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_8_CONTROL_POINT_PATCHLIST;
		case PT_9_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_9_CONTROL_POINT_PATCHLIST;
		case PT_10_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_10_CONTROL_POINT_PATCHLIST;
		case PT_11_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_11_CONTROL_POINT_PATCHLIST;
		case PT_12_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_12_CONTROL_POINT_PATCHLIST;
		case PT_13_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_13_CONTROL_POINT_PATCHLIST;
		case PT_14_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_14_CONTROL_POINT_PATCHLIST;
		case PT_15_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_15_CONTROL_POINT_PATCHLIST;
		case PT_16_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_16_CONTROL_POINT_PATCHLIST;
		case PT_17_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_17_CONTROL_POINT_PATCHLIST;
		case PT_18_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_18_CONTROL_POINT_PATCHLIST;
		case PT_19_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_19_CONTROL_POINT_PATCHLIST;
		case PT_20_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_20_CONTROL_POINT_PATCHLIST;
		case PT_21_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_21_CONTROL_POINT_PATCHLIST;
		case PT_22_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_22_CONTROL_POINT_PATCHLIST;
		case PT_23_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_23_CONTROL_POINT_PATCHLIST;
		case PT_24_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_24_CONTROL_POINT_PATCHLIST;
		case PT_25_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_25_CONTROL_POINT_PATCHLIST;
		case PT_26_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_26_CONTROL_POINT_PATCHLIST;
		case PT_27_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_27_CONTROL_POINT_PATCHLIST;
		case PT_28_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_28_CONTROL_POINT_PATCHLIST;
		case PT_29_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_29_CONTROL_POINT_PATCHLIST;
		case PT_30_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_30_CONTROL_POINT_PATCHLIST;
		case PT_31_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_31_CONTROL_POINT_PATCHLIST;
		case PT_32_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_32_CONTROL_POINT_PATCHLIST;
		default:
			LOG(core::log_err, L"Unknown primitive type: %u", PrimitiveType);
		};

		return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	}

	struct D3D11CommandContextP
	{
		D3D11DynamicRHI* D3D11RHI = nullptr;
	};

	D3D11CommandContext::D3D11CommandContext(D3D11DynamicRHI* D3D11RHI)
		:Impl(std::make_shared<D3D11CommandContextP>())
	{
		Impl->D3D11RHI = D3D11RHI;
	}

	D3D11CommandContext::~D3D11CommandContext()
	{

	}

	void D3D11CommandContext::SetViewPort(int32_t TopLeftX, int32_t TopLeftY, int32_t SizeX, int32_t SizeY)
	{
		Impl->D3D11RHI->GetStateCache().CurrentNumberOfViewports = 1;

		auto& ViewPort = Impl->D3D11RHI->GetStateCache().CurrentViewport[0];
		ViewPort.Width = static_cast<float>(SizeX);
		ViewPort.Height = static_cast<float>(SizeY);
		ViewPort.MinDepth = 0.0f;
		ViewPort.MaxDepth = 1.0f;
		ViewPort.TopLeftX = static_cast<float>(TopLeftX);
		ViewPort.TopLeftY = static_cast<float>(TopLeftY);

		Impl->D3D11RHI->GetDeviceContext()->RSSetViewports(Impl->D3D11RHI->GetStateCache().CurrentNumberOfViewports, &ViewPort);
	}

	void D3D11CommandContext::SetRenderTarget(std::shared_ptr< RHITexture2D> Tex, std::shared_ptr< RHITexture2D> Depth)
	{
		auto TexRHI = RHIResourceCast(Tex.get());
		auto DepthRHI = RHIResourceCast(Depth.get());
		if (TexRHI)
		{
			auto RenderTargetView = TexRHI->GetRTV();
			Impl->D3D11RHI->GetDeviceContext()->OMSetRenderTargets(1, &RenderTargetView, DepthRHI ? DepthRHI->GetDSV() : nullptr);
		}
		else
		{
			Impl->D3D11RHI->GetDeviceContext()->OMSetRenderTargets(0, nullptr, DepthRHI ? DepthRHI->GetDSV() : nullptr);
		}
	}

	void D3D11CommandContext::SetRenderTarget(std::shared_ptr< RHIRenderTarget> RenderTarget)
	{
		auto RenderTargetRHI = RHIResourceCast(RenderTarget.get());
		if (RenderTargetRHI)
		{
			auto RTV = RenderTargetRHI->GetRTV();
			Impl->D3D11RHI->GetDeviceContext()->OMSetRenderTargets(1, &RTV, RenderTargetRHI->GetDSV());
		}
		else
		{
			Impl->D3D11RHI->GetDeviceContext()->OMSetRenderTargets(0, nullptr, nullptr);
		}
	}

	void D3D11CommandContext::Clear(std::shared_ptr< RHIRenderTarget> RenderTarget, const core::FLinearColor& Color, float Depth /*= 1.0f*/, uint8_t Stencil /*= 0*/)
	{
		auto RenderTargetRHI = RHIResourceCast(RenderTarget.get());
		auto DeviceContex = Impl->D3D11RHI->GetDeviceContext();

		auto RTV = RenderTargetRHI->GetRTV();
		if (RTV != NULL)
		{
			DeviceContex->ClearRenderTargetView(RTV, &Color.R);
		}

		auto DSV = RenderTargetRHI->GetDSV();
		if (DSV != NULL)
		{
			DeviceContex->ClearDepthStencilView(DSV, D3D11_CLEAR_DEPTH, Depth, Stencil);
		}
	}

	void D3D11CommandContext::RHIEndDrawing()
	{
		ClearAllShaderResources();
		ClearState();
	}

	void D3D11CommandContext::RHISetShaderSampler(EShaderFrequency ShaderType, uint32_t SamplerIndex, std::shared_ptr< RHISamplerState> NewState)
	{
		D3D11SamplerState* SamplerStateRHI = RHIResourceCast(NewState.get());
		D3D11StateCacheBase& StateCache = Impl->D3D11RHI->GetStateCache();

		switch (ShaderType)
		{
		case SF_Vertex:
			StateCache.SetSamplerState<SF_Vertex>(SamplerStateRHI->GetNativeSampleState(), SamplerIndex);
			break;
		case SF_Hull:
			StateCache.SetSamplerState<SF_Hull>(SamplerStateRHI->GetNativeSampleState(), SamplerIndex);
			break;
		case SF_Domain:
			StateCache.SetSamplerState<SF_Domain>(SamplerStateRHI->GetNativeSampleState(), SamplerIndex);
			break;
		case SF_Pixel:
			StateCache.SetSamplerState<SF_Pixel>(SamplerStateRHI->GetNativeSampleState(), SamplerIndex);
			break;
		case SF_Geometry:
			StateCache.SetSamplerState<SF_Geometry>(SamplerStateRHI->GetNativeSampleState(), SamplerIndex);
			break;
		case SF_Compute:
			StateCache.SetSamplerState<SF_Compute>(SamplerStateRHI->GetNativeSampleState(), SamplerIndex);
			break;
		default:
			Assert(false);
			break;
		}
		
	}

	void D3D11CommandContext::RHISetRasterizerState(std::shared_ptr<RHIRasterizerState> NewStateRHI)
	{
		D3D11RasterizerState* RasterizerStateRHI = RHIResourceCast(NewStateRHI.get());
		D3D11StateCacheBase& StateCache = Impl->D3D11RHI->GetStateCache();
		StateCache.SetRasterizerState(RasterizerStateRHI->GetNativeRasterizerState());
	}

	void D3D11CommandContext::RHISetBlendState(std::shared_ptr<RHIBlendState> NewState, const core::FLinearColor& BlendFactor)
	{
		D3D11BlendState* BlendStateRHI = RHIResourceCast(NewState.get());
		D3D11StateCacheBase& StateCache = Impl->D3D11RHI->GetStateCache();
		StateCache.SetBlendState(BlendStateRHI->GetNativeBlendState(), (const float*)&BlendFactor, 0xffffffff);
	}

	void D3D11CommandContext::RHISetBlendFactor(const core::FLinearColor& BlendFactor)
	{
		D3D11StateCacheBase& StateCache = Impl->D3D11RHI->GetStateCache();
		StateCache.SetBlendFactor((const float*)&BlendFactor, 0xffffffff);
	}

	void D3D11CommandContext::RHISetDepthStencilState(std::shared_ptr< RHIDepthStencilState> NewState, uint32_t StencilRef)
	{
		D3D11DepthStencilState* DepthStencilStateRHI = RHIResourceCast(NewState.get());
		D3D11StateCacheBase& StateCache = Impl->D3D11RHI->GetStateCache();
		StateCache.SetDepthStencilState(DepthStencilStateRHI->GetNativeDepthStencilState(), StencilRef);
	}

	void D3D11CommandContext::RHISetStencilRef(uint32_t StencilRef)
	{
		D3D11StateCacheBase& StateCache = Impl->D3D11RHI->GetStateCache();
		StateCache.SetStencilRef(StencilRef);
	}

	void D3D11CommandContext::RHISetGraphicsPipelineState(const GraphicsPipelineStateInitializer& Initializer)
	{
		if (Initializer.BlendState)
		{
			RHISetBlendState(Initializer.BlendState, core::FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
		}
		if (Initializer.DepthStencilState)
		{
			RHISetDepthStencilState(Initializer.DepthStencilState, 0);
		}

		if (Initializer.RasterizerState)
		{
			RHISetRasterizerState(Initializer.RasterizerState);
		}

		D3D11StateCacheBase& StateCache = Impl->D3D11RHI->GetStateCache();

		if (Initializer.VertexShader)
		{
			D3D11VertexShader* VertexShaderRHI = RHIResourceCast(Initializer.VertexShader.get());
			StateCache.SetInputLayout(VertexShaderRHI->GetNativeInputLayout());
			StateCache.SetVertexShader(VertexShaderRHI->GetNativeVertexShader());
		}
		else
		{
			StateCache.SetVertexShader(nullptr);
		}

		if (Initializer.PixelShader)
		{
			D3D11PixelShader* PixelShaderRHI = RHIResourceCast(Initializer.PixelShader.get());
			StateCache.SetPixelShader(PixelShaderRHI->GetNativePixelShader());
		}
		else
		{
			StateCache.SetPixelShader(nullptr);
		}

		
		StateCache.SetPrimitiveTopology(GetD3D11PrimitiveType(Initializer.PrimitiveType, false));
	}

	void D3D11CommandContext::RHIUpdateUniformBuffer(std::shared_ptr<RHIUniformBuffer> UniformBufferRHI, const void* Contents)
	{
		// Update the contents of the uniform buffer.
		uint32_t ConstantBufferSize = UniformBufferRHI->GetConstantBufferSize();

		if (ConstantBufferSize > 0)
		{
			auto DeviceContex = Impl->D3D11RHI->GetDeviceContext();
			D3D11UniformBuffer* UniformBuffer = RHIResourceCast(UniformBufferRHI.get());

			D3D11_MAPPED_SUBRESOURCE MappedSubresource;
			// Discard previous results since we always do a full update
			VERIFYD3D11RESULT(DeviceContex->Map(UniformBuffer->GetNativeUniformBuffer(), 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedSubresource));
			Assert(MappedSubresource.RowPitch >= ConstantBufferSize);
			memcpy(MappedSubresource.pData, Contents, ConstantBufferSize);
			DeviceContex->Unmap(UniformBuffer->GetNativeUniformBuffer(), 0);
		}
	}

	void D3D11CommandContext::RHISetShaderTexture(EShaderFrequency ShaderType, uint32_t TextureIndex, std::shared_ptr<RHITexture2D> Texture2DRHI)
	{
		D3D11Texture2D* Texture2D = RHIResourceCast(Texture2DRHI.get());
		D3D11StateCacheBase& StateCache = Impl->D3D11RHI->GetStateCache();
		switch (ShaderType)
		{
		case SF_Vertex:
			StateCache.SetShaderResourceView<SF_Vertex>(Texture2D->GetSRV(), TextureIndex);
			break;
		case SF_Compute:
			StateCache.SetShaderResourceView<SF_Compute>(Texture2D->GetSRV(), TextureIndex);
			break;
		case SF_Pixel:
			StateCache.SetShaderResourceView<SF_Pixel>(Texture2D->GetSRV(), TextureIndex);
			break;

		default:
			break;
		}
	}

	void D3D11CommandContext::RHISetShaderUniformBuffer(EShaderFrequency ShaderType, uint32_t BufferIndex, std::shared_ptr<RHIUniformBuffer> UniformBufferRHI)
	{
		D3D11UniformBuffer* UnifromBuffer = RHIResourceCast(UniformBufferRHI.get());
		D3D11StateCacheBase& StateCache = Impl->D3D11RHI->GetStateCache();
		switch (ShaderType)
		{
		case SF_Vertex:
			StateCache.SetConstantBuffer<SF_Vertex>(UnifromBuffer->GetNativeUniformBuffer(), BufferIndex);
			break;
		case SF_Compute:
			StateCache.SetConstantBuffer<SF_Compute>(UnifromBuffer->GetNativeUniformBuffer(), BufferIndex);
			break;
		case SF_Pixel:
			StateCache.SetConstantBuffer<SF_Pixel>(UnifromBuffer->GetNativeUniformBuffer(), BufferIndex);
			break;

		default:
			break;
		}
	}

	void D3D11CommandContext::DrawPrimitive(std::shared_ptr<RHIVertexBuffer> VertexBufferRHI, std::shared_ptr<RHIIndexBuffer> IndexBufferRHI)
	{
		D3D11VertexBuffer* VertexBuffer = RHIResourceCast(VertexBufferRHI.get());
		D3D11IndexBuffer* IndexBuffer = RHIResourceCast(IndexBufferRHI.get());

		D3D11StateCacheBase& StateCache = Impl->D3D11RHI->GetStateCache();
		StateCache.SetStreamSource(VertexBuffer->GetNativeBuffer(), 0, VertexBuffer->GetStride(), 0);
		StateCache.SetIndexBuffer(IndexBuffer->GetNativeBuffer(), static_cast<DXGI_FORMAT>(IndexBuffer->GetIndexFormat()), 0);
		Impl->D3D11RHI->GetDeviceContext()->DrawIndexed(IndexBuffer->GetIndexCount(), 0, 0);

	}

	void D3D11CommandContext::DrawPrimitive(std::shared_ptr<RHIVertexBuffer> VertexBufferRHI)
	{
		D3D11VertexBuffer* VertexBuffer = RHIResourceCast(VertexBufferRHI.get());
		D3D11StateCacheBase& StateCache = Impl->D3D11RHI->GetStateCache();

		StateCache.SetStreamSource(VertexBuffer->GetNativeBuffer(), 0, VertexBuffer->GetStride(), 0);
		StateCache.SetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
		Impl->D3D11RHI->GetDeviceContext()->Draw(VertexBuffer->GetCount(), 0);

	}

	void D3D11CommandContext::DrawPrimitive(const std::array<std::shared_ptr<RHIVertexBuffer>, VT_Max>& VertexBufferArrayRHI, std::shared_ptr<RHIIndexBuffer> IndexBufferRHI)
	{
		D3D11StateCacheBase& StateCache = Impl->D3D11RHI->GetStateCache();
		int32_t StreamIndex = 0;
		for (const auto& BufferRHI: VertexBufferArrayRHI)
		{
			if (BufferRHI)
			{
				D3D11VertexBuffer* Buffer = RHIResourceCast(BufferRHI.get());
				StateCache.SetStreamSource(Buffer->GetNativeBuffer(), StreamIndex++, Buffer->GetStride(), 0);
			}
		}
		D3D11IndexBuffer* IndexBuffer = RHIResourceCast(IndexBufferRHI.get());
		StateCache.SetIndexBuffer(IndexBuffer->GetNativeBuffer(), static_cast<DXGI_FORMAT>(IndexBuffer->GetIndexFormat()), 0);
		Impl->D3D11RHI->GetDeviceContext()->DrawIndexed(IndexBuffer->GetIndexCount(), 0, 0);
	}

	void D3D11CommandContext::ClearAllShaderResources()
	{
		D3D11StateCacheBase& StateCache = Impl->D3D11RHI->GetStateCache();
		StateCache.ClearConstantBuffers<SF_Vertex>();
		StateCache.ClearConstantBuffers<SF_Hull>();
		StateCache.ClearConstantBuffers<SF_Domain>();
		StateCache.ClearConstantBuffers<SF_Geometry>();
		StateCache.ClearConstantBuffers<SF_Pixel>();
		StateCache.ClearConstantBuffers<SF_Compute>();
	}

	void D3D11CommandContext::ClearState()
	{
		D3D11StateCacheBase& StateCache = Impl->D3D11RHI->GetStateCache();
		StateCache.ClearState();
	}

}