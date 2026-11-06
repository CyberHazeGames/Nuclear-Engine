#pragma once

#include <Nuclear.Platform/Export.h>
#include <cstdint>
#include <string>
#include <vector>

namespace Nuclear::Platform::Diagnostics
{
	/// Unavailable distinguishes missing driver/OS support from a failed query.
	enum class QueryStatus { Success, Unavailable, Error };

	/// Native codes are diagnostic Win32 errors or HRESULTs, never required for normal branching.
	struct QueryResult
	{
		QueryStatus mStatus = QueryStatus::Success; ///< Success means every output field was refreshed.
		std::int64_t mNativeError = 0; ///< Zero for success or a locally detected unavailable value.
		explicit operator bool() const { return mStatus == QueryStatus::Success; }
	};

	/// Static processor information; counts include all Windows processor groups.
	struct CpuInfo
	{
		std::string mName; ///< Processor brand string, when available.
		std::string mVendor; ///< CPU vendor identifier, when available.
		std::uint32_t mPhysicalCores = 0; ///< Active physical processor cores.
		std::uint32_t mLogicalProcessors = 0; ///< Active logical processors across all groups.
		std::uint32_t mPageSize = 0; ///< Operating-system page size in bytes.
	};

	/// Point-in-time physical and process memory, in bytes except the cumulative fault count.
	struct MemoryInfo
	{
		std::uint64_t mTotalPhysicalBytes = 0; ///< Installed physical memory visible to the OS.
		std::uint64_t mAvailablePhysicalBytes = 0; ///< Physical memory currently available to applications.
		std::uint64_t mWorkingSetBytes = 0; ///< Resident pages mapped into this process, including shared pages.
		std::uint64_t mPeakWorkingSetBytes = 0; ///< Process-lifetime peak resident working set.
		std::uint64_t mPrivateCommittedBytes = 0; ///< Committed private process memory; not virtual reservation size.
		std::uint64_t mPageFaultCount = 0; ///< Process-lifetime soft plus hard page faults.
	};

	/// Cumulative counters for interval-based utilization; CPU times are in 100-nanosecond units.
	struct CpuCounters
	{
		std::uint64_t mTimestampNanoseconds = 0; ///< Monotonic sample timestamp, unrelated to wall-clock time.
		std::uint64_t mProcessTime = 0; ///< Kernel plus user CPU time consumed by all process threads.
		std::uint64_t mSystemTime = 0; ///< System kernel plus user time; includes idle time.
		std::uint64_t mSystemIdleTime = 0; ///< Idle time included in mSystemTime.
		std::uint32_t mLogicalProcessors = 0; ///< Normalization factor for process-wide CPU utilization.
		bool mSystemTimesAvailable = false; ///< False on multi-group systems to avoid reporting partial system load.
	};

	/// Adapter identity and static capacity. Capacity is not current process usage or budget.
	struct GpuInfo
	{
		std::uint64_t mAdapterId = 0; ///< Opaque Windows adapter LUID, valid only for the current machine/session.
		std::string mName; ///< UTF-8 display adapter description.
		std::uint32_t mVendorId = 0; ///< PCI vendor ID, when provided by the driver.
		std::uint32_t mDeviceId = 0; ///< PCI device ID, when provided by the driver.
		std::uint64_t mDedicatedVideoBytes = 0; ///< Dedicated video memory capacity.
		std::uint64_t mSharedSystemBytes = 0; ///< Maximum shared-system memory capacity reported by DXGI.
		bool mSoftware = false; ///< True for a software adapter.
	};

	/// Current process usage and OS-assigned budgets on one GPU, in bytes, for node zero.
	struct GpuMemoryInfo
	{
		std::uint64_t mLocalUsageBytes = 0; ///< This process's local-segment usage, not whole-GPU usage.
		std::uint64_t mLocalBudgetBytes = 0; ///< OS-assigned local-segment budget, which may change.
		std::uint64_t mNonLocalUsageBytes = 0; ///< This process's non-local segment usage.
		std::uint64_t mNonLocalBudgetBytes = 0; ///< OS-assigned non-local segment budget.
	};

	/// Returns monotonic nanoseconds for elapsed-time measurements; not CPU clock cycles.
	NPAPI std::uint64_t GetTimestampNanoseconds();
	/// Queries processor identity/topology without requiring platform/window initialization.
	NPAPI QueryResult QueryCpuInfo(CpuInfo& info);
	/// Queries physical and current-process memory; output is unchanged on failure.
	NPAPI QueryResult QueryMemoryInfo(MemoryInfo& info);
	/// Queries cumulative CPU counters; the caller computes differences between successive samples.
	NPAPI QueryResult QueryCpuCounters(CpuCounters& counters);
	/// Enumerates DXGI adapters; replaces the list on success, preserves it on failure.
	NPAPI QueryResult EnumerateGpus(std::vector<GpuInfo>& adapters);
	/// Queries the exact enumerated adapter ID; unsupported budget interfaces return Unavailable.
	NPAPI QueryResult QueryGpuMemory(std::uint64_t adapterId, GpuMemoryInfo& info);
}
