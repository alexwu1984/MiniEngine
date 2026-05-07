#include "D3D12/D3D12GpuTimestampRing.h"
#include "D3D12/D3D12Adapter.h"
#include "D3D12/D3D12DirectCommandListManager.h"
#include "D3D12/D3D12WindowDevice.h"
#include "core/logger.h"

using namespace RenderCore;

FD3D12GpuTimestampRing::FD3D12GpuTimestampRing(std::weak_ptr<FD3D12Device> InDevice)
	: ParentDevice(std::move(InDevice))
{
}

FD3D12GpuTimestampRing::~FD3D12GpuTimestampRing()
{
	Destroy();
}

void FD3D12GpuTimestampRing::Destroy()
{
	std::lock_guard<std::mutex> Lock(Mutex);
	QueryHeap.reset();
	ReadbackBuffer.reset();
	bInitialized = false;
	bHasRecording = false;
	RecordingSeq = 0;
	NumQueriesWritten = 0;
	NamesThisFrame.clear();
	for (uint32_t i = 0; i < kRingFrames; ++i)
	{
		FenceValueWhenSlotSubmitted[i] = 0;
		bSlotFenceValid[i] = false;
		RecordingSeqWhenWritten[i] = 0;
		SlotBook[i] = {};
	}
	SlotToFenceForGpuRead = UINT32_MAX;
}

void FD3D12GpuTimestampRing::LazyInitInternal()
{
	if (bInitialized)
		return;

	std::shared_ptr<FD3D12Device> Dev = ParentDevice.lock();
	if (!Dev || !Dev->GetDevice())
		return;

	ID3D12Device* D = Dev->GetDevice();
	ID3D12CommandQueue* Q = Dev->GetD3DCommandQueue(ED3D12CommandQueueType::Default);
	if (!Q)
		return;

	UINT64 Freq = 0;
	if (FAILED(Q->GetTimestampFrequency(&Freq)) || Freq == 0)
	{
		core::LOG(core::log_war, L"D3D12 GPU timestamps: GetTimestampFrequency failed or zero; RDG GPU timings disabled.");
		return;
	}
	GpuMillisecondsPerTick = 1000.0 / double(Freq);

	const UINT HeapCount = kRingFrames * kMaxQueriesPerFrame;
	D3D12_QUERY_HEAP_DESC HeapDesc = {};
	HeapDesc.Count = HeapCount;
	HeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
	HeapDesc.NodeMask = 0;
	if (FAILED(D->CreateQueryHeap(&HeapDesc, IID_PPV_ARGS(QueryHeap.get_init_ref()))))
	{
		core::LOG(core::log_war, L"D3D12 GPU timestamps: CreateQueryHeap failed; RDG GPU timings disabled.");
		return;
	}

	const UINT64 BufferBytes = kRingFrames * kReadbackSlotStrideBytes;
	D3D12_HEAP_PROPERTIES HeapProps = {};
	HeapProps.Type = D3D12_HEAP_TYPE_READBACK;
	HeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	HeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	HeapProps.CreationNodeMask = 1;
	HeapProps.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC BufDesc = {};
	BufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	BufDesc.Alignment = 0;
	BufDesc.Width = BufferBytes;
	BufDesc.Height = 1;
	BufDesc.DepthOrArraySize = 1;
	BufDesc.MipLevels = 1;
	BufDesc.Format = DXGI_FORMAT_UNKNOWN;
	BufDesc.SampleDesc.Count = 1;
	BufDesc.SampleDesc.Quality = 0;
	BufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	BufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	if (FAILED(D->CreateCommittedResource(&HeapProps, D3D12_HEAP_FLAG_NONE, &BufDesc, D3D12_RESOURCE_STATE_COPY_DEST,
										 nullptr, IID_PPV_ARGS(ReadbackBuffer.get_init_ref()))))
	{
		QueryHeap.reset();
		core::LOG(core::log_war, L"D3D12 GPU timestamps: readback CreateCommittedResource failed; RDG GPU timings disabled.");
		return;
	}

	bInitialized = true;
}

void FD3D12GpuTimestampRing::BeginRecording(ID3D12GraphicsCommandList* Cmd)
{
	if (!Cmd)
		return;

	std::lock_guard<std::mutex> Lock(Mutex);
	LazyInitInternal();
	if (!bInitialized)
		return;

	RecordingSeq++;
	ActiveSlot = static_cast<uint32_t>(RecordingSeq % kRingFrames);
	HeapBaseQueryIndex = ActiveSlot * kMaxQueriesPerFrame;
	NumQueriesWritten = 0;
	NamesThisFrame.clear();

	Cmd->EndQuery(QueryHeap.get(), D3D12_QUERY_TYPE_TIMESTAMP, HeapBaseQueryIndex);
	NumQueriesWritten = 1;
	bHasRecording = true;
}

void FD3D12GpuTimestampRing::AfterPass(ID3D12GraphicsCommandList* Cmd, const char* PassNameUtf8)
{
	if (!Cmd || !PassNameUtf8)
		return;

	std::lock_guard<std::mutex> Lock(Mutex);
	if (!bInitialized || !bHasRecording)
		return;
	if (NumQueriesWritten >= kMaxQueriesPerFrame)
		return;

	Cmd->EndQuery(QueryHeap.get(), D3D12_QUERY_TYPE_TIMESTAMP, HeapBaseQueryIndex + NumQueriesWritten);
	if (PassNameUtf8[0])
		NamesThisFrame.emplace_back(PassNameUtf8);
	else
		NamesThisFrame.emplace_back("<pass>");
	NumQueriesWritten++;
}

