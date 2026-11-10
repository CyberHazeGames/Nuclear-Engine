#include "ColorPickerTests.h"
#include <EditorColorPicker.h>
#include <EditorColorWheel.h>
#include <EditorColorProperty.h>
#include <EditorColorPickerWindow.h>
#include <EditorSettingsDocument.h>
#include <EditorSettings.h>
#include <EditorHistory.h>
#include <EntityEditor.h>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace Nuclear::Editor::Tests
{
	namespace
	{
		using namespace UI;
		void Check(bool condition, const char* message)
		{
			if (!condition) throw std::runtime_error(message);
		}
		bool Near(float first, float second) { return std::abs(first - second) < .0001f; }

		template<class T, class Predicate> T* Find(UIContext& context, Predicate predicate)
		{
			Widget marker;
			for (WidgetId id = 1; id < marker.GetId(); ++id)
				if (auto* widget = dynamic_cast<T*>(context.Find(id)); widget && predicate(*widget)) return widget;
			return nullptr;
		}
		TextBox& Field(UIContext& context, const std::string& title)
		{
			auto* label = Find<TextBlock>(context, [&](const auto& label) { return label.GetText() == title; });
			Check(label != nullptr, "Color property label exists");
			auto* input = Find<TextBox>(context, [&](const auto& input) {
				return input.GetParent() == label->GetParent() && input.GetId() > label->GetId();
			});
			Check(input != nullptr, "Color property has an input");
			return *input;
		}
		void Edit(UIContext& context, const std::string& title, const std::string& value)
		{
			auto callback = Field(context, title).OnCommitted;
			callback(value);
		}
		void Pointer(UIContext& context, Platform::EventType type, Vec2 position, bool down = false)
		{
			Platform::Event event;
			event.mType = type;
			event.mMouseX = int(std::lround(position.x * context.GetScale()));
			event.mMouseY = int(std::lround(position.y * context.GetScale()));
			event.mButton = MOUSE_BUTTON_LEFT;
			event.mPressed = down;
			context.ProcessEvent(event);
		}
		UIContext& OpenChooser(UIContext& context, EditorColorPickerWindow& windows)
		{
			auto* property = Find<EditorColorProperty>(context, [](const auto&) { return true; });
			Check(property != nullptr, "Compact color property exists");
			Check(!Find<EditorColorPicker>(context, [](const auto&) { return true; }), "Full picker is absent from the property panel");
			property->OnClicked();
			Check(!windows.GetWindow(), "Native chooser creation is deferred outside input dispatch");
			windows.Update();
			Check(windows.GetWindow() && windows.GetWindow()->GetWindow().GetNativeHandle(), "Property activation creates an external native window");
			windows.GetWindow()->GetWindow().Display(false);
			return windows.GetWindow()->GetContext();
		}
		void ChooserAction(EditorColorPickerWindow& windows, const std::string& title)
		{
			auto& context = windows.GetWindow()->GetContext();
			auto* label = Find<TextBlock>(context, [&](const auto& text) { return text.GetText() == title; });
			auto* button = label ? dynamic_cast<Button*>(label->GetParent()) : nullptr;
			Check(button != nullptr, "Native chooser action exists");
			button->OnClicked();
			windows.Update();
			Check(!windows.GetWindow(), "Chooser action releases the native host");
		}
	}

	void RunColorPickerTests()
	{
		UIContext context;
		context.SetViewport(232, 600);
		int commits = 0;
		auto picker = std::make_unique<EditorColorPicker>(Math::Vector4(.2f, .3f, .4f, .5f),
			[&](Math::Vector4) { ++commits; });
		auto* color = picker.get();
		context.AddToViewport(std::move(picker));
		Edit(context, "Red", "0.75");
		Check(color->GetColor() == Math::Vector4(.75f, .3f, .4f, .5f), "RGB edits preserve other channels exactly");
		Edit(context, "Alpha", "0.25");
		Edit(context, "Hue", "120");
		Edit(context, "Saturation", "1");
		Edit(context, "Value", "1");
		Check(color->GetColor() == Math::Vector4(0, 1, 0, .25f), "HSV edits update RGB while preserving alpha");
		Check(Field(context, "Red").GetText() == "0.000" &&
			Field(context, "HEX #RRGGBBAA (linear 8-bit)").GetText() == "#00FF0040", "Numeric and HEX controls synchronize");
		Edit(context, "HEX #RRGGBBAA (linear 8-bit)", "80402080");
		const auto hexColor = color->GetColor();
		Check(Near(hexColor.r, 128.f / 255) && Near(hexColor.g, 64.f / 255) && Near(hexColor.a, 128.f / 255),
			"RGBA HEX accepts optional hash and decodes all channels");
		const int beforeInvalid = commits;
		for (const char* invalid : {"nan", "inf", "-0.1", "1.5", "0.1junk", ""}) Edit(context, "Red", invalid);
		for (const char* invalid : {"#12345", "#GG000000", "#123456789", "#123456"})
			Edit(context, "HEX #RRGGBBAA (linear 8-bit)", invalid);
		Check(color->GetColor() == hexColor && commits == beforeInvalid, "Invalid color input never commits");
		color->SetColor(Math::Vector4(0, 0, 0, .5f));
		Edit(context, "Hue", "240");
		Edit(context, "Saturation", "1");
		Check(commits == beforeInvalid, "Hue/saturation at black do not add no-op commits");
		Edit(context, "Value", "1");
		Check(color->GetColor() == Math::Vector4(0, 0, 1, .5f), "Black retains selected hue and saturation");
		Edit(context, "Hue", "360");
		Check(color->GetColor() == Math::Vector4(1, 0, 0, .5f), "Hue 360 wraps to red");
		context.Clear();

		auto hdrPicker = std::make_unique<EditorColorPicker>(Math::Vector3(4, 2, 0),
			[&](Math::Vector3) { ++commits; }, 100000.f);
		color = hdrPicker.get();
		context.AddToViewport(std::move(hdrPicker));
		Check(color->GetColor() == Math::Vector4(4, 2, 0, 1), "Vec3 picker preserves HDR input");
		Edit(context, "Hue", "120");
		Check(color->GetColor() == Math::Vector4(0, 4, 0, 1), "Hue editing preserves HDR brightness");
		Edit(context, "HEX #RRGGBB (linear 8-bit)", "aBcDeF");
		Check(Near(color->GetColor().b, 239.f / 255) && color->GetColor().a == 1, "Vec3 HEX is case insensitive and stays opaque");
		Check(!Find<TextBlock>(context, [](const auto& label) { return label.GetText() == "Alpha"; }), "Vec3 omits alpha fields");
		context.Clear();

		// Fixed logical geometry checks input routing at two physical DPI scales.
		for (float scale : {1.f, 1.5f})
		{
			context.SetViewport(232 * scale, 202 * scale, scale);
			auto wheel = std::make_unique<EditorColorWheel>(true);
			auto* surface = wheel.get();
			surface->SetColor({1, 0, 0, .5f});
			int previews = 0;
			commits = 0;
			surface->OnPreview = [&]() { ++previews; };
			surface->OnCommitted = [&]() { ++commits; };
			context.AddToViewport(std::move(wheel));
			Pointer(context, Platform::EventType::MouseButton, {88, 20}, true);
			Pointer(context, Platform::EventType::MouseMove, {88, -50});
			Check(context.GetCapturedWidget() == surface->GetId() && previews >= 2 && commits == 0,
				"Wheel captures out-of-bounds movement and previews without committing");
			Pointer(context, Platform::EventType::MouseButton, {88, -50});
			Check(!context.GetCapturedWidget() && commits == 1 && Near(surface->GetHSV().x, 90) && surface->GetHSV().y == 1,
				"Release commits once and clamps saturation at the disk edge");
			const auto beforeCancel = surface->GetColor();
			Pointer(context, Platform::EventType::MouseButton, {150, 88}, true);
			Platform::Event escape;
			escape.mType = Platform::EventType::Key;
			escape.mKey = Platform::Input::KEYCODE_ESCAPE;
			escape.mPressed = true;
			context.ProcessEvent(escape);
			Check(surface->GetColor() == beforeCancel && commits == 1 && !context.GetCapturedWidget(), "Escape restores a gesture without a commit");
			Pointer(context, Platform::EventType::MouseButton, {192, 88}, true);
			Pointer(context, Platform::EventType::MouseButton, {192, 88});
			Check(Near(surface->GetHSV().z, .5f), "Value strip edits brightness");
			Pointer(context, Platform::EventType::MouseButton, {220, 132}, true);
			Pointer(context, Platform::EventType::MouseButton, {220, 132});
			Check(Near(surface->GetColor().a, .25f), "Alpha strip edits opacity");
			const auto beforeDisable = surface->GetColor();
			Pointer(context, Platform::EventType::MouseButton, {150, 88}, true);
			surface->SetEnabled(false);
			Check(surface->GetColor() == beforeDisable && !context.GetCapturedWidget(), "Disabling a captured wheel cancels its preview");
			context.Clear();
		}

		// Exercise the actual light inspector callback and its shared history.
		auto& scene = Core::Scene::Get();
		auto& registry = scene.GetRegistry();
		const auto entity = scene.CreateEntity("Color picker fixture").GetEntityID();
		auto& light = registry.emplace<Components::LightComponent>(entity);
		const auto original = light.GetColor().Get4f();
		EditorHistory history;
		EditorColorPickerWindow windows;
		EntityEditor inspector;
		context.SetViewport(400, 1600);
		context.AddToViewport(inspector.Build(entity, [](bool) {}, history, windows));
		auto& lightChooser = OpenChooser(context, windows);
		Edit(lightChooser, "HEX #RRGGBB (linear 8-bit)", "#FF0000");
		Check(light.GetColor().Get4f() == original && !history.CanUndo(), "Chooser edits remain local before Apply");
		windows.Render();
		ChooserAction(windows, "Apply");
		Check(light.GetColor().r == 1 && light.GetColor().g == 0 && light.GetColor().b == 0, "Light inspector uses the shared picker");
		Check(history.Undo() && light.GetColor().Get4f() == original && !history.CanUndo(), "Light color commits as one undo action");
		Check(history.Redo() && light.GetColor().g == 0, "Light color supports redo");
		auto& cancelChooser = OpenChooser(context, windows);
		Edit(cancelChooser, "HEX #RRGGBB (linear 8-bit)", "#00FF00");
		ChooserAction(windows, "Cancel");
		Check(light.GetColor().g == 0, "Cancel leaves the light unchanged");
		OpenChooser(context, windows);
		Platform::Event close;
		close.pWindow = &windows.GetWindow()->GetWindow();
		close.mType = Platform::EventType::WindowClose;
		Check(windows.ProcessEvent(close), "Native window close is routed to the chooser");
		windows.Update();
		Check(!windows.GetWindow() && light.GetColor().g == 0, "Native close cancels without applying");
		OpenChooser(context, windows);
		context.Clear();
		windows.Update();
		Check(!windows.GetWindow(), "Replacing the property tree safely closes its external chooser");
		registry.destroy(entity);

		const auto accent = EditorSettings::Get().GetAccentColor();
		context.AddToViewport(EditorSettingsDocument::Build(windows));
		auto& themeChooser = OpenChooser(context, windows);
		Edit(themeChooser, "HEX #RRGGBB (linear 8-bit)", "#0080FF");
		ChooserAction(windows, "Apply");
		Check(EditorSettings::Get().GetAccentColor().b == 1 && Near(EditorSettings::Get().GetAccentColor().g, 128.f / 255),
			"Theme document applies the shared picker to the live accent");
		context.Clear();
		EditorSettings::Get().SetAccentColor(accent);
		std::cout << "Color picker RGB(A), HSV, HEX, HDR, gestures, DPI, cancellation, light history and theme passed.\n";
	}
}
