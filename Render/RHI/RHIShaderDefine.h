#pragma once
#include "RHI/DynamicRHI.h"
#include "RHI/RHICommandContext.h"
#include "RHI/RHIUniformBuffer.h"

#define BEGIN_SHADER_STRUCT(Name,CBIndex)\
	struct Name##Wrap\
	{\
		std::shared_ptr<RenderCore::RHIUniformBuffer> CB_;\
		std::unordered_map<std::string, int32_t> TextureIndexMap_;\
		int CBIndex_ = CBIndex;\
		RenderCore::DynamicRHI* _RHI = nullptr;\
		struct Name {

#define BEGIN_STRUCT_CONSTRUCT(Name)\
	};\
	Name Data;\
	void SetShaderTexture(RenderCore::EShaderFrequency ShaderType, const std::string& name, std::shared_ptr<RenderCore::RHITexture2D> Texture2DRHI)\
	{\
		auto itr = TextureIndexMap_.find(name);\
		if (itr != TextureIndexMap_.end())\
		{\
			int texture_index = itr->second;\
			_RHI->GetDefaultCommandContext()->RHISetShaderTexture(ShaderType,texture_index, Texture2DRHI);\
		}\
	}\
	void UpdateUniformBuffer()\
	{\
		_RHI->GetDefaultCommandContext()->RHIUpdateUniformBuffer(CB_, &Data);\
	}\
	void SetShaderUniformBuffer(RenderCore::EShaderFrequency ShaderType)\
	{\
		_RHI->GetDefaultCommandContext()->RHISetShaderUniformBuffer(ShaderType,CBIndex_,CB_);\
	}\
	Name##Wrap(RenderCore::DynamicRHI* RHI){\
	_RHI = RHI;\
	CB_ = RHI->RHICreateUniformBuffer(&Data,sizeof(Data));\
	uint32_t offset = 0;

#define INIT_TEXTURE_INDEX(Name,Index) TextureIndexMap_.insert({Name,Index});

#define END_STRUCT_CONSTRUCT }

#define DECLARE_PARAM(Type,ValName) Type ValName;
#define DECLARE_PARAM_VALUE(Type,ValName,Val) Type ValName = Val;
#define DECLARE_ARRAY_PARAM(Type,Count,ValName) Type ValName##[Count];

#define END_SHADER_STRUCT };

#define DECLARE_SHADER_STRUCT_MEMBER(Name) Name##Wrap m_##Name##UniformBuffer;
#define GET_SHADER_STRUCT_MEMBER(Name) m_##Name##UniformBuffer
#define GET_UNIFORMDATA(Name) m_##Name##UniformBuffer.Data
#define GET_P_UNIFORMDATA(P,Name) P->GET_UNIFORMDATA(Name)