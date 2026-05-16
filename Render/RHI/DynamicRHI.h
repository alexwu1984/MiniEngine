#pragma once
#include "win/win32.h"
#include "RHI/RHIDefinitions.h"
#include "core/color.h"
#include <functional>

namespace RenderCore
{
	enum class RHIAPIType
	{
		E_D3D11,
		E_D3D12,
	};

	class RHICommandContext;
	class RHIViewPort;
	class RHIVertexBuffer;
	class RHIIndexBuffer;
	class RHIUniformBuffer;
	class RHITexture2D;
	class RHITexture1D;
	class RHIRenderTarget;
	class RHIVertexShader;
	class RHIPixelShader;
	class RHIComputeShader;
	class RHIVertexDeclare;
	struct RHIShaderMacro;
	struct SamplerStateInitializerRHI;
	class RHISamplerState;
	struct RasterizerStateInitializerRHI;
	class RHIRasterizerState;
	class BlendStateInitializerRHI;
	class RHIBlendState;
	struct DepthStencilStateInitializerRHI;
	class RHIDepthStencilState;
	class RHITextureCube;
	class RHIUnorderedAccessView;
	class RHIStructuredBuffer;
	class RHITilePool;

	/** Thread-safe latch: any D3D thread may set it when the GPU device is lost (removal / Present failure). */
	bool RHI_HasFatalDeviceLossForShell();
	/** Win32: thread that runs the shell message loop (e.g. wWinMain / PeekMessage). Used to WM_QUIT immediately from render threads. */
	void RHI_SetShellMessageThreadIdForFatalDeviceLossQuit(uint32_t win32ThreadId);
	/**
	 * One-shot log + sets RHI_HasFatalDeviceLossForShell + posts WM_QUIT to the shell thread when its id was registered.
	 * Called from D3D11/D3D12 Present / RHIBeginFrame paths.
	 */
	void RHI_NotifyFatalGpuDeviceLoss(const wchar_t* apiLabel, HRESULT hrPresentOrZero, HRESULT hrDeviceRemovedReason);

	class DynamicRHI
	{
	public:
		DynamicRHI() = default;
		virtual ~DynamicRHI();

		/** Initializes the RHI; separate from IDynamicRHIModule::CreateRHI so that GDynamicRHI is set when it is called. */
		virtual void Init() = 0;

		/**
		 * RHI synchronization contract — engine / scene code should use these names only (avoid API-specific primitives in callers).
		 * Caller must drain the game-thread render-queue (e.g. FlushRenderingCommands) before these when flushing recorded work matters.
		 */
		/** Ensures deferred RHI work reaches the scheduler (recording→submission path flushed; GPU may still be busy). Each backend interprets appropriately. */
		virtual void RHIFlushSubmissionPipeline() {}
		/** Block until GPU work submitted through this RHI is idle (normally implies RHIFlushSubmissionPipeline internally). Default is no-op. */
		virtual void RHIWaitForGpuIdle() {}
		/** Suggested parallel frame-slot count for transient per-frame GPU resources vs pipeline overlap (swap-chain / buffering heuristic); 0 reserved = invalid, treat as≥1 externally. */
		virtual uint32_t RHIRecommendedParallelFrameResourceSlots() const { return 2u; }

		/**
		 * Polled only from the Win32 message-loop thread (e.g. AppWindow before Idle), not from the game tick / render worker.
		 * When true, the shell should terminate the loop (PostQuitMessage) so COM/D3D are not called repeatedly after device removal (_com_error).
		 */
		virtual bool RHIHasFatalDeviceLossForShell() const { return RHI_HasFatalDeviceLossForShell(); }

		/** Back-compat: same as RHIWaitForGpuIdle(). */
		virtual void Wait() { RHIWaitForGpuIdle(); }

		/** Shutdown the RHI; handle shutdown and resource destruction before the RHI's actual destructor is called (so that all resources of the RHI are still available for shutdown). */
		virtual void Shutdown() = 0;

