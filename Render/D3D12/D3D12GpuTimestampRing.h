#pragma once

#include "win/com_ptr.h"
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

struct ID3D12Device;
struct ID3D12GraphicsCommandList;
struct ID3D12QueryHeap;
struct ID3D12Resource;

namespace RenderCore
{
	class FD3D12Device;

	/** RDG pass GPU timings via D3D12 TIMESTAMP queries + ResolveQueryData (multi-frame ring). */
	class FD3D12GpuTimestampRing final
	{
	public:
		static constexpr uint32_t kRingFrames = 4;
		static constexpr uint32_t kMaxQueriesPerFrame = 160;
		static constexpr uint64_t kReadbackSlotStrideBytes = 4096ull;

		explicit FD3D12GpuTimestampRing(std::weak_ptr<FD3D12Device> InDevice);
		~FD3D12GpuTimestampRing();

		void Destroy();

		void BeginRecording(ID3D12GraphicsCommandList* Cmd);
		void AfterPass(ID3D12GraphicsCommandList* Cmd, const char* PassNameUtf8);
		void EndRecordingResolve(ID3D12GraphicsCommandList* Cmd);

		void NotifyAdapterFrameFence(uint64_t AdapterFrameFenceSignaledValue);

		void TryConsume(std::vector<std::pair<std::string, double>>& OutPassGpuMs);

		bool IsInitialized() const { return bInitialized; }

	private:
		void LazyInitInternal();

		std::weak_ptr<FD3D12Device> ParentDevice;
		std::mutex Mutex;

		bool bInitialized = false;
		bool bHasRecording = false;
		double GpuMillisecondsPerTick = 0.0;

		win32::com_ptr<ID3D12QueryHeap> QueryHeap;
		win32::com_ptr<ID3D12Resource> ReadbackBuffer;

		uint64_t RecordingSeq = 0;
		uint32_t ActiveSlot = 0;
		uint32_t HeapBaseQueryIndex = 0;
		uint32_t NumQueriesWritten = 0;
		std::vector<std::string> NamesThisFrame;

		uint32_t SlotToFenceForGpuRead = UINT32_MAX;
		uint64_t FenceValueWhenSlotSubmitted[kRingFrames]{};

		struct FSlotBookkeeping
		{
			uint32_t NumQueries = 0;
			uint32_t HeapBase = 0;
			uint64_t ReadbackOffsetBytes = 0;
			std::vector<std::string> Names;
		};
		FSlotBookkeeping SlotBook[kRingFrames]{};
		bool bSlotFenceValid[kRingFrames]{};
		uint64_t RecordingSeqWhenWritten[kRingFrames]{};
	};

} // namespace RenderCore
