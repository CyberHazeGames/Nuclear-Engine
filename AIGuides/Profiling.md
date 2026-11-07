# CPU, GPU, memory, and performance profiling

Platform diagnostics live in [Diagnostics.h](../Nuclear.Platform/include/Nuclear.Platform/Diagnostics.h), with a Windows-only implementation in [Win32Diagnostics.cpp](../Nuclear.Platform/Source/Windows/Win32Diagnostics.cpp). Engine capture and UI live under [Profiling headers](../Nuclear.Engine/include/Profiling) and [Profiling sources](../Nuclear.Engine/Source/Profiling). Nuclear.Platform remains independent of Diligent, ImGui, and Nuclear.Engine.

## Platform CPU, GPU, and memory queries

`Nuclear::Platform::Diagnostics` works without window initialization:

- `GetTimestampNanoseconds` returns a monotonic elapsed-time clock, not CPU cycles or wall-clock/calendar time.
- `QueryCpuInfo` returns the CPU brand/vendor, physical cores, logical processors across all Windows processor groups, and OS page size.
- `QueryCpuCounters` returns cumulative process/system CPU times in **100-nanosecond units**, plus a monotonic sample timestamp in nanoseconds. Compute differences between samples. System counters are marked unavailable on multi-processor-group machines because Windows' `GetSystemTimes` would only cover one group; process counters remain usable.
- `QueryMemoryInfo` reports OS-visible total/available physical bytes, process resident working set and peak, private committed bytes, and the OS page-fault count. Working set includes shared pages; private commit is not reserved virtual address space.
- `EnumerateGpus` returns adapter names, PCI vendor/device IDs, opaque session-local adapter LUIDs, software flags, and capacity. `QueryGpuMemory` uses the **exact LUID** to query node-zero local/non-local memory budgets and usage for the current process. These are not whole-GPU usage figures, static VRAM capacity, or GPU utilization percentages.

Queries return `QueryResult`, distinguishing `Success`, `Unavailable`, and `Error`, with native diagnostic codes. Output references/lists remain unchanged on failure. Windows uses standard system APIs, DXGI, and the SDK's `dxgi.lib`/`psapi.lib`; no new external SDK is required. A GPU driver may not expose memory budgets.

## Enabling capture and opening the profiler

`EngineStartupDesc::AutoInitProfilingModule` defaults to true. The engine registers `ProfilingModule` after Graphics, when graphics is enabled, and releases profiler queries before graphics teardown. CPU-only module initialization is supported. `EnableProfiling` defaults to false; this avoids recording frame data until requested. Enable it in the startup description or at runtime:

```cpp
Nuclear::Core::EngineStartupDesc desc;
desc.EnableProfiling = true;

// During an initialized engine session:
Nuclear::Profiling::ProfilingModule::Get().SetEnabled(true);
```

Open **Window > Performance & Memory Profiler** in the editor, then enable **Capture**.
Tools can call `DrawProfilerWindow(bool* open)` inside their own active ImGui frame.
That detailed window shows frame timing history, CPU scope totals by thread, pipeline
counters, process memory, selected-adapter memory budgets, and explicit allocation categories.
The sample selector uses [retained UI](UI.md#retained-sample-selector): enable
**Performance & memory** for its compact process CPU, working-set, GPU memory/frame,
and tracked-allocation readouts. **Capture performance** and **Clear history**
control the same profiling module without depending on an ImGui context.

The engine starts capture after event dispatch and before queued main-thread work/module updates; it records client Update/Render CPU regions. GPU frame queries end before Present, while CPU frame timing ends after Present and therefore includes presentation blocking. Direct `Engine::BeginFrame`/`EndFrame` calls use the same capture boundary. Event dispatch and any work before `BeginFrame` are not included. The detailed profiler's FPS is derived from captured CPU wall time; the sample selector footer uses engine frame delta time. Neither is a GPU utilization measurement.

`GetSnapshot()` returns an owned, mutex-consistent copy. History retains at most **240 completed frames**, with at most **2048 CPU scope records per frame**. Process/system/selected-GPU memory counters are sampled at most every **500 ms** during capture. CPU utilization needs two interval samples; process CPU is normalized to all logical processors, from 0 to 100 percent. A saturated single core on a 16-thread machine is roughly 6.25 percent of machine capacity.

GPU results update their original frame IDs asynchronously. Invalid/disjoint results, pending results, unsupported features, and dropped submissions never become valid zero-duration samples. Drop/error counters are exposed. If multiple GPUs share the renderer's PCI IDs, automatic memory-adapter selection is withheld; select an exact LUID in the UI or with `SetGpuMemoryAdapter`. The memory adapter can intentionally differ from the rendering adapter, and the UI identifies the selected adapter.

## CPU scopes and elapsed queries

```cpp
#include <Profiling/Profiler.h>

void UpdateSimulation()
{
    PROFILE_CPU("Simulation");
    // Work whose inclusive wall time should be attributed to this frame/thread.
}
```

`CpuScope` copies a label of at most 128 characters into fixed storage on entry. Recording is mutex-protected and supports worker threads. Finish/join scopes before their frame ends; cross-frame or over-capacity records are dropped rather than attributed to a later frame. Inclusive nested scope totals overlap, and worker wall-time totals can exceed frame wall time. Capture/history storage still allocates memory; this is not a global allocation-free instrumentation system.

`Profiler_CPU` remains available as a simple monotonic timer: `End()` returns **milliseconds**, without resetting; `Reset()` starts a new measurement. It uses `steady_clock` consistently.

## GPU query API

[GpuQuery](../Nuclear.Engine/include/Profiling/GpuQuery.h) supports paired-timestamp **Duration**, single **Timestamp**, sample-count **Occlusion**, and **PipelineStatistics** queries. Initialize it with a live Diligent device and a fixed pool capacity of 1..256 submissions (default 8). Use the initialized device's immediate context, on its owning render thread. Initialization retains the device; destroy/shut down the query before renderer shutdown.

For Duration/Occlusion/PipelineStatistics, pair `Begin(context, frameId)` with `End(context)`. Timestamp uses `WriteTimestamp(context, frameId)`. Poll `TryGetResult` in later frames; `Pending` leaves output unchanged. The API does not flush, spin, wait for idle, grow pending pools, or overwrite in-flight results. A full pool returns `CapacityExceeded`. Timestamp results include raw ticks/frequency; duration results are **milliseconds**. Invalid timing frequency/order returns `Error`, also preserving output.

GPU data availability requires normal command submission/frame completion. In D3D11, timestamp data is not available until that frame's `FinishFrame`; ordinary engine Present supplies the lifecycle. A custom rendering loop must honor the Diligent context lifecycle. Duration uses independent timestamp pairs and permits overlapping regions. Native pipeline-statistics and occlusion queries of the same type must not overlap on a context. The profiling module already occupies a pipeline-statistics query while frame capture is enabled; disable frame capture when issuing your own overlapping pipeline-statistics query. OpenGL only populates clipping invocations in pipeline-statistics data; other counters are backend-dependent.

Graphics initialization requests timestamp, pipeline-statistics, and occlusion support as **optional** for D3D11/D3D12/OpenGL/Vulkan. Unsupported hardware does not fail startup; inspect query status and snapshot support flags. Only the D3D11 runtime path has been exercised by the profiling integration test so far.

The compatibility `Profiler_GPU`/`PROFILE_GPU` helper now uses the bounded query implementation. `End(double&)` writes a completed result in **seconds** and preserves output while pending; its previous by-value output bug is fixed. The legacy no-argument `End()` still returns zero while pending, so new callers should prefer explicit `GpuQueryStatus`. Static legacy helpers must call `Shutdown()` before renderer teardown to release their retained query/device references.

## Explicit memory accounting

[MemoryProfiler](../Nuclear.Engine/include/Profiling/MemoryProfiler.h) accounts only allocations the caller explicitly registers. It does not replace new/delete, intercept engine/vendor allocators, collect call stacks, or free caller memory:

```cpp
auto& memory = Nuclear::Profiling::MemoryProfiler::Get();
memory.TrackAllocation(pointer, sizeInBytes, "Mesh staging");
// Use the allocation normally.
memory.TrackFree(pointer); // Record before freeing/reusing the identity.
// The owning allocator performs the real deallocation.
```

The tracker is thread-safe and starts enabled. It retains at most **65,536 live identities**, **256 categories**, and 128 characters per category name. Null/zero-sized, duplicate, overflowing, or over-capacity records are rejected. The rejection count identifies incomplete accounting. Disabling new tracking still permits frees of existing records. Unknown/double frees return false. `ResetCounters` preserves live allocations while resetting peaks/cumulative counters. Explicit tracked bytes are shown separately from total process working set/private commit; they should not be interpreted as complete engine memory usage or GPU allocation attribution.

## Validation

Separate test projects are deliberately outside the solution's default build. Use the normalized Visual Studio environment from [BuildAndValidation.md](BuildAndValidation.md):

```powershell
msbuild Nuclear.Platform/Nuclear.Platform.DiagnosticsTests.vcxproj /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64
./Bin/DebugX64/Nuclear.Platform.DiagnosticsTests.exe

msbuild Nuclear.Platform/Nuclear.Profiling.CpuTests.vcxproj /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64
./Bin/DebugX64/Nuclear.Profiling.CpuTests.exe

msbuild Nuclear.Platform/Nuclear.Profiling.GpuTests.vcxproj /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64
Push-Location Samples
try { & ../Bin/DebugX64/Nuclear.Profiling.GpuTests.exe }
finally { Pop-Location }
```

On 2026-11-06, platform diagnostics passed in Debug and Release on Windows x64. The Debug CPU tests verified topology/memory queries, interval validity, concurrent explicit allocation accounting, CPU scopes/aggregation, cross-frame rejection, and history/capture limits without a window. The Debug GPU integration test rendered 70 frames with audio/scripting/physics/worker-thread initialization disabled, drew the profiler UI, retrieved seven results each from duration/timestamp/occlusion/pipeline-statistics queries, verified bounded pending capacity/frame correlation, and shut down cleanly on D3D11 with an NVIDIA GeForce RTX 5070 Ti. The full Debug solution build passed. This does not establish a full Release engine build, manual editor menu interaction, hardware-counter/temperature/clock profiling, or validation of D3D12/OpenGL/Vulkan runtime query behavior.
