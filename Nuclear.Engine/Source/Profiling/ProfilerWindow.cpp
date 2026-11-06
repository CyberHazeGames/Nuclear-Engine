#include <Profiling/ProfilingModule.h>
#include <Profiling/MemoryProfiler.h>
#include <Graphics/ImGui.h>
#include <algorithm>

namespace Nuclear::Profiling
{
	static double MiB(Uint64 bytes) { return static_cast<double>(bytes) / (1024.0 * 1024.0); }

	void DrawProfilerWindow(bool* open)
	{
		if (!ImGui::GetCurrentContext())
		{
			return;
		}
		if (!ImGui::Begin("Performance & Memory Profiler", open))
		{
			ImGui::End();
			return;
		}
		auto& profiler = ProfilingModule::Get();
		const auto snapshot = profiler.GetSnapshot();
		bool enabled = snapshot.mEnabled;
		if (ImGui::Checkbox("Capture", &enabled))
		{
			profiler.SetEnabled(enabled);
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear history"))
		{
			profiler.ClearHistory();
		}
		ImGui::Text("CPU: %s (%u cores / %u logical)", snapshot.mCpu.mName.c_str(),
			snapshot.mCpu.mPhysicalCores, snapshot.mCpu.mLogicalProcessors);
		if (snapshot.mCounters.mProcessCpuValid)
		{
			ImGui::Text("Process CPU: %.1f%% of machine capacity", snapshot.mCounters.mProcessCpuPercent);
		}
		if (snapshot.mCounters.mSystemCpuValid)
		{
			ImGui::Text("System CPU: %.1f%%", snapshot.mCounters.mSystemCpuPercent);
		}
		std::vector<float> cpu, gpu;
		double total = 0;
		for (const auto& frame : snapshot.mFrames)
		{
			cpu.push_back(static_cast<float>(frame.mCpuMilliseconds));
			total += frame.mCpuMilliseconds;
			if (frame.mGpuValid)
			{
				gpu.push_back(static_cast<float>(frame.mGpuMilliseconds));
			}
		}
		if (!cpu.empty())
		{
			const auto [minimum, maximum] = std::minmax_element(cpu.begin(), cpu.end());
			const double average = total / cpu.size();
			ImGui::Text("CPU frame: %.3f ms avg (%.3f min / %.3f max), %.1f FPS", average,
				*minimum, *maximum, average > 0 ? 1000.0 / average : 0.0);
			ImGui::PlotLines("CPU ms", cpu.data(), static_cast<int>(cpu.size()), 0, nullptr, 0, FLT_MAX, ImVec2(0, 60));
		}
		if (!gpu.empty())
		{
			ImGui::Text("Latest completed GPU frame: %.3f ms (results arrive later)", gpu.back());
			ImGui::PlotLines("GPU ms", gpu.data(), static_cast<int>(gpu.size()), 0, nullptr, 0, FLT_MAX, ImVec2(0, 60));
		}
		else
		{
			ImGui::TextUnformatted(snapshot.mGpuTimingSupported ? "GPU timing: pending capture/results" : "GPU timing: unsupported/disabled by renderer");
		}
		ImGui::Text("Dropped CPU/GPU: %llu / %llu; invalid GPU: %llu",
			snapshot.mDroppedCpuScopes, snapshot.mDroppedGpuQueries, snapshot.mInvalidGpuResults);

		if (!snapshot.mFrames.empty() && ImGui::CollapsingHeader("CPU scopes (inclusive wall time)"))
		{
			if (ImGui::BeginTable("CpuScopeTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
			{
				ImGui::TableSetupColumn("Scope");
				ImGui::TableSetupColumn("Thread");
				ImGui::TableSetupColumn("Calls");
				ImGui::TableSetupColumn("Total ms");
				ImGui::TableSetupColumn("Max ms");
				ImGui::TableHeadersRow();
				for (const auto& scope : snapshot.mFrames.back().mCpuTimings)
				{
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(scope.mName.c_str());
					ImGui::TableNextColumn();
					ImGui::Text("%llu", scope.mThread);
					ImGui::TableNextColumn();
					ImGui::Text("%u", scope.mCalls);
					ImGui::TableNextColumn();
					ImGui::Text("%.3f", scope.mTotalMilliseconds);
					ImGui::TableNextColumn();
					ImGui::Text("%.3f", scope.mMaximumMilliseconds);
				}
				ImGui::EndTable();
			}
		}
		if (ImGui::CollapsingHeader("GPU pipeline statistics"))
		{
			ImGui::TextWrapped("Counters depend on backend support; OpenGL only supplies clipping invocations.");
			const auto frame = std::find_if(snapshot.mFrames.rbegin(), snapshot.mFrames.rend(),
				[](const auto& value) { return value.mPipelineValid; });
			if (frame != snapshot.mFrames.rend())
			{
				ImGui::Text("Frame %llu: vertices %llu, primitives %llu", frame->mFrame,
					frame->mPipeline.mInputVertices, frame->mPipeline.mInputPrimitives);
				ImGui::Text("Shader invocations: VS %llu, PS %llu, CS %llu", frame->mPipeline.mVertexInvocations,
					frame->mPipeline.mPixelInvocations, frame->mPipeline.mComputeInvocations);
				ImGui::Text("Clipping invocations: %llu", frame->mPipeline.mClippingInvocations);
			}
			else
			{
				ImGui::TextUnformatted(snapshot.mPipelineSupported ? "Pending results" : "Unsupported/disabled by renderer");
			}
		}
		if (ImGui::CollapsingHeader("Process & GPU memory", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (snapshot.mCounters.mMemoryValid)
			{
				const auto& memory = snapshot.mCounters.mMemory;
				ImGui::Text("Working set: %.2f MiB (peak %.2f)", MiB(memory.mWorkingSetBytes), MiB(memory.mPeakWorkingSetBytes));
				ImGui::Text("Private committed: %.2f MiB; faults: %llu", MiB(memory.mPrivateCommittedBytes), memory.mPageFaultCount);
				ImGui::Text("Physical available: %.2f / %.2f MiB", MiB(memory.mAvailablePhysicalBytes), MiB(memory.mTotalPhysicalBytes));
			}
			else
			{
				ImGui::TextUnformatted("Process memory: capture disabled or query unavailable");
			}
			const auto selected = std::find_if(snapshot.mGpus.begin(), snapshot.mGpus.end(),
				[&](const auto& adapter) { return adapter.mAdapterId == snapshot.mGpuAdapterId; });
			const std::string selectedName = selected == snapshot.mGpus.end() ? "Select / inspect GPU" :
				selected->mName + " [" + std::to_string(selected->mAdapterId) + "]";
			if (ImGui::BeginCombo("Memory adapter", selectedName.c_str()))
			{
				for (const auto& adapter : snapshot.mGpus)
				{
					const auto label = adapter.mName + " [" + std::to_string(adapter.mAdapterId) + "]";
					if (ImGui::Selectable(label.c_str(), adapter.mAdapterId == snapshot.mGpuAdapterId))
					{
						profiler.SetGpuMemoryAdapter(adapter.mAdapterId);
					}
				}
				ImGui::EndCombo();
			}
			if (snapshot.mCounters.mGpuMemoryValid)
			{
				const auto& memory = snapshot.mCounters.mGpuMemory;
				ImGui::Text("GPU process local: %.2f / %.2f MiB budget", MiB(memory.mLocalUsageBytes), MiB(memory.mLocalBudgetBytes));
				ImGui::Text("GPU process non-local: %.2f / %.2f MiB budget", MiB(memory.mNonLocalUsageBytes), MiB(memory.mNonLocalBudgetBytes));
			}
			else
			{
				ImGui::TextUnformatted("GPU budget: pending, unsupported, or no exact adapter selected");
			}
		}
		if (ImGui::CollapsingHeader("Explicit allocation tracking"))
		{
			const auto memory = MemoryProfiler::Get().GetSnapshot();
			ImGui::Text("Tracked live %.2f MiB, peak %.2f MiB, %llu allocations", MiB(memory.mLiveBytes),
				MiB(memory.mPeakBytes), memory.mLiveAllocations);
			ImGui::Text("Rejected records: %llu", memory.mRejectedAllocations);
			ImGui::TextWrapped("Only allocations explicitly registered with MemoryProfiler are included. Process memory above includes all process activity.");
			for (const auto& category : memory.mCategories)
			{
				ImGui::Text("%s: %.2f MiB live / %.2f MiB peak (%llu allocations)", category.mName.c_str(),
					MiB(category.mLiveBytes), MiB(category.mPeakBytes), category.mLiveAllocations);
			}
			if (ImGui::Button("Reset allocation counters"))
			{
				MemoryProfiler::Get().ResetCounters();
			}
		}
		ImGui::End();
	}
}
