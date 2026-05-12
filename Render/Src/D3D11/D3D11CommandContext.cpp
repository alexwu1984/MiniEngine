#include "D3D11/D3D11CommandContext.h"
#include <d3d11_1.h>
#include "win/com_ptr.h"
#include "RHIPrivate/D3D11RHIPrivate.h"
#include "RHIPrivate/D3D11StateCachePrivate.h"
#include "D3D11/D3D11RHI.h"
#include "D3D11/D3D11ReourceTraits.h"
#include "D3D11/D3D11Texture2D.h"
#include "core/logger.h"
#include "win/high_precision_tick.h"

namespace RenderCore
{
	namespace
	{
		// D3D11 forbids the same subresource from being bound as a UAV and an SRV at the same time (any stage).
		// After raster passes, SceneColor often remains on PS SRV slots while compute binds it as a UAV (e.g. TAA sharpener).
		static void D3D11UnbindShaderResourceViewsUsingResource(ID3D11DeviceContext* ctx, ID3D11Resource* targetResource)
		{
			if (!ctx || !targetResource)
				return;

			win32::com_ptr<ID3D11DeviceContext1> ctx1;
			if (FAILED(ctx->QueryInterface(IID_PPV_ARGS(ctx1.get_init_ref()))))
			{
				ID3D11ShaderResourceView* nullSrvs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT]{};
				ctx->PSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullSrvs);
				return;
			}

			const UINT maxSlots = D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT;
			ID3D11ShaderResourceView* nullSrv = nullptr;

			using GetSrvFn = void (STDMETHODCALLTYPE ID3D11DeviceContext1::*)(UINT, UINT, ID3D11ShaderResourceView**);
			using SetSrvFn = void (STDMETHODCALLTYPE ID3D11DeviceContext::*)(UINT, UINT, ID3D11ShaderResourceView* const*);

			static const GetSrvFn kGetSrv[] = {
				&ID3D11DeviceContext1::VSGetShaderResources,
				&ID3D11DeviceContext1::HSGetShaderResources,
				&ID3D11DeviceContext1::DSGetShaderResources,
				&ID3D11DeviceContext1::GSGetShaderResources,
				&ID3D11DeviceContext1::PSGetShaderResources,
				&ID3D11DeviceContext1::CSGetShaderResources,
			};
			static const SetSrvFn kSetSrv[] = {
				&ID3D11DeviceContext::VSSetShaderResources,
				&ID3D11DeviceContext::HSSetShaderResources,
				&ID3D11DeviceContext::DSSetShaderResources,
				&ID3D11DeviceContext::GSSetShaderResources,
				&ID3D11DeviceContext::PSSetShaderResources,
				&ID3D11DeviceContext::CSSetShaderResources,
			};

