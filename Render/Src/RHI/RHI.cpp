#include "RHI/RHI.h"
#include "win/win32.h"
#include "core/commandline.h"

namespace RenderCore
{
	//
//	Pixel format information.
//

	FPixelFormatInfo	GPixelFormats[PF_MAX_COUT] =
	{
		// Name						BlockSizeX	BlockSizeY	BlockSizeZ	BlockBytes	NumComponents	PlatformFormat	Supported		UnrealFormat

		{ TEXT("unknown"),			0,			0,			0,			0,			0,				0,				0,				PF_Unknown			},
		{ TEXT("A32B32G32R32F"),	1,			1,			1,			16,			4,				0,				1,				PF_A32B32G32R32F	},
		{ TEXT("B8G8R8A8"),			1,			1,			1,			4,			4,				0,				1,				PF_B8G8R8A8			},
		{ TEXT("G8"),				1,			1,			1,			1,			1,				0,				1,				PF_G8				},
		{ TEXT("G16"),				1,			1,			1,			2,			1,				0,				1,				PF_G16				},
		{ TEXT("DXT1"),				4,			4,			1,			8,			3,				0,				1,				PF_DXT1				},
		{ TEXT("DXT3"),				4,			4,			1,			16,			4,				0,				1,				PF_DXT3				},
		{ TEXT("DXT5"),				4,			4,			1,			16,			4,				0,				1,				PF_DXT5				},
		{ TEXT("UYVY"),				2,			1,			1,			4,			4,				0,				0,				PF_UYVY				},
		{ TEXT("FloatRGB"),			1,			1,			1,			4,			3,				0,				1,				PF_FloatRGB			},
		{ TEXT("FloatRGBA"),		1,			1,			1,			8,			4,				0,				1,				PF_FloatRGBA		},
		{ TEXT("DepthStencil"),		1,			1,			1,			4,			1,				0,				0,				PF_DepthStencil		},
		{ TEXT("ShadowDepth"),		1,			1,			1,			4,			1,				0,				0,				PF_ShadowDepth		},
		{ TEXT("R32_FLOAT"),		1,			1,			1,			4,			1,				0,				1,				PF_R32_FLOAT		},
		{ TEXT("G16R16"),			1,			1,			1,			4,			2,				0,				1,				PF_G16R16			},
		{ TEXT("G16R16F"),			1,			1,			1,			4,			2,				0,				1,				PF_G16R16F			},
		{ TEXT("G16R16F_FILTER"),	1,			1,			1,			4,			2,				0,				1,				PF_G16R16F_FILTER	},
		{ TEXT("G32R32F"),			1,			1,			1,			8,			2,				0,				1,				PF_G32R32F			},
		{ TEXT("A2B10G10R10"),      1,          1,          1,          4,          4,              0,              1,				PF_A2B10G10R10		},
		{ TEXT("A16B16G16R16"),		1,			1,			1,			8,			4,				0,				1,				PF_A16B16G16R16		},
		{ TEXT("D24"),				1,			1,			1,			4,			1,				0,				1,				PF_D24				},
		{ TEXT("PF_R16F"),			1,			1,			1,			2,			1,				0,				1,				PF_R16F				},
		{ TEXT("PF_R16F_FILTER"),	1,			1,			1,			2,			1,				0,				1,				PF_R16F_FILTER		},
		{ TEXT("BC5"),				4,			4,			1,			16,			2,				0,				1,				PF_BC5				},
		{ TEXT("V8U8"),				1,			1,			1,			2,			2,				0,				1,				PF_V8U8				},
		{ TEXT("A1"),				1,			1,			1,			1,			1,				0,				0,				PF_A1				},
		{ TEXT("FloatR11G11B10"),	1,			1,			1,			4,			3,				0,				0,				PF_FloatR11G11B10	},
		{ TEXT("A8"),				1,			1,			1,			1,			1,				0,				1,				PF_A8				},
		{ TEXT("R32_UINT"),			1,			1,			1,			4,			1,				0,				1,				PF_R32_UINT			},
		{ TEXT("R32_SINT"),			1,			1,			1,			4,			1,				0,				1,				PF_R32_SINT			},

		// IOS Support
		{ TEXT("PVRTC2"),			8,			4,			1,			8,			4,				0,				0,				PF_PVRTC2			},
		{ TEXT("PVRTC4"),			4,			4,			1,			8,			4,				0,				0,				PF_PVRTC4			},

		{ TEXT("R16_UINT"),			1,			1,			1,			2,			1,				0,				1,				PF_R16_UINT			},
		{ TEXT("R16_SINT"),			1,			1,			1,			2,			1,				0,				1,				PF_R16_SINT			},
		{ TEXT("R16G16B16A16_UINT"),1,			1,			1,			8,			4,				0,				1,				PF_R16G16B16A16_UINT},
		{ TEXT("R16G16B16A16_SINT"),1,			1,			1,			8,			4,				0,				1,				PF_R16G16B16A16_SINT},
		{ TEXT("R5G6B5_UNORM"),     1,          1,          1,          2,          3,              0,              1,              PF_R5G6B5_UNORM		},
		{ TEXT("R8G8B8A8"),			1,			1,			1,			4,			4,				0,				1,				PF_R8G8B8A8			},
		{ TEXT("A8R8G8B8"),			1,			1,			1,			4,			4,				0,				1,				PF_A8R8G8B8			},
		{ TEXT("BC4"),				4,			4,			1,			8,			1,				0,				1,				PF_BC4				},
		{ TEXT("R8G8"),				1,			1,			1,			2,			2,				0,				1,				PF_R8G8				},

		{ TEXT("ATC_RGB"),			4,			4,			1,			8,			3,				0,				0,				PF_ATC_RGB			},
		{ TEXT("ATC_RGBA_E"),		4,			4,			1,			16,			4,				0,				0,				PF_ATC_RGBA_E		},
		{ TEXT("ATC_RGBA_I"),		4,			4,			1,			16,			4,				0,				0,				PF_ATC_RGBA_I		},
		{ TEXT("X24_G8"),			1,			1,			1,			1,			1,				0,				0,				PF_X24_G8			},
		{ TEXT("ETC1"),				4,			4,			1,			8,			3,				0,				0,				PF_ETC1				},
		{ TEXT("ETC2_RGB"),			4,			4,			1,			8,			3,				0,				0,				PF_ETC2_RGB			},
		{ TEXT("ETC2_RGBA"),		4,			4,			1,			16,			4,				0,				0,				PF_ETC2_RGBA		},
		{ TEXT("PF_R32G32B32A32_UINT"),1,		1,			1,			16,			4,				0,				1,				PF_R32G32B32A32_UINT},
		{ TEXT("PF_R16G16_UINT"),	1,			1,			1,			4,			4,				0,				1,				PF_R16G16_UINT},

		// ASTC support
		{ TEXT("ASTC_4x4"),			4,			4,			1,			16,			4,				0,				0,				PF_ASTC_4x4			},
		{ TEXT("ASTC_6x6"),			6,			6,			1,			16,			4,				0,				0,				PF_ASTC_6x6			},
		{ TEXT("ASTC_8x8"),			8,			8,			1,			16,			4,				0,				0,				PF_ASTC_8x8			},
		{ TEXT("ASTC_10x10"),		10,			10,			1,			16,			4,				0,				0,				PF_ASTC_10x10		},
		{ TEXT("ASTC_12x12"),		12,			12,			1,			16,			4,				0,				0,				PF_ASTC_12x12		},

		{ TEXT("BC6H"),				4,			4,			1,			16,			3,				0,				1,				PF_BC6H				},
		{ TEXT("BC7"),				4,			4,			1,			16,			4,				0,				1,				PF_BC7				},
		{ TEXT("R8_UINT"),			1,			1,			1,			1,			1,				0,				1,				PF_R8_UINT			},
		{ TEXT("L8"),				1,			1,			1,			1,			1,				0,				0,				PF_L8				},
		{ TEXT("XGXR8"),			1,			1,			1,			4,			4,				0,				1,				PF_XGXR8 			},
		{ TEXT("R8G8B8A8_UINT"),	1,			1,			1,			4,			4,				0,				1,				PF_R8G8B8A8_UINT	},
		{ TEXT("R8G8B8A8_SNORM"),	1,			1,			1,			4,			4,				0,				1,				PF_R8G8B8A8_SNORM	},

		{ TEXT("R16G16B16A16_UINT"),1,			1,			1,			8,			4,				0,				1,				PF_R16G16B16A16_UNORM },
		{ TEXT("R16G16B16A16_SINT"),1,			1,			1,			8,			4,				0,				1,				PF_R16G16B16A16_SNORM },
		{ TEXT("PLATFORM_HDR_0"),	0,			0,			0,			0,			0,				0,				0,				PF_PLATFORM_HDR_0   },
		{ TEXT("PLATFORM_HDR_1"),	0,			0,			0,			0,			0,				0,				0,				PF_PLATFORM_HDR_1   },
		{ TEXT("PLATFORM_HDR_2"),	0,			0,			0,			0,			0,				0,				0,				PF_PLATFORM_HDR_2   },

		// NV12 contains 2 textures: R8 luminance plane followed by R8G8 1/4 size chrominance plane.
		// BlockSize/BlockBytes/NumComponents values don't make much sense for this format, so set them all to one.
		{ TEXT("NV12"),				1,			1,			1,			1,			1,				0,				0,				PF_NV12             },

		{ TEXT("PF_R32G32_UINT"),   1,   		1,			1,			8,			2,				0,				1,				PF_R32G32_UINT      },

		{ TEXT("PF_ETC2_R11_EAC"),  4,   		4,			1,			8,			1,				0,				0,				PF_ETC2_R11_EAC     },
		{ TEXT("PF_ETC2_RG11_EAC"), 4,   		4,			1,			16,			2,				0,				0,				PF_ETC2_RG11_EAC    },
		{ TEXT("R8"),				1,			1,			1,			1,			1,				0,				1,				PF_R8				},
	};

