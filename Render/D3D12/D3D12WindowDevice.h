#pragma once
#include "D3D12/D3D12RHICommon.h"

struct ID3D12Device;

namespace RenderCore
{
	class D3D12Device :public D3D12AdapterChild, public std::enable_shared_from_this<D3D12Device>
	{
	public:
		D3D12Device(std::weak_ptr<D3D12Adapter> InAdapter);
		virtual ~D3D12Device();

		/** Initialized members*/
		void Initialize();

		void CreateCommandContexts();

		void InitPlatformSpecific();
		/**
		* Cleanup the device.
		* This function must be called from the main game thread.
		*/
		virtual void Cleanup();

		ID3D12Device* GetDevice();
	};
}