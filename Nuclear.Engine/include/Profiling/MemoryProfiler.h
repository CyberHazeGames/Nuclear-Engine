#pragma once

#include <NE_Common.h>
#include <string>
#include <vector>

namespace Nuclear::Profiling
{
	/// Explicitly tracked allocations for one caller-defined category; all sizes are bytes.
	struct MemoryCategory
	{
		std::string mName; ///< Category copied from TrackAllocation, at most 128 characters.
		Uint64 mLiveBytes = 0; ///< Sum of tracked allocations not yet freed.
		Uint64 mPeakBytes = 0; ///< Maximum tracked live bytes since ResetCounters.
		Uint64 mAllocatedBytes = 0; ///< Cumulative bytes tracked since ResetCounters.
		Uint64 mFreedBytes = 0; ///< Cumulative bytes removed since ResetCounters.
		Uint64 mLiveAllocations = 0; ///< Number of currently tracked allocation identities.
	};

	/// Tracked accounting, distinct from OS process memory or renderer allocation estimates.
	struct MemoryProfile
	{
		Uint64 mLiveBytes = 0; ///< Total live tracked bytes.
		Uint64 mPeakBytes = 0; ///< Peak live tracked bytes since ResetCounters.
		Uint64 mLiveAllocations = 0; ///< Current tracked allocation count.
		Uint64 mRejectedAllocations = 0; ///< Invalid/duplicate/over-capacity attempts; indicates incomplete coverage.
		std::vector<MemoryCategory> mCategories; ///< Category totals sorted by live bytes.
	};

	/// Thread-safe opt-in accounting; never hooks new/delete or frees caller memory.
	/// At most 65,536 live identities and 256 categories are retained. Starts enabled.
	class NEAPI MemoryProfiler
	{
	public:
		/// Returns the shared tracker without requiring engine initialization.
		static MemoryProfiler& Get();
		~MemoryProfiler();
		MemoryProfiler(const MemoryProfiler&) = delete;
		MemoryProfiler& operator=(const MemoryProfiler&) = delete;
		/// Stops/starts new records; frees of existing records remain accounted while disabled.
		void SetEnabled(bool enabled);
		/// Tracks a non-null caller-owned identity and positive size; rejects duplicates/full/invalid records.
		bool TrackAllocation(const void* identity, Uint64 bytes, const std::string& category = "General");
		/// Removes a tracked identity; false for unknown/already-freed identities.
		bool TrackFree(const void* identity);
		/// Copies consistent totals without exposing internal storage or caller pointers.
		MemoryProfile GetSnapshot() const;
		/// Resets peaks/cumulative counters while retaining live records and their categories.
		void ResetCounters();
	private:
		MemoryProfiler();
		struct Impl;
		Impl* pImpl; ///< Owned bounded accounting state protected by a mutex.
	};
}