	static struct FValidatePixelFormats
	{
		FValidatePixelFormats()
		{
			for (int32_t Index = 0; Index < UE_ARRAY_COUNT(GPixelFormats); ++Index)
			{
				// Make sure GPixelFormats has an entry for every unreal format
				Assert((EPixelFormat)Index == GPixelFormats[Index].UnrealFormat);
			}
		}
	} ValidatePixelFormats;


	uint32_t GetTypeHash(const SamplerStateInitializerRHI& Initializer)
	{
		uint32_t Hash = GetTypeHash(Initializer.Filter);
		Hash = Templates::HashCombine(Hash, GetTypeHash(Initializer.AddressU));
		Hash = Templates::HashCombine(Hash, GetTypeHash(Initializer.AddressV));
		Hash = Templates::HashCombine(Hash, GetTypeHash(Initializer.AddressW));
		Hash = Templates::HashCombine(Hash, Templates::GetTypeHash(Initializer.MipBias));
		Hash = Templates::HashCombine(Hash, Templates::GetTypeHash(Initializer.MinMipLevel));
		Hash = Templates::HashCombine(Hash, Templates::GetTypeHash(Initializer.MaxMipLevel));
		Hash = Templates::HashCombine(Hash, Templates::GetTypeHash(Initializer.MaxAnisotropy));
		Hash = Templates::HashCombine(Hash, Templates::GetTypeHash(Initializer.BorderColor));
		Hash = Templates::HashCombine(Hash, GetTypeHash(Initializer.SamplerComparisonFunction));
		return Hash;
	}

