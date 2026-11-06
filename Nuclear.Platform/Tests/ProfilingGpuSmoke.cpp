#include <Core/Engine.h>
#include <Graphics/GraphicsModule.h>
#include <Graphics/ImGui.h>
#include <Profiling/Profiling.h>
#include <Diligent/Graphics/GraphicsEngine/interface/SwapChain.h>
#include <Windows.h>
#include <array>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

using namespace Nuclear;
using namespace Nuclear::Profiling;

static void Require(bool condition, const char* message)
{
	if (!condition)
	{
		throw std::runtime_error(message);
	}
}

class ProfilingClient final : public Core::Client
{
public:
	std::array<GpuQuery, 4> mQueries;
	std::array<bool, 4> mSupported{};
	std::array<unsigned, 4> mResults{};
	GpuQuery mBounded;
	unsigned mFrames = 0;
	bool mSawGpuFrame = false;
	bool mSawPipeline = false;
	bool mSawCpuScope = false;

	void Initialize() override
	{
		auto* device = Graphics::GraphicsModule::Get().GetDevice();
		auto* context = Graphics::GraphicsModule::Get().GetContext();
		for (unsigned index = 0; index < mQueries.size(); ++index)
		{
			const auto status = mQueries[index].Initialize(device, static_cast<GpuQueryType>(index), "Query test", 8);
			Require(status == GpuQueryStatus::Success || status == GpuQueryStatus::Unsupported, "GPU query creation failed");
			mSupported[index] = status == GpuQueryStatus::Success;
		}
		if (mSupported[1])
		{
			Require(mBounded.Initialize(device, GpuQueryType::Timestamp, "Bounded timestamps", 2) == GpuQueryStatus::Success,
				"Timestamp pool creation failed");
			Require(mBounded.WriteTimestamp(context, 101) == GpuQueryStatus::Success, "First timestamp submission failed");
			Require(mBounded.WriteTimestamp(context, 102) == GpuQueryStatus::Success, "Second timestamp submission failed");
			Require(mBounded.WriteTimestamp(context, 103) == GpuQueryStatus::CapacityExceeded, "Full query pool grew or overwrote pending results");
			Require(mBounded.Begin(context) == GpuQueryStatus::InvalidState, "Timestamp accepted Begin");
			GpuQueryResult output;
			output.mFrame = 999;
			const auto polled = mBounded.TryGetResult(output);
			Require(polled == GpuQueryStatus::Success || (polled == GpuQueryStatus::Pending && output.mFrame == 999),
				"Pending poll modified output or failed");
		}
	}

	void Update(float) override
	{
		++mFrames;
		auto* window = Core::Engine::Get().GetMainWindow();
		if (mFrames == 1)
		{
			window->Display(false);
		}
		if (mFrames == 8)
		{
			ProfilingModule::Get().SetEnabled(true);
		}
		for (unsigned index = 0; index < mQueries.size(); ++index)
		{
			GpuQueryResult result;
			const auto status = mQueries[index].TryGetResult(result);
			if (status == GpuQueryStatus::Success)
			{
				++mResults[index];
				Require(result.mFrame >= 1 && result.mFrame < 8, "GPU result lost originating frame");
				if (index <= 1)
				{
					Require(result.mFrequency && result.mDurationMilliseconds >= 0, "GPU timing was invalid");
				}
			}
			else
			{
				Require(status == GpuQueryStatus::Pending || status == GpuQueryStatus::Unsupported, "GPU result invalid/disjoint");
			}
		}
		const auto snapshot = ProfilingModule::Get().GetSnapshot();
		for (const auto& frame : snapshot.mFrames)
		{
			mSawGpuFrame = mSawGpuFrame || frame.mGpuValid;
			mSawPipeline = mSawPipeline || frame.mPipelineValid;
			mSawCpuScope = mSawCpuScope || !frame.mCpuTimings.empty();
		}
		if (mFrames == 70)
		{
			PostMessageW(static_cast<HWND>(window->GetNativeHandle()), WM_CLOSE, 0, 0);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	void Render(float) override
	{
		PROFILE_CPU("GPU test client");
		auto& graphics = Graphics::GraphicsModule::Get();
		auto* context = graphics.GetContext();
		std::array<bool, 4> active{};
		if (mFrames < 8)
		{
			for (unsigned index : { 0u, 2u, 3u })
			{
				if (mSupported[index])
				{
					Require(mQueries[index].Begin(context, mFrames) == GpuQueryStatus::Success, "Begin GPU region failed");
					active[index] = true;
				}
			}
		}
		auto* target = graphics.GetSwapChain()->GetCurrentBackBufferRTV();
		auto* depth = graphics.GetSwapChain()->GetDepthBufferDSV();
		context->SetRenderTargets(1, &target, depth, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
		const float color[] = { 0.15f, 0.2f, 0.25f, 1.0f };
		context->ClearRenderTarget(target, color, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
		for (unsigned index : { 0u, 2u, 3u })
		{
			if (active[index])
			{
				Require(mQueries[index].End(context) == GpuQueryStatus::Success, "End GPU region failed");
			}
		}
		if (mFrames < 8 && mSupported[1])
		{
			Require(mQueries[1].WriteTimestamp(context, mFrames) == GpuQueryStatus::Success, "Timestamp recording failed");
		}
		DrawProfilerWindow();
	}

	void Shutdown() override
	{
		for (auto& query : mQueries)
		{
			query.Shutdown();
		}
		mBounded.Shutdown();
	}
};

int main()
{
	try
	{
		Core::EngineStartupDesc desc;
		desc.AutoInitAudioModule = false;
		desc.AutoInitScriptingModule = false;
		desc.AutoInitPhysXModule = false;
		desc.AutoInitThreadingModule = false;
		desc.mAssetsLibraryPath = "../Assets/";
		Require(Core::Engine::Get().Start(desc), "Profiling engine startup failed");
		ProfilingClient client;
		Core::Engine::Get().LoadClient(&client);
		Core::Engine::Get().EndClient();
		const auto captured = ProfilingModule::Get().GetSnapshot();
		Require(client.mSawCpuScope, "No CPU scopes captured");
		Require(!captured.mGpuTimingSupported || client.mSawGpuFrame, "No asynchronous GPU frame timings arrived");
		Require(!captured.mPipelineSupported || client.mSawPipeline, "No GPU pipeline results arrived");
		for (unsigned index = 0; index < client.mQueries.size(); ++index)
		{
			std::cout << "Query " << index << " supported=" << client.mSupported[index] << " results=" << client.mResults[index] << "\n";
			Require(!client.mSupported[index] || client.mResults[index], "Supported query produced no results");
		}
		Core::Engine::Get().Shutdown();
		Require(!Core::Engine::Get().GetMainWindow()->GetNativeHandle(), "Profiling prevented window teardown");
		std::cout << "GPU queries, asynchronous frame correlation, profiler UI, and shutdown checks passed.\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		Core::Engine::Get().Shutdown();
		std::cerr << error.what() << "\n";
		return 1;
	}
}
