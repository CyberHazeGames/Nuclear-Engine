#include <Profiling/MemoryProfiler.h>
#include <algorithm>
#include <limits>
#include <mutex>
#include <unordered_map>

namespace Nuclear::Profiling
{
	struct MemoryProfiler::Impl
	{
		struct Allocation { Uint64 mBytes; std::string mCategory; };
		mutable std::mutex mMutex;
		std::unordered_map<const void*, Allocation> mAllocations;
		std::unordered_map<std::string, MemoryCategory> mCategories;
		Uint64 mLive = 0;
		Uint64 mPeak = 0;
		Uint64 mRejected = 0;
		bool mEnabled = true;
	};

	MemoryProfiler::MemoryProfiler() : pImpl(new Impl()) {}
	MemoryProfiler::~MemoryProfiler() { delete pImpl; }
	MemoryProfiler& MemoryProfiler::Get()
	{
		static MemoryProfiler profiler;
		return profiler;
	}

	void MemoryProfiler::SetEnabled(bool enabled)
	{
		std::lock_guard lock(pImpl->mMutex);
		pImpl->mEnabled = enabled;
	}

	bool MemoryProfiler::TrackAllocation(const void* identity, Uint64 bytes, const std::string& category)
	{
		std::lock_guard lock(pImpl->mMutex);
		if (!pImpl->mEnabled)
		{
			return false;
		}
		const auto existing = pImpl->mCategories.find(category);
		const auto maximum = std::numeric_limits<Uint64>::max();
		if (!identity || !bytes || category.empty() || category.size() > 128 ||
			pImpl->mAllocations.size() >= 65536 || pImpl->mAllocations.contains(identity) ||
			(existing == pImpl->mCategories.end() && pImpl->mCategories.size() >= 256) ||
			bytes > maximum - pImpl->mLive || (existing != pImpl->mCategories.end() &&
			(bytes > maximum - existing->second.mAllocatedBytes || bytes > maximum - existing->second.mLiveBytes)))
		{
			++pImpl->mRejected;
			return false;
		}
		auto& stats = pImpl->mCategories[category];
		stats.mName = category;
		pImpl->mAllocations.emplace(identity, Impl::Allocation{ bytes, category });
		stats.mLiveBytes += bytes;
		stats.mAllocatedBytes += bytes;
		++stats.mLiveAllocations;
		stats.mPeakBytes = std::max(stats.mPeakBytes, stats.mLiveBytes);
		pImpl->mLive += bytes;
		pImpl->mPeak = std::max(pImpl->mPeak, pImpl->mLive);
		return true;
	}

	bool MemoryProfiler::TrackFree(const void* identity)
	{
		std::lock_guard lock(pImpl->mMutex);
		const auto allocation = pImpl->mAllocations.find(identity);
		if (allocation == pImpl->mAllocations.end())
		{
			return false;
		}
		auto& category = pImpl->mCategories.at(allocation->second.mCategory);
		category.mLiveBytes -= allocation->second.mBytes;
		category.mFreedBytes += allocation->second.mBytes;
		--category.mLiveAllocations;
		pImpl->mLive -= allocation->second.mBytes;
		pImpl->mAllocations.erase(allocation);
		return true;
	}

	MemoryProfile MemoryProfiler::GetSnapshot() const
	{
		std::lock_guard lock(pImpl->mMutex);
		MemoryProfile result;
		result.mLiveBytes = pImpl->mLive;
		result.mPeakBytes = pImpl->mPeak;
		result.mLiveAllocations = pImpl->mAllocations.size();
		result.mRejectedAllocations = pImpl->mRejected;
		for (const auto& [name, stats] : pImpl->mCategories)
		{
			result.mCategories.push_back(stats);
		}
		std::sort(result.mCategories.begin(), result.mCategories.end(), [](const auto& left, const auto& right)
		{
			return left.mLiveBytes > right.mLiveBytes || (left.mLiveBytes == right.mLiveBytes && left.mName < right.mName);
		});
		return result;
	}

	void MemoryProfiler::ResetCounters()
	{
		std::lock_guard lock(pImpl->mMutex);
		pImpl->mPeak = pImpl->mLive;
		pImpl->mRejected = 0;
		for (auto& [name, stats] : pImpl->mCategories)
		{
			stats.mPeakBytes = stats.mLiveBytes;
			stats.mAllocatedBytes = stats.mLiveBytes;
			stats.mFreedBytes = 0;
		}
	}
}
