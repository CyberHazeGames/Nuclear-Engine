#pragma once

#include <NE_Common.h>
#include <Graphics/GraphicsModule.h>
#include <Profiling/GpuQuery.h>
#include <Profiling/ProfilingModule.h>
#include <chrono>
#include <utility>

namespace Nuclear::Profiling
{
    /// Compatibility CPU elapsed-time query. End returns wall milliseconds without resetting.
    class Profiler_CPU
    {
    public:
        /// Starts a monotonic elapsed-time measurement.
        Profiler_CPU() : mStart(std::chrono::steady_clock::now()) {}
        /// Returns elapsed milliseconds since construction or Reset.
        Float64 End() const
        {
            return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - mStart).count();
        }
        /// Starts a fresh measurement.
        void Reset() { mStart = std::chrono::steady_clock::now(); }
    protected:
        std::chrono::steady_clock::time_point mStart; ///< Monotonic measurement origin.
    };

    /// Compatibility GPU duration helper. New callers should use GpuQuery for explicit status/frame IDs.
    /// Use on the render thread and destroy before the graphics device is shut down.
    class Profiler_GPU
    {
    public:
        /// Copies a debug label; query allocation is delayed until Begin.
        explicit Profiler_GPU(std::string name) : mName(std::move(name)) {}
        /// Starts one duration region if graphics/timestamp support and pool capacity are available.
        void Begin()
        {
            auto* device = Graphics::GraphicsModule::Get().GetDevice();
            if (!mQuery.IsSupported() && device)
            {
                mQuery.Initialize(device, GpuQueryType::Duration, mName);
            }
            mStarted = mQuery.Begin(Graphics::GraphicsModule::Get().GetContext()) == GpuQueryStatus::Success;
        }
        /// Ends the region and returns an older completed duration in seconds, or zero while pending.
        /// Prefer the reference overload to distinguish pending results from a real zero duration.
        Float64 End()
        {
            Float64 seconds = 0;
            End(seconds);
            return seconds;
        }
        /// Ends the region; writes seconds only when a valid result is ready. False preserves output.
        bool End(Float64& seconds)
        {
            if (!mStarted)
            {
                return false;
            }
            mStarted = false;
            mQuery.End(Graphics::GraphicsModule::Get().GetContext());
            GpuQueryResult result;
            if (mQuery.TryGetResult(result) != GpuQueryStatus::Success)
            {
                return false;
            }
            seconds = result.mDurationMilliseconds / 1000.0;
            return true;
        }
        /// Releases retained device/query references before renderer shutdown.
        void Shutdown()
        {
            mQuery.Shutdown();
            mStarted = false;
        }
    protected:
        GpuQuery mQuery; ///< Fixed-capacity asynchronous duration pool.
        std::string mName; ///< Owned debug label.
        bool mStarted = false; ///< Whether the most recent Begin submitted a region.
    };
}

#define PROFILE_GPU(name) \
    static ::Nuclear::Profiling::Profiler_GPU name(#name); \
    name.Begin()