			for (int stage = 0; stage < 6; ++stage)
			{
				for (UINT slot = 0; slot < maxSlots; ++slot)
				{
					ID3D11ShaderResourceView* srv = nullptr;
					(ctx1.get()->*kGetSrv[stage])(slot, 1, &srv);
					if (!srv)
						continue;
					ID3D11Resource* resFromSrv = nullptr;
					srv->GetResource(&resFromSrv);
					srv->Release();
					if (resFromSrv == targetResource)
						(ctx->*kSetSrv[stage])(slot, 1, &nullSrv);
					if (resFromSrv)
						resFromSrv->Release();
				}
			}
		}
	}

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

	class FD3D11GpuPassTimestamps
	{
	public:
		static constexpr uint32_t kRing = 4;
		static constexpr uint32_t kMaxQ = 160;

		explicit FD3D11GpuPassTimestamps(ID3D11Device* InDev)
			: Dev(InDev)
		{
		}

		void Destroy()
		{
			DisjointQueries.clear();
			TsQueries.clear();
			bReady = false;
			bInBatch = false;
			bAwaitingCpuRead = false;
			RecordingSeq = 0;
			Awaiting = {};
			NamesThisFrame.clear();
		}

		void LazyCreate()
		{
			if (bReady || !Dev)
				return;
			DisjointQueries.resize(kRing);
			TsQueries.resize(kRing * kMaxQ);
			for (uint32_t S = 0; S < kRing; ++S)
			{
				D3D11_QUERY_DESC Qd = { D3D11_QUERY_TIMESTAMP_DISJOINT, 0 };
				if (FAILED(Dev->CreateQuery(&Qd, DisjointQueries[S].get_init_ref())))
					return;
			}
			for (uint32_t I = 0; I < kRing * kMaxQ; ++I)
			{
				D3D11_QUERY_DESC Qd = { D3D11_QUERY_TIMESTAMP, 0 };
				if (FAILED(Dev->CreateQuery(&Qd, TsQueries[I].get_init_ref())))
					return;
			}
			bReady = true;
		}

		void BeginRecording(ID3D11DeviceContext* Ctx)
		{
			if (!Ctx || !Dev)
				return;
			LazyCreate();
			if (!bReady)
				return;

			RecordingSeq++;
			ActiveSlot = static_cast<uint32_t>(RecordingSeq % kRing);
			BaseIdx = ActiveSlot * kMaxQ;
			NumWritten = 0;
			NamesThisFrame.clear();

			Ctx->Begin(DisjointQueries[ActiveSlot].get());
			Ctx->End(TsQueries[BaseIdx].get());
			NumWritten = 1;
			bInBatch = true;
		}

		void AfterPass(ID3D11DeviceContext* Ctx, const char* NameUtf8)
		{
			if (!Ctx || !bReady || !bInBatch)
				return;
			if (NumWritten >= kMaxQ)
				return;
			Ctx->End(TsQueries[BaseIdx + NumWritten].get());
			NamesThisFrame.emplace_back(NameUtf8 ? NameUtf8 : "");
			NumWritten++;
		}

		void EndRecording(ID3D11DeviceContext* Ctx)
		{
			if (!Ctx || !bReady || !bInBatch)
				return;
			Ctx->End(DisjointQueries[ActiveSlot].get());
			bInBatch = false;

			Awaiting.NumQueries = NumWritten;
			Awaiting.BaseIdx = BaseIdx;
			Awaiting.Names = NamesThisFrame;
			Awaiting.DisjointSlot = ActiveSlot;
			bAwaitingCpuRead = true;
		}

		void TryConsume(ID3D11DeviceContext* Ctx, std::vector<std::pair<std::string, double>>& Out)
		{
			Out.clear();
			if (!Ctx || !bReady || !bAwaitingCpuRead)
				return;
			if (Awaiting.DisjointSlot >= kRing)
				return;

			D3D11_QUERY_DATA_TIMESTAMP_DISJOINT Dj = {};
			HRESULT Hr = Ctx->GetData(DisjointQueries[Awaiting.DisjointSlot].get(), &Dj, sizeof(Dj), D3D11_ASYNC_GETDATA_DONOTFLUSH);
			if (Hr == S_FALSE)
				return;
			if (FAILED(Hr) || Dj.Disjoint)
			{
				bAwaitingCpuRead = false;
				return;
			}

			const double MsPerTick = 1000.0 / double(Dj.Frequency);
			std::vector<uint64_t> Ticks(Awaiting.NumQueries);
			for (uint32_t I = 0; I < Awaiting.NumQueries; ++I)
			{
				Hr = Ctx->GetData(TsQueries[Awaiting.BaseIdx + I].get(), &Ticks[I], sizeof(uint64_t), D3D11_ASYNC_GETDATA_DONOTFLUSH);
				if (Hr == S_FALSE)
					return;
				if (FAILED(Hr))
				{
					bAwaitingCpuRead = false;
					return;
				}
			}

			bAwaitingCpuRead = false;

			if (Awaiting.NumQueries < 2 || Awaiting.Names.size() + 1 != Awaiting.NumQueries)
				return;

			uint64_t Prev = Ticks[0];
			for (uint32_t I = 0; I < Awaiting.Names.size(); ++I)
			{
				const uint64_t Cur = Ticks[I + 1];
				const uint64_t D = (Cur >= Prev) ? (Cur - Prev) : 0ull;
				Out.emplace_back(Awaiting.Names[I], double(D) * MsPerTick);
				Prev = Cur;
			}
		}

	private:
		ID3D11Device* Dev = nullptr;
		bool bReady = false;
		bool bInBatch = false;
		bool bAwaitingCpuRead = false;
		uint64_t RecordingSeq = 0;
		uint32_t ActiveSlot = 0;
		uint32_t BaseIdx = 0;
		uint32_t NumWritten = 0;
		std::vector<std::string> NamesThisFrame;

		std::vector<win32::com_ptr<ID3D11Query>> DisjointQueries;
		std::vector<win32::com_ptr<ID3D11Query>> TsQueries;

		struct FAwaiting
		{
			uint32_t NumQueries = 0;
			uint32_t BaseIdx = 0;
			uint32_t DisjointSlot = 0;
			std::vector<std::string> Names;
		};
		FAwaiting Awaiting;
	};

	struct D3D11CommandContextP
	{
		D3D11DynamicRHI* D3D11RHI = nullptr;
		std::unique_ptr<FD3D11GpuPassTimestamps> GpuPassTimestamps;
	};

	D3D11CommandContext::D3D11CommandContext(D3D11DynamicRHI* D3D11RHI)
		:Impl(std::make_shared<D3D11CommandContextP>())
	{
		Impl->D3D11RHI = D3D11RHI;
	}

	D3D11CommandContext::~D3D11CommandContext()
	{
		if (Impl && Impl->GpuPassTimestamps)
			Impl->GpuPassTimestamps->Destroy();
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

	void D3D11CommandContext::SetRenderTarget(std::shared_ptr< RHIRenderTarget> RenderTarget, int32_t IndexMip )
	{
		auto RenderTargetRHI = RHIResourceCast(RenderTarget.get());
		if (RenderTargetRHI)
		{
			auto& RTVS = RenderTargetRHI->GetRTVS();
			const auto mipIt = RTVS.find(static_cast<uint32_t>(IndexMip));
			if (mipIt != RTVS.end() && !mipIt->second.empty() && mipIt->second[0])
			{
				auto RTV = mipIt->second[0];
				Impl->D3D11RHI->GetDeviceContext()->OMSetRenderTargets(1, &RTV, RenderTargetRHI->GetDSV());
			}
			else if (RenderTargetRHI->GetDSV())
			{
				Impl->D3D11RHI->GetDeviceContext()->OMSetRenderTargets(0, nullptr, RenderTargetRHI->GetDSV());
			}
			else
			{
				Impl->D3D11RHI->GetDeviceContext()->OMSetRenderTargets(0, nullptr, nullptr);
			}
		}
		else
		{
			Impl->D3D11RHI->GetDeviceContext()->OMSetRenderTargets(0, nullptr, nullptr);
		}
	}

	void D3D11CommandContext::SetRenderTarget(std::shared_ptr< RHITextureCube> TextureCube, int32_t IndexView, int32_t IndexMip)
	{
		auto TextureCubeRHI = RHIResourceCast(TextureCube.get());
		if (TextureCubeRHI)
		{
			auto CubeRRVS = TextureCubeRHI->GetRTVS();
			ID3D11DepthStencilView* dsv = nullptr;
			if (const auto dt = TextureCubeRHI->GetDepthTex())
			{
				if (ID3D11DepthStencilView* faceDsv = dt->GetCubeFaceDSV(static_cast<uint32_t>(IndexView)))
					dsv = faceDsv;
				else
					dsv = dt->GetDSV();
			}
			auto mipIt = CubeRRVS.find(static_cast<uint32_t>(IndexMip));
			const bool hasRtv = mipIt != CubeRRVS.end() && IndexView >= 0 && static_cast<size_t>(IndexView) < mipIt->second.size() && mipIt->second[static_cast<size_t>(IndexView)];
			if (hasRtv)
			{
				ID3D11RenderTargetView* rtv = mipIt->second[static_cast<size_t>(IndexView)].get();
				Impl->D3D11RHI->GetDeviceContext()->OMSetRenderTargets(1, &rtv, dsv);
			}
			else if (dsv)
				Impl->D3D11RHI->GetDeviceContext()->OMSetRenderTargets(0, nullptr, dsv);
			else
				Impl->D3D11RHI->GetDeviceContext()->OMSetRenderTargets(0, nullptr, nullptr);
		}
		else
		{
			Impl->D3D11RHI->GetDeviceContext()->OMSetRenderTargets(0, nullptr, nullptr);
		}
	}

	void D3D11CommandContext::SetRenderTarget(const std::vector<std::shared_ptr<RHITexture2D>>& Targets, std::shared_ptr< RHITexture2D> Depth)
	{
		auto DepthRHI = RHIResourceCast(Depth.get());
		std::vector<ID3D11RenderTargetView*> D3D11TargetViews;
		for (auto Target: Targets)
		{
			auto RenderTargetRHI = RHIResourceCast(Target.get());
			if (RenderTargetRHI && RenderTargetRHI->GetRTV())
			{
				D3D11TargetViews.emplace_back(RenderTargetRHI->GetRTV());
			}
		}
		Impl->D3D11RHI->GetDeviceContext()->OMSetRenderTargets((uint32_t)D3D11TargetViews.size(), D3D11TargetViews.data(), DepthRHI ? DepthRHI->GetDSV() : nullptr);
	}

	void D3D11CommandContext::Clear(std::shared_ptr< RHITextureCube> TextureCube, int32_t Face, int32_t Mip, const core::FLinearColor& Color, float Depth /*= 1.0f*/, uint8_t Stencil /*= 0*/)
	{
		auto TextureCubeRHI = RHIResourceCast(TextureCube.get());
		auto DeviceContex = Impl->D3D11RHI->GetDeviceContext();

		if (TextureCubeRHI)
		{
			if (const auto dt = TextureCubeRHI->GetDepthTex())
			{
				if (ID3D11DepthStencilView* faceDsv = dt->GetCubeFaceDSV(static_cast<uint32_t>(Face)))
				{
					DeviceContex->ClearDepthStencilView(faceDsv, D3D11_CLEAR_DEPTH, Depth, Stencil);
					return;
				}
			}
			auto& CubeRRVS = TextureCubeRHI->GetRTVS();
			const auto mipIt = CubeRRVS.find(static_cast<uint32_t>(Mip));
			if (mipIt != CubeRRVS.end() && Face >= 0 && static_cast<size_t>(Face) < mipIt->second.size() && mipIt->second[static_cast<size_t>(Face)])
				DeviceContex->ClearRenderTargetView(mipIt->second[static_cast<size_t>(Face)].get(), &Color.R);

		}
	}

	void D3D11CommandContext::Clear(std::shared_ptr< RHIRenderTarget> RenderTarget, const core::FLinearColor& Color, float Depth /*= 1.0f*/, uint8_t Stencil /*= 0*/)
	{
		auto TextureRHI = RHIResourceCast(RenderTarget.get());
		auto DeviceContex = Impl->D3D11RHI->GetDeviceContext();

		auto RTV = TextureRHI->GetRTV();
		if (RTV != nullptr)
		{
			DeviceContex->ClearRenderTargetView(RTV, &Color.R);
		}

		auto DSV = TextureRHI->GetDSV();
		if (DSV != nullptr)
		{
			DeviceContex->ClearDepthStencilView(DSV, D3D11_CLEAR_DEPTH, Depth, Stencil);
		}
	}

	void D3D11CommandContext::Clear(std::shared_ptr< RHITexture2D> RenderTarget, std::shared_ptr<RHITexture2D> DepthTarget, const core::FLinearColor& Color, float Depth /*= 1.0f*/, uint8_t Stencil /*= 0*/)
	{
		auto RenderTargetRHI = RHIResourceCast(RenderTarget.get());
		auto DeviceContex = Impl->D3D11RHI->GetDeviceContext();

		auto RTV = RenderTargetRHI->GetRTV();
		if (RTV != nullptr)
		{
			DeviceContex->ClearRenderTargetView(RTV, &Color.R);
		}

		if (DepthTarget)
		{
			auto DepthTargetRHI = RHIResourceCast(DepthTarget.get());
			auto DSV = DepthTargetRHI->GetDSV();
			if (DSV != nullptr)
			{
				DeviceContex->ClearDepthStencilView(DSV, D3D11_CLEAR_DEPTH, Depth, Stencil);
			}
		}

	}

	void D3D11CommandContext::Clear(std::vector<std::shared_ptr<RHITexture2D>> Targets, std::shared_ptr<RHITexture2D> DepthTarget, const core::FLinearColor& Color, float Depth /*= 1.0f*/, uint8_t Stencil /*= 0*/)
	{
		auto DeviceContex = Impl->D3D11RHI->GetDeviceContext();
		auto DepthRHI = RHIResourceCast(DepthTarget.get());
		for (auto Target : Targets)
		{
			auto RenderTargetRHI = RHIResourceCast(Target.get());
			if (RenderTargetRHI && RenderTargetRHI->GetRTV())
			{
				DeviceContex->ClearRenderTargetView(RenderTargetRHI->GetRTV(), &Color.R);
			}
		}

		auto DepthTargetRHI = RHIResourceCast(DepthTarget.get());
		auto DSV = DepthTargetRHI->GetDSV();
		if (DSV != nullptr)
		{
			DeviceContex->ClearDepthStencilView(DSV, D3D11_CLEAR_DEPTH, Depth, Stencil);
		}
	}

	void D3D11CommandContext::RHIEndDrawing()
	{
		win32::RecordPresentFrameForFpsLog();
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
		if (!UniformBufferRHI || !Contents)
			return;
		// Update the contents of the uniform buffer.
		uint32_t ConstantBufferSize = UniformBufferRHI->GetConstantBufferSize();

		if (ConstantBufferSize > 0)
		{
			auto DeviceContex = Impl->D3D11RHI->GetDeviceContext();
			D3D11UniformBuffer* UniformBuffer = RHIResourceCast(UniformBufferRHI.get());

			D3D11_MAPPED_SUBRESOURCE MappedSubresource;
			// Discard previous results since we always do a full update
			VERIFYD3DRESULT(DeviceContex->Map(UniformBuffer->GetNativeUniformBuffer(), 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedSubresource));
			Assert(MappedSubresource.RowPitch >= ConstantBufferSize);
			memcpy(MappedSubresource.pData, Contents, ConstantBufferSize);
			DeviceContex->Unmap(UniformBuffer->GetNativeUniformBuffer(), 0);
		}
	}

	void D3D11CommandContext::RHISetShaderTexture(EShaderFrequency ShaderType, uint32_t TextureIndex, std::shared_ptr<RHITexture2D> Texture2DRHI)
	{
		D3D11Texture2D* Texture2D = RHIResourceCast(Texture2DRHI.get());
		D3D11StateCacheBase& StateCache = Impl->D3D11RHI->GetStateCache();
		if (!Texture2D)
		{
			ID3D11ShaderResourceView* NullSrv = nullptr;
			switch (ShaderType)
			{
			case SF_Vertex:
				StateCache.SetShaderResourceView<SF_Vertex>(NullSrv, TextureIndex);
				break;
			case SF_Compute:
				StateCache.SetShaderResourceView<SF_Compute>(NullSrv, TextureIndex);
				break;
			case SF_Pixel:
				StateCache.SetShaderResourceView<SF_Pixel>(NullSrv, TextureIndex);
				break;
			default:
				break;
			}
			return;
		}
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

	void D3D11CommandContext::RHISetShaderTexture(EShaderFrequency ShaderType, uint32_t TextureIndex, std::shared_ptr<RHITextureCube> TextureCubeRHI)
	{
		D3D11TextureCube* TextureCube = RHIResourceCast(TextureCubeRHI.get());
		if (!TextureCube)
		{
			return;
		}
		D3D11StateCacheBase& StateCache = Impl->D3D11RHI->GetStateCache();
		switch (ShaderType)
		{
		case SF_Vertex:
			StateCache.SetShaderResourceView<SF_Vertex>(TextureCube->GetSRV(), TextureIndex);
			break;
		case SF_Compute:
			StateCache.SetShaderResourceView<SF_Compute>(TextureCube->GetSRV(), TextureIndex);
			break;
		case SF_Pixel:
			StateCache.SetShaderResourceView<SF_Pixel>(TextureCube->GetSRV(), TextureIndex);
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

	void D3D11CommandContext::RHISetShaderStructuredBuffer(EShaderFrequency ShaderType, uint32_t SRVIndex, std::shared_ptr<RHIStructuredBuffer> BufferRHI)
	{
		D3D11StructuredBuffer* StructuredBuffer = RHIResourceCast(BufferRHI.get());
		ID3D11ShaderResourceView* SRV = StructuredBuffer ? StructuredBuffer->GetSRV() : nullptr;
		D3D11StateCacheBase& StateCache = Impl->D3D11RHI->GetStateCache();
		switch (ShaderType)
		{
		case SF_Vertex:
			StateCache.SetShaderResourceView<SF_Vertex>(SRV, SRVIndex);
			break;
		case SF_Compute:
			StateCache.SetShaderResourceView<SF_Compute>(SRV, SRVIndex);
			break;
		case SF_Pixel:
			StateCache.SetShaderResourceView<SF_Pixel>(SRV, SRVIndex);
			break;
		default:
			break;
		}
	}

	void D3D11CommandContext::RHISetGraphicsRoot32BitConstants(uint32_t /*RootParameterIndex*/, uint32_t /*Num32BitValues*/, const void* /*SrcData*/, uint32_t /*DestOffsetIn32BitValues*/)
	{
	}

	void D3D11CommandContext::RHISetUAVParameter(uint32_t UAVIndex, std::shared_ptr<RHIUnorderedAccessView> UAV)
	{
		D3D11UnorderedAccessView* UAVRHI = RHIResourceCast(UAV.get());
		if (UAVRHI)
		{
			win32::com_ptr<ID3D11UnorderedAccessView> D3D11UAV = UAVRHI->GetNativeUAV();
			win32::com_ptr<ID3D11Resource> uavResource;
			if (D3D11UAV)
				D3D11UAV->GetResource(uavResource.get_init_ref());
			ID3D11DeviceContext* const ctx = Impl->D3D11RHI->GetDeviceContext();
			if (uavResource)
			{
#if D3D11_ALLOW_STATE_CACHE
				Impl->D3D11RHI->GetStateCache().UnbindShaderResourceViewsBoundToResource(uavResource.get());
#else
				D3D11UnbindShaderResourceViewsUsingResource(ctx, uavResource.get());
#endif
			}
			uint32_t InitialCount = -1;
			ID3D11UnorderedAccessView* pUav = D3D11UAV.get();
			ctx->CSSetUnorderedAccessViews(UAVIndex, 1, &pUav, &InitialCount);
		}
	}

	void D3D11CommandContext::RHISetShaderStructuredBufferUAV(uint32_t UAVIndex, std::shared_ptr<RHIStructuredBuffer> BufferRHI)
	{
		D3D11StructuredBuffer* Buffer = RHIResourceCast(BufferRHI.get());
		ID3D11UnorderedAccessView* UAV = (Buffer && Buffer->HasUAV()) ? Buffer->GetUAV() : nullptr;
		ID3D11DeviceContext* const ctx = Impl->D3D11RHI->GetDeviceContext();
		if (!ctx)
			return;
		// Drop any cached PS / VS / CS SRV bindings to the same underlying buffer before this UAV slot picks it up;
		// D3D11 will otherwise emit warning #3146080 (DEVICE_*_SETSHADERRESOURCES_HAZARD) on the next draw.
		if (UAV)
		{
			win32::com_ptr<ID3D11Resource> uavResource;
			UAV->GetResource(uavResource.get_init_ref());
			if (uavResource)
			{
#if D3D11_ALLOW_STATE_CACHE
				Impl->D3D11RHI->GetStateCache().UnbindShaderResourceViewsBoundToResource(uavResource.get());
#else
				D3D11UnbindShaderResourceViewsUsingResource(ctx, uavResource.get());
#endif
			}
		}
		uint32_t InitialCount = static_cast<uint32_t>(-1);
		ctx->CSSetUnorderedAccessViews(UAVIndex, 1, &UAV, &InitialCount);
	}

	void D3D11CommandContext::DrawPrimitive(std::shared_ptr<RHIVertexBuffer> VertexBufferRHI, std::shared_ptr<RHIIndexBuffer> IndexBufferRHI)
	{
		DrawPrimitiveInstanced(VertexBufferRHI, IndexBufferRHI, 1u, 0u);
	}

	void D3D11CommandContext::DrawPrimitiveInstanced(std::shared_ptr<RHIVertexBuffer> VertexBufferRHI, std::shared_ptr<RHIIndexBuffer> IndexBufferRHI, uint32_t InstanceCount, uint32_t StartInstanceLocation)
	{
		if (InstanceCount == 0)
			return;

		D3D11VertexBuffer* VertexBuffer = RHIResourceCast(VertexBufferRHI.get());
		D3D11IndexBuffer* IndexBuffer = RHIResourceCast(IndexBufferRHI.get());
		if (!VertexBuffer || !IndexBuffer)
			return;

		D3D11StateCacheBase& StateCache = Impl->D3D11RHI->GetStateCache();
		StateCache.SetStreamSource(VertexBuffer->GetNativeBuffer(), 0, VertexBuffer->GetStride(), 0);
		StateCache.SetIndexBuffer(IndexBuffer->GetNativeBuffer(), static_cast<DXGI_FORMAT>(IndexBuffer->GetIndexFormat()), 0);
		Impl->D3D11RHI->GetDeviceContext()->DrawIndexedInstanced(IndexBuffer->GetIndexCount(), InstanceCount, 0, 0, StartInstanceLocation);
	}

	void D3D11CommandContext::DrawPrimitive(std::shared_ptr<RHIVertexBuffer> VertexBufferRHI)
	{
		if (!VertexBufferRHI)
			return;
		D3D11VertexBuffer* VertexBuffer = RHIResourceCast(VertexBufferRHI.get());
		if (!VertexBuffer)
			return;
		DrawPrimitive(VertexBufferRHI, static_cast<uint32_t>(VertexBuffer->GetCount()), 0u);
	}

	void D3D11CommandContext::DrawPrimitive(std::shared_ptr<RHIVertexBuffer> VertexBufferRHI, uint32_t VertexCount, uint32_t StartVertexLocation)
	{
		if (VertexCount == 0u)
			return;
		D3D11VertexBuffer* VertexBuffer = RHIResourceCast(VertexBufferRHI.get());
		if (!VertexBuffer)
			return;
		D3D11StateCacheBase& StateCache = Impl->D3D11RHI->GetStateCache();

		StateCache.SetStreamSource(VertexBuffer->GetNativeBuffer(), 0, VertexBuffer->GetStride(), 0);
		StateCache.SetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
		Impl->D3D11RHI->GetDeviceContext()->Draw(VertexCount, StartVertexLocation);
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
		if (!IndexBuffer)
		{
			return;
		}
		for (uint32_t ClearSlot = static_cast<uint32_t>(StreamIndex); ClearSlot < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; ++ClearSlot)
			StateCache.SetStreamSource(nullptr, ClearSlot, 0, 0);
		StateCache.SetIndexBuffer(IndexBuffer->GetNativeBuffer(), static_cast<DXGI_FORMAT>(IndexBuffer->GetIndexFormat()), 0);
		Impl->D3D11RHI->GetDeviceContext()->DrawIndexed(IndexBuffer->GetIndexCount(), 0, 0);
	}

	void D3D11CommandContext::DrawPrimitiveInstanced(const std::array<std::shared_ptr<RHIVertexBuffer>, VT_Max>& VertexBufferArrayRHI, std::shared_ptr<RHIIndexBuffer> IndexBufferRHI, uint32_t InstanceCount,
													 uint32_t StartInstanceLocation)
	{
		if (InstanceCount == 0)
			return;

		D3D11StateCacheBase& StateCache = Impl->D3D11RHI->GetStateCache();
		int32_t StreamIndex = 0;
		for (const auto& BufferRHI : VertexBufferArrayRHI)
		{
			if (BufferRHI)
			{
				D3D11VertexBuffer* Buffer = RHIResourceCast(BufferRHI.get());
				StateCache.SetStreamSource(Buffer->GetNativeBuffer(), StreamIndex++, Buffer->GetStride(), 0);
			}
		}
		D3D11IndexBuffer* IndexBuffer = RHIResourceCast(IndexBufferRHI.get());
		if (!IndexBuffer)
			return;
		for (uint32_t ClearSlot = static_cast<uint32_t>(StreamIndex); ClearSlot < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; ++ClearSlot)
			StateCache.SetStreamSource(nullptr, ClearSlot, 0, 0);
		StateCache.SetIndexBuffer(IndexBuffer->GetNativeBuffer(), static_cast<DXGI_FORMAT>(IndexBuffer->GetIndexFormat()), 0);
		Impl->D3D11RHI->GetDeviceContext()->DrawIndexedInstanced(IndexBuffer->GetIndexCount(), InstanceCount, 0, 0, StartInstanceLocation);
	}

	void D3D11CommandContext::Draw(uint32_t VertexCount, uint32_t VertexStartOffset /*= 0*/)
	{
		D3D11StateCacheBase& StateCache = Impl->D3D11RHI->GetStateCache();

		StateCache.SetStreamSource(nullptr, 0, 0, 0);
		StateCache.SetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
		Impl->D3D11RHI->GetDeviceContext()->Draw(VertexCount, VertexStartOffset);
	}

	void D3D11CommandContext::GenerateMips(std::shared_ptr<RHITextureCube> TextureCubeRHI)
	{
		D3D11TextureCube* TextureCube = RHIResourceCast(TextureCubeRHI.get());
		if (TextureCube)
		{
			Impl->D3D11RHI->GetDeviceContext()->GenerateMips(TextureCube->GetSRV());
		}
	}

	void D3D11CommandContext::RHISetComputePipelineState(const ComputePipelineStateInitializer& Initializer)
	{
		D3D11StateCacheBase& StateCache = Impl->D3D11RHI->GetStateCache();
		if (Initializer.ComputeShader)
		{
			D3D11ComputeShader* ComputeShader = RHIResourceCast(Initializer.ComputeShader.get());
			StateCache.SetComputeShader(ComputeShader->GetNativeComputeShader());
		}
		else
		{
			StateCache.SetComputeShader(nullptr);
		}
	}

	void D3D11CommandContext::RHIDispatchComputeShader(uint32_t ThreadGroupCountX, uint32_t ThreadGroupCountY, uint32_t ThreadGroupCountZ)
	{
		D3D11StateCacheBase& StateCache = Impl->D3D11RHI->GetStateCache();
		Impl->D3D11RHI->GetDeviceContext()->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
		for (int32_t slot = 0; slot < 8; ++slot)
		{
			ID3D11UnorderedAccessView* nullUAV = nullptr;
			Impl->D3D11RHI->GetDeviceContext()->CSSetUnorderedAccessViews(slot, 1, &nullUAV, nullptr);
		}

		StateCache.SetComputeShader(nullptr);
	}

	void D3D11CommandContext::RHICopyResource(std::shared_ptr< RHITexture2D> DstTex, std::shared_ptr< RHITexture2D> SrcTex)
	{
		D3D11Texture2D* DstTexRHI = RHIResourceCast(DstTex.get());
		D3D11Texture2D* SrcTexRHI = RHIResourceCast(SrcTex.get());

		Impl->D3D11RHI->GetDeviceContext()->CopyResource(DstTexRHI->GetNativeTex(), SrcTexRHI->GetNativeTex());
		Impl->D3D11RHI->GetDeviceContext()->Flush();
	}

	void D3D11CommandContext::RHICopyResource2D(std::shared_ptr< RHITexture2D> DstTex, std::shared_ptr< RHITexture2D> SrcTex, core::vec4u rect)
	{
		D3D11Texture2D* DstTexRHI = RHIResourceCast(DstTex.get());
		D3D11Texture2D* SrcTexRHI = RHIResourceCast(SrcTex.get());
		D3D11_BOX srcBox;
		srcBox.left = rect.left();
		srcBox.top = rect.top();
		srcBox.front = 0;
		srcBox.right = rect.right();
		srcBox.bottom = rect.bottom();
		srcBox.back = 1;

		Impl->D3D11RHI->GetDeviceContext()->CopySubresourceRegion(
			DstTexRHI->GetNativeTex(),
			0,    
			0, 0, 0,  
			SrcTexRHI->GetNativeTex(),
			0, 
			&srcBox
		);
	}

	void D3D11CommandContext::RDGApplyPassBeginBarriers(const FRDGTextureBarrierDesc* Items, size_t Count, ERDGPassQueue PassQueue)
	{
		(void)Items;
		(void)Count;
		(void)PassQueue;
	}

	void D3D11CommandContext::RDGBeginGpuPassTimingFrame()
	{
		if (!Impl->D3D11RHI || !Impl->D3D11RHI->GetDevice())
			return;
		if (!Impl->GpuPassTimestamps)
			Impl->GpuPassTimestamps = std::make_unique<FD3D11GpuPassTimestamps>(Impl->D3D11RHI->GetDevice());
		Impl->GpuPassTimestamps->BeginRecording(Impl->D3D11RHI->GetDeviceContext());
	}

	void D3D11CommandContext::RDGWriteGpuTimestampAfterPass(const char* PassNameUtf8)
	{
		if (!Impl->GpuPassTimestamps || !Impl->D3D11RHI)
			return;
		Impl->GpuPassTimestamps->AfterPass(Impl->D3D11RHI->GetDeviceContext(), PassNameUtf8);
	}

	void D3D11CommandContext::RDGResolveGpuPassTimingsEndOfFrame()
	{
		if (!Impl->GpuPassTimestamps || !Impl->D3D11RHI)
			return;
		Impl->GpuPassTimestamps->EndRecording(Impl->D3D11RHI->GetDeviceContext());
	}

	void D3D11CommandContext::RDGTryConsumePreviousFrameGpuPassTimings(std::vector<std::pair<std::string, double>>& OutPassGpuMs)
	{
		if (!Impl->GpuPassTimestamps || !Impl->D3D11RHI)
			return;
		Impl->GpuPassTimestamps->TryConsume(Impl->D3D11RHI->GetDeviceContext(), OutPassGpuMs);
	}

	bool D3D11CommandContext::UpdateTileMappings(std::shared_ptr< RHITilePool> TilePool, std::shared_ptr< RHITexture2D> TexRHI)
	{
		return TilePool->UpdateTileMappings(TexRHI);
	}

	void D3D11CommandContext::UpdateTiles(std::shared_ptr< RHITilePool> TilePool, std::shared_ptr< RHITexture2D> TexRHI, std::shared_ptr<uint8_t> Data)
	{
		int32_t SizeX = 0;
		int32_t SizeY = 0;
		TilePool->UpdateTiles(TexRHI,Data);
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

	void D3D11CommandContext::RHIClearState()
	{
		ClearState();
	}
}