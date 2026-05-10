#include "Render/SceneTextures.h"
#include "Render/RenderTexturePool.h"
#include "RHI/RHITexture2D.h"
#include "RHI/RHIUnorderedAccessView.h"
#include "RHI/DynamicRHI.h"

using namespace RenderCore;

namespace Engine
{
	struct FSceneTexturesPrivate
	{
		DynamicRHI* RHI = nullptr;
		std::shared_ptr<RHITexture2D> Depth;
		std::shared_ptr<RHITexture2D> SceneColor;
		std::shared_ptr<RHITexture2D> SceneColorWithBloom;
		std::shared_ptr<RHITexture2D> SceneColorWithSSR;
		std::shared_ptr<RHIUnorderedAccessView> SceneColorUAV;
		std::shared_ptr<RHITexture2D> MotionVector;
		std::shared_ptr<RHITexture2D> NormalBuffer;
		std::shared_ptr<RHITexture2D> EmissiveBuffer;
		std::shared_ptr<RHITexture2D> MetallicSpecularRoughness;
		std::shared_ptr<RHITexture2D> MaterialAux;
		std::shared_ptr<RHITexture2D> SceneColorPreLighting;
	};

	namespace
	{
		void ReleaseTex2DToPool(std::shared_ptr<RHITexture2D>& Tex, EPixelFormat Format, int32_t CreateFlags, uint32_t NumMips)
		{
			if (!Tex)
				return;
			const core::vec2i Sz = Tex->GetSize();
			RenderTexturePool::Get().ReleaseTexture2D(Format, CreateFlags, Sz.x, Sz.y, NumMips, std::move(Tex));
			Tex.reset();
		}

		void ReleaseAllSceneTexturesResources(FSceneTexturesPrivate* d)
		{
			if (!d)
				return;
			d->SceneColorUAV.reset();

			ReleaseTex2DToPool(d->Depth, EPixelFormat::PF_ShadowDepth,
							   static_cast<int32_t>(ETextureCreateFlags::TexCreate_DepthStencilTargetable | ETextureCreateFlags::TexCreate_ShaderResource), 1);
			ReleaseTex2DToPool(d->MotionVector, EPixelFormat::PF_FloatRGBA,
							   static_cast<int32_t>(ETextureCreateFlags::TexCreate_RenderTargetable | ETextureCreateFlags::TexCreate_ShaderResource), 1);
			ReleaseTex2DToPool(d->SceneColor, EPixelFormat::PF_FloatRGBA,
							   static_cast<int32_t>(ETextureCreateFlags::TexCreate_RenderTargetable | ETextureCreateFlags::TexCreate_ShaderResource
													| ETextureCreateFlags::TexCreate_UAV),
							   1);
			ReleaseTex2DToPool(d->SceneColorWithBloom, EPixelFormat::PF_FloatRGBA,
							   static_cast<int32_t>(ETextureCreateFlags::TexCreate_RenderTargetable | ETextureCreateFlags::TexCreate_ShaderResource), 1);
			ReleaseTex2DToPool(d->SceneColorWithSSR, EPixelFormat::PF_FloatRGBA,
							   static_cast<int32_t>(ETextureCreateFlags::TexCreate_RenderTargetable | ETextureCreateFlags::TexCreate_ShaderResource), 1);
			ReleaseTex2DToPool(d->NormalBuffer, EPixelFormat::PF_FloatRGBA,
							   static_cast<int32_t>(ETextureCreateFlags::TexCreate_RenderTargetable | ETextureCreateFlags::TexCreate_ShaderResource), 1);
			ReleaseTex2DToPool(d->EmissiveBuffer, EPixelFormat::PF_FloatRGBA,
							   static_cast<int32_t>(ETextureCreateFlags::TexCreate_RenderTargetable | ETextureCreateFlags::TexCreate_ShaderResource), 1);
			ReleaseTex2DToPool(d->MetallicSpecularRoughness, EPixelFormat::PF_FloatRGBA,
							   static_cast<int32_t>(ETextureCreateFlags::TexCreate_RenderTargetable | ETextureCreateFlags::TexCreate_ShaderResource), 1);
			ReleaseTex2DToPool(d->MaterialAux, EPixelFormat::PF_FloatRGBA,
							   static_cast<int32_t>(ETextureCreateFlags::TexCreate_RenderTargetable | ETextureCreateFlags::TexCreate_ShaderResource), 1);
			ReleaseTex2DToPool(d->SceneColorPreLighting, EPixelFormat::PF_FloatRGBA,
							   static_cast<int32_t>(ETextureCreateFlags::TexCreate_RenderTargetable | ETextureCreateFlags::TexCreate_ShaderResource), 1);
		}
	} // namespace