	bool operator== (const SamplerStateInitializerRHI& A, const SamplerStateInitializerRHI& B)
	{
		bool bSame =
			A.Filter == B.Filter &&
			A.AddressU == B.AddressU &&
			A.AddressV == B.AddressV &&
			A.AddressW == B.AddressW &&
			A.MipBias == B.MipBias &&
			A.MinMipLevel == B.MinMipLevel &&
			A.MaxMipLevel == B.MaxMipLevel &&
			A.MaxAnisotropy == B.MaxAnisotropy &&
			A.BorderColor == B.BorderColor &&
			A.SamplerComparisonFunction == B.SamplerComparisonFunction;
		return bSame;
	}

	uint32_t GetTypeHash(const RasterizerStateInitializerRHI& Initializer)
	{
		uint32_t Hash = GetTypeHash(Initializer.FillMode);
		Hash = Templates::HashCombine(Hash, GetTypeHash(Initializer.CullMode));
		Hash = Templates::HashCombine(Hash, Templates::GetTypeHash(Initializer.DepthBias));
		Hash = Templates::HashCombine(Hash, Templates::GetTypeHash(Initializer.SlopeScaleDepthBias));
		Hash = Templates::HashCombine(Hash, Templates::GetTypeHash(Initializer.bAllowMSAA));
		Hash = Templates::HashCombine(Hash, Templates::GetTypeHash(Initializer.bEnableLineAA));
		return Hash;
	}

