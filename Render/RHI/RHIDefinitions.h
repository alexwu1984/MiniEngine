#pragma once
#include "core/inc.h"

namespace RenderCore
{
	enum EPixelFormat : uint8_t
	{
		PF_Unknown = 0,
		PF_A32B32G32R32F = 1,
		PF_B8G8R8A8 = 2,
		PF_G8 = 3,
		PF_G16 = 4,
		PF_DXT1 = 5,
		PF_DXT3 = 6,
		PF_DXT5 = 7,
		PF_UYVY = 8,
		PF_FloatRGB = 9,
		PF_FloatRGBA = 10,
		PF_DepthStencil = 11,
		PF_ShadowDepth = 12,
		PF_R32_FLOAT = 13,
		PF_G16R16 = 14,
		PF_G16R16F = 15,
		PF_G16R16F_FILTER = 16,
		PF_G32R32F = 17,
		PF_A2B10G10R10 = 18,
		PF_A16B16G16R16 = 19,
		PF_D24 = 20,
		PF_R16F = 21,
		PF_R16F_FILTER = 22,
		PF_BC5 = 23,
		PF_V8U8 = 24,
		PF_A1 = 25,
		PF_FloatR11G11B10 = 26,
		PF_A8 = 27,
		PF_R32_UINT = 28,
		PF_R32_SINT = 29,
		PF_PVRTC2 = 30,
		PF_PVRTC4 = 31,
		PF_R16_UINT = 32,
		PF_R16_SINT = 33,
		PF_R16G16B16A16_UINT = 34,
		PF_R16G16B16A16_SINT = 35,
		PF_R5G6B5_UNORM = 36,
		PF_R8G8B8A8 = 37,
		PF_A8R8G8B8 = 38,	// Only used for legacy loading; do NOT use!
		PF_BC4 = 39,
		PF_R8G8 = 40,
		PF_ATC_RGB = 41,	// Unsupported Format
		PF_ATC_RGBA_E = 42,	// Unsupported Format
		PF_ATC_RGBA_I = 43,	// Unsupported Format
		PF_X24_G8 = 44,	// Used for creating SRVs to alias a DepthStencil buffer to read Stencil. Don't use for creating textures.
		PF_ETC1 = 45,	// Unsupported Format
		PF_ETC2_RGB = 46,
		PF_ETC2_RGBA = 47,
		PF_R32G32B32A32_UINT = 48,
		PF_R16G16_UINT = 49,
		PF_ASTC_4x4 = 50,	// 8.00 bpp
		PF_ASTC_6x6 = 51,	// 3.56 bpp
		PF_ASTC_8x8 = 52,	// 2.00 bpp
		PF_ASTC_10x10 = 53,	// 1.28 bpp
		PF_ASTC_12x12 = 54,	// 0.89 bpp
		PF_BC6H = 55,
		PF_BC7 = 56,
		PF_R8_UINT = 57,
		PF_L8 = 58,
		PF_XGXR8 = 59,
		PF_R8G8B8A8_UINT = 60,
		PF_R8G8B8A8_SNORM = 61,
		PF_R16G16B16A16_UNORM = 62,
		PF_R16G16B16A16_SNORM = 63,
		PF_PLATFORM_HDR_0 = 64,
		PF_PLATFORM_HDR_1 = 65,	// Reserved.
		PF_PLATFORM_HDR_2 = 66,	// Reserved.
		PF_NV12 = 67,
		PF_R32G32_UINT = 68,
		PF_ETC2_R11_EAC = 69,
		PF_ETC2_RG11_EAC = 70,
		PF_R8 = 71,
		PF_MAX_COUT = 72,
	};

	/** Information about a pixel format. */
	struct FPixelFormatInfo
	{
		const TCHAR* Name;
		int32_t		BlockSizeX,
			BlockSizeY,
			BlockSizeZ,
			BlockBytes,
			NumComponents;
		/** Platform specific token, e.g. D3DFORMAT with D3DDrv										*/
		uint32_t		PlatformFormat;
		/** Whether the texture format is supported on the current platform/ rendering combination	*/
		bool			Supported;
		EPixelFormat	UnrealFormat;
	};

	extern FPixelFormatInfo GPixelFormats[PF_MAX_COUT];

	enum EShaderFrequency : uint8_t
	{
		SF_Vertex = 0,
		SF_Hull = 1,
		SF_Domain = 2,
		SF_Pixel = 3,
		SF_Geometry = 4,
		SF_Compute = 5,
		SF_RayGen = 6,
		SF_RayMiss = 7,
		SF_RayHitGroup = 8,
		SF_RayCallable = 9,

		SF_NumFrequencies = 10,

		// Number of standard SM5-style shader frequencies for graphics pipeline (excluding compute)
		SF_NumGraphicsFrequencies = 5,

		// Number of standard SM5-style shader frequencies (including compute)
		SF_NumStandardFrequencies = 6,

		SF_NumBits = 4,
	};
	static_assert(SF_NumFrequencies <= (1 << SF_NumBits), "SF_NumFrequencies will not fit on SF_NumBits");

	/** Maximum number of miplevels in a texture. */
	enum { MAX_TEXTURE_MIP_COUNT = 15 };

	/** Maximum number of static/skeletal mesh LODs */
	enum { MAX_MESH_LOD_COUNT = 8 };

	/** Maximum number of immutable samplers in a PSO. */
	enum
	{
		MaxImmutableSamplers = 2
	};

	/** The maximum number of vertex elements which can be used by a vertex declaration. */
	enum
	{
		MaxVertexElementCount = 16,
		MaxVertexElementCount_NumBits = 4,
	};
	static_assert(MaxVertexElementCount <= (1 << MaxVertexElementCount_NumBits), "MaxVertexElementCount will not fit on MaxVertexElementCount_NumBits");

	/** The alignment in bytes between elements of array shader parameters. */
	enum { ShaderArrayElementAlignBytes = 16 };

	/** The number of render-targets that may be simultaneously written to. */
	enum
	{
		MaxSimultaneousRenderTargets = 8,
		MaxSimultaneousRenderTargets_NumBits = 3,
	};
	static_assert(MaxSimultaneousRenderTargets <= (1 << MaxSimultaneousRenderTargets_NumBits), "MaxSimultaneousRenderTargets will not fit on MaxSimultaneousRenderTargets_NumBits");

	/** The number of UAVs that may be simultaneously bound to a shader. */
	enum { MaxSimultaneousUAVs = 8 };

	/**
 *	Resource usage flags - for vertex and index buffers.
 */
	enum EBufferUsageFlags
	{
		BUF_None = 0x0000,


		// Mutually exclusive write-frequency flags

		/** The buffer will be written to once. */
		BUF_Static = 0x0001,

		/**
		* The buffer will be written to occasionally, GPU read only, CPU write only.  The data lifetime is until the next update, or the buffer is destroyed.
		*/
		BUF_Dynamic = 0x0002,

		/** The buffer's data will have a lifetime of one frame.  It MUST be written to each frame, or a new one created each frame. */
		BUF_Volatile = 0x0004,

		// Mutually exclusive bind flags.
		BUF_UnorderedAccess = 0x0008, // Allows an unordered access view to be created for the buffer.

		/** Create a byte address buffer, which is basically a structured buffer with a uint32 type. */
		BUF_ByteAddressBuffer = 0x0020,

		/** Buffer that the GPU will use as a source for a copy. */
		BUF_SourceCopy = 0x0040,

		/** Create a buffer that can be bound as a stream output target. */
		BUF_StreamOutput = 0x0080,

		/** Create a buffer which contains the arguments used by DispatchIndirect or DrawIndirect. */
		BUF_DrawIndirect = 0x0100,

		/**
		* Create a buffer that can be bound as a shader resource.
		* This is only needed for buffer types which wouldn't ordinarily be used as a shader resource, like a vertex buffer.
		*/
		BUF_ShaderResource = 0x0200,

		/**
		* Request that this buffer is directly CPU accessible
		* (@todo josh: this is probably temporary and will go away in a few months)
		*/
		BUF_KeepCPUAccessible = 0x0400,

		/**
		* Provide information that this buffer will contain only one vertex, which should be delivered to every primitive drawn.
		* This is necessary for OpenGL implementations, which need to handle this case very differently (and can't handle GL_HALF_FLOAT in such vertices at all).
		*/
		BUF_ZeroStride = 0x0800,

		/** Buffer should go in fast vram (hint only). Requires BUF_Transient */
		BUF_FastVRAM = 0x1000,

		/** Buffer should be allocated from transient memory. */
		BUF_Transient = 0x2000,

		/** Create a buffer that can be shared with an external RHI or process. */
		BUF_Shared = 0x4000,

		/**
			* Buffer contains opaque ray tracing acceleration structure data.
			* Resources with this flag can't be bound directly to any shader stage and only can be used with ray tracing APIs.
			* This flag is mutually exclusive with all other buffer flags except BUF_Static.
		*/
		BUF_AccelerationStructure = 0x8000,

		BUF_VertexBuffer = 0x10000,
		BUF_IndexBuffer = 0x20000,
		BUF_StructuredBuffer = 0x40000,

		// Helper bit-masks
		BUF_AnyDynamic = (BUF_Dynamic | BUF_Volatile),
	};

	/** Flags used for texture creation */
	enum ETextureCreateFlags : int32_t
	{
		TexCreate_None = 0,

		// Texture can be used as a render target
		TexCreate_RenderTargetable = 1 << 0,
		// Texture can be used as a resolve target
		TexCreate_ResolveTargetable = 1 << 1,
		// Texture can be used as a depth-stencil target.
		TexCreate_DepthStencilTargetable = 1 << 2,
		// Texture can be used as a shader resource.
		TexCreate_ShaderResource = 1 << 3,
		// Texture is encoded in sRGB gamma space
		TexCreate_SRGB = 1 << 4,
		// Texture data is writable by the CPU
		TexCreate_CPUWritable = 1 << 5,
		// Texture will be created with an un-tiled format
		TexCreate_NoTiling = 1 << 6,
		// Texture will be used for video decode
		TexCreate_VideoDecode = 1 << 7,
		// Texture that may be updated every frame
		TexCreate_Dynamic = 1 << 8,
		// Texture will be used as a render pass attachment that will be read from
		TexCreate_InputAttachmentRead = 1 << 9,
		/** Texture represents a foveation attachment */
		TexCreate_Foveation = 1 << 10,
		// Disable automatic defragmentation if the initial texture memory allocation fails.
		//TexCreate_DisableAutoDefrag		 UE_DEPRECATED(4.26, "TexCreate_DisableAutoDefrag is deprecated and getting removed; please don't use.") = 1 << 10,
		// This texture has no GPU or CPU backing. It only exists in tile memory on TBDR GPUs (i.e., mobile).
		TexCreate_Memoryless = 1 << 11,
		// Create the texture with the flag that allows mip generation later, only applicable to D3D11
		TexCreate_GenerateMipCapable = 1 << 12,
		// The texture can be partially allocated in fastvram
		TexCreate_FastVRAMPartialAlloc = 1 << 13,
		// Do not create associated shader resource view, only applicable to D3D11 and D3D12
		TexCreate_DisableSRVCreation = 1 << 14,
		// Do not allow Delta Color Compression (DCC) to be used with this texture
		TexCreate_DisableDCC = 1 << 15,
		// UnorderedAccessView (DX11 only)
		// Warning: Causes additional synchronization between draw calls when using a render target allocated with this flag, use sparingly
		// See: GCNPerformanceTweets.pdf Tip 37
		TexCreate_UAV = 1 << 16,
		// Render target texture that will be displayed on screen (back buffer)
		TexCreate_Presentable = 1 << 17,
		// Texture data is accessible by the CPU
		TexCreate_CPUReadback = 1 << 18,
		// Texture was processed offline (via a texture conversion process for the current platform)
		TexCreate_OfflineProcessed = 1 << 19,
		// Texture needs to go in fast VRAM if available (HINT only)
		TexCreate_FastVRAM = 1 << 20,
		// by default the texture is not showing up in the list - this is to reduce clutter, using the FULL option this can be ignored
		TexCreate_HideInVisualizeTexture = 1 << 21,
		// Texture should be created in virtual memory, with no physical memory allocation made
		// You must make further calls to RHIVirtualTextureSetFirstMipInMemory to allocate physical memory
		// and RHIVirtualTextureSetFirstMipVisible to map the first mip visible to the GPU
		TexCreate_Virtual = 1 << 22,
		// Creates a RenderTargetView for each array slice of the texture
		// Warning: if this was specified when the resource was created, you can't use SV_RenderTargetArrayIndex to route to other slices!
		TexCreate_TargetArraySlicesIndependently = 1 << 23,
		// Texture that may be shared with DX9 or other devices
		TexCreate_Shared = 1 << 24,
		// RenderTarget will not use full-texture fast clear functionality.
		TexCreate_NoFastClear = 1 << 25,
		// Texture is a depth stencil resolve target
		TexCreate_DepthStencilResolveTarget = 1 << 26,
		// Flag used to indicted this texture is a streamable 2D texture, and should be counted towards the texture streaming pool budget.
		TexCreate_Streamable = 1 << 27,
		// Render target will not FinalizeFastClear; Caches and meta data will be flushed, but clearing will be skipped (avoids potentially trashing metadata)
		TexCreate_NoFastClearFinalize = 1 << 28,
		// Hint to the driver that this resource is managed properly by the engine for Alternate-Frame-Rendering in mGPU usage.
		TexCreate_AFRManual = 1 << 29,
		// Workaround for 128^3 volume textures getting bloated 4x due to tiling mode on PS4
		TexCreate_ReduceMemoryWithTilingMode = 1 << 30,
		/** Texture should be allocated from transient memory. */
		TexCreate_Transient = 1 << 31
	};
}