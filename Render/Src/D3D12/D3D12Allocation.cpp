#include "D3D12/D3D12Allocation.h"
#include "D3D12/D3D12WindowDevice.h"
#include "D3D12/D3D12DirectCommandListManager.h"

namespace RenderCore
{
	std::vector<win32::com_ptr<ID3D12DescriptorHeap> > FD3D12ResourceAllocator::sm_DescriptorPool;

	FD3D12ResourceAllocator::FD3D12ResourceAllocator(std::weak_ptr<FD3D12Device> InParentDevice,
		D3D12_DESCRIPTOR_HEAP_TYPE InHeapType)
		: FD3D12DeviceChild(InParentDevice)
		, HeapType(InHeapType)
		, CurrentHeap(nullptr)
		, DescriptorSize(0)
	{

	}

	D3D12_CPU_DESCRIPTOR_HANDLE FD3D12ResourceAllocator::Allocate(uint32_t Count)
	{
		if (CurrentHeap == nullptr || RemainingFreeHandles < Count)
		{
			CurrentHeap = RequestNewHeap(GetParentDevice(),HeapType);
			CurrentCpuAddress = CurrentHeap->GetCPUDescriptorHandleForHeapStart();
			RemainingFreeHandles = sm_NumDescriptorsPerHeap;
			if (DescriptorSize == 0)
			{
				DescriptorSize = GetParentDevice()->GetDevice()->GetDescriptorHandleIncrementSize(HeapType);
			}
		}

		D3D12_CPU_DESCRIPTOR_HANDLE Result = CurrentCpuAddress;
		CurrentCpuAddress.ptr += Count * DescriptorSize;
		RemainingFreeHandles -= Count;

		return Result;
	}

	void FD3D12ResourceAllocator::DestroyAll()
	{
		sm_DescriptorPool.clear();
	}

	ID3D12DescriptorHeap* FD3D12ResourceAllocator::RequestNewHeap(std::shared_ptr<FD3D12Device> InDevice,D3D12_DESCRIPTOR_HEAP_TYPE Type)
	{
		Assert(InDevice.get());
		D3D12_DESCRIPTOR_HEAP_DESC Desc = {};
		Desc.NumDescriptors = sm_NumDescriptorsPerHeap;
		Desc.Type = Type;
		Desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		Desc.NodeMask = 1;

		win32::com_ptr<ID3D12DescriptorHeap> descriptorHeap;
		VERIFYD3DRESULT(InDevice->GetDevice()->CreateDescriptorHeap(&Desc, IID_PPV_ARGS(descriptorHeap.get_init_ref())));
		sm_DescriptorPool.emplace_back(descriptorHeap);
		return descriptorHeap.get();
	}

	LinearAllocationPage::LinearAllocationPage(std::weak_ptr<FD3D12Device> ParentDevice, ID3D12Resource* InResource, 
												D3D12_RESOURCE_STATES InitialState, D3D12_RESOURCE_DESC const& InDesc, 
												D3D12_HEAP_TYPE InHeapType /*= D3D12_HEAP_TYPE_DEFAULT*/)
		:FD3D12Resource(ParentDevice,InResource,InitialState,InDesc,InHeapType)
	{
		Map();
	}

	LinearAllocationPage::~LinearAllocationPage()
	{
		Unmap();
	}

	ELinearAllocatorType LinearAllocationPageManager::ms_TypeCounter = GpuExclusive;

	LinearAllocationPageManager::LinearAllocationPageManager(std::weak_ptr<FD3D12Device> InParentDevice)
		:FD3D12DeviceChild(InParentDevice)
	{
		AllocatorType = ms_TypeCounter;
		ms_TypeCounter = (ELinearAllocatorType)(ms_TypeCounter + 1);
		Assert(ms_TypeCounter <= NumAllocatorTypes);
	}