		virtual const TCHAR* GetName() = 0;

		/** Backend API; used with MaterialBase::WantsRHIBindless() to decide RHI_BINDLESS shader macros (D3D12 only). */
		virtual RHIAPIType GetRHIAPIType() const { return RHIAPIType::E_D3D11; }

		// Frame boundary hooks: engine systems (e.g. transient pooling) can attach per-frame work here.
		using FrameCallback = std::function<void()>;
		void SetFrameCallbacks(FrameCallback InBeginFrame, FrameCallback InEndFrame)
		{
			BeginFrameCallback = std::move(InBeginFrame);
			EndFrameCallback = std::move(InEndFrame);
		}

		virtual void RHIBeginFrame()
		{
			if (BeginFrameCallback)
				BeginFrameCallback();
		}

		virtual void RHIEndFrame()
		{
			if (EndFrameCallback)
				EndFrameCallback();
		}

		virtual std::shared_ptr< RHICommandContext> GetDefaultCommandContext() = 0;
		virtual std::shared_ptr< RHICommandContext> GetDefaultAsyncComputeContext() = 0;
		virtual std::shared_ptr< RHIViewPort> RHICreateViewport(void* WindowHandle, uint32_t SizeX, uint32_t SizeY, bool bIsFullscreen, EPixelFormat PreferredPixelFormat) { return nullptr; }
		virtual std::shared_ptr< RHIVertexBuffer> RHICreateVertexBuffer(const void* InData, EBufferUsageFlags InUsage, int32_t StrideByteWidth, int32_t Count) = 0;
		virtual void RHIUpdateVertexBuffer(std::shared_ptr< RHIVertexBuffer> VertexBuffer, const void* InData, int32_t nVertex, int32_t sizePerVertex) = 0;
		virtual std::shared_ptr< RHIIndexBuffer> RHICreateIndexBuffer(const uint16_t* InData, EBufferUsageFlags InUsage, int32_t IndexCount) = 0;
		virtual std::shared_ptr< RHIIndexBuffer> RHICreateIndexBuffer(const uint32_t* InData, EBufferUsageFlags InUsage, int32_t IndexCount) = 0;

		virtual std::shared_ptr< RHIUniformBuffer> RHICreateUniformBuffer(uint32_t ConstantBufferSize) = 0;
		virtual std::shared_ptr< RHIUniformBuffer> RHICreateUniformBuffer(const void* Contents, uint32_t ConstantBufferSize) = 0;

		/**
		 * Create an HLSL `StructuredBuffer<T>` SRV; bound at draw time via RHISetShaderStructuredBuffer.
		 * Pass BUF_Dynamic + Initial=null for buffers updated each frame (clustered light table, etc.).
		 * Default returns null so backends can opt in incrementally; engine code must check the result.
		 */
		virtual std::shared_ptr< RHIStructuredBuffer> RHICreateStructuredBuffer(uint32_t ElementStride, uint32_t ElementCount, EBufferUsageFlags Usage, const void* InitialData) { return nullptr; }