void FD3D12GpuTimestampRing::EndRecordingResolve(ID3D12GraphicsCommandList* Cmd)
{
	if (!Cmd)
		return;

	std::lock_guard<std::mutex> Lock(Mutex);
	if (!bInitialized || !bHasRecording || NumQueriesWritten == 0 || !QueryHeap || !ReadbackBuffer)
	{
		bHasRecording = false;
		return;
	}

	const uint64_t DstOffsetBytes = uint64_t(ActiveSlot) * kReadbackSlotStrideBytes;
	Cmd->ResolveQueryData(QueryHeap.get(), D3D12_QUERY_TYPE_TIMESTAMP, HeapBaseQueryIndex, NumQueriesWritten, ReadbackBuffer.get(),
						  DstOffsetBytes);

	SlotBook[ActiveSlot].NumQueries = NumQueriesWritten;
	SlotBook[ActiveSlot].HeapBase = HeapBaseQueryIndex;
	SlotBook[ActiveSlot].ReadbackOffsetBytes = DstOffsetBytes;
	SlotBook[ActiveSlot].Names = NamesThisFrame;

	RecordingSeqWhenWritten[ActiveSlot] = RecordingSeq;
	SlotToFenceForGpuRead = ActiveSlot;
	bHasRecording = false;
}

void FD3D12GpuTimestampRing::NotifyAdapterFrameFence(uint64_t AdapterFrameFenceSignaledValue)
{
	std::lock_guard<std::mutex> Lock(Mutex);
	if (SlotToFenceForGpuRead == UINT32_MAX || SlotToFenceForGpuRead >= kRingFrames)
		return;
	const uint32_t S = SlotToFenceForGpuRead;
	FenceValueWhenSlotSubmitted[S] = AdapterFrameFenceSignaledValue;
	bSlotFenceValid[S] = true;
	SlotToFenceForGpuRead = UINT32_MAX;
}

void FD3D12GpuTimestampRing::TryConsume(std::vector<std::pair<std::string, double>>& OutPassGpuMs)
{
	OutPassGpuMs.clear();

	std::shared_ptr<FD3D12Device> Dev = ParentDevice.lock();
	std::shared_ptr<FD3D12Adapter> Adapter = Dev ? Dev->TryGetParentAdapter() : nullptr;
	if (!Adapter.get())
		return;

	FD3D12ManualFence& FrameFence = Adapter->GetFrameFence();

	std::lock_guard<std::mutex> Lock(Mutex);
	if (!bInitialized || !ReadbackBuffer)
		return;

	uint32_t BestSlot = UINT32_MAX;
	uint64_t BestSeq = 0;
	FrameFence.UpdateLastCompletedFence();
	for (uint32_t Slot = 0; Slot < kRingFrames; ++Slot)
	{
		if (!bSlotFenceValid[Slot])
			continue;
		const uint64_t NeedFence = FenceValueWhenSlotSubmitted[Slot];
		if (FrameFence.GetLastCompletedFenceFast() < NeedFence)
			continue;
		const uint64_t Seq = RecordingSeqWhenWritten[Slot];
		if (BestSlot == UINT32_MAX || Seq >= BestSeq)
		{
			BestSeq = Seq;
			BestSlot = Slot;
		}
	}

	if (BestSlot == UINT32_MAX)
		return;

	const uint64_t NeedFence = FenceValueWhenSlotSubmitted[BestSlot];
	FrameFence.WaitForFence(NeedFence);

	const FSlotBookkeeping& B = SlotBook[BestSlot];
	if (B.NumQueries < 2 || B.Names.size() + 1 != B.NumQueries)
	{
		bSlotFenceValid[BestSlot] = false;
		return;
	}

	void* Mapped = nullptr;
	D3D12_RANGE ReadRange{(SIZE_T)B.ReadbackOffsetBytes, (SIZE_T)(B.ReadbackOffsetBytes + B.NumQueries * sizeof(uint64_t))};
	if (FAILED(ReadbackBuffer->Map(0, &ReadRange, &Mapped)) || !Mapped)
	{
		bSlotFenceValid[BestSlot] = false;
		return;
	}

	const uint64_t* Ticks = reinterpret_cast<const uint64_t*>(static_cast<const uint8_t*>(Mapped) + B.ReadbackOffsetBytes);
	uint64_t Prev = Ticks[0];
	for (uint32_t i = 0; i < B.Names.size(); ++i)
	{
		const uint64_t Cur = Ticks[i + 1];
		const uint64_t D = (Cur >= Prev) ? (Cur - Prev) : 0ull;
		const double Ms = double(D) * GpuMillisecondsPerTick;
		OutPassGpuMs.emplace_back(B.Names[i], Ms);
		Prev = Cur;
	}

	ReadbackBuffer->Unmap(0, nullptr);
	bSlotFenceValid[BestSlot] = false;
}