	bool operator== (const RasterizerStateInitializerRHI& A, const RasterizerStateInitializerRHI& B)
	{
		bool bSame =
			A.FillMode == B.FillMode &&
			A.CullMode == B.CullMode &&
			A.DepthBias == B.DepthBias &&
			A.SlopeScaleDepthBias == B.SlopeScaleDepthBias &&
			A.bAllowMSAA == B.bAllowMSAA &&
			A.bEnableLineAA == B.bEnableLineAA;
		return bSame;
	}

	uint32_t GetTypeHash(const DepthStencilStateInitializerRHI& Initializer)
	{
		uint32_t Hash = Templates::GetTypeHash(Initializer.bEnableDepthWrite);
		Hash = Templates::HashCombine(Hash, GetTypeHash(Initializer.DepthTest));
		Hash = Templates::HashCombine(Hash, Templates::GetTypeHash(Initializer.bEnableFrontFaceStencil));
		Hash = Templates::HashCombine(Hash, GetTypeHash(Initializer.FrontFaceStencilTest));
		Hash = Templates::HashCombine(Hash, GetTypeHash(Initializer.FrontFaceStencilFailStencilOp));
		Hash = Templates::HashCombine(Hash, GetTypeHash(Initializer.FrontFaceDepthFailStencilOp));
		Hash = Templates::HashCombine(Hash, GetTypeHash(Initializer.FrontFacePassStencilOp));
		Hash = Templates::HashCombine(Hash, Templates::GetTypeHash(Initializer.bEnableBackFaceStencil));
		Hash = Templates::HashCombine(Hash, GetTypeHash(Initializer.BackFaceStencilTest));
		Hash = Templates::HashCombine(Hash, GetTypeHash(Initializer.BackFaceStencilFailStencilOp));
		Hash = Templates::HashCombine(Hash, GetTypeHash(Initializer.BackFaceDepthFailStencilOp));
		Hash = Templates::HashCombine(Hash, GetTypeHash(Initializer.BackFacePassStencilOp));
		Hash = Templates::HashCombine(Hash, Templates::GetTypeHash(Initializer.StencilReadMask));
		Hash = Templates::HashCombine(Hash, Templates::GetTypeHash(Initializer.StencilWriteMask));
		return Hash;
	}

	bool operator== (const DepthStencilStateInitializerRHI& A, const DepthStencilStateInitializerRHI& B)
	{
		bool bSame =
			A.bEnableDepthWrite == B.bEnableDepthWrite &&
			A.DepthTest == B.DepthTest &&
			A.bEnableFrontFaceStencil == B.bEnableFrontFaceStencil &&
			A.FrontFaceStencilTest == B.FrontFaceStencilTest &&
			A.FrontFaceStencilFailStencilOp == B.FrontFaceStencilFailStencilOp &&
			A.FrontFaceDepthFailStencilOp == B.FrontFaceDepthFailStencilOp &&
			A.FrontFacePassStencilOp == B.FrontFacePassStencilOp &&
			A.bEnableBackFaceStencil == B.bEnableBackFaceStencil &&
			A.BackFaceStencilTest == B.BackFaceStencilTest &&
			A.BackFaceStencilFailStencilOp == B.BackFaceStencilFailStencilOp &&
			A.BackFaceDepthFailStencilOp == B.BackFaceDepthFailStencilOp &&
			A.BackFacePassStencilOp == B.BackFacePassStencilOp &&
			A.StencilReadMask == B.StencilReadMask &&
			A.StencilWriteMask == B.StencilWriteMask;
		return bSame;
	}

