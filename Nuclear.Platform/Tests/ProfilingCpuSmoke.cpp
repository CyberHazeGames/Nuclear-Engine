#include <Nuclear.Platform/Diagnostics.h>
#include <Profiling/Profiler.h>
#include <Profiling/MemoryProfiler.h>
#include <array>
#include <future>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>

using namespace Nuclear;
using namespace Nuclear::Profiling;
using namespace std::chrono_literals;

static void Require(bool condition, const char* message)
{
	if (!condition)
	{
		throw std::runtime_error(message);
	}
}

static void CheckDiagnostics()
{
	Platform::Diagnostics::CpuInfo cpu;
	Require(static_cast<bool>(Platform::Diagnostics::QueryCpuInfo(cpu)), "CPU query failed");
	Require(cpu.mPhysicalCores && cpu.mLogicalProcessors >= cpu.mPhysicalCores && cpu.mPageSize, "Invalid CPU topology");
	Platform::Diagnostics::MemoryInfo memory;
	Require(static_cast<bool>(Platform::Diagnostics::QueryMemoryInfo(memory)), "Memory query failed");
	Require(memory.mTotalPhysicalBytes >= memory.mAvailablePhysicalBytes && memory.mWorkingSetBytes &&
		memory.mPeakWorkingSetBytes >= memory.mWorkingSetBytes && memory.mPrivateCommittedBytes, "Invalid memory counters");
	Platform::Diagnostics::CpuCounters before, after;
	Require(static_cast<bool>(Platform::Diagnostics::QueryCpuCounters(before)), "CPU counters failed");
	Profiler_CPU timer;
	std::this_thread::sleep_for(2ms);
	Require(timer.End() >= 1.0, "CPU elapsed query did not report milliseconds");
	Require(static_cast<bool>(Platform::Diagnostics::QueryCpuCounters(after)), "Second CPU counter query failed");
	Require(after.mTimestampNanoseconds > before.mTimestampNanoseconds && after.mProcessTime >= before.mProcessTime,
		"CPU counters are not monotonic");
	std::vector<Platform::Diagnostics::GpuInfo> adapters;
	const auto enumerated = Platform::Diagnostics::EnumerateGpus(adapters);
	Require(static_cast<bool>(enumerated), "GPU enumeration failed");
	for (const auto& adapter : adapters)
	{
		Platform::Diagnostics::GpuMemoryInfo gpuMemory;
		const auto status = Platform::Diagnostics::QueryGpuMemory(adapter.mAdapterId, gpuMemory);
		Require(static_cast<bool>(status) || status.mStatus == Platform::Diagnostics::QueryStatus::Unavailable,
			"GPU budget query did not distinguish unavailable support");
		std::cout << "GPU: " << adapter.mName << " budget available=" << static_cast<bool>(status) << "\n";
	}
	Platform::Diagnostics::GpuMemoryInfo sentinel;
	sentinel.mLocalBudgetBytes = 123;
	Require(!Platform::Diagnostics::QueryGpuMemory(std::numeric_limits<Uint64>::max(), sentinel), "Invalid GPU ID was accepted");
	Require(sentinel.mLocalBudgetBytes == 123, "Failed GPU query modified output");
	std::cout << "CPU: " << cpu.mName << ", " << cpu.mPhysicalCores << " cores / " << cpu.mLogicalProcessors << " logical\n";
}

static void CheckMemoryTracker()
{
	auto& profiler = MemoryProfiler::Get();
	std::array<std::byte, 64> first{}, second{};
	Require(profiler.TrackAllocation(first.data(), first.size(), "Test"), "Allocation tracking failed");
	Require(!profiler.TrackAllocation(first.data(), first.size(), "Test"), "Duplicate allocation was accepted");
	Require(!profiler.TrackAllocation(second.data(), std::numeric_limits<Uint64>::max(), "Test"), "Overflow was accepted");
	auto snapshot = profiler.GetSnapshot();
	Require(snapshot.mLiveBytes == 64 && snapshot.mPeakBytes == 64 && snapshot.mLiveAllocations == 1,
		"Tracked live/peak totals are wrong");
	profiler.ResetCounters();
	Require(profiler.GetSnapshot().mLiveBytes == 64, "Reset discarded a live allocation");
	profiler.SetEnabled(false);
	Require(!profiler.TrackAllocation(second.data(), second.size(), "Test"), "Disabled tracker accepted new records");
	Require(profiler.TrackFree(first.data()), "Disabled tracker did not free an existing record");
	Require(!profiler.TrackFree(first.data()), "Double free was accepted");
	profiler.SetEnabled(true);
	std::vector<std::future<void>> workers;
	for (int thread = 0; thread < 4; ++thread)
	{
		workers.push_back(std::async(std::launch::async, [&profiler]
		{
			std::array<std::byte, 32> allocation{};
			for (int iteration = 0; iteration < 100; ++iteration)
			{
				Require(profiler.TrackAllocation(allocation.data(), allocation.size(), "Workers"), "Concurrent track failed");
				Require(profiler.TrackFree(allocation.data()), "Concurrent free failed");
			}
		}));
	}
	for (auto& worker : workers)
	{
		worker.get();
	}
	Require(profiler.GetSnapshot().mLiveBytes == 0 && profiler.GetSnapshot().mLiveAllocations == 0,
		"Concurrent tracking leaked accounting records");
}

