#pragma once

/**
 * @file RHIPathContracts.h
 *
 * -----------------------------------------------------------------------------
 * Phase A — pipeline contracts (read this like UE 4.x: FRHI paths + threading + flush + device locks).
 * -----------------------------------------------------------------------------
 *
 * Threads (MiniEngine)
 * - Game thread : simulation / World tick, SubmitSceneForRendering (build packet, enqueue RenderThread FIFO).
 * - Render thread (recording): AppendCommand FIFO, registered via RHI_RegisterRHIRecordingThread;
 *   constructs ID3D12CommandLists, sends work with ENQUEUE_RHI_SUBMIT_COMMAND.
 * - RHISubmit thread (execution, disabled with -norhithread): RHI_SubmitOrInline / RHI_ExecuteDeferredOrInline;
 *   runs ExecuteCommandLists and related CreatePipeline work; aligns with Queue Signal/Wait semantics.
 *
 * “May record barriers / dispatch lists?” vs GPU completion
 * - Central checks live in D3D12RHIRecording.h — a single per-thread ERHIRecordingContextScope stack.
 * - Draining the RenderThread queue (FlushRenderingCommands) is NOT the same as GPU idle; use RHIWaitForGpuIdle.
 *
 * -----------------------------------------------------------------------------
 * Flush semantics (summary)
 * - ERenderQueueFlushCategory — see Engine/Engine/Render/RenderQueueSynchronization.h (reload, throttle, load sync…).
 *
 * -----------------------------------------------------------------------------
 * Device-level locks (D3D12)
 * - RHI_D3D12ScopedQueueSubmitLock serializes Execute / Signal / Wait on the Queue; complements the submit FIFO worker
 *   for callers that touch the Queue without going through the same enqueue path (idle fences, Present-related work).
 *
 * -----------------------------------------------------------------------------
 * Top-level entry points — quick map for reviewers (grep anchors)
 *
 * | Entry / bucket                    | Recording | Submit / Device | Notes |
 * |-----------------------------------|-----------|-----------------|-------|
 * | RHIBeginFrame / RHIEndFrame       | yes       | indirect        | Push/Pop-if-top ERHIRecordingContextScope::RHIFrameBoundary |
 * | Frame body                        | yes       | via ENQUEUE_*   | D3D12RHI_ScopedRecordingContext InsideFrameTick after RHIBeginFrame |
 * | RHICreate* → InitializeTex/Buf   | yes       | yes             | D3D12RHI_ScopedRecordingContext OutsideFrameResourceUpload |
 * | RHIWaitForGpuIdle / BlockIdle     | optionally| Fence/Wait      | often ERHIRecordingContextScope::GpuDrainIdle |
 * | SwapChain resize / ctor / dtor    | scoped    | Signal etc.     | GpuDrainIdle / SwapChainMaintenance / DeviceLifetimeBatch per path |
 */
