#include <Profiling/ProfilingModule.h>
#include <Graphics/GraphicsModule.h>
#include <algorithm>
#include <cmath>
#include <deque>
#include <mutex>
#include <thread>

namespace Nuclear::Profiling
{
	namespace Diagnostics = Platform::Diagnostics;

	struct ProfilingModule::Impl
	{
		mutable std::mutex mMutex;
		PerformanceSnapshot mSnapshot;
		Diagnostics::CpuCounters mPreviousCpu;
		std::deque<FrameProfile> mHistory;
		FrameProfile mCurrent;
		GpuQuery mDuration;
		GpuQuery mPipeline;
		Diligent::RefCntAutoPtr<Diligent::IDeviceContext> pContext;
		Uint64 mFrameCounter = 0;
		Uint64 mFrameStart = 0;
		Uint64 mLastSample = 0;
		Uint32 mScopeCount = 0;
		bool mInitialized = false;
		bool mActive = false;
		bool mDurationActive = false;
		bool mPipelineActive = false;

		void Poll(GpuQuery& query)
		{
			for (Uint32 count = 0; count < 8; ++count)
			{
				GpuQueryResult result;
				const auto status = query.TryGetResult(result);
				if (status == GpuQueryStatus::Pending || status == GpuQueryStatus::Unsupported)
				{
					break;
				}
				if (status != GpuQueryStatus::Success)
				{
					++mSnapshot.mInvalidGpuResults;
					continue;
				}
				const auto frame = std::find_if(mHistory.begin(), mHistory.end(), [&](const FrameProfile& value)
				{
					return value.mFrame == result.mFrame;
				});
				if (frame != mHistory.end())
				{
					if (result.mType == GpuQueryType::Duration)
					{
						frame->mGpuMilliseconds = result.mDurationMilliseconds;
						frame->mGpuValid = true;
					}
					else
					{
						frame->mPipeline = result.mPipeline;
						frame->mPipelineValid = true;
					}
				}
			}
		}
	};

	ProfilingModule::ProfilingModule() : pImpl(new Impl()) {}
	ProfilingModule::~ProfilingModule()
	{
		Shutdown();
		delete pImpl;
	}
	ProfilingModule& ProfilingModule::Get()
	{
		static ProfilingModule profiler;
		return profiler;
	}

	bool ProfilingModule::OnInitialize()
	{
		Shutdown();
		std::lock_guard lock(pImpl->mMutex);
		pImpl->mSnapshot = {};
		pImpl->mHistory.clear();
		Diagnostics::QueryCpuInfo(pImpl->mSnapshot.mCpu);
		Diagnostics::EnumerateGpus(pImpl->mSnapshot.mGpus);
		auto* device = Graphics::GraphicsModule::Get().GetDevice();
		pImpl->pContext = Graphics::GraphicsModule::Get().GetContext();
		if (device && pImpl->pContext)
		{
			pImpl->mSnapshot.mGpuTimingSupported = pImpl->mDuration.Initialize(device,
				GpuQueryType::Duration, "Profiler frame duration") == GpuQueryStatus::Success;
			pImpl->mSnapshot.mPipelineSupported = pImpl->mPipeline.Initialize(device,
				GpuQueryType::PipelineStatistics, "Profiler frame pipeline") == GpuQueryStatus::Success;
			const auto& actual = device->GetAdapterInfo();
			Uint32 matches = 0;
			for (const auto& gpu : pImpl->mSnapshot.mGpus)
			{
				if (gpu.mVendorId == actual.VendorId && gpu.mDeviceId == actual.DeviceId && actual.VendorId)
				{
					pImpl->mSnapshot.mGpuAdapterId = gpu.mAdapterId;
					++matches;
				}
			}
			if (matches != 1)
			{
				pImpl->mSnapshot.mGpuAdapterId = 0;
			}
		}
		pImpl->mPreviousCpu = {};
		pImpl->mLastSample = 0;
		pImpl->mInitialized = true;
		return true;
	}

	void ProfilingModule::Shutdown()
	{
		EndGpuFrame();
		std::lock_guard lock(pImpl->mMutex);
		pImpl->mInitialized = false;
		pImpl->mActive = false;
		pImpl->mSnapshot.mEnabled = false;
		pImpl->mDuration.Shutdown();
		pImpl->mPipeline.Shutdown();
		pImpl->pContext.Release();
	}

