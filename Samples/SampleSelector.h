#pragma once
#include "Common.h"
#include "Playground.h"
#include "Sample1.h"
#include "Sample2.h"
#include "Sample3.h"
#include "Sample4.h"
#include "Sample5.h"
#include "UISample.h"
#include <cmath>
#include <iomanip>
#include <sstream>

/// Retained sample launcher. Navigation is queued until Render finishes this client;
/// Main owns the selected sample and starts it after input dispatch has unwound.
class SampleSelector : public Core::Client
{
public:
	/// Sample factories are evaluated by the outer application loop, never inside UI callbacks.
	enum class Selection
	{
		None,
		RuntimeUI,
		BasicRendering,
		RenderingPipelines,
		PhysicsAndScripting,
		MultipleCameras,
		AsyncAssets,
		Playground
	};

	Assets::Shader* PBR = nullptr; ///< Borrowed shader used by the existing library import action.

	/// Builds the viewport tree and restores launcher presentation settings.
	void Load() override
	{
		mSelection = Selection::None;
		mBuildRequested = false;
		mBuildStatusPresented = false;
		mTimingElapsed = 0;
		mTimingFrames = 0;
		Platform::Input::Get().SetMouseInputMode(Platform::Input::MouseInputMode::Normal);
		Core::Engine::Get().SetSwapChainSyncInterval(sVSync ? 1 : 0);
		auto& context = UI::UIModule::Get().GetContext();
		int width = 0;
		int height = 0;
		Core::Engine::Get().GetMainWindow()->GetSize(width, height);
		ResizeUI(width, height);
		context.AddToViewport(BuildWidgets());
		context.SetInputMode(UI::InputMode::UIOnly);
		UpdatePerformanceText();
	}

	/// Updates retained readouts at most four times per second; time is engine frame seconds.
	void Update(float deltaTime) override
	{
		if (!std::isfinite(deltaTime) || deltaTime <= 0)
			return;
		mTimingElapsed += deltaTime;
		++mTimingFrames;
		if (mTimingElapsed < .25f)
			return;
		std::ostringstream timing;
		timing << std::fixed << std::setprecision(1) << mTimingFrames / mTimingElapsed << " FPS   |   "
			   << std::setprecision(2) << mTimingElapsed * 1000 / mTimingFrames << " ms / frame";
		SetText(mTimingLabel, timing.str());
		UpdatePerformanceText();
		mTimingElapsed = 0;
		mTimingFrames = 0;
	}

	/// Resizes graphics and scales the launcher to keep all controls inside smaller viewports.
	void OnWindowResize(int width, int height) override
	{
		Graphics::GraphicsModule::Get().ResizeSwapChain(width, height);
		ResizeUI(width, height);
	}

	/// Returns an owned instance for the queued selection, or null when the window simply closed.
	/// Call after LoadClient returns and the selector has been ended.
	std::unique_ptr<Core::Client> CreateSelectedSample() const
	{
		switch (mSelection)
		{
		case Selection::RuntimeUI:
			return std::make_unique<UISample>();
		case Selection::BasicRendering:
			return std::make_unique<Sample1>();
		case Selection::RenderingPipelines:
			return std::make_unique<Sample2>();
		case Selection::PhysicsAndScripting:
			return std::make_unique<Sample3>();
		case Selection::MultipleCameras:
			return std::make_unique<Sample4>();
		case Selection::AsyncAssets:
			return std::make_unique<Sample5>();
		case Selection::Playground:
			return std::make_unique<Playground>();
		default:
			return nullptr;
		}
	}

	void ImportModels()
	{
		PBR = GetAssetManager().Import<Assets::Shader>("@NuclearAssets@/Shaders/PBR/PBR.NuclearShader");

		GetAssetManager().Import<Assets::Mesh>("@CommonAssets@/Models/CrytekNanosuit/nanosuit.obj");

		GetAssetManager().Import<Assets::Mesh>("@CommonAssets@/Models/CrytekCyborg/cyborg.obj");

		GetAssetManager().Import<Assets::Mesh>("@CommonAssets@/Models/Bob/boblampclean.md5mesh");

		GetAssetManager().Import<Assets::Mesh>("@CommonAssets@/Models/vampire/dancing_vampire.dae");

		Assets::MeshImportingDesc sponza_desc;
		sponza_desc.pMaterialShader = PBR;
		GetAssetManager().Import<Assets::Mesh>("@CommonAssets@/Models/CrytekSponza/Sponza.fbx", sponza_desc);

		Assets::MeshImportingDesc ModelDesc;
		ModelDesc.mImportMaterial = false;
		ModelDesc.mImportAnimations = false;

		GetAssetManager().Import<Assets::Mesh>("@CommonAssets@/Models/shaderball/shaderball.obj", ModelDesc);
	}

	void ImportShaders()
	{

		GetAssetManager().Import<Assets::Shader>("@NuclearAssets@/Shaders/BlinnPhong.NuclearShader");
		GetAssetManager().Import<Assets::Shader>("@NuclearAssets@/Shaders/DiffuseOnly.NuclearShader");
	}

	void ImportPBRAssets()
	{
		Assets::Material RustedIron;
		Assets::Material Plastic;
		Assets::Material Grass;
		Assets::Material Gold;
		Assets::Material Wall;

		Assets::TextureImportingDesc texdesc;
		texdesc.mCommonOptions.mExportPath = "@Assets@/Textures/PBR/RustedIron/";

		Assets::MaterialTextureSet PBRRustedIron;
		PBRRustedIron.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/PBR/RustedIron/albedo.png", texdesc) , Assets::TextureUsageType::Diffuse });
		PBRRustedIron.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/PBR/RustedIron/metallic.png", texdesc) , Assets::TextureUsageType::Specular });
		PBRRustedIron.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/PBR/RustedIron/normal.png", texdesc) , Assets::TextureUsageType::Normal });
		PBRRustedIron.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/PBR/RustedIron/roughness.png", texdesc) ,  Assets::TextureUsageType::Roughness });
		PBRRustedIron.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/PBR/RustedIron/ao.png", texdesc) ,  Assets::TextureUsageType::AO });
		RustedIron.GetTextures().push_back(PBRRustedIron);
		RustedIron.SetName("RustedIron");

		texdesc.mCommonOptions.mExportPath = "@Assets@/Textures/PBR/plastic/";

		Assets::MaterialTextureSet PBRPlastic;
		PBRPlastic.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/PBR/plastic/albedo.png", texdesc) , Assets::TextureUsageType::Diffuse });
		PBRPlastic.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/PBR/plastic/metallic.png", texdesc) , Assets::TextureUsageType::Specular });
		PBRPlastic.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/PBR/plastic/normal.png", texdesc) , Assets::TextureUsageType::Normal });
		PBRPlastic.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/PBR/plastic/roughness.png", texdesc) ,  Assets::TextureUsageType::Roughness });
		PBRPlastic.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/PBR/plastic/ao.png", texdesc) ,  Assets::TextureUsageType::AO });
		Plastic.GetTextures().push_back(PBRPlastic);
		Plastic.SetName("Plastic");

		texdesc.mCommonOptions.mExportPath = "@Assets@/Textures/PBR/grass/";

		Assets::MaterialTextureSet PBRGrass;
		PBRGrass.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/PBR/grass/albedo.png", texdesc) , Assets::TextureUsageType::Diffuse });
		PBRGrass.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/PBR/grass/metallic.png", texdesc) , Assets::TextureUsageType::Specular });
		PBRGrass.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/PBR/grass/normal.png", texdesc) , Assets::TextureUsageType::Normal });
		PBRGrass.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/PBR/grass/roughness.png", texdesc) ,  Assets::TextureUsageType::Roughness });
		PBRGrass.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/PBR/grass/ao.png", texdesc) ,  Assets::TextureUsageType::AO });
		Grass.GetTextures().push_back(PBRGrass);
		Grass.SetName("Grass");

		texdesc.mCommonOptions.mExportPath = "@Assets@/Textures/PBR/wall/";

		Assets::MaterialTextureSet PBRWall;
		PBRWall.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/PBR/wall/albedo.png", texdesc) , Assets::TextureUsageType::Diffuse });
		PBRWall.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/PBR/wall/metallic.png", texdesc) , Assets::TextureUsageType::Specular });
		PBRWall.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/PBR/wall/normal.png", texdesc) , Assets::TextureUsageType::Normal });
		PBRWall.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/PBR/wall/roughness.png", texdesc) ,  Assets::TextureUsageType::Roughness });
		PBRWall.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/PBR/wall/ao.png", texdesc) ,  Assets::TextureUsageType::AO });
		Wall.GetTextures().push_back(PBRWall);
		Wall.SetName("Wall");

		texdesc.mCommonOptions.mExportPath = "@Assets@/Textures/PBR/gold/";

		Assets::MaterialTextureSet PBRGold;
		PBRGold.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/PBR/gold/albedo.png", texdesc) , Assets::TextureUsageType::Diffuse });
		PBRGold.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/PBR/gold/metallic.png", texdesc) , Assets::TextureUsageType::Specular });
		PBRGold.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/PBR/gold/normal.png", texdesc) , Assets::TextureUsageType::Normal });
		PBRGold.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/PBR/gold/roughness.png", texdesc) ,  Assets::TextureUsageType::Roughness });
		PBRGold.mData.push_back({ GetAssetManager().Import<Assets::Texture>("@CommonAssets@/Textures/PBR/gold/ao.png", texdesc) ,  Assets::TextureUsageType::AO });
		Gold.GetTextures().push_back(PBRGold);
		Gold.SetName("Gold");

		RustedIron.Create(PBR);
		Plastic.Create(PBR);
		Grass.Create(PBR);
		Wall.Create(PBR);
		Gold.Create(PBR);


		//Export Materials
		std::string path = "@Assets@/Materials/PBR/";
		GetAssetManager().Export(&RustedIron, true, path);
		GetAssetManager().Export(&Plastic, true, path);
		GetAssetManager().Export(&Grass, true, path);
		GetAssetManager().Export(&Wall, true, path);
		GetAssetManager().Export(&Gold, true, path);
	}

	/// Clears the scene background; retained UI is composed by Engine::EndFrame.
	/// Pending actions execute here, outside the input callback/destruction guard.
	void Render(float) override
	{
		auto& graphics = Graphics::GraphicsModule::Get();
		auto* target = graphics.GetSwapChain()->GetCurrentBackBufferRTV();
		graphics.GetContext()->SetRenderTargets(1, &target, nullptr,
												Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
		const float clear[] = {.012f, .02f, .035f, 1};
		graphics.GetContext()->ClearRenderTarget(target, clear, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
		if (mSelection != Selection::None)
		{
			Core::Engine::Get().EndClient();
			return;
		}
		if (mBuildRequested)
		{
			// Present the status once before starting the existing synchronous import flow.
			if (!mBuildStatusPresented)
			{
				mBuildStatusPresented = true;
				return;
			}
			mBuildRequested = false;
			try
			{
				ImportModels();
				ImportPBRAssets();
				SetText(mStatusLabel,
						"Import requests submitted. Check the engine log for missing content or failures.");
			}
			catch (const std::exception& error)
			{
				SetText(mStatusLabel, std::string("Asset import failed: ") + error.what());
				CLIENT_ERROR("[SampleSelector] Asset import failed: {0}", error.what());
			}
			if (auto* button = UI::UIModule::Get().GetContext().Find(mBuildButton))
				button->SetEnabled(true);
		}
	}

private:
	/// Sets physical viewport dimensions and a scale that fits the 1280x720 launcher design.
	static void ResizeUI(int width, int height)
	{
		const float scale = width > 0 && height > 0 ? std::min({1.0f, width / 1280.0f, height / 720.0f}) : 1;
		UI::UIModule::Get().GetContext().SetViewport(static_cast<float>(width), static_cast<float>(height), scale);
	}

	/// Adds a styled label; the returned reference is borrowed from its parent.
	static UI::TextBlock& AddLabel(UI::PanelWidget& parent, std::string text, float size, UI::Color color = {})
	{
		auto& label = parent.Add<UI::TextBlock>(std::move(text));
		label.SetFontSize(size);
		label.SetColor(color);
		label.SetWrap(true);
		return label;
	}

	/// Adds an action with a title and optional subtitle; callbacks must queue client transitions.
	static UI::Button& AddAction(UI::PanelWidget& parent, const char* title, const char* subtitle,
								 std::function<void()> callback)
	{
		auto& button = parent.Add<UI::Button>();
		button.SetPadding({14, 6, 14, 6});
		auto& content = button.Add<UI::VerticalBox>();
		content.SetSpacing(3);
		AddLabel(content, title, 18);
		if (subtitle && *subtitle)
			AddLabel(content, subtitle, 13, {.55f, .65f, .78f, 1});
		button.OnClicked = std::move(callback);
		return button;
	}

	/// Builds navigation, settings, library actions, and compact performance/memory readouts.
	std::unique_ptr<UI::Widget> BuildWidgets()
	{
		using namespace UI;
		auto canvas = std::make_unique<CanvasPanel>();
		auto& card = canvas->Add<Border>();
		card.SetBrush({nullptr, {.025f, .04f, .065f, 1}});
		card.SetPadding({24, 20, 24, 20});
		Slot cardSlot;
		cardSlot.AnchorMax = {1, 1};
		cardSlot.Offsets = {24, 24, 24, 24};
		canvas->SetSlot(card.GetId(), cardSlot);
		auto& layout = card.Add<VerticalBox>();
		layout.SetSpacing(12);
		auto& header = layout.Add<VerticalBox>();
		header.SetSpacing(4);
		AddLabel(header, "NUCLEAR ENGINE", 30, {.4f, .78f, 1, 1});
		AddLabel(header, "Samples & experiments", 16, {.55f, .65f, .78f, 1});

		auto& columns = layout.Add<HorizontalBox>();
		columns.SetSpacing(24);
		Slot bodySlot;
		bodySlot.Fill = 1;
		layout.SetSlot(columns.GetId(), bodySlot);
		auto& samples = columns.Add<VerticalBox>();
		samples.SetSpacing(8);
		Slot columnSlot;
		columnSlot.Fill = 1.65f;
		columns.SetSlot(samples.GetId(), columnSlot);
		AddLabel(samples, "SELECT A SAMPLE", 18, {.65f, .75f, .9f, 1});
		auto addSample = [this, &samples](const char* name, const char* description, Selection selection)
		{
			AddAction(samples, name, description,
					  [this, selection]()
					  {
						  if (mSelection == Selection::None)
							  mSelection = selection;
					  });
		};
		addSample("Runtime UI", "Basic retained widgets", Selection::RuntimeUI);
		addSample("Sample 1", "Models, animation & basic rendering", Selection::BasicRendering);
		addSample("Sample 2", "Rendering pipelines & shading models", Selection::RenderingPipelines);
		addSample("Sample 3", "PhysX & C# scripting (work in progress)", Selection::PhysicsAndScripting);
		addSample("Sample 4", "Multiple cameras", Selection::MultipleCameras);
		addSample("Sample 5", "Asynchronous asset importing", Selection::AsyncAssets);
		addSample("Playground", "Engine experiments & audio", Selection::Playground);

		auto& toolsBorder = columns.Add<Border>();
		toolsBorder.SetBrush({nullptr, {.04f, .06f, .095f, 1}});
		toolsBorder.SetPadding({18, 16, 18, 16});
		columnSlot.Fill = 1;
		columns.SetSlot(toolsBorder.GetId(), columnSlot);
		auto& tools = toolsBorder.Add<VerticalBox>();
		tools.SetSpacing(10);
		AddLabel(tools, "TOOLS & SETTINGS", 18, {.65f, .75f, .9f, 1});
		auto& vsync = tools.Add<CheckBox>();
		AddLabel(vsync, "VSync", 16);
		vsync.SetChecked(sVSync);
		vsync.OnChanged = [](bool enabled)
		{
			sVSync = enabled;
			Core::Engine::Get().SetSwapChainSyncInterval(enabled ? 1 : 0);
		};
		auto& performance = tools.Add<CheckBox>();
		AddLabel(performance, "Performance & memory", 16);
		performance.SetChecked(sShowPerformance);
		performance.OnChanged = [this](bool visible)
		{
			sShowPerformance = visible;
			if (auto* panel = UIModule::Get().GetContext().Find(mPerformancePanel))
				panel->SetVisibility(visible ? Visibility::Visible : Visibility::Collapsed);
			if (visible)
				UpdatePerformanceText();
		};

		auto& performancePanel = tools.Add<Border>();
		mPerformancePanel = performancePanel.GetId();
		performancePanel.SetBrush({nullptr, {.02f, .035f, .06f, 1}});
		performancePanel.SetPadding({10, 10, 10, 10});
		performancePanel.SetVisibility(sShowPerformance ? Visibility::Visible : Visibility::Collapsed);
		auto& diagnostics = performancePanel.Add<VerticalBox>();
		diagnostics.SetSpacing(6);
		auto& capture = diagnostics.Add<CheckBox>();
		AddLabel(capture, "Capture performance", 14);
		capture.SetChecked(Profiling::ProfilingModule::Get().GetSnapshot().mEnabled);
		capture.OnChanged = [](bool enabled) { Profiling::ProfilingModule::Get().SetEnabled(enabled); };
		AddAction(diagnostics, "Clear history", nullptr,
				  [this]()
				  {
					  Profiling::ProfilingModule::Get().ClearHistory();
					  UpdatePerformanceText();
				  });
		mPerformanceLabel = AddLabel(diagnostics, "Capture paused", 13).GetId();

		auto& build = AddAction(tools, "Build assets library", "Import models & PBR materials",
								[this]()
								{
									mBuildRequested = true;
									mBuildStatusPresented = false;
									SetText(mStatusLabel, "Building assets library...");
									if (auto* button = UIModule::Get().GetContext().Find(mBuildButton))
										button->SetEnabled(false);
								});
		mBuildButton = build.GetId();
		mStatusLabel = AddLabel(tools, "Asset imports use local sample content.", 13, {.55f, .65f, .78f, 1}).GetId();
		mTimingLabel =
			AddLabel(layout, "Tab to focus  |  Enter / Space to activate", 14, {.55f, .65f, .78f, 1}).GetId();
		return canvas;
	}

	/// Resolves the live label by stable handle; no-op after the selector tree has been removed.
	static void SetText(UI::WidgetId id, const std::string& text)
	{
		if (auto* label = dynamic_cast<UI::TextBlock*>(UI::UIModule::Get().GetContext().Find(id)))
			label->SetText(text);
	}

	/// Copies the profiler's bounded snapshot only when the retained diagnostics panel is open.
	void UpdatePerformanceText()
	{
		if (!sShowPerformance)
			return;
		const auto snapshot = Profiling::ProfilingModule::Get().GetSnapshot();
		const auto tracked = Profiling::MemoryProfiler::Get().GetSnapshot();
		const auto& counters = snapshot.mCounters;
		auto mib = [](Uint64 bytes) { return bytes / (1024.0 * 1024.0); };
		std::ostringstream text;
		text << std::fixed << std::setprecision(1);
		text << (snapshot.mEnabled ? "Capture active" : "Capture paused") << "\n";
		text << "Process CPU: ";
		if (counters.mProcessCpuValid)
			text << counters.mProcessCpuPercent << "%";
		else
			text << "unavailable";
		text << "\nWorking set: ";
		if (counters.mMemoryValid)
			text << mib(counters.mMemory.mWorkingSetBytes) << " MiB";
		else
			text << "unavailable";
		text << "\nGPU memory: ";
		if (counters.mGpuMemoryValid)
			text << mib(counters.mGpuMemory.mLocalUsageBytes) << " / " << mib(counters.mGpuMemory.mLocalBudgetBytes)
				 << " MiB";
		else
			text << "unavailable";
		text << "\nGPU frame: ";
		auto gpuFrame = std::find_if(snapshot.mFrames.rbegin(), snapshot.mFrames.rend(),
									 [](const auto& frame) { return frame.mGpuValid; });
		if (gpuFrame != snapshot.mFrames.rend())
			text << gpuFrame->mGpuMilliseconds << " ms";
		else
			text << (snapshot.mGpuTimingSupported ? "pending" : "unavailable");
		text << "\nTracked allocations: " << mib(tracked.mLiveBytes) << " MiB";
		SetText(mPerformanceLabel, text.str());
	}

	Selection mSelection = Selection::None; ///< Queued navigation consumed by the outer application loop.
	bool mBuildRequested = false; ///< Runs the existing import action on the rendering thread.
	bool mBuildStatusPresented = false; ///< Allows one status frame before synchronous work starts.
	float mTimingElapsed = 0; ///< Accumulated seconds for the current frame-rate sample.
	unsigned mTimingFrames = 0; ///< Frames in the current timing interval.
	UI::WidgetId mTimingLabel = 0; ///< Stable frame timing label handle.
	UI::WidgetId mStatusLabel = 0; ///< Stable asset status label handle.
	UI::WidgetId mBuildButton = 0; ///< Stable import action handle.
	UI::WidgetId mPerformancePanel = 0; ///< Stable collapsible diagnostics panel handle.
	UI::WidgetId mPerformanceLabel = 0; ///< Stable diagnostic readout handle.
	inline static bool sVSync = false; ///< User preference survives returning from a sample.
	inline static bool sShowPerformance = false; ///< Retains diagnostics visibility between selector instances.
};