		virtual std::shared_ptr< RHITexture2D> RHICreateTexture2D(EPixelFormat format, int32_t Flags, int32_t width, int32_t height,uint32_t NumMips, void* pBuffer = nullptr, int rowBytes = 0) = 0;
		virtual std::shared_ptr< RHITexture2D> RHICreateTexture2D(const std::wstring& FileName) = 0;
		virtual std::shared_ptr< RHITexture2D> RHICreateTexture2D(const core::FLinearColor& Color) = 0;
		virtual std::shared_ptr< RHITexture2D> RHICreateHDRTexture2D(const std::wstring& FileName) = 0;
		virtual std::shared_ptr< RHITexture1D> RHICreateTexture1D(EPixelFormat Format, int32_t Flags, int32_t SizeX, void* InBuffer, int RowBytes) = 0;
		virtual std::shared_ptr< RHITextureCube> RHICreateTextureCube(EPixelFormat Format, int32_t SizeX, int32_t SizeY, uint32_t NumMips, bool CreateDepth) = 0;
		virtual std::shared_ptr< RHIUnorderedAccessView> RHICreateUnorderedAccessView(EPixelFormat Format, int32_t SizeX, int32_t SizeY) = 0;
		virtual std::shared_ptr< RHIUnorderedAccessView> RHICreateUnorderedAccessView(std::shared_ptr< RHITexture2D> Tex2D) = 0;
		/** When true (D3D12 only), pooled UAV textures may use placed resources sharing VRAM within an aliasing heap. Other backends ignore the hint. */
		virtual std::shared_ptr<RHIUnorderedAccessView> RHICreateUnorderedAccessViewForTransientPool(
			EPixelFormat Format, int32_t SizeX, int32_t SizeY, bool bPreferAliasingHeap)
		{
			(void)bPreferAliasingHeap;
			return RHICreateUnorderedAccessView(Format, SizeX, SizeY);
		}

		virtual std::shared_ptr< RHIRenderTarget> RHICreateRenderTarget(EPixelFormat Format, int32_t SizeX, int32_t SizeY, uint32_t NumMips,bool IsMultiSampled, bool CreateDepth) = 0;

		virtual std::shared_ptr< RHIVertexShader> RHICreateVertexShader(const std::wstring& FileName, const std::string& VSMain, const RHIVertexDeclare& VertexDeclare, const std::vector<RHIShaderMacro>& MacroDefines ) = 0;
		virtual std::shared_ptr< RHIPixelShader> RHICreatePixelShader(const std::wstring& FileName, const std::string& PSMain, const std::vector<RHIShaderMacro>& MacroDefines) = 0;
		virtual std::shared_ptr< RHIComputeShader> RHICreateComputeShader(const std::wstring& FileName, const std::string& CSMain, const std::vector<RHIShaderMacro>& MacroDefines) = 0;

		virtual std::shared_ptr< RHISamplerState> RHICreateSamplerState(const SamplerStateInitializerRHI& Initializer) = 0;
		virtual std::shared_ptr< RHIRasterizerState> RHICreateRasterizerState(const RasterizerStateInitializerRHI& Initializer) = 0;
		virtual std::shared_ptr< RHIBlendState> RHICreateBlendState(const BlendStateInitializerRHI& Initializer) = 0;
		virtual std::shared_ptr< RHIDepthStencilState> RHICreateDepthStencilState(const DepthStencilStateInitializerRHI& Initializer) = 0;
		virtual std::shared_ptr< RHITilePool> RHICreateTilePool(std::shared_ptr< RHITexture2D> Tex2D) = 0;
	
	private:
		FrameCallback BeginFrameCallback;
		FrameCallback EndFrameCallback;
	};

	class IDynamicRHIModule /*: public IModuleInterface*/
	{
	public:
		virtual ~IDynamicRHIModule() {}
		/** Checks whether the RHI is supported by the current system. */
		virtual bool IsSupported() = 0;

		/** Creates a new instance of the dynamic RHI implemented by the module. */
		virtual std::shared_ptr<DynamicRHI> CreateRHI() = 0;
	};

	bool IsRHIDeviceAMD();

	// to trigger GPU specific optimizations and fallbacks
	bool IsRHIDeviceIntel();

	// to trigger GPU specific optimizations and fallbacks
	bool IsRHIDeviceNVIDIA();

	/**
*	Each platform that utilizes dynamic RHIs should implement this function
*	Called to create the instance of the dynamic RHI.
*/
	std::shared_ptr<DynamicRHI> PlatformCreateDynamicRHI(RHIAPIType apiType);
	std::shared_ptr<DynamicRHI> GetDynamicRHI();
	void ReleasePlatformModule();

	extern uint32_t GRHIVendorId;
	extern std::wstring GRHIAdapterName;
	extern uint32_t GRHIDeviceId;
	extern uint32_t GRHIDeviceRevision;
}

