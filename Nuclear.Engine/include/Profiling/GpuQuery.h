#pragma once

#include <NE_Common.h>
#include <string>

namespace Diligent { struct IRenderDevice; struct IDeviceContext; }

namespace Nuclear::Profiling
{
	/// Duration uses paired timestamps so independently named regions may overlap.
	enum class GpuQueryType { Duration, Timestamp, Occlusion, PipelineStatistics };
	/// Success from TryGetResult means fresh data; Pending never overwrites the output.
	enum class GpuQueryStatus { Success, Pending, Unsupported, InvalidState, CapacityExceeded, Error };

	/// Pipeline counts for one region; individual counters depend on backend support.
	struct PipelineStatistics
	{
		Uint64 mInputVertices = 0; ///< Input-assembler vertices.
		Uint64 mInputPrimitives = 0; ///< Input-assembler primitives.
		Uint64 mClippingInvocations = 0; ///< Primitives entering clipping.
		Uint64 mClippingPrimitives = 0; ///< Primitives leaving clipping.
		Uint64 mVertexInvocations = 0; ///< Vertex shader invocations.
		Uint64 mPixelInvocations = 0; ///< Pixel shader invocations.
		Uint64 mGeometryInvocations = 0; ///< Geometry shader invocations.
		Uint64 mGeometryPrimitives = 0; ///< Geometry shader output primitives.
		Uint64 mHullInvocations = 0; ///< Hull shader invocations.
		Uint64 mDomainInvocations = 0; ///< Domain shader invocations.
		Uint64 mComputeInvocations = 0; ///< Compute shader invocations.
	};

	/// Valid only for the corresponding query type; duration is milliseconds, timestamps are ticks.
	struct GpuQueryResult
	{
		GpuQueryType mType = GpuQueryType::Duration; ///< Determines which payload is meaningful.
		Uint64 mFrame = 0; ///< Caller-supplied frame ID, preserved across asynchronous completion.
		double mDurationMilliseconds = 0; ///< Elapsed GPU work for Duration.
		Uint64 mTimestamp = 0; ///< Raw hardware timestamp for Timestamp.
		Uint64 mFrequency = 0; ///< Timestamp frequency in ticks/second.
		Uint64 mSamplesPassed = 0; ///< Depth/stencil-passing samples for Occlusion.
		PipelineStatistics mPipeline; ///< PipelineStatistics payload.
	};

	/// Bounded asynchronous queries on one immediate context's owning thread.
	/// Initialize/Shutdown and recording/polling must use that thread. Native pipeline-statistics
	/// or occlusion queries of the same type must not overlap on a context. Duration queries can.
	class NEAPI GpuQuery
	{
	public:
		/// Constructs an empty, unsupported query without touching the renderer.
		GpuQuery();
		/// Releases queries and retained device/context references. Ends an active region first.
		~GpuQuery();
		GpuQuery(const GpuQuery&) = delete;
		GpuQuery& operator=(const GpuQuery&) = delete;

		/// Retains device and allocates a fixed pool of 1..256 submissions; requires enabled features.
		GpuQueryStatus Initialize(Diligent::IRenderDevice* device, GpuQueryType type,
			const std::string& name = "Nuclear query", Uint32 capacity = 8);
		/// Ends an active region and releases all pending results without waiting for the GPU.
		void Shutdown();
		/// Starts a non-timestamp region. A full pending pool returns CapacityExceeded, never waits.
		GpuQueryStatus Begin(Diligent::IDeviceContext* context, Uint64 frame = 0);
		/// Ends the active region on the same context. Results become available in a later frame.
		GpuQueryStatus End(Diligent::IDeviceContext* context);
		/// Records a Timestamp submission; Timestamp queries do not use Begin/End.
		GpuQueryStatus WriteTimestamp(Diligent::IDeviceContext* context, Uint64 frame = 0);
		/// Polls the oldest submission once. No flush, spin, or CPU/GPU synchronization is performed.
		/// Output is unchanged unless Success is returned; invalid/disjoint timing returns Error.
		GpuQueryStatus TryGetResult(GpuQueryResult& result);
		/// Returns whether initialization succeeded for the requested feature.
		bool IsSupported() const;
		/// Returns the number of ended submissions awaiting retrieval.
		Uint32 GetPendingCount() const;

	private:
		struct Impl;
		Impl* pImpl; ///< Owned query pool; released before graphics device shutdown.
	};
}
