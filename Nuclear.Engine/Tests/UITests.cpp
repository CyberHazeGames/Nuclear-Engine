#include "../../Samples/UISample.h"
#include <Assets/AssetLibrary.h>
#include <Assets/AssetManager.h>
#include <Assets/Font.h>
#include <Core/Engine.h>
#include <Graphics/GraphicsModule.h>
#include <Nuclear.Platform/Platform.h>
#include <UI/UI.h>
#include <Windows.h>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace Nuclear;
using namespace Nuclear::UI;

namespace
{
	void Check(bool condition, const char* message)
	{
		if (!condition)
			throw std::runtime_error(message);
	}
	bool Near(float left, float right)
	{
		return std::abs(left - right) < .01f;
	}
	Platform::Event Mouse(Platform::EventType type, int x, int y, bool pressed = false)
	{
		Platform::Event event;
		event.mType = type;
		event.mMouseX = x;
		event.mMouseY = y;
		event.mButton = MOUSE_BUTTON_LEFT;
		event.mPressed = pressed;
		return event;
	}
	void Click(UIContext& context, int x, int y)
	{
		context.ProcessEvent(Mouse(Platform::EventType::MouseButton, x, y, true));
		context.ProcessEvent(Mouse(Platform::EventType::MouseButton, x, y, false));
	}
	void Key(UIContext& context, Platform::Input::KeyCode code, bool pressed, bool repeat = false)
	{
		Platform::Event event;
		event.mType = Platform::EventType::Key;
		event.mKey = code;
		event.mPressed = pressed;
		event.mRepeat = repeat;
		context.ProcessEvent(event);
	}

	void PreviewEventTests()
	{
		UIContext context;
		context.SetViewport(400, 200);
		auto input = std::make_unique<TextBox>("Preview");
		const auto id = input->GetId();
		int committed = 0;
		input->OnCommitted = [&](const std::string&) { ++committed; };
		context.AddToViewport(std::move(input));
		context.SetFocus(id);
		context.OnPreviewEvent = [](const Platform::Event& event) {
			return event.mType == Platform::EventType::Key && event.mKey == Platform::Input::KEYCODE_RETURN;
		};
		Key(context, Platform::Input::KEYCODE_RETURN, true);
		Check(committed == 0, "Application shortcuts consume events before focused controls");
		int observed = 0;
		context.OnPreviewEvent = [&](const Platform::Event&) { ++observed; return false; };
		Key(context, Platform::Input::KEYCODE_RETURN, true);
		Check(committed == 1 && observed == 1, "Unconsumed preview events reach focused controls");
		Platform::Event focus;
		focus.mType = Platform::EventType::FocusChanged;
		context.ProcessEvent(focus);
		Check(!context.GetFocusedWidget() && observed == 2, "Focus loss reaches the filter and cancels controls");
		Key(context, Platform::Input::KEYCODE_RETURN, true);
		Check(observed == 2, "Unfocused windows cannot invoke application shortcuts");
		focus.mFocused = true;
		context.ProcessEvent(focus);
		context.SetInputMode(InputMode::GameOnly);
		Key(context, Platform::Input::KEYCODE_RETURN, true);
		Check(observed == 3, "GameOnly routing bypasses application UI shortcuts");
		context.Clear();
		Check(!context.OnPreviewEvent, "Context teardown releases application shortcut captures");
	}

	void LayoutTests()
	{
		UIContext context;
		context.SetViewport(800, 600, 2);
		auto canvas = std::make_unique<CanvasPanel>();
		auto& anchored = canvas->Add<Button>();
		Slot slot;
		slot.AnchorMin = slot.AnchorMax = {1, 1};
		slot.Pivot = {1, 1};
		slot.Offsets = {-10, -20, 100, 40};
		canvas->SetSlot(anchored.GetId(), slot);
		auto& stretched = canvas->Add<Border>();
		slot = {};
		slot.AnchorMax = {1, 1};
		slot.Offsets = {10, 15, 30, 25};
		canvas->SetSlot(stretched.GetId(), slot);
		context.AddToViewport(std::move(canvas));
		context.Layout();
		Check(Near(anchored.GetBounds().x, 290) && Near(anchored.GetBounds().y, 240), "Anchor/pivot logical sizing");
		Check(Near(stretched.GetBounds().Width, 360) && Near(stretched.GetBounds().Height, 260), "Stretched margins");
		context.SetViewport(1000, 800, 2);
		context.Layout();
		Check(Near(anchored.GetBounds().x, 390), "Resize invalidates anchored layout");
		Check(context.ProcessEvent(Mouse(Platform::EventType::MouseButton, 800, 700, true)), "Scaled hit testing");
		context.Clear();

		auto column = std::make_unique<VerticalBox>();
		column->SetSpacing(5);
		auto& first = column->Add<SizeBox>();
		first.SetSizeLimits({50, 20}, {50, 20});
		auto& second = column->Add<SizeBox>();
		second.SetSizeLimits({50, 30}, {50, 30});
		first.SetVisibility(Visibility::Hidden);
		context.AddToViewport(std::move(column));
		context.Layout();
		Check(Near(second.GetBounds().y, 25), "Hidden retains layout space");
		first.SetVisibility(Visibility::Collapsed);
		context.Layout();
		Check(Near(second.GetBounds().y, 0) && Near(first.GetDesiredSize().y, 0), "Collapsed removes space and gaps");
		context.Clear();

		auto row = std::make_unique<HorizontalBox>();
		row->SetSpacing(10);
		auto& fixed = row->Add<SizeBox>();
		fixed.SetSizeLimits({50, 20}, {50, 20});
		auto& fillA = row->Add<Border>();
		auto& fillB = row->Add<Border>();
		slot = {};
		slot.Fill = 1;
		row->SetSlot(fillA.GetId(), slot);
		slot.Fill = 3;
		row->SetSlot(fillB.GetId(), slot);
		context.SetViewport(470, 100);
		context.AddToViewport(std::move(row));
		context.Layout();
		Check(Near(fillA.GetBounds().Width, 100) && Near(fillB.GetBounds().Width, 300),
			  "Weighted fill distributes remaining space");
	}

	void InputTests()
	{
		UIContext context;
		context.SetViewport(300, 200);
		auto canvas = std::make_unique<CanvasPanel>();
		auto* panel = canvas.get();
		auto& lower = canvas->Add<Button>();
		auto& upper = canvas->Add<Button>();
		const auto lowerId = lower.GetId();
		const auto upperId = upper.GetId();
		int lowerClicks = 0;
		int upperClicks = 0;
		lower.OnClicked = [&]() { ++lowerClicks; };
		upper.OnClicked = [&]() { ++upperClicks; };
		context.AddToViewport(std::move(canvas));
		Click(context, 10, 10);
		Check(upperClicks == 1 && lowerClicks == 0, "Topmost hit target only");
		context.ProcessEvent(Mouse(Platform::EventType::MouseButton, 10, 10, true));
		context.ProcessEvent(Mouse(Platform::EventType::MouseButton, 200, 100, false));
		Check(upperClicks == 1 && !context.GetCapturedWidget(), "Release outside cancels button click");
		context.ProcessEvent(Mouse(Platform::EventType::MouseButton, 10, 10, true));
		upper.SetEnabled(false);
		Check(context.GetCapturedWidget() == 0 && context.GetFocusedWidget() == 0, "Disabled subtree cancels input");
		context.ProcessEvent(Mouse(Platform::EventType::MouseButton, 10, 10, false));
		Check(upperClicks == 1 && lowerClicks == 0, "No activation after disabling pressed control");
		upper.SetEnabled(true);
		context.ResetInput();
		Key(context, Platform::Input::KEYCODE_TAB, true);
		Check(context.GetFocusedWidget() == lowerId, "Tab begins at first focusable");
		Key(context, Platform::Input::KEYCODE_LSHIFT, true);
		Key(context, Platform::Input::KEYCODE_TAB, true);
		Key(context, Platform::Input::KEYCODE_LSHIFT, false);
		Check(context.GetFocusedWidget() == upperId, "Shift Tab wraps backwards");
		Key(context, Platform::Input::KEYCODE_SPACE, true);
		Key(context, Platform::Input::KEYCODE_SPACE, true, true);
		Key(context, Platform::Input::KEYCODE_SPACE, false);
		Check(upperClicks == 2, "Keyboard repeat activates only once on release");
		upper.OnClicked = [&]() { panel->RemoveChild(upperId); };
		Click(context, 10, 10);
		Check(!context.Find(upperId) && !context.GetCapturedWidget() && !context.GetFocusedWidget(),
			  "Self removal is deferred and clears handles");
		lower.OnClicked = [&]() { context.Clear(); };
		Click(context, 10, 10);
		Check(!context.Find(lowerId), "Clearing the viewport from a callback is safe");

		auto slider = std::make_unique<Slider>();
		auto* sliderPointer = slider.get();
		const auto sliderId = slider->GetId();
		int changes = 0;
		slider->OnChanged = [&](float) { ++changes; };
		context.AddToViewport(std::move(slider));
		context.ProcessEvent(Mouse(Platform::EventType::MouseButton, 20, 20, true));
		context.ProcessEvent(Mouse(Platform::EventType::MouseMove, 400, 20));
		Check(Near(sliderPointer->GetValue(), 1) && context.GetCapturedWidget() == sliderId && changes > 0,
			  "Slider captures and clamps outside viewport");
		Platform::Event focus;
		focus.mType = Platform::EventType::FocusChanged;
		focus.mFocused = false;
		context.ProcessEvent(focus);
		Check(!context.GetCapturedWidget() && !context.WantsMouse() && !context.WantsKeyboard(),
			  "Focus loss clears active controls");
		focus.mFocused = true;
		context.ProcessEvent(focus);
		context.SetInputMode(InputMode::GameOnly);
		Click(context, 100, 20);
		Check(!context.WantsMouse() && !context.GetFocusedWidget(), "GameOnly bypasses UI");
		context.SetInputMode(InputMode::UIOnly);
		Check(context.WantsMouse() && context.WantsKeyboard(), "UIOnly captures gameplay input queries");
		context.SetViewport(0, 0);
		Check(!context.WantsMouse(), "Minimized viewport suspends UI input");
	}

	void OwnershipTransferTests()
	{
		UIContext source;
		UIContext destination;
		source.SetViewport(300, 200);
		destination.SetViewport(300, 200);
		auto sourceRoot = std::make_unique<CanvasPanel>();
		auto* original = sourceRoot.get();
		auto& input = original->Add<TextBox>("preserved document contents");
		const auto inputId = input.GetId();
		source.AddToViewport(std::move(sourceRoot));
		Click(source, 10, 10);
		Check(source.GetFocusedWidget() == inputId, "Transfer starts with a focused input field");
		auto contents = original->TakeChild(inputId);
		Check(contents.get() == &input && !source.Find(inputId) && !source.GetFocusedWidget() &&
			!input.GetContext() && !input.GetParent(), "Taking a child cancels input and releases the original context registration");
		auto targetRoot = std::make_unique<CanvasPanel>();
		targetRoot->AddChild(std::move(contents));
		destination.AddToViewport(std::move(targetRoot));
		Check(destination.Find(inputId) == &input && input.GetText() == "preserved document contents",
			"Ownership transfer preserves the same widget identity and editing contents");
		Check(!original->TakeChild(inputId), "Taking an absent child returns null");
		auto& button = original->Add<Button>();
		const auto buttonId = button.GetId();
		bool rejected = false;
		button.OnClicked = [&]() {
			try { original->TakeChild(buttonId); }
			catch (const std::logic_error&) { rejected = true; }
		};
		Click(source, 10, 10);
		Check(rejected && source.Find(buttonId) == &button && !source.IsDispatching(),
			"Synchronous ownership transfer during input dispatch is rejected without damaging the tree");
	}

	void ClippingAndTextTests()
	{
		UIContext context;
		context.SetViewport(200, 100);
		auto canvas = std::make_unique<CanvasPanel>();
		auto& parent = canvas->Add<CanvasPanel>();
		Slot slot;
		slot.Offsets = {0, 0, 40, 40};
		canvas->SetSlot(parent.GetId(), slot);
		auto& child = parent.Add<Button>();
		slot.Offsets = {30, 0, 80, 40};
		parent.SetSlot(child.GetId(), slot);
		int clicks = 0;
		child.OnClicked = [&]() { ++clicks; };
		context.AddToViewport(std::move(canvas));
		Click(context, 60, 20);
		Check(clicks == 0, "Clipped child cannot receive pointer events");
		Click(context, 35, 20);
		Check(clicks == 1, "Visible child fragment is clickable");
		const auto& list = context.BuildDrawList();
		Check(!list.Quads.empty() && list.Quads.front().Clip.Width <= 10, "Painting uses inherited intersection clip");

		Assets::Font font;
		font.LineHeight = 1;
		for (Uint32 codepoint : {Uint32('A'), Uint32('V'), Uint32('?'), Uint32(' ')})
		{
			Assets::FontCharacter glyph;
			glyph.CodePoint = codepoint;
			glyph.Advance = .6f;
			font.Characters[codepoint] = glyph;
		}
		font.Kerning[(Uint64('A') << 32) | 'V'] = -.1f;
		auto size = DrawList::MeasureText(&font, "AV", 10);
		Check(Near(size.x, 11) && Near(size.y, 10), "Fractional advances and kerning");
		size = DrawList::MeasureText(&font, "AVAV", 10, 12);
		Check(Near(size.x, 11) && Near(size.y, 20), "Wrapping matches measured line widths");
		size = DrawList::MeasureText(&font, "A\nV", 10);
		Check(Near(size.x, 6) && Near(size.y, 20), "Explicit newlines");
		size = DrawList::MeasureText(&font, "\xF0\x9F\x98\x80", 10);
		Check(Near(size.x, 6), "Unsupported Unicode scalar uses one replacement glyph");
		size = DrawList::MeasureText(&font, "\xED\xA0\x80", 10);
		Check(Near(size.x, 6), "UTF8 surrogate is rejected");
		font.Create({});
		Check(font.GetState() == Assets::IAsset::State::Unknown, "Invalid font upload remains unusable");
	}

	class GPUClient final : public Core::Client
	{
	public:
		int Frames = 0;
		bool VerifiedPixels = false;
		bool VerifiedText = false;
		bool SavedImage = false;
		int ButtonClicks = 0;
		bool VerifiedMinimize = false;
		void Load() override
		{
			Sample.Load();
			auto& ui = UIModule::Get().GetContext();
			Check(ui.GetDefaultFont() && ui.GetDefaultFont()->mTextureView, "Default font GPU upload");
			Check(ui.GetDefaultFont()->Characters.size() > 70, "Default font glyph repertoire");
			Byte distances[16];
			for (unsigned index = 0; index < 16; ++index)
				distances[index] = static_cast<Byte>(index * 17);
			EdgeProbe.Create({16, 1, distances});
			Check(EdgeProbe.mTextureView != nullptr, "SDF edge probe upload");
			Check(EdgeRenderer.Initialize(), "SDF edge probe renderer initialization");
			auto probes = std::make_unique<CanvasPanel>();
			auto& red = probes->Add<Border>();
			red.SetBrush({nullptr, {1, 0, 0, 1}});
			Slot slot;
			slot.Offsets = {5, 5, 40, 40};
			probes->SetSlot(red.GetId(), slot);
			auto& blue = probes->Add<Border>();
			blue.SetBrush({nullptr, {0, 0, 1, .5f}});
			slot.Offsets = {25, 5, 40, 40};
			probes->SetSlot(blue.GetId(), slot);
			auto& button = probes->Add<Button>();
			button.Add<TextBlock>("Probe");
			button.OnClicked = [this]() { ++ButtonClicks; };
			slot.Offsets = {5, 60, 100, 34};
			probes->SetSlot(button.GetId(), slot);
			auto& clipPanel = probes->Add<CanvasPanel>();
			slot.Offsets = {5, 150, 30, 30};
			probes->SetSlot(clipPanel.GetId(), slot);
			auto& clipped = clipPanel.Add<Border>();
			clipped.SetBrush({nullptr, {0, 1, 0, 1}});
			slot.Offsets = {20, 0, 40, 30};
			clipPanel.SetSlot(clipped.GetId(), slot);
			ui.AddToViewport(std::move(probes), 10);
		}
		void Update(float) override
		{
			++Frames;
			if (Frames == 2 || Frames == 10)
			{
				auto window = static_cast<HWND>(Core::Engine::Get().GetMainWindow()->GetNativeHandle());
				PostMessageW(window, WM_MOUSEMOVE, 0, MAKELPARAM(20, 75));
				PostMessageW(window, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(20, 75));
				PostMessageW(window, WM_LBUTTONUP, 0, MAKELPARAM(20, 75));
			}
			if (Frames == 8)
				Core::Engine::Get().GetMainWindow()->SetSize(1100, 800);
			if (Frames == 12)
			{
				Assets::AssetManager::Get().FlushContainers();
				// FlushContainers is currently a stub; release the actual library map too.
				Assets::AssetLibrary::Get().mImportedFonts.Release();
				Check(UIModule::Get().GetContext().GetDefaultFont()->mTextureView != nullptr,
					  "Private default font survives library flush");
			}
			if (Frames == 18)
				UIModule::Get().GetContext().SetViewport(1100, 800, 1.25f);
			if (Frames == 20)
				ShowWindow(static_cast<HWND>(Core::Engine::Get().GetMainWindow()->GetNativeHandle()), SW_MINIMIZE);
			if (Frames == 22)
			{
				VerifiedMinimize = UIModule::Get().GetContext().GetPixelSize().x == 0 &&
					UIModule::Get().GetContext().BuildDrawList().Quads.empty();
				ShowWindow(static_cast<HWND>(Core::Engine::Get().GetMainWindow()->GetNativeHandle()), SW_RESTORE);
			}
		}
		void Render(float dt) override
		{
			Sample.Render(dt);
			if (Frames == 5 || Frames == 15)
			{
				// Explicit draw/readback is test-only; production rendering happens in EndFrame.
				UIModule::Get().Render();
				Readback();
			}
			if (Frames == 30)
				Core::Engine::Get().EndClient();
		}
		void OnWindowResize(int width, int height) override
		{
			Sample.OnWindowResize(width, height);
		}

	private:
		UISample Sample;
		Assets::Font EdgeProbe;
		UIRenderer EdgeRenderer;
		void Readback()
		{
			using namespace Diligent;
			auto& graphics = Graphics::GraphicsModule::Get();
			auto* context = graphics.GetContext();
			auto* source = graphics.GetSwapChain()->GetCurrentBackBufferRTV()->GetTexture();
			auto desc = source->GetDesc();
			// A fractional straight contour isolates antialiasing from font shapes.
			// A two-pixel smoothing band leaves a visible halo at x=17; one pixel does not.
			DrawList edge;
			const UI::Rect clip{0, 0, float(desc.Width), float(desc.Height)};
			edge.Add({5, 200, 40, 40}, clip, {0, 0, 0, 1});
			edge.Add({10.25f, 210, 16, 16}, clip, {1, 1, 1, 1}, EdgeProbe.mTextureView,
				{0, 0, 1, 1}, true);
			EdgeRenderer.Render(edge, {float(desc.Width), float(desc.Height)}, 1);
			Check(desc.Format == TEX_FORMAT_RGBA8_UNORM || desc.Format == TEX_FORMAT_RGBA8_UNORM_SRGB,
				  "Expected RGBA8 readback target");
			desc.Name = "UI validation readback";
			desc.Usage = USAGE_STAGING;
			desc.BindFlags = BIND_NONE;
			desc.CPUAccessFlags = CPU_ACCESS_READ;
			RefCntAutoPtr<ITexture> staging;
			graphics.GetDevice()->CreateTexture(desc, nullptr, &staging);
			Check(staging != nullptr, "Readback staging allocation");
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
			Check(mapped.pData != nullptr, "Readback mapping");
			auto pixel = [&](unsigned x, unsigned y)
			{ return static_cast<const unsigned char*>(mapped.pData) + y * mapped.Stride + x * 4; };
			const auto* edgeOutside = pixel(17, 215);
			const auto* edgeInside = pixel(18, 215);
			const bool srgb = desc.Format == TEX_FORMAT_RGBA8_UNORM_SRGB;
			Check(edgeOutside[0] < 3 && edgeInside[0] > (srgb ? 230 : 208) &&
				edgeInside[0] < (srgb ? 245 : 222), "SDF edges use a single-pixel antialiasing band");
			const auto* red = pixel(10, 10);
			const auto* blend = pixel(30, 10);
			VerifiedPixels = red[0] > 245 && red[1] < 5 && red[2] < 5 && blend[0] > 120 && blend[0] < 136 &&
							 blend[2] > 120 && blend[2] < 136;
			const auto* clipped = pixel(30, 160);
			const auto* outsideClip = pixel(45, 160);
			VerifiedPixels = VerifiedPixels && clipped[1] > 245 && outsideClip[1] < 20;
			// Look for the cyan title's high coverage pixels in the centered panel.
			unsigned glyphPixels = 0;
			const unsigned startX = (desc.Width - 520) / 2 + 24;
			const unsigned startY = (desc.Height - 470) / 2 + 24;
			for (unsigned y = startY; y < startY + 38; ++y)
				for (unsigned x = startX; x < startX + 180; ++x)
				{
					const auto* value = pixel(x, y);
					if (value[1] > 150 && value[2] > 200)
						++glyphPixels;
				}
			VerifiedText = glyphPixels > 100;
			std::filesystem::create_directories("../Nuclear.Engine/Build");
			std::ofstream image("../Nuclear.Engine/Build/UIShowcase.ppm", std::ios::binary);
			image << "P6\n" << desc.Width << ' ' << desc.Height << "\n255\n";
			for (unsigned y = 0; y < desc.Height; ++y)
				for (unsigned x = 0; x < desc.Width; ++x)
					image.write(reinterpret_cast<const char*>(pixel(x, y)), 3);
			SavedImage = image.good();
			context->UnmapTextureSubresource(staging, 0, 0);
			Check(VerifiedPixels, "GPU rectangles, painter order, and alpha blending");
			Check(VerifiedText, "GPU SDF text coverage");
		}
	};

	void EditorControlTests()
	{
		UIContext context;
		context.SetViewport(240, 80);
		auto input = std::make_unique<TextBox>("ab");
		auto* text = input.get();
		context.AddToViewport(std::move(input));
		Click(context, 10, 10);
		auto type = [&context](Uint32 codepoint)
		{
			Platform::Event event;
			event.mType = Platform::EventType::TextInput;
			event.mCodepoint = codepoint;
			context.ProcessEvent(event);
		};
		type(0x1f600);
		Check(text->GetText().size() == 6, "Text input encodes supplementary Unicode scalars");
		Key(context, Platform::Input::KEYCODE_LEFT, true);
		Key(context, Platform::Input::KEYCODE_DELETE, true);
		Check(text->GetText() == "ab", "Delete removes an entire UTF-8 codepoint");
		type(0xd800);
		Check(text->GetText() == "ab", "Text editor rejects surrogate codepoints");
		Key(context, Platform::Input::KEYCODE_LCTRL, true);
		Key(context, Platform::Input::KEYCODE_A, true);
		Key(context, Platform::Input::KEYCODE_LCTRL, false);
		type('7');
		Check(text->GetText() == "7", "Select all replaces the complete string");
		const auto inputId = text->GetId();
		text->OnCommitted = [&context, inputId](const std::string&) { context.RemoveFromViewport(inputId); };
		Key(context, Platform::Input::KEYCODE_RETURN, true);
		Check(!context.Find(inputId), "Text commit can remove its own widget safely");
		auto host = std::make_unique<ContentWidget>();
		auto* content = host.get();
		auto& oldInput = host->Add<TextBox>("focused child");
		const auto oldId = oldInput.GetId();
		context.AddToViewport(std::move(host));
		Click(context, 10, 10);
		Check(context.GetFocusedWidget() == oldId, "Content replacement starts with focused child");
		content->SetContent(std::make_unique<TextBox>("replacement"));
		Check(!context.Find(oldId) && context.GetFocusedWidget() == 0,
			"Replacing focused content cancels input before detaching the child");
		context.Clear();

		auto scroll = std::make_unique<ScrollBox>();
		auto* viewport = scroll.get();
		auto& column = scroll->Add<VerticalBox>();
		column.SetSpacing(4);
		Button* last = nullptr;
		for (int index = 0; index < 12; ++index)
		{
			last = &column.Add<Button>();
			last->SetSizeLimits({100, 30}, {100, 30});
		}
		context.AddToViewport(std::move(scroll));
		context.Layout();
		context.ProcessEvent(Mouse(Platform::EventType::MouseMove, 10, 10));
		Platform::Event wheel;
		wheel.mType = Platform::EventType::MouseWheel;
		wheel.mWheelY = -2;
		context.ProcessEvent(wheel);
		context.Layout();
		Check(Near(viewport->GetOffset(), 96), "Wheel bubbles from button to scroll viewport");
		for (int index = 0; index < 13; ++index) Key(context, Platform::Input::KEYCODE_TAB, true);
		context.Layout();
		Check(context.GetFocusedWidget() == last->GetId(), "Tab reaches clipped scroll descendants");
		Check(last->GetBounds().y >= 0 && last->GetBounds().y + last->GetBounds().Height <= 80.01f,
			"Focused scroll descendant is brought into view");
		viewport->SetOffset(100000);
		float end = viewport->GetOffset();
		context.SetViewport(240, 1000);
		context.Layout();
		Check(end > 0 && Near(viewport->GetOffset(), 0), "Resize clamps offset when content fits");
	}

	void GPUTests(bool enableImGui)
	{
		Core::EngineStartupDesc desc;
		desc.AutoInitAudioModule = false;
		desc.AutoInitPhysXModule = false;
		desc.AutoInitScriptingModule = false;
		desc.AutoInitThreadingModule = false;
		desc.EnableImGui = enableImGui;
		desc.mAssetsLibraryPath = "../Assets/";
		desc.mEngineWindowDesc.WindowWidth = 1280;
		desc.mEngineWindowDesc.WindowHeight = 720;
		desc.mEngineWindowDesc.Title = "Nuclear retained UI validation";
		auto& engine = Core::Engine::Get();
		Check(engine.Start(desc), "UI engine startup");
		for (int run = 0; run < 2; ++run)
		{
			GPUClient client;
			engine.LoadClient(&client);
			engine.EndClient();
			Check(client.Frames == 30 && client.VerifiedPixels && client.VerifiedText && client.SavedImage && client.VerifiedMinimize,
				  "GPU client completed 30 frames");
			std::cout << "Platform button activations=" << client.ButtonClicks << '\n';
			Check(client.ButtonClicks == 2, "Platform mouse events activate retained button through engine routing");
			Check(UIModule::Get().GetContext().BuildDrawList().Quads.empty(), "Client end clears viewport");
			UIModule::Get().GetContext().SetViewport(1280, 720, 1);
			engine.GetMainWindow()->SetSize(1280, 720);
		}
		engine.Shutdown();
		Check(!engine.GetMainWindow()->GetNativeHandle() && !UIModule::Get().IsInitialized(),
			  "UI shutdown releases window and module");
		std::cout << "D3D11 UI passed 60 frames, GPU readback, resize, scaling, asset flush and client reload. ImGui="
				  << enableImGui << '\n';
	}
} // namespace

int main(int argc, char** argv)
{
	try
	{
		LayoutTests();
		PreviewEventTests();
		InputTests();
		OwnershipTransferTests();
		ClippingAndTextTests();
		EditorControlTests();
		std::cout << "UI layout, input, clipping, lifetime, and text checks passed.\n";
		if (argc > 1 && std::string(argv[1]) == "--gpu")
			GPUTests(argc > 2 && std::string(argv[2]) == "--imgui");
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << "UI test failed: " << error.what() << '\n';
		return 1;
	}
}