	void ProfilingModule::SetEnabled(bool enabled)
	{
		std::lock_guard lock(pImpl->mMutex);
		if (enabled && !pImpl->mSnapshot.mEnabled)
		{
			pImpl->mLastSample = 0;
			pImpl->mPreviousCpu = {};
			pImpl->mSnapshot.mCounters = {};
		}
		pImpl->mSnapshot.mEnabled = enabled;
	}

	void ProfilingModule::OnUpdate(float)
	{
		const auto now = Diagnostics::GetTimestampNanoseconds();
		Uint64 adapterId = 0;
		{
			std::lock_guard lock(pImpl->mMutex);
			if (!pImpl->mInitialized || !pImpl->mSnapshot.mEnabled ||
				(pImpl->mLastSample && now - pImpl->mLastSample < 500000000))
			{
				return;
			}
			pImpl->mLastSample = now;
			adapterId = pImpl->mSnapshot.mGpuAdapterId;
		}
		PerformanceCounters counters;
		counters.mMemoryValid = static_cast<bool>(Diagnostics::QueryMemoryInfo(counters.mMemory));
		counters.mGpuMemoryValid = adapterId && static_cast<bool>(Diagnostics::QueryGpuMemory(adapterId, counters.mGpuMemory));
		Diagnostics::CpuCounters cpu;
		const auto queried = Diagnostics::QueryCpuCounters(cpu);
		std::lock_guard lock(pImpl->mMutex);
		const auto& previous = pImpl->mPreviousCpu;
		if (queried && previous.mTimestampNanoseconds && cpu.mTimestampNanoseconds > previous.mTimestampNanoseconds &&
			cpu.mProcessTime >= previous.mProcessTime && cpu.mLogicalProcessors == previous.mLogicalProcessors)
		{
			const double elapsed = static_cast<double>(cpu.mTimestampNanoseconds - previous.mTimestampNanoseconds);
			counters.mProcessCpuPercent = std::clamp(static_cast<double>(cpu.mProcessTime - previous.mProcessTime) *
				10000.0 / elapsed / cpu.mLogicalProcessors, 0.0, 100.0);
			counters.mProcessCpuValid = true;
			if (cpu.mSystemTimesAvailable && previous.mSystemTimesAvailable && cpu.mSystemTime > previous.mSystemTime &&
				cpu.mSystemIdleTime >= previous.mSystemIdleTime)
			{
				const auto total = cpu.mSystemTime - previous.mSystemTime;
				const auto idle = cpu.mSystemIdleTime - previous.mSystemIdleTime;
				counters.mSystemCpuPercent = std::clamp(100.0 * (1.0 - static_cast<double>(idle) / total), 0.0, 100.0);
				counters.mSystemCpuValid = true;
			}
		}
		pImpl->mPreviousCpu = queried ? cpu : Diagnostics::CpuCounters{};
		pImpl->mSnapshot.mCounters = counters;
	}

	Uint64 ProfilingModule::BeginFrame()
	{
		std::lock_guard lock(pImpl->mMutex);
		if (!pImpl->mInitialized || !pImpl->mSnapshot.mEnabled || pImpl->mActive)
		{
			return 0;
		}
		pImpl->Poll(pImpl->mDuration);
		pImpl->Poll(pImpl->mPipeline);
		pImpl->mCurrent = {};
		pImpl->mCurrent.mFrame = ++pImpl->mFrameCounter;
		pImpl->mFrameStart = Diagnostics::GetTimestampNanoseconds();
		pImpl->mScopeCount = 0;
		pImpl->mActive = true;
		if (pImpl->pContext)
		{
			const auto duration = pImpl->mDuration.Begin(pImpl->pContext, pImpl->mFrameCounter);
			const auto pipeline = pImpl->mPipeline.Begin(pImpl->pContext, pImpl->mFrameCounter);
			pImpl->mDurationActive = duration == GpuQueryStatus::Success;
			pImpl->mPipelineActive = pipeline == GpuQueryStatus::Success;
			pImpl->mSnapshot.mDroppedGpuQueries += duration == GpuQueryStatus::CapacityExceeded;
			pImpl->mSnapshot.mDroppedGpuQueries += pipeline == GpuQueryStatus::CapacityExceeded;
		}
		return pImpl->mCurrent.mFrame;
	}

	void ProfilingModule::EndGpuFrame()
	{
		std::lock_guard lock(pImpl->mMutex);
		if (pImpl->mDurationActive)
		{
			pImpl->mDuration.End(pImpl->pContext);
			pImpl->mDurationActive = false;
		}
		if (pImpl->mPipelineActive)
		{
			pImpl->mPipeline.End(pImpl->pContext);
			pImpl->mPipelineActive = false;
		}
	}

