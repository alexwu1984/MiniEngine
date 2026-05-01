#include "Render/GBuffer.h"
#include "Render/RenderTexturePool.h"
#include "RHI/RHITexture2D.h"
#include "RHI/RHIUnorderedAccessView.h"
#include "RHI/DynamicRHI.h"

using namespace RenderCore;

namespace Engine
{
	struct GBufferPrivate
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

		void ReleaseAllGBufferResources(GBufferPrivate* d)
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
			ReleaseTex2DToPool(d->SceneColorPreLighting, EPixelFormat::PF_FloatRGBA,
							   static_cast<int32_t>(ETextureCreateFlags::TexCreate_RenderTargetable | ETextureCreateFlags::TexCreate_ShaderResource), 1);
		}
	} // namespace

	GBuffer::GBuffer(DynamicRHI* RHI)
		:d_ptr(new GBufferPrivate())
	{
		C_P(GBuffer);
		d->RHI = RHI;
	}

	GBuffer::~GBuffer()
	{
		ReleaseAllGBufferResources(d_ptr);
		delete d_ptr;
	}

	void GBuffer::InitResource(GBufferFlagBits Flag, uint32_t Width, uint32_t Height)
	{
		C_P(GBuffer);
		// Resize / re-init: return previous textures to the pool first (correct keys), then acquire new sizes.
		ReleaseAllGBufferResources(d);

		RenderTexturePool& Pool = RenderTexturePool::Get();

		if (Flag & GBUFFER_DEPTH)
		{
			d->Depth = Pool.AcquireTexture2D(
				d->RHI,
				EPixelFormat::PF_ShadowDepth,
				static_cast<int32_t>(ETextureCreateFlags::TexCreate_DepthStencilTargetable | ETextureCreateFlags::TexCreate_ShaderResource),
				(int32_t)Width,
				(int32_t)Height,
				1);
		}
		if (Flag & GBUFFER_MOTION_VECTORS)
		{
			d->MotionVector = Pool.AcquireTexture2D(
				d->RHI,
				EPixelFormat::PF_FloatRGBA,
				static_cast<int32_t>(ETextureCreateFlags::TexCreate_RenderTargetable | ETextureCreateFlags::TexCreate_ShaderResource),
				(int32_t)Width,
				(int32_t)Height,
				1);
		}
		if (Flag & GBUFFER_SCENE_COLOR)
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
		if (Flag & GBUFFER_NORMAL_BUFFER)
		{
			d->NormalBuffer = Pool.AcquireTexture2D(
				d->RHI,
				EPixelFormat::PF_FloatRGBA,
				static_cast<int32_t>(ETextureCreateFlags::TexCreate_RenderTargetable | ETextureCreateFlags::TexCreate_ShaderResource),
				(int32_t)Width,
				(int32_t)Height,
				1);
		}
		if (Flag & GBUFFER_EMISSIVE_BUFFER)
		{
			d->EmissiveBuffer = Pool.AcquireTexture2D(
				d->RHI,
				EPixelFormat::PF_FloatRGBA,
				static_cast<int32_t>(ETextureCreateFlags::TexCreate_RenderTargetable | ETextureCreateFlags::TexCreate_ShaderResource),
				(int32_t)Width,
				(int32_t)Height,
				1);
		}
		if (Flag & GBUFFER_METALLIC_ROUGHNESS_BUFFER)
		{
			d->MetallicSpecularRoughness = Pool.AcquireTexture2D(
				d->RHI,
				EPixelFormat::PF_FloatRGBA,
				static_cast<int32_t>(ETextureCreateFlags::TexCreate_RenderTargetable | ETextureCreateFlags::TexCreate_ShaderResource),
				(int32_t)Width,
				(int32_t)Height,
				1);
		}
	}

	void GBuffer::InitDefaultSceneTargets(uint32_t Width, uint32_t Height)
	{
		InitResource(static_cast<GBufferFlagBits>(GBufferFlagBits::GBUFFER_DEPTH |
												  GBufferFlagBits::GBUFFER_MOTION_VECTORS |
												  GBufferFlagBits::GBUFFER_SCENE_COLOR |
												  GBufferFlagBits::GBUFFER_NORMAL_BUFFER |
												  GBufferFlagBits::GBUFFER_EMISSIVE_BUFFER |
												  GBufferFlagBits::GBUFFER_METALLIC_ROUGHNESS_BUFFER),
					 Width, Height);
	}

	std::shared_ptr<RHITexture2D> GBuffer::GetDepth() const
	{
		C_P(const GBuffer);
		return d->Depth;
	}

	std::shared_ptr<RHITexture2D> GBuffer::GetSceneColor() const
	{
		C_P(const GBuffer);
		return d->SceneColor;
	}

	std::shared_ptr<RHITexture2D> GBuffer::GetSceneColorWithSSR() const
	{
		C_P(const GBuffer);
		return d->SceneColorWithSSR;
	}

	std::shared_ptr<RHITexture2D> GBuffer::GetSceneColorWithBloom() const
	{
		C_P(const GBuffer);
		return d->SceneColorWithBloom;
	}

	std::shared_ptr<RHIUnorderedAccessView> GBuffer::GetSceneColorUAV() const
	{
		C_P(const GBuffer);
		return d->SceneColorUAV;
	}

	std::shared_ptr<RHITexture2D> GBuffer::GetMotionVector() const
	{
		C_P(const GBuffer);
		return d->MotionVector;
	}

	std::shared_ptr<RHITexture2D> GBuffer::GetNormalBuffer() const
	{
		C_P(const GBuffer);
		return d->NormalBuffer;
	}

	std::shared_ptr<RHITexture2D> GBuffer::GetEmissiveBuffer() const
	{
		C_P(const GBuffer);
		return d->EmissiveBuffer;
	}


	std::shared_ptr<RHITexture2D> GBuffer::GetMetallicRoughnessBuffer() const
	{
		C_P(const GBuffer);
		return d->MetallicSpecularRoughness;
	}

	std::shared_ptr<RHITexture2D> GBuffer::GetSceneColorPreLighting() const
	{
		C_P(const GBuffer);
		return d->SceneColorPreLighting;
	}

}
