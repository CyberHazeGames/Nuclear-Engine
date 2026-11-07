#include "../SampleSelector.h"
#include <Windows.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unordered_set>

namespace
{
	void Check(bool condition, const char* message)
	{
		if (!condition)
			throw std::runtime_error(message);
	}

	void PostKey(Platform::Input::KeyCode code, int count = 1)
	{
		const auto window = static_cast<HWND>(Core::Engine::Get().GetMainWindow()->GetNativeHandle());
		const unsigned scan =
			code == Platform::Input::KEYCODE_TAB ? 0x0f : (code == Platform::Input::KEYCODE_SPACE ? 0x39 : 0x1c);
		const WPARAM key = code == Platform::Input::KEYCODE_TAB
							   ? VK_TAB
							   : (code == Platform::Input::KEYCODE_SPACE ? VK_SPACE : VK_RETURN);
		for (int index = 0; index < count; ++index)
		{
			PostMessageW(window, WM_KEYDOWN, key, 1 | (scan << 16));
			PostMessageW(window, WM_KEYUP, key, 1 | (scan << 16) | (1u << 30) | (1u << 31));
		}
	}

	UI::CheckBox& FocusedCheckBox()
	{
		auto& context = UI::UIModule::Get().GetContext();
		auto* checkbox = dynamic_cast<UI::CheckBox*>(context.Find(context.GetFocusedWidget()));
		Check(checkbox != nullptr, "Expected checkbox in launcher focus order");
		return *checkbox;
	}

	void SaveFrame(const char* path)
	{
		using namespace Diligent;
		auto& graphics = Graphics::GraphicsModule::Get();
		auto* context = graphics.GetContext();
		auto* source = graphics.GetSwapChain()->GetCurrentBackBufferRTV()->GetTexture();
		auto description = source->GetDesc();
		Check(description.Format == TEX_FORMAT_RGBA8_UNORM, "Expected RGBA8 selector readback");
		description.Usage = USAGE_STAGING;
		description.BindFlags = BIND_NONE;
		description.CPUAccessFlags = CPU_ACCESS_READ;
		RefCntAutoPtr<ITexture> staging;
		graphics.GetDevice()->CreateTexture(description, nullptr, &staging);
		Check(staging != nullptr, "Selector readback allocation");
		context->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);
		CopyTextureAttribs copy;
		copy.pSrcTexture = source;
		copy.pDstTexture = staging;
		copy.SrcTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
		copy.DstTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
		context->CopyTexture(copy);
		context->WaitForIdle();
		MappedTextureSubresource mapped;
		context->MapTextureSubresource(staging, 0, 0, MAP_READ, MAP_FLAG_NONE, nullptr, mapped);
		Check(mapped.pData != nullptr, "Selector readback mapping");
		std::ofstream image(path, std::ios::binary);
		image << "P6\n" << description.Width << ' ' << description.Height << "\n255\n";
		unsigned titlePixels = 0;
		for (unsigned y = 0; y < description.Height; ++y)
		{
			const auto* row = static_cast<const unsigned char*>(mapped.pData) + y * mapped.Stride;
			for (unsigned x = 0; x < description.Width; ++x)
			{
				image.write(reinterpret_cast<const char*>(row + x * 4), 3);
				if (row[x * 4 + 1] > 150 && row[x * 4 + 2] > 220)
					++titlePixels;
			}
		}
		const bool saved = image.good();
		context->UnmapTextureSubresource(staging, 0, 0);
		Check(saved && titlePixels > 150, "Retained selector title rendered into back buffer");
	}

	class SelectorClient final : public SampleSelector
	{
	public:
		explicit SelectorClient(bool mouseSelection) : mMouseSelection(mouseSelection) {}
		int Frames = 0;
		bool VerifiedControls = false;
		bool VerifiedResize = false;

		void Update(float deltaTime) override
		{
			SampleSelector::Update(deltaTime);
			++Frames;
			auto& context = UI::UIModule::Get().GetContext();
			if (Frames == 2)
			{
				context.ResetInput();
				PostKey(Platform::Input::KEYCODE_TAB, 8);
			}
			if (Frames == 3)
			{
				mInitialVSync = FocusedCheckBox().IsChecked();
				PostKey(Platform::Input::KEYCODE_SPACE);
			}
			if (Frames == 4)
			{
				Check(FocusedCheckBox().IsChecked() != mInitialVSync, "VSync toggles through platform keyboard input");
				PostKey(Platform::Input::KEYCODE_TAB);
			}
			if (Frames == 5 && !FocusedCheckBox().IsChecked())
				PostKey(Platform::Input::KEYCODE_SPACE);
			if (Frames == 6)
				PostKey(Platform::Input::KEYCODE_TAB);
			if (Frames == 7)
			{
				mInitialCapture = Profiling::ProfilingModule::Get().GetSnapshot().mEnabled;
				Check(FocusedCheckBox().IsChecked() == mInitialCapture, "Capture checkbox reflects profiler state");
				PostKey(Platform::Input::KEYCODE_SPACE);
			}
			if (Frames == 8)
			{
				Check(Profiling::ProfilingModule::Get().GetSnapshot().mEnabled != mInitialCapture,
					  "Retained capture checkbox controls profiling module");
				PostKey(Platform::Input::KEYCODE_SPACE);
				PostKey(Platform::Input::KEYCODE_TAB);
				Core::Engine::Get().GetMainWindow()->SetSize(960, 540);
				VerifiedControls = true;
			}
			if (Frames == 9)
				PostKey(Platform::Input::KEYCODE_SPACE);
			if (Frames == 10)
			{
				Check(Profiling::ProfilingModule::Get().GetSnapshot().mFrames.empty(),
					  "Retained clear-history action clears profiling data");
				Check(std::abs(context.GetScale() - .75f) < .01f, "Selector scales with resized viewport");
				context.ResetInput();
				std::unordered_set<UI::WidgetId> visited;
				for (int index = 0; index < 12; ++index)
				{
					Platform::Event tab;
					tab.mType = Platform::EventType::Key;
					tab.mKey = Platform::Input::KEYCODE_TAB;
					tab.mPressed = true;
					context.ProcessEvent(tab);
					auto* focused = context.Find(context.GetFocusedWidget());
					Check(focused != nullptr, "All launcher actions are keyboard reachable");
					Check(visited.insert(focused->GetId()).second,
						  "Each expanded launcher action has a distinct focus target");
					const auto bounds = focused->GetBounds();
					Check(bounds.x >= 0 && bounds.y >= 0 && bounds.x + bounds.Width <= 1280 &&
							  bounds.y + bounds.Height <= 720,
						  "Launcher control remains inside viewport after resize");
					mBuildAction = focused->GetId();
				}
				VerifiedResize = true;
			}
			if (Frames == 11)
				PostKey(Platform::Input::KEYCODE_SPACE);
			if (Frames == 12)
			{
				Check(context.Find(mBuildAction) && !context.Find(mBuildAction)->IsEnabled(),
					  "Asset-library action queues work and disables repeated submissions");
				context.ResetInput();
				if (mMouseSelection)
				{
					Platform::Event tab;
					tab.mType = Platform::EventType::Key;
					tab.mKey = Platform::Input::KEYCODE_TAB;
					tab.mPressed = true;
					context.ProcessEvent(tab);
					const auto bounds = context.Find(context.GetFocusedWidget())->GetBounds();
					const int x = static_cast<int>((bounds.x + 20) * context.GetScale());
					const int y = static_cast<int>((bounds.y + 20) * context.GetScale());
					auto window = static_cast<HWND>(Core::Engine::Get().GetMainWindow()->GetNativeHandle());
					PostMessageW(window, WM_MOUSEMOVE, 0, MAKELPARAM(x, y));
					PostMessageW(window, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
					PostMessageW(window, WM_LBUTTONUP, 0, MAKELPARAM(x, y));
				}
				else
				{
					PostKey(Platform::Input::KEYCODE_TAB);
					PostKey(Platform::Input::KEYCODE_RETURN);
				}
			}
			Check(Frames < 40, "Selector navigation timed out");
		}

		void Render(float deltaTime) override
		{
			SampleSelector::Render(deltaTime);
			if (Core::Engine::Get().GetClient() != this)
				return;
			if (Frames == 2 || Frames == 10)
			{
				UI::UIModule::Get().Render();
				SaveFrame(Frames == 2 ? "../Nuclear.Engine/Build/Selector1280.ppm"
									  : "../Nuclear.Engine/Build/Selector960.ppm");
			}
		}

	private:
		bool mMouseSelection = false;
		bool mInitialVSync = false;
		bool mInitialCapture = false;
		UI::WidgetId mBuildAction = 0;
	};

	class SelectedClient final : public Core::Client
	{
	public:
		explicit SelectedClient(std::unique_ptr<Core::Client> sample) : pSample(std::move(sample)) {}
		int Frames = 0;
		void Load() override
		{
			pSample->Load();
		}
		void Update(float dt) override
		{
			pSample->Update(dt);
			if (++Frames == 3)
			{
				UI::UIModule::Get().GetContext().ResetInput();
				PostKey(Platform::Input::KEYCODE_TAB, 4);
				PostKey(Platform::Input::KEYCODE_SPACE);
			}
			Check(Frames < 20, "Selected sample return timed out");
		}
		void Render(float dt) override
		{
			pSample->Render(dt);
		}
		void Shutdown() override
		{
			pSample->Shutdown();
		}
		void OnWindowResize(int width, int height) override
		{
			pSample->OnWindowResize(width, height);
		}

	private:
		std::unique_ptr<Core::Client> pSample;
	};
} // namespace

int main(int argc, char** argv)
{
	using namespace Nuclear;
	auto& engine = Core::Engine::Get();
	try
	{
		Core::EngineStartupDesc desc;
		desc.EnableImGui = argc > 1 && std::string(argv[1]) == "--imgui";
		desc.AutoInitAudioModule = false;
		desc.AutoInitPhysXModule = false;
		desc.AutoInitScriptingModule = false;
		desc.AutoInitThreadingModule = false;
		desc.AutoInitRenderingModule = false;
		desc.mEngineWindowDesc.WindowWidth = 1280;
		desc.mEngineWindowDesc.WindowHeight = 720;
		desc.mEngineWindowDesc.Title = "Retained sample selector validation";
		desc.mAssetsLibraryPath = "../Assets/";
		Check(engine.Start(desc), "Selector engine startup");
		for (int run = 0; run < 2; ++run)
		{
			SelectorClient selector(run != 0);
			Check(!selector.CreateSelectedSample(), "No sample is constructed before selection");
			engine.LoadClient(&selector);
			engine.EndClient();
			Check(selector.VerifiedControls && selector.VerifiedResize && selector.Frames == 13,
				  "Selector settings, resize and deferred navigation completed");
			Check(UI::UIModule::Get().GetContext().BuildDrawList().Quads.empty(),
				  "Selector tree removed before sample launch");
			auto selected = selector.CreateSelectedSample();
			Check(dynamic_cast<UISample*>(selected.get()) != nullptr,
				  "Runtime UI selection creates the expected sample");
			SelectedClient sample(std::move(selected));
			engine.LoadClient(&sample);
			engine.EndClient();
			Check(sample.Frames == 3 && !engine.ShouldClose(),
				  "Sample returns to launcher without closing application");
			engine.GetMainWindow()->SetSize(1280, 720);
		}
		engine.Shutdown();
		std::cout << "Retained selector passed settings, capture, resize, keyboard/mouse launch, sample return and "
					 "reload. ImGui="
				  << desc.EnableImGui << '\n';
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << "Selector test failed: " << error.what() << '\n';
		engine.Shutdown();
		return 1;
	}
}
