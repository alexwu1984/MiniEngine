#pragma once
#include "D3D12/D3D12RHICommon.h"

namespace RenderCore
{
	struct FD3D12GenerateMipsPrivate;
	class RHITextureCube;
	class D3D12CommandContext;

	class FD3D12GenerateMips : public FD3D12AdapterChild
	{
	public:
		FD3D12GenerateMips(std::weak_ptr<FD3D12Adapter> InParent);
		~FD3D12GenerateMips();
		void InitResource();
		void GenerateForCube(std::shared_ptr<RHITextureCube> TextureCubeRHI, D3D12CommandContext* CommandContext);
	private:
		FD3D12GenerateMipsPrivate* d_ptr = nullptr;
	};
}