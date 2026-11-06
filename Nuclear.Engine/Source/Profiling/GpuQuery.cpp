#include <Profiling/GpuQuery.h>
#include <Diligent/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <Diligent/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <Diligent/Graphics/GraphicsEngine/interface/Query.h>
#include <Diligent/Common/interface/RefCntAutoPtr.hpp>
#include <deque>
#include <vector>

namespace Nuclear::Profiling
{
	struct GpuQuery::Impl
	{
		struct Slot
		{
			Diligent::RefCntAutoPtr<Diligent::IQuery> pStart;
			Diligent::RefCntAutoPtr<Diligent::IQuery> pEnd;
			Uint64 mFrame = 0;
			bool mBusy = false;
		};
		Diligent::RefCntAutoPtr<Diligent::IRenderDevice> pDevice;
		Diligent::RefCntAutoPtr<Diligent::IDeviceContext> pActiveContext;
		GpuQueryType mType = GpuQueryType::Duration;
		std::vector<Slot> mSlots;
		std::deque<Uint32> mPending;
		int mActive = -1;
	};

	GpuQuery::GpuQuery() : pImpl(new Impl()) {}
	GpuQuery::~GpuQuery()
	{
		Shutdown();
		delete pImpl;
	}

	GpuQueryStatus GpuQuery::Initialize(Diligent::IRenderDevice* device, GpuQueryType type,
		const std::string& name, Uint32 capacity)
	{
		using namespace Diligent;
		Shutdown();
		if (!device || !capacity || capacity > 256 || name.size() > 128)
		{
			return GpuQueryStatus::InvalidState;
		}
		const auto& features = device->GetDeviceInfo().Features;
		QUERY_TYPE nativeType = QUERY_TYPE_UNDEFINED;
		switch (type)
		{
		case GpuQueryType::Duration:
		case GpuQueryType::Timestamp:
			if (features.TimestampQueries == DEVICE_FEATURE_STATE_DISABLED)
			{
				return GpuQueryStatus::Unsupported;
			}
			nativeType = QUERY_TYPE_TIMESTAMP;
			break;
		case GpuQueryType::Occlusion:
			if (features.OcclusionQueries == DEVICE_FEATURE_STATE_DISABLED)
			{
				return GpuQueryStatus::Unsupported;
			}
			nativeType = QUERY_TYPE_OCCLUSION;
			break;
		case GpuQueryType::PipelineStatistics:
			if (features.PipelineStatisticsQueries == DEVICE_FEATURE_STATE_DISABLED)
			{
				return GpuQueryStatus::Unsupported;
			}
			nativeType = QUERY_TYPE_PIPELINE_STATISTICS;
			break;
		default:
			return GpuQueryStatus::InvalidState;
		}

		pImpl->pDevice = device;
		pImpl->mType = type;
		pImpl->mSlots.resize(capacity);
		QueryDesc desc{ nativeType };
		desc.Name = name.c_str();
		for (auto& slot : pImpl->mSlots)
		{
			device->CreateQuery(desc, &slot.pStart);
			if (type == GpuQueryType::Duration)
			{
				device->CreateQuery(desc, &slot.pEnd);
			}
			if (!slot.pStart || (type == GpuQueryType::Duration && !slot.pEnd))
			{
				Shutdown();
				return GpuQueryStatus::Error;
			}
		}
		return GpuQueryStatus::Success;
	}

	void GpuQuery::Shutdown()
	{
		if (pImpl->mActive >= 0 && pImpl->pActiveContext)
		{
			End(pImpl->pActiveContext);
		}
		pImpl->mPending.clear();
		pImpl->mSlots.clear();
		pImpl->pActiveContext.Release();
		pImpl->pDevice.Release();
		pImpl->mActive = -1;
	}

	GpuQueryStatus GpuQuery::Begin(Diligent::IDeviceContext* context, Uint64 frame)
	{
		if (!IsSupported())
		{
			return GpuQueryStatus::Unsupported;
		}
		if (!context || context->GetDesc().IsDeferred || pImpl->mActive >= 0 || pImpl->mType == GpuQueryType::Timestamp)
		{
			return GpuQueryStatus::InvalidState;
		}
		for (Uint32 index = 0; index < pImpl->mSlots.size(); ++index)
		{
			auto& slot = pImpl->mSlots[index];
			if (slot.mBusy)
			{
				continue;
			}
			slot.mBusy = true;
			slot.mFrame = frame;
			pImpl->mActive = static_cast<int>(index);
			pImpl->pActiveContext = context;
			if (pImpl->mType == GpuQueryType::Duration)
			{
				context->EndQuery(slot.pStart);
			}
			else
			{
				context->BeginQuery(slot.pStart);
			}
			return GpuQueryStatus::Success;
		}
		return GpuQueryStatus::CapacityExceeded;
	}