	LinearAllocationPage* LinearAllocationPageManager::RequestPage()
	{
		LinearAllocationPage* Page = nullptr;

		if (!RetiredPages.empty() )
		{
			auto QueueType = GetCommandQueueType(GET_QUEUE_TYPE(RetiredPages.front()->GetFenceValue()));
			auto& CommandListManager = GetParentDevice()->GetCommandListManager(QueueType);
			if (CommandListManager.GetFence().IsFenceComplete(RetiredPages.front()->GetFenceValue()))
			{
				Page = RetiredPages.front();
				RetiredPages.pop();
			}
		}
		else
		{
			Page = CreateNewPage();
			StandardPagePool.push(Page);
		}
		return Page;
	}

	void LinearAllocationPageManager::DiscardStandardPages(uint64_t FenceID, const std::vector<LinearAllocationPage*>& Pages)
	{
		for (auto Iter = Pages.begin(); Iter != Pages.end(); ++Iter)
		{
			(*Iter)->SetFenceValue(FenceID);
			RetiredPages.push(*Iter);
		}
	}

	void LinearAllocationPageManager::DiscardLargePages(uint64_t FenceID, const std::vector<LinearAllocationPage*>& Pages)
	{
		if (!LargePagePool.empty())
		{
			auto QueueType = GetCommandQueueType(GET_QUEUE_TYPE(LargePagePool.front()->GetFenceValue()));
			auto& CommandListManager = GetParentDevice()->GetCommandListManager(QueueType);

			while (!LargePagePool.empty() && CommandListManager.GetFence().IsFenceComplete(LargePagePool.front()->GetFenceValue()))
			{
				LinearAllocationPage* Page = LargePagePool.front();
				Page->Release();
				LargePagePool.pop();
			}
		}

		for (auto Iter = Pages.begin(); Iter != Pages.end(); ++Iter)
		{
			(*Iter)->SetFenceValue(FenceID);
			LargePagePool.push(*Iter);
		}
	}

	LinearAllocationPage* LinearAllocationPageManager::CreateNewPage(size_t PageSize /*= 0*/)
	{
		D3D12_HEAP_PROPERTIES HeapProps;
		HeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		HeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		HeapProps.CreationNodeMask = 1;
		HeapProps.VisibleNodeMask = 1;

		D3D12_RESOURCE_DESC ResourceDesc;
		ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		ResourceDesc.Alignment = 0;
		ResourceDesc.Height = 1;
		//uboResourceDesc.Width = (sizeof(m_uboVS) + 255) & ~255;
		ResourceDesc.DepthOrArraySize = 1;
		ResourceDesc.MipLevels = 1;
		ResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
		ResourceDesc.SampleDesc.Count = 1;
		ResourceDesc.SampleDesc.Quality = 0;
		ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		ResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		D3D12_RESOURCE_STATES DefaultUsage;
		if (AllocatorType == ELinearAllocatorType::GpuExclusive)
		{
			HeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
			ResourceDesc.Width = PageSize == 0 ? GpuAllocatorPageSize : PageSize;
			ResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
			DefaultUsage = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		}
		else
		{
			HeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
			ResourceDesc.Width = PageSize == 0 ? CpuAllocatorPageSize : PageSize;
			ResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
			DefaultUsage = D3D12_RESOURCE_STATE_GENERIC_READ;
		}

		ID3D12Resource* pBuffer = nullptr;
		VERIFYD3DRESULT(GetParentDevice()->GetDevice()->CreateCommittedResource(
			&HeapProps,
			D3D12_HEAP_FLAG_NONE,
			&ResourceDesc,
			DefaultUsage,
			nullptr,
			IID_PPV_ARGS(&pBuffer)));

		pBuffer->SetName(L"LinearAllocatorPage");

		LinearAllocationPage * AllocationPage =  new LinearAllocationPage(GetParentDevice(),pBuffer, DefaultUsage,ResourceDesc, HeapProps.Type);
		AllocationPage->AddRef();
		return AllocationPage;
	}

