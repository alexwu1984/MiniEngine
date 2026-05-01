#include "Render/SceneTextures.h"

#include "Render/RenderTexturePool.h"

#include "RHI/RHITexture2D.h"

#include "RHI/RHIUnorderedAccessView.h"

#include "RHI/DynamicRHI.h"

using namespace RenderCore;

namespace Engine
{
	struct SceneTexturesPrivate
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

		void ReleaseAllSceneTexturesResources(SceneTexturesPrivate* d)
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

	SceneTextures::SceneTextures(DynamicRHI* RHI)
		:d_ptr(new SceneTexturesPrivate())
	{
		C_P(SceneTextures);
		d->RHI = RHI;
	}

	SceneTextures::~SceneTextures()
	{
		ReleaseAllSceneTexturesResources(d_ptr);
		delete d_ptr;
	}

	void SceneTextures::InitResource(ESceneTexturesFlags Flags, uint32_t Width, uint32_t Height)
	{
		C_P(SceneTextures);
		ReleaseAllSceneTexturesResources(d);

		RenderTexturePool& Pool = RenderTexturePool::Get();
		const uint32_t F = static_cast<uint32_t>(Flags);

		if (F & static_cast<uint32_t>(ESceneTexturesFlags::SceneDepth))
		{
			d->Depth = Pool.AcquireTexture2D(
				d->RHI,
				EPixelFormat::PF_ShadowDepth,
				static_cast<int32_t>(ETextureCreateFlags::TexCreate_DepthStencilTargetable | ETextureCreateFlags::TexCreate_ShaderResource),
				(int32_t)Width,
				(int32_t)Height,
				1);
		}
		if (F & static_cast<uint32_t>(ESceneTexturesFlags::SceneVelocity))
		{
			d->MotionVector = Pool.AcquireTexture2D(
				d->RHI,
				EPixelFormat::PF_FloatRGBA,
				static_cast<int32_t>(ETextureCreateFlags::TexCreate_RenderTargetable | ETextureCreateFlags::TexCreate_ShaderResource),
				(int32_t)Width,
				(int32_t)Height,
				1);
		}
		if (F & static_cast<uint32_t>(ESceneTexturesFlags::SceneColor))
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
		if (F & static_cast<uint32_t>(ESceneTexturesFlags::DeferredNormals))
		{
			d->NormalBuffer = Pool.AcquireTexture2D(
				d->RHI,
				EPixelFormat::PF_FloatRGBA,
				static_cast<int32_t>(ETextureCreateFlags::TexCreate_RenderTargetable | ETextureCreateFlags::TexCreate_ShaderResource),
				(int32_t)Width,
				(int32_t)Height,
				1);
		}
		if (F & static_cast<uint32_t>(ESceneTexturesFlags::DeferredEmissive))
		{
			d->EmissiveBuffer = Pool.AcquireTexture2D(
				d->RHI,
				EPixelFormat::PF_FloatRGBA,
				static_cast<int32_t>(ETextureCreateFlags::TexCreate_RenderTargetable | ETextureCreateFlags::TexCreate_ShaderResource),
				(int32_t)Width,
				(int32_t)Height,
				1);
		}
		if (F & static_cast<uint32_t>(ESceneTexturesFlags::DeferredMetallicRoughness))
		{
			d->MetallicSpecularRoughness = Pool.AcquireTexture2D(
				d->RHI,
				EPixelFormat::PF_FloatRGBA,
				static_cast<int32_t>(ETextureCreateFlags::TexCreate_RenderTargetable | ETextureCreateFlags::TexCreate_ShaderResource),
				(int32_t)Width,
				(int32_t)Height,
				1);
		}
		if (F & static_cast<uint32_t>(ESceneTexturesFlags::DeferredMaterialAux))
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

	void SceneTextures::InitDefaultSceneTargets(uint32_t Width, uint32_t Height)
	{
		InitResource(static_cast<ESceneTexturesFlags>(static_cast<uint32_t>(ESceneTexturesFlags::SceneDepth)
													  | static_cast<uint32_t>(ESceneTexturesFlags::SceneVelocity)
													  | static_cast<uint32_t>(ESceneTexturesFlags::SceneColor)
													  | static_cast<uint32_t>(ESceneTexturesFlags::DeferredNormals)
													  | static_cast<uint32_t>(ESceneTexturesFlags::DeferredEmissive)
													  | static_cast<uint32_t>(ESceneTexturesFlags::DeferredMetallicRoughness)
													  | static_cast<uint32_t>(ESceneTexturesFlags::DeferredMaterialAux)),
					 Width, Height);
	}

	std::shared_ptr<RHITexture2D> SceneTextures::GetDepth() const
	{
		C_P(const SceneTextures);
		return d->Depth;
	}

	std::shared_ptr<RHITexture2D> SceneTextures::GetSceneColor() const
	{
		C_P(const SceneTextures);
		return d->SceneColor;
	}

	std::shared_ptr<RHITexture2D> SceneTextures::GetSceneColorWithSSR() const
	{
		C_P(const SceneTextures);
		return d->SceneColorWithSSR;
	}

	std::shared_ptr<RHITexture2D> SceneTextures::GetSceneColorWithBloom() const
	{
		C_P(const SceneTextures);
		return d->SceneColorWithBloom;
	}

	std::shared_ptr<RHIUnorderedAccessView> SceneTextures::GetSceneColorUAV() const
	{
		C_P(const SceneTextures);
		return d->SceneColorUAV;
	}

	std::shared_ptr<RHITexture2D> SceneTextures::GetMotionVector() const
	{
		C_P(const SceneTextures);
		return d->MotionVector;
	}

	std::shared_ptr<RHITexture2D> SceneTextures::GetNormalBuffer() const
	{
		C_P(const SceneTextures);
		return d->NormalBuffer;
	}

	std::shared_ptr<RHITexture2D> SceneTextures::GetEmissiveBuffer() const
	{
		C_P(const SceneTextures);
		return d->EmissiveBuffer;
	}

	std::shared_ptr<RHITexture2D> SceneTextures::GetMetallicRoughnessBuffer() const
	{
		C_P(const SceneTextures);
		return d->MetallicSpecularRoughness;
	}

	std::shared_ptr<RHITexture2D> SceneTextures::GetMaterialAuxBuffer() const
	{
		C_P(const SceneTextures);
		return d->MaterialAux;
	}

	std::shared_ptr<RHITexture2D> SceneTextures::GetSceneColorPreLighting() const
	{
		C_P(const SceneTextures);
		return d->SceneColorPreLighting;
	}

}