	GpuQueryStatus GpuQuery::End(Diligent::IDeviceContext* context)
	{
		if (pImpl->mActive < 0 || !context || context != pImpl->pActiveContext.RawPtr())
		{
			return GpuQueryStatus::InvalidState;
		}
		auto& slot = pImpl->mSlots[pImpl->mActive];
		context->EndQuery(pImpl->mType == GpuQueryType::Duration ? slot.pEnd.RawPtr() : slot.pStart.RawPtr());
		pImpl->mPending.push_back(static_cast<Uint32>(pImpl->mActive));
		pImpl->mActive = -1;
		pImpl->pActiveContext.Release();
		return GpuQueryStatus::Success;
	}

	GpuQueryStatus GpuQuery::WriteTimestamp(Diligent::IDeviceContext* context, Uint64 frame)
	{
		if (!IsSupported())
		{
			return GpuQueryStatus::Unsupported;
		}
		if (!context || context->GetDesc().IsDeferred || pImpl->mType != GpuQueryType::Timestamp)
		{
			return GpuQueryStatus::InvalidState;
		}
		for (Uint32 index = 0; index < pImpl->mSlots.size(); ++index)
		{
			auto& slot = pImpl->mSlots[index];
			if (!slot.mBusy)
			{
				slot.mBusy = true;
				slot.mFrame = frame;
				context->EndQuery(slot.pStart);
				pImpl->mPending.push_back(index);
				return GpuQueryStatus::Success;
			}
		}
		return GpuQueryStatus::CapacityExceeded;
	}

	GpuQueryStatus GpuQuery::TryGetResult(GpuQueryResult& result)
	{
		using namespace Diligent;
		if (!IsSupported())
		{
			return GpuQueryStatus::Unsupported;
		}
		if (pImpl->mPending.empty())
		{
			return GpuQueryStatus::Pending;
		}
		auto& slot = pImpl->mSlots[pImpl->mPending.front()];
		GpuQueryResult data;
		data.mType = pImpl->mType;
		data.mFrame = slot.mFrame;
		bool valid = true;
		switch (pImpl->mType)
		{
		case GpuQueryType::Duration:
		case GpuQueryType::Timestamp:
		{
			QueryDataTimestamp start;
			if (!slot.pStart->GetData(&start, sizeof(start), false))
			{
				return GpuQueryStatus::Pending;
			}
			data.mTimestamp = start.Counter;
			data.mFrequency = start.Frequency;
			valid = start.Frequency != 0;
			if (pImpl->mType == GpuQueryType::Duration)
			{
				QueryDataTimestamp end;
				if (!slot.pEnd->GetData(&end, sizeof(end), false))
				{
					return GpuQueryStatus::Pending;
				}
				valid = valid && start.Frequency == end.Frequency && end.Counter >= start.Counter;
				if (valid)
				{
					data.mDurationMilliseconds = static_cast<double>(end.Counter - start.Counter) * 1000.0 / start.Frequency;
				}
			}
			break;
		}
		case GpuQueryType::Occlusion:
		{
			QueryDataOcclusion native;
			if (!slot.pStart->GetData(&native, sizeof(native), false))
			{
				return GpuQueryStatus::Pending;
			}
			data.mSamplesPassed = native.NumSamples;
			break;
		}
		case GpuQueryType::PipelineStatistics:
		{
			QueryDataPipelineStatistics native;
			if (!slot.pStart->GetData(&native, sizeof(native), false))
			{
				return GpuQueryStatus::Pending;
			}
			data.mPipeline = { native.InputVertices, native.InputPrimitives, native.ClippingInvocations,
				native.ClippingPrimitives, native.VSInvocations, native.PSInvocations, native.GSInvocations,
				native.GSPrimitives, native.HSInvocations, native.DSInvocations, native.CSInvocations };
			break;
		}
		}
		slot.pStart->Invalidate();
		if (slot.pEnd)
		{
			slot.pEnd->Invalidate();
		}
		slot.mBusy = false;
		pImpl->mPending.pop_front();
		if (!valid)
		{
			return GpuQueryStatus::Error;
		}
		result = data;
		return GpuQueryStatus::Success;
	}

	bool GpuQuery::IsSupported() const { return !pImpl->mSlots.empty(); }
	Uint32 GpuQuery::GetPendingCount() const { return static_cast<Uint32>(pImpl->mPending.size()); }
}