static void CheckCpuCapture()
{
	auto& profiler = ProfilingModule::Get();
	Require(profiler.OnInitialize(), "CPU-only profiler initialization failed");
	Require(profiler.BeginFrame() == 0, "Capture should start disabled");
	profiler.SetEnabled(true);
	profiler.OnUpdate(0);
	Require(!profiler.GetSnapshot().mCounters.mProcessCpuValid, "First CPU sample reported an interval");
	std::this_thread::sleep_for(510ms);
	profiler.OnUpdate(0);
	Require(profiler.GetSnapshot().mCounters.mProcessCpuValid, "Second CPU interval was not available");
	const auto frame = profiler.BeginFrame();
	Require(frame && profiler.BeginFrame() == 0, "Nested BeginFrame reset active capture");
	{
		PROFILE_CPU("Outer");
		{
			PROFILE_CPU("Inner");
			std::this_thread::sleep_for(1ms);
		}
		std::vector<std::future<void>> workers;
		for (int index = 0; index < 4; ++index)
		{
			workers.push_back(std::async(std::launch::async, []
			{
				for (int scope = 0; scope < 20; ++scope)
				{
					PROFILE_CPU("Worker");
				}
			}));
		}
		for (auto& worker : workers)
		{
			worker.get();
		}
	}
	profiler.RecordCpu("Known", frame, 99, 0.5);
	profiler.RecordCpu("Known", frame, 99, 0.5);
	profiler.EndFrame();
	auto snapshot = profiler.GetSnapshot();
	Require(snapshot.mFrames.size() == 1 && snapshot.mFrames.front().mCpuMilliseconds > 0,
		"CPU frame duration was not retained");
	Require(!snapshot.mFrames.front().mGpuValid, "CPU-only capture invented a GPU timing");
	bool found = false;
	for (const auto& timing : snapshot.mFrames.front().mCpuTimings)
	{
		if (timing.mName == "Known")
		{
			found = timing.mCalls == 2 && timing.mTotalMilliseconds == 1.0 && timing.mMaximumMilliseconds == 0.5;
		}
	}
	Require(found, "CPU aggregation lost calls or units");
	profiler.BeginFrame();
	auto* lateScope = new CpuScope("Cross-frame");
	profiler.EndFrame();
	const auto next = profiler.BeginFrame();
	delete lateScope;
	for (int scope = 0; scope < 2060; ++scope)
	{
		profiler.RecordCpu("Bounded", next, 1, 0.1);
	}
	profiler.SetEnabled(false);
	profiler.EndFrame();
	Require(profiler.GetSnapshot().mDroppedCpuScopes >= 13, "Cross-frame/over-capacity scopes were not rejected");
	profiler.ClearHistory();
	profiler.SetEnabled(true);
	for (int index = 0; index < 245; ++index)
	{
		profiler.BeginFrame();
		profiler.EndFrame();
	}
	Require(profiler.GetSnapshot().mFrames.size() == 240, "Frame history is not bounded");
	profiler.Shutdown();
	Require(profiler.BeginFrame() == 0, "Shutdown did not stop capture");
	GpuQuery query;
	GpuQueryResult sentinel;
	sentinel.mFrame = 123;
	Require(query.Initialize(nullptr, GpuQueryType::Duration) == GpuQueryStatus::InvalidState, "Null device was accepted");
	Require(query.TryGetResult(sentinel) == GpuQueryStatus::Unsupported && sentinel.mFrame == 123,
		"Unsupported query changed output");
}

int main()
{
	try
	{
		CheckDiagnostics();
		CheckMemoryTracker();
		CheckCpuCapture();
		std::cout << "CPU diagnostics, memory accounting, and bounded concurrent profiling checks passed.\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << "\n";
		return 1;
	}
}
