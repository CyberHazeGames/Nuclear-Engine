#include <Nuclear.Platform/Diagnostics.h>
#include <chrono>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>

using namespace Nuclear::Platform::Diagnostics;

static void Require(bool condition, const char* message)
{
	if (!condition)
	{
		throw std::runtime_error(message);
	}
}

int main()
{
	try
	{
		CpuInfo cpu;
		Require(static_cast<bool>(QueryCpuInfo(cpu)), "CPU topology query failed");
		Require(cpu.mPhysicalCores && cpu.mLogicalProcessors >= cpu.mPhysicalCores && cpu.mPageSize,
			"CPU topology counters are invalid");
		MemoryInfo memory;
		Require(static_cast<bool>(QueryMemoryInfo(memory)), "Process/system memory query failed");
		Require(memory.mAvailablePhysicalBytes <= memory.mTotalPhysicalBytes && memory.mWorkingSetBytes &&
			memory.mPeakWorkingSetBytes >= memory.mWorkingSetBytes, "Memory counters are invalid");
		CpuCounters first, second;
		Require(static_cast<bool>(QueryCpuCounters(first)), "CPU counter query failed");
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
		Require(static_cast<bool>(QueryCpuCounters(second)) && second.mTimestampNanoseconds > first.mTimestampNanoseconds &&
			second.mProcessTime >= first.mProcessTime, "CPU counters are not monotonic");
		std::vector<GpuInfo> adapters;
		Require(static_cast<bool>(EnumerateGpus(adapters)), "GPU enumeration failed");
		for (const auto& adapter : adapters)
		{
			GpuMemoryInfo usage;
			const auto status = QueryGpuMemory(adapter.mAdapterId, usage);
			Require(static_cast<bool>(status) || status.mStatus == QueryStatus::Unavailable,
				"GPU budget query did not distinguish unavailable support");
		}
		GpuMemoryInfo sentinel;
		sentinel.mLocalUsageBytes = 123;
		Require(!QueryGpuMemory(std::numeric_limits<std::uint64_t>::max(), sentinel) && sentinel.mLocalUsageBytes == 123,
			"Failed GPU query modified output");
		std::cout << "Platform CPU/GPU/memory diagnostics passed: " << cpu.mName << ", adapters=" << adapters.size() << "\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << "\n";
		return 1;
	}
}
