#include <Nuclear.Platform/Diagnostics.h>
#include <Windows.h>
#include <Psapi.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <chrono>
#include <cstring>
#include <intrin.h>
#include <limits>

namespace Nuclear::Platform::Diagnostics
{
	using Microsoft::WRL::ComPtr;

	static std::uint64_t FileTimeValue(const FILETIME& time)
	{
		return (static_cast<std::uint64_t>(time.dwHighDateTime) << 32) | time.dwLowDateTime;
	}

	static QueryResult LastError()
	{
		return { QueryStatus::Error, static_cast<std::int64_t>(::GetLastError()) };
	}

	std::uint64_t GetTimestampNanoseconds()
	{
		return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
			std::chrono::steady_clock::now().time_since_epoch()).count());
	}

	QueryResult QueryCpuInfo(CpuInfo& info)
	{
		CpuInfo result;
		SYSTEM_INFO system{};
		GetNativeSystemInfo(&system);
		result.mPageSize = system.dwPageSize;
		result.mLogicalProcessors = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
		if (!result.mLogicalProcessors)
		{
			return LastError();
		}

		DWORD bytes = 0;
		GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &bytes);
		if (!bytes || ::GetLastError() != ERROR_INSUFFICIENT_BUFFER)
		{
			return LastError();
		}
		std::vector<std::byte> storage(bytes);
		if (!GetLogicalProcessorInformationEx(RelationProcessorCore,
			reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(storage.data()), &bytes))
		{
			return LastError();
		}
		for (DWORD offset = 0; offset < bytes;)
		{
			const auto* entry = reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(storage.data() + offset);
			if (!entry->Size || entry->Size > bytes - offset)
			{
				return { QueryStatus::Error, ERROR_INVALID_DATA };
			}
			if (entry->Relationship == RelationProcessorCore)
			{
				++result.mPhysicalCores;
			}
			offset += entry->Size;
		}

#if defined(_M_X64) || defined(_M_IX86)
		int registers[4]{};
		__cpuid(registers, 0);
		char vendor[13]{};
		std::memcpy(vendor, &registers[1], 4);
		std::memcpy(vendor + 4, &registers[3], 4);
		std::memcpy(vendor + 8, &registers[2], 4);
		result.mVendor = vendor;
		__cpuid(registers, static_cast<int>(0x80000000u));
		if (static_cast<unsigned>(registers[0]) >= 0x80000004u)
		{
			char brand[49]{};
			for (unsigned index = 0; index < 3; ++index)
			{
				__cpuid(registers, static_cast<int>(0x80000002u + index));
				std::memcpy(brand + index * 16, registers, 16);
			}
			result.mName = brand;
			const auto first = result.mName.find_first_not_of(' ');
			if (first != std::string::npos)
			{
				result.mName.erase(0, first);
			}
			const auto last = result.mName.find_last_not_of(' ');
			if (last != std::string::npos)
			{
				result.mName.resize(last + 1);
			}
		}
#endif
		info = std::move(result);
		return {};
	}

	QueryResult QueryMemoryInfo(MemoryInfo& info)
	{
		MEMORYSTATUSEX memory{};
		memory.dwLength = sizeof(memory);
		PROCESS_MEMORY_COUNTERS_EX process{};
		if (!GlobalMemoryStatusEx(&memory) || !GetProcessMemoryInfo(GetCurrentProcess(),
			reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&process), sizeof(process)))
		{
			return LastError();
		}
		MemoryInfo result;
		result.mTotalPhysicalBytes = memory.ullTotalPhys;
		result.mAvailablePhysicalBytes = memory.ullAvailPhys;
		result.mWorkingSetBytes = process.WorkingSetSize;
		result.mPeakWorkingSetBytes = process.PeakWorkingSetSize;
		result.mPrivateCommittedBytes = process.PrivateUsage;
		result.mPageFaultCount = process.PageFaultCount;
		info = result;
		return {};
	}

	QueryResult QueryCpuCounters(CpuCounters& counters)
	{
		FILETIME creation{}, exit{}, kernel{}, user{};
		if (!GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user))
		{
			return LastError();
		}
		CpuCounters result;
		result.mTimestampNanoseconds = GetTimestampNanoseconds();
		result.mProcessTime = FileTimeValue(kernel) + FileTimeValue(user);
		result.mLogicalProcessors = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
		if (!result.mLogicalProcessors)
		{
			return LastError();
		}
		// GetSystemTimes only covers one processor group on large Windows systems.
		if (GetActiveProcessorGroupCount() == 1)
		{
			FILETIME idle{};
			if (!GetSystemTimes(&idle, &kernel, &user))
			{
				return LastError();
			}
			result.mSystemTime = FileTimeValue(kernel) + FileTimeValue(user);
			result.mSystemIdleTime = FileTimeValue(idle);
			result.mSystemTimesAvailable = true;
		}
		counters = result;
		return {};
	}

	QueryResult EnumerateGpus(std::vector<GpuInfo>& adapters)
	{
		ComPtr<IDXGIFactory1> factory;
		const auto created = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
		if (FAILED(created))
		{
			return { QueryStatus::Error, created };
		}
		std::vector<GpuInfo> result;
		for (UINT index = 0;; ++index)
		{
			ComPtr<IDXGIAdapter1> adapter;
			const auto status = factory->EnumAdapters1(index, &adapter);
			if (status == DXGI_ERROR_NOT_FOUND)
			{
				break;
			}
			DXGI_ADAPTER_DESC1 desc{};
			if (FAILED(status))
			{
				return { QueryStatus::Error, status };
			}
			const auto described = adapter->GetDesc1(&desc);
			if (FAILED(described))
			{
				return { QueryStatus::Error, described };
			}
			GpuInfo info;
			info.mAdapterId = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(desc.AdapterLuid.HighPart)) << 32) |
				desc.AdapterLuid.LowPart;
			const int length = WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, nullptr, 0, nullptr, nullptr);
			if (length > 0)
			{
				info.mName.resize(length);
				WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, info.mName.data(), length, nullptr, nullptr);
				info.mName.pop_back();
			}
			info.mVendorId = desc.VendorId;
			info.mDeviceId = desc.DeviceId;
			info.mDedicatedVideoBytes = desc.DedicatedVideoMemory;
			info.mSharedSystemBytes = desc.SharedSystemMemory;
			info.mSoftware = (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;
			result.push_back(std::move(info));
		}
		adapters = std::move(result);
		return {};
	}

	QueryResult QueryGpuMemory(std::uint64_t adapterId, GpuMemoryInfo& info)
	{
		ComPtr<IDXGIFactory4> factory;
		auto status = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
		if (FAILED(status))
		{
			return { QueryStatus::Unavailable, status };
		}
		LUID luid{};
		luid.LowPart = static_cast<DWORD>(adapterId);
		luid.HighPart = static_cast<LONG>(adapterId >> 32);
		ComPtr<IDXGIAdapter3> adapter;
		status = factory->EnumAdapterByLuid(luid, IID_PPV_ARGS(&adapter));
		if (FAILED(status))
		{
			return { QueryStatus::Unavailable, status };
		}
		DXGI_QUERY_VIDEO_MEMORY_INFO local{}, nonLocal{};
		status = adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &local);
		if (SUCCEEDED(status))
		{
			status = adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &nonLocal);
		}
		if (FAILED(status))
		{
			return { QueryStatus::Unavailable, status };
		}
		info = { local.CurrentUsage, local.Budget, nonLocal.CurrentUsage, nonLocal.Budget };
		return {};
	}
}