	uint32_t GetTypeHash(const BlendStateInitializerRHI::FRenderTarget& Initializer)
	{
		uint32_t Hash = GetTypeHash(Initializer.ColorBlendOp);
		Hash = Templates::HashCombine(Hash, GetTypeHash(Initializer.ColorDestBlend));
		Hash = Templates::HashCombine(Hash, GetTypeHash(Initializer.ColorSrcBlend));
		Hash = Templates::HashCombine(Hash, GetTypeHash(Initializer.AlphaBlendOp));
		Hash = Templates::HashCombine(Hash, GetTypeHash(Initializer.AlphaDestBlend));
		Hash = Templates::HashCombine(Hash, GetTypeHash(Initializer.AlphaSrcBlend));
		Hash = Templates::HashCombine(Hash, GetTypeHash(Initializer.ColorWriteMask));
		return Hash;
	}

	bool operator==(const BlendStateInitializerRHI::FRenderTarget& A, const BlendStateInitializerRHI::FRenderTarget& B)
	{
		bool bSame =
			A.ColorBlendOp == B.ColorBlendOp &&
			A.ColorDestBlend == B.ColorDestBlend &&
			A.ColorSrcBlend == B.ColorSrcBlend &&
			A.AlphaBlendOp == B.AlphaBlendOp &&
			A.AlphaDestBlend == B.AlphaDestBlend &&
			A.AlphaSrcBlend == B.AlphaSrcBlend &&
			A.ColorWriteMask == B.ColorWriteMask;
		return bSame;
	}

	uint32_t GetTypeHash(const BlendStateInitializerRHI& Initializer)
	{
		uint32_t Hash = GetTypeHash(Initializer.bUseIndependentRenderTargetBlendStates);
		for (int32_t i = 0; i < MaxSimultaneousRenderTargets; ++i)
		{
			Hash = Templates::HashCombine(Hash, GetTypeHash(Initializer.RenderTargets[i]));
		}

		return Hash;
	}

	bool operator== (const BlendStateInitializerRHI& A, const BlendStateInitializerRHI& B)
	{
		bool bSame = A.bUseIndependentRenderTargetBlendStates == B.bUseIndependentRenderTargetBlendStates;
		for (int32_t i = 0; i < MaxSimultaneousRenderTargets && bSame; ++i)
		{
			bSame = bSame && A.RenderTargets[i] == B.RenderTargets[i];
		}
		return bSame;
	}

	bool D3D12RHI_ShouldCreateWithD3DDebug()
	{
		return core::CommandLine::Get().GetSwitch("d3ddebug") ||
			core::CommandLine::Get().GetSwitch("dxdebug");
	}

	static bool CmdBool(const char* name, bool defaultIfPresent = true)
	{
		int v = defaultIfPresent ? 1 : 0;
		if (core::CommandLine::Get().GetInteger(name, v))
			return v != 0;
		return core::CommandLine::Get().GetName(name);
	}

	bool D3D12RHI_ShouldEnableMemMon()
	{
#if WITH_D3D12_MEMMON
		return CmdBool("d3d12_memmon", false) || CmdBool("d3d_mem", false);
#else
		return false;
#endif
	}

	bool D3D12RHI_ShouldEnableMemMonStacks()
	{
#if WITH_D3D12_MEMMON
		if (!D3D12RHI_ShouldEnableMemMon())
			return false;
		return CmdBool("d3d12_memmon_stacks", false);
#else
		return false;
#endif
	}

	bool D3D12RHI_ShouldEnableMemMonDeep()
	{
#if WITH_D3D12_MEMMON
		if (!D3D12RHI_ShouldEnableMemMon())
			return false;
		return CmdBool("d3d12_memmon_deep", false) || CmdBool("d3d_mem_deep", false);
#else
		return false;
#endif
	}
}