	FSceneTextures::FSceneTextures(DynamicRHI* RHI)
		:d_ptr(new FSceneTexturesPrivate())
	{
		C_P(FSceneTextures);
		d->RHI = RHI;
	}

	FSceneTextures::~FSceneTextures()
	{
		ReleaseAllSceneTexturesResources(d_ptr);
		delete d_ptr;
	}

	void FSceneTextures::InitResource(EFSceneTexturesFlags Flags, uint32_t Width, uint32_t Height)
	{
		C_P(FSceneTextures);
		ReleaseAllSceneTexturesResources(d);

		RenderTexturePool& Pool = RenderTexturePool::Get();
		const uint32_t F = static_cast<uint32_t>(Flags);

		if (F & static_cast<uint32_t>(EFSceneTexturesFlags::SceneDepth))
		{
			d->Depth = Pool.AcquireTexture2D(
				d->RHI,
				EPixelFormat::PF_ShadowDepth,
				static_cast<int32_t>(ETextureCreateFlags::TexCreate_DepthStencilTargetable | ETextureCreateFlags::TexCreate_ShaderResource),
				(int32_t)Width,
				(int32_t)Height,
				1);
		}
		if (F & static_cast<uint32_t>(EFSceneTexturesFlags::SceneVelocity))
		{
			d->MotionVector = Pool.AcquireTexture2D(
				d->RHI,
				EPixelFormat::PF_FloatRGBA,
				static_cast<int32_t>(ETextureCreateFlags::TexCreate_RenderTargetable | ETextureCreateFlags::TexCreate_ShaderResource),
				(int32_t)Width,
				(int32_t)Height,
				1);
		}
		if (F & static_cast<uint32_t>(EFSceneTexturesFlags::SceneColor))
		{
			d->SceneColor = Pool.AcquireTexture2D(
				d->RHI,
				EPixelFormat::PF_FloatRGBA,
				static_cast<int32_t>(ETextureCreateFlags::TexCreate_RenderTargetable | ETextureCreateFlags::TexCreate_ShaderResource
									 | ETextureCreateFlags::TexCreate_UAV),
				(int32_t)Width,
				(int32_t)Height,
				1);
			if (d->SceneColor)
				d->SceneColorUAV = d->RHI->RHICreateUnorderedAccessView(d->SceneColor);
			d->SceneColorWithBloom = Pool.AcquireTexture2D(
				d->RHI,
				EPixelFormat::PF_FloatRGBA,
				static_cast<int32_t>(ETextureCreateFlags::TexCreate_RenderTargetable | ETextureCreateFlags::TexCreate_ShaderResource),
				(int32_t)Width,
				(int32_t)Height,
				1);
			d->SceneColorWithSSR = Pool.AcquireTexture2D(
				d->RHI,
				EPixelFormat::PF_FloatRGBA,
				static_cast<int32_t>(ETextureCreateFlags::TexCreate_RenderTargetable | ETextureCreateFlags::TexCreate_ShaderResource),
				(int32_t)Width,
				(int32_t)Height,
				1);
			d->SceneColorPreLighting = Pool.AcquireTexture2D(
				d->RHI,
				EPixelFormat::PF_FloatRGBA,
				static_cast<int32_t>(ETextureCreateFlags::TexCreate_RenderTargetable | ETextureCreateFlags::TexCreate_ShaderResource),
				(int32_t)Width,
				(int32_t)Height,
				1);
		}
		if (F & static_cast<uint32_t>(EFSceneTexturesFlags::DeferredNormals))
		{
			d->NormalBuffer = Pool.AcquireTexture2D(
				d->RHI,
				EPixelFormat::PF_FloatRGBA,
				static_cast<int32_t>(ETextureCreateFlags::TexCreate_RenderTargetable | ETextureCreateFlags::TexCreate_ShaderResource),
				(int32_t)Width,
				(int32_t)Height,
				1);
		}
		if (F & static_cast<uint32_t>(EFSceneTexturesFlags::DeferredEmissive))
		{
			d->EmissiveBuffer = Pool.AcquireTexture2D(
				d->RHI,
				EPixelFormat::PF_FloatRGBA,
				static_cast<int32_t>(ETextureCreateFlags::TexCreate_RenderTargetable | ETextureCreateFlags::TexCreate_ShaderResource),
				(int32_t)Width,
				(int32_t)Height,
				1);
		}
		if (F & static_cast<uint32_t>(EFSceneTexturesFlags::DeferredMetallicRoughness))
		{
			d->MetallicSpecularRoughness = Pool.AcquireTexture2D(
				d->RHI,
				EPixelFormat::PF_FloatRGBA,
				static_cast<int32_t>(ETextureCreateFlags::TexCreate_RenderTargetable | ETextureCreateFlags::TexCreate_ShaderResource),
				(int32_t)Width,
				(int32_t)Height,
				1);
		}
		if (F & static_cast<uint32_t>(EFSceneTexturesFlags::DeferredMaterialAux))
		{
			d->MaterialAux = Pool.AcquireTexture2D(
				d->RHI,
				EPixelFormat::PF_FloatRGBA,
				static_cast<int32_t>(ETextureCreateFlags::TexCreate_RenderTargetable | ETextureCreateFlags::TexCreate_ShaderResource),
				(int32_t)Width,
				(int32_t)Height,
				1);
		}
	}

	void FSceneTextures::InitDefaultSceneTargets(uint32_t Width, uint32_t Height)
	{
		InitResource(static_cast<EFSceneTexturesFlags>(static_cast<uint32_t>(EFSceneTexturesFlags::SceneDepth)
													  | static_cast<uint32_t>(EFSceneTexturesFlags::SceneVelocity)
													  | static_cast<uint32_t>(EFSceneTexturesFlags::SceneColor)
													  | static_cast<uint32_t>(EFSceneTexturesFlags::DeferredNormals)
													  | static_cast<uint32_t>(EFSceneTexturesFlags::DeferredEmissive)
													  | static_cast<uint32_t>(EFSceneTexturesFlags::DeferredMetallicRoughness)
													  | static_cast<uint32_t>(EFSceneTexturesFlags::DeferredMaterialAux)),
					 Width, Height);
	}

	std::shared_ptr<RHITexture2D> FSceneTextures::GetDepth() const
	{
		C_P(const FSceneTextures);
		return d->Depth;
	}

	std::shared_ptr<RHITexture2D> FSceneTextures::GetSceneColor() const
	{
		C_P(const FSceneTextures);
		return d->SceneColor;
	}

	std::shared_ptr<RHITexture2D> FSceneTextures::GetSceneColorWithSSR() const
	{
		C_P(const FSceneTextures);
		return d->SceneColorWithSSR;
	}

	std::shared_ptr<RHITexture2D> FSceneTextures::GetSceneColorWithBloom() const
	{
		C_P(const FSceneTextures);
		return d->SceneColorWithBloom;
	}

	std::shared_ptr<RHIUnorderedAccessView> FSceneTextures::GetSceneColorUAV() const
	{
		C_P(const FSceneTextures);
		return d->SceneColorUAV;
	}

	std::shared_ptr<RHITexture2D> FSceneTextures::GetMotionVector() const
	{
		C_P(const FSceneTextures);
		return d->MotionVector;
	}

	std::shared_ptr<RHITexture2D> FSceneTextures::GetNormalBuffer() const
	{
		C_P(const FSceneTextures);
		return d->NormalBuffer;
	}

	std::shared_ptr<RHITexture2D> FSceneTextures::GetEmissiveBuffer() const
	{
		C_P(const FSceneTextures);
		return d->EmissiveBuffer;
	}

	std::shared_ptr<RHITexture2D> FSceneTextures::GetMetallicRoughnessBuffer() const
	{
		C_P(const FSceneTextures);
		return d->MetallicSpecularRoughness;
	}

	std::shared_ptr<RHITexture2D> FSceneTextures::GetMaterialAuxBuffer() const
	{
		C_P(const FSceneTextures);
		return d->MaterialAux;
	}

	std::shared_ptr<RHITexture2D> FSceneTextures::GetSceneColorPreLighting() const
	{
		C_P(const FSceneTextures);
		return d->SceneColorPreLighting;
	}

}