	void LinearAllocationPageManager::Destroy()
	{
		while (!LargePagePool.empty())
		{
			LargePagePool.front()->Release();
			LargePagePool.pop();
		}
		while (!StandardPagePool.empty())
		{
			StandardPagePool.front()->Release();
			StandardPagePool.pop();
		}

		while (!RetiredPages.empty())
		{
			RetiredPages.front()->Release();
			RetiredPages.pop();
		}
	}

	ELinearAllocatorType LinearAllocationPageManager::GetAllocatorType() const
	{
		return AllocatorType;
	}

	LinearAllocator::LinearAllocator(ELinearAllocatorType Type, std::weak_ptr<FD3D12Device> ParentDevice)
		:FD3D12DeviceChild(ParentDevice)
		,m_AllocatorType(Type)
		, m_CurrentPage(nullptr)
		, m_CurrentOffset(0)
	{
		Assert(Type > ELinearAllocatorType::InvalidAllocator && Type < ELinearAllocatorType::NumAllocatorTypes);
		m_PageSize = (Type == ELinearAllocatorType::GpuExclusive ? GpuAllocatorPageSize : CpuAllocatorPageSize);
	}

	FAllocation LinearAllocator::Allocate(size_t SizeInBytes, size_t Alignment /*= DEFAULT_ALIGN*/)
	{
		const size_t AlignmentMask = Alignment - 1;
		Assert((AlignmentMask & Alignment) == 0);
		const size_t AlignedSize = math::AlignUpWithMask(SizeInBytes, AlignmentMask);

		if (AlignedSize > m_PageSize)
			return AllocateLargePage(AlignedSize);

		m_CurrentOffset = math::AlignUp(m_CurrentOffset, Alignment);
		if (m_CurrentOffset + AlignedSize > m_PageSize)
		{
			Assert(m_CurrentPage != nullptr);
			// make sure current page is in UsingPages
			m_StandardPages.push_back(m_CurrentPage);
			m_CurrentPage = nullptr;
		}

		if (m_CurrentPage == nullptr)
		{
			m_CurrentPage = GetParentDevice()->GetLinearPageManager(m_AllocatorType).RequestPage();
			Assert(m_CurrentPage != nullptr);
			m_CurrentOffset = 0;
		}

		Assert(m_CurrentPage != nullptr);
		FAllocation allocation;
		allocation.D3d12Resource = m_CurrentPage->GetResource();
		allocation.Offset = m_CurrentOffset;
		allocation.CPU = (uint8_t*)m_CurrentPage->GetResourceBaseAddress() + m_CurrentOffset;
		allocation.GpuAddress = m_CurrentPage->GetGPUVirtualAddress() + m_CurrentOffset;
		m_CurrentOffset += AlignedSize;
		return allocation;
	}

	void LinearAllocator::CleanupUsedPages(uint64_t FenceID)
	{
		if (m_CurrentPage != nullptr)
		{
			m_StandardPages.push_back(m_CurrentPage);
			m_CurrentPage = nullptr;
			m_CurrentOffset = 0;
		}
		for (auto Iter = m_StandardPages.begin(); Iter != m_StandardPages.end(); ++Iter)
		{
			(*Iter)->SetFenceValue(FenceID);
		}

		GetParentDevice()->GetLinearPageManager(m_AllocatorType).DiscardStandardPages(FenceID, m_StandardPages);
		m_StandardPages.clear();

		GetParentDevice()->GetLinearPageManager(m_AllocatorType).DiscardLargePages(FenceID, m_LargePages);
		m_LargePages.clear();
	}

	FAllocation LinearAllocator::AllocateLargePage(size_t SizeInBytes)
	{
		LinearAllocationPage* Page = GetParentDevice()->GetLinearPageManager(m_AllocatorType).CreateNewPage(SizeInBytes);
		m_LargePages.push_back(Page);

		FAllocation allocation;
		allocation.D3d12Resource = Page->GetResource();
		allocation.Offset = 0;
		allocation.CPU = (uint8_t*)Page->GetResourceBaseAddress();
		allocation.GpuAddress = Page->GetGPUVirtualAddress();
		return allocation;
	}

}