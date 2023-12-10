#include "Render/Bloom.h"
#include "Render/BlurPS.h"
#include "RHI/RHIShdader.h"
#include "RHI/RHIShaderDefine.h"
#include "RHI/RHIPipeLineState.h"
#include "RHI/RHICachedStates.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHITexture2D.h"
#include "RHI/RHIRenderTarget.h"
#include "Render/BlurPS.h"
#include "Render/DownSamplePS.h"

namespace Engine
{
	using namespace RenderCore;

	struct BloomPrivate
	{
		DynamicRHI* RHI;
		std::shared_ptr<BlurPS> Blur;
		std::shared_ptr< DownSamplePS> DownSample;

		std::shared_ptr< RHIVertexShader> VertexShader;
		std::shared_ptr< RHIPixelShader> PixelShader;

		BloomPrivate(DynamicRHI* _RHI) :
			RHI(_RHI)
		{

		}
	};

	Bloom::Bloom(DynamicRHI* RHI)
		:d_ptr(new BloomPrivate(RHI))
	{

	}

	Bloom::~Bloom()
	{
		delete d_ptr;
	}

	void Bloom::InitResource()
	{
		C_P(Bloom);
		d->Blur = std::make_shared<BlurPS>(d->RHI);
		d->Blur->InitResource();
		d->DownSample = std::make_shared<DownSamplePS>(d->RHI);
		d->DownSample->InitResource();
	}

	void Bloom::Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer)
	{
		C_P(Bloom);
		d->DownSample->Draw(RHIContext, TargetBuffer);
		d->Blur->Draw(RHIContext, d->DownSample->GetDownSampleTarget()->GetTex(), TargetBuffer);
	}

}