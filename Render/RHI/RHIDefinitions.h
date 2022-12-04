#pragma once
#include "core/inc.h"

namespace RenderCore
{
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
}