#pragma once
#include <Core/EngineModule.h>
#include <Profiling/GpuQuery.h>
#include <Nuclear.Platform/Diagnostics.h>
#include <string>
#include <string_view>
#include <vector>

namespace Nuclear::Profiling
{
	/// Inclusive CPU scope totals for one label/thread in a completed frame.
	struct CpuTiming
	{
		std::string mName; ///< Copied label, at most 128 characters.
		Uint64 mThread = 0; ///< Process-local hashed thread identity.
		Uint32 mCalls = 0; ///< Completed scopes with this label/thread.
		double mTotalMilliseconds = 0; ///< Inclusive wall time; nested regions overlap.
		double mMaximumMilliseconds = 0; ///< Longest invocation in this frame.
	};

	/// Bounded completed-frame history; GPU fields may arrive asynchronously.
	struct FrameProfile
	{
		Uint64 mFrame = 0; ///< Increasing profiler frame ID.
		double mCpuMilliseconds = 0; ///< BeginFrame through EndFrame wall time, including presentation.
		double mGpuMilliseconds = 0; ///< GPU work before presentation, meaningful only when mGpuValid.
		bool mGpuValid = false; ///< False for pending, unsupported, dropped, or invalid data.
		bool mPipelineValid = false; ///< Pipeline counts have arrived for this frame.
		PipelineStatistics mPipeline; ///< Main-context pipeline counts.
		std::vector<CpuTiming> mCpuTimings; ///< CPU scopes sorted by inclusive total wall time.
	};

	/// Half-second process/system samples, with explicit validity for pending/unavailable data.
	struct PerformanceCounters
	{
		Platform::Diagnostics::MemoryInfo mMemory; ///< Working set/private commit and physical memory.
		Platform::Diagnostics::GpuMemoryInfo mGpuMemory; ///< Selected GPU's process usage/budget.
		double mProcessCpuPercent = 0; ///< 0..100 normalized to all logical processors.
		double mSystemCpuPercent = 0; ///< 0..100; unavailable on multi-processor-group systems.
		bool mProcessCpuValid = false; ///< Requires two successful interval samples.
		bool mSystemCpuValid = false; ///< Requires supported counters and two interval samples.
		bool mMemoryValid = false; ///< Memory query succeeded in this interval.
		bool mGpuMemoryValid = false; ///< Exact-adapter budget query succeeded in this interval.
	};

	/// Owned snapshot for tools; contains no renderer-owned pointers.
	struct PerformanceSnapshot
	{
		Platform::Diagnostics::CpuInfo mCpu; ///< Cached CPU identity/topology.
		std::vector<Platform::Diagnostics::GpuInfo> mGpus; ///< Enumerated adapter identities.
		Uint64 mGpuAdapterId = 0; ///< Memory adapter selected by unique PCI match or explicit LUID.
		PerformanceCounters mCounters; ///< Most recent OS sample.
		std::vector<FrameProfile> mFrames; ///< Oldest to newest; at most 240 completed frames.
		Uint64 mDroppedCpuScopes = 0; ///< Invalid, cross-frame, or over-capacity records.
		Uint64 mDroppedGpuQueries = 0; ///< GPU submissions skipped because the pool was full.
		Uint64 mInvalidGpuResults = 0; ///< Invalid/disjoint results, never interpreted as zero duration.
		bool mEnabled = false; ///< Capture is opt-in and disabled by default.
		bool mGpuTimingSupported = false; ///< Actual renderer timestamp feature is enabled.
		bool mPipelineSupported = false; ///< Actual renderer pipeline-statistics feature is enabled.
	};

	/// Engine-thread frame/GPU capture with thread-safe CPU recording and snapshot copies.
	/// The module initializes after Graphics and releases its queries before device shutdown.
	class NEAPI ProfilingModule : public Core::EngineModule
	{
	public:
		/// Engine registers the shared module when AutoInitProfilingModule is enabled.
		static ProfilingModule& Get();
		~ProfilingModule();
		ProfilingModule(const ProfilingModule&) = delete;
		ProfilingModule& operator=(const ProfilingModule&) = delete;
		/// Creates optional GPU pools and caches hardware identity; supports CPU-only operation.
		bool OnInitialize() override;
		/// Stops capture and releases queries without waiting; completed history remains readable.
		void Shutdown() override;
		/// Samples OS counters at most every 500 ms while capture is enabled.
		void OnUpdate(float deltaTime) override;
		/// Enables future frames; an active frame may still finish safely when disabled.
		void SetEnabled(bool enabled);
		/// Starts capture if enabled/initialized and inactive; returns zero otherwise.
		Uint64 BeginFrame();
		/// Ends GPU work before Present; repeated calls are safe.
		void EndGpuFrame();
		/// Finalizes CPU scopes/history, ending any remaining GPU region first.
		void EndFrame();
		/// Returns a capture ID, or zero when CPU scopes should not record.
		Uint64 GetActiveFrame() const;
		/// Records an inclusive wall-time scope; at most 2048 records per frame.
		void RecordCpu(std::string_view name, Uint64 frame, Uint64 thread, double milliseconds);
		/// Copies consistent current counters/history; caller owns the returned storage.
		PerformanceSnapshot GetSnapshot() const;
		/// Selects an enumerated GPU LUID explicitly; default selection requires a unique PCI match.
		bool SetGpuMemoryAdapter(Uint64 adapterId);
		/// Clears completed history/drop counters; active capture is unaffected.
		void ClearHistory();
	private:
		ProfilingModule();
		struct Impl;
		Impl* pImpl; ///< Owned capture state and bounded GPU pools.
	};

	/// Inclusive CPU scope; must finish within its starting frame and before profiler destruction.
	class NEAPI CpuScope
	{
	public:
		/// Starts only during capture; copies a non-null label of at most 128 characters.
		explicit CpuScope(const char* name);
		~CpuScope();
		CpuScope(const CpuScope&) = delete;
		CpuScope& operator=(const CpuScope&) = delete;
	private:
		char mName[129]{}; ///< Owned bounded label; scope entry performs no string allocation.
		Uint64 mFrame = 0; ///< Capture frame token; zero disables recording.
		Uint64 mStart = 0; ///< Monotonic start timestamp in nanoseconds.
	};

	/// Draws the performance/memory window inside an active ImGui frame.
	NEAPI void DrawProfilerWindow(bool* open = nullptr);
}

#define NE_PROFILE_JOIN_IMPL(a, b) a##b
#define NE_PROFILE_JOIN(a, b) NE_PROFILE_JOIN_IMPL(a, b)
#define PROFILE_CPU(label) ::Nuclear::Profiling::CpuScope NE_PROFILE_JOIN(neCpuScope_, __LINE__)(label)