	void ProfilingModule::EndFrame()
	{
		EndGpuFrame();
		std::lock_guard lock(pImpl->mMutex);
		if (!pImpl->mActive)
		{
			return;
		}
		pImpl->mCurrent.mCpuMilliseconds = static_cast<double>(Diagnostics::GetTimestampNanoseconds() - pImpl->mFrameStart) / 1e6;
		auto& scopes = pImpl->mCurrent.mCpuTimings;
		std::sort(scopes.begin(), scopes.end(), [](const auto& left, const auto& right)
		{
			return left.mTotalMilliseconds > right.mTotalMilliseconds;
		});
		pImpl->mHistory.push_back(std::move(pImpl->mCurrent));
		if (pImpl->mHistory.size() > 240)
		{
			pImpl->mHistory.pop_front();
		}
		pImpl->mActive = false;
	}

	Uint64 ProfilingModule::GetActiveFrame() const
	{
		std::lock_guard lock(pImpl->mMutex);
		return pImpl->mActive && pImpl->mSnapshot.mEnabled ? pImpl->mCurrent.mFrame : 0;
	}

	void ProfilingModule::RecordCpu(std::string_view name, Uint64 frame, Uint64 thread, double milliseconds)
	{
		if (!frame)
		{
			return;
		}
		std::lock_guard lock(pImpl->mMutex);
		if (!pImpl->mActive || frame != pImpl->mCurrent.mFrame || name.empty() || name.size() > 128 ||
			!std::isfinite(milliseconds) || milliseconds < 0 || pImpl->mScopeCount >= 2048)
		{
			++pImpl->mSnapshot.mDroppedCpuScopes;
			return;
		}
		++pImpl->mScopeCount;
		auto& scopes = pImpl->mCurrent.mCpuTimings;
		auto existing = std::find_if(scopes.begin(), scopes.end(), [&](const CpuTiming& value)
		{
			return value.mName == name && value.mThread == thread;
		});
		if (existing == scopes.end())
		{
			scopes.push_back({ std::string(name), thread, 1, milliseconds, milliseconds });
		}
		else
		{
			++existing->mCalls;
			existing->mTotalMilliseconds += milliseconds;
			existing->mMaximumMilliseconds = std::max(existing->mMaximumMilliseconds, milliseconds);
		}
	}

	PerformanceSnapshot ProfilingModule::GetSnapshot() const
	{
		std::lock_guard lock(pImpl->mMutex);
		auto result = pImpl->mSnapshot;
		result.mFrames.assign(pImpl->mHistory.begin(), pImpl->mHistory.end());
		return result;
	}

	bool ProfilingModule::SetGpuMemoryAdapter(Uint64 adapterId)
	{
		std::lock_guard lock(pImpl->mMutex);
		const auto& gpus = pImpl->mSnapshot.mGpus;
		if (std::none_of(gpus.begin(), gpus.end(), [&](const auto& gpu) { return gpu.mAdapterId == adapterId; }))
		{
			return false;
		}
		pImpl->mSnapshot.mGpuAdapterId = adapterId;
		pImpl->mSnapshot.mCounters.mGpuMemoryValid = false;
		pImpl->mLastSample = 0;
		return true;
	}

	void ProfilingModule::ClearHistory()
	{
		std::lock_guard lock(pImpl->mMutex);
		pImpl->mHistory.clear();
		pImpl->mSnapshot.mDroppedCpuScopes = 0;
		pImpl->mSnapshot.mDroppedGpuQueries = 0;
		pImpl->mSnapshot.mInvalidGpuResults = 0;
	}

	CpuScope::CpuScope(const char* name)
	{
		mFrame = ProfilingModule::Get().GetActiveFrame();
		if (!mFrame || !name)
		{
			mFrame = 0;
			return;
		}
		std::size_t length = 0;
		while (length < 129 && name[length])
		{
			++length;
		}
		if (!length || length > 128)
		{
			mFrame = 0;
			return;
		}
		std::copy_n(name, length, mName);
		mStart = Diagnostics::GetTimestampNanoseconds();
	}

	CpuScope::~CpuScope()
	{
		if (mFrame)
		{
			const double milliseconds = static_cast<double>(Diagnostics::GetTimestampNanoseconds() - mStart) / 1e6;
			try
			{
				ProfilingModule::Get().RecordCpu(mName, mFrame,
					static_cast<Uint64>(std::hash<std::thread::id>{}(std::this_thread::get_id())), milliseconds);
			}
			catch (...)
			{
				// A profiling allocation/recording failure must not throw from a scope destructor.
			}
		}
	}
}
