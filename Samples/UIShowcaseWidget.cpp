#include "UIShowcaseWidget.h"
#include <Core/Engine.h>
#include <Fallbacks/FallbacksModule.h>
#include <UI/UI.h>

void UIShowcaseWidget::Construct()
{
	using namespace Nuclear;
	using namespace Nuclear::UI;
	auto overlay = std::make_unique<Overlay>();
	auto& canvas = overlay->Add<CanvasPanel>();

	auto& sizing = canvas.Add<SizeBox>();
	sizing.SetSizeLimits({360, 420}, {520, 520});
	Slot center;
	center.AnchorMin = center.AnchorMax = {.5f, .5f};
	center.Pivot = {.5f, .5f};
	center.Offsets = {0, 0, 520, 470};
	canvas.SetSlot(sizing.GetId(), center);
	auto& border = sizing.Add<Border>();
	border.SetBrush({nullptr, {.035f, .055f, .09f, .97f}});
	border.SetPadding({24, 24, 24, 24});
	auto& column = border.Add<VerticalBox>();
	column.SetSpacing(12);
	auto& title = column.Add<TextBlock>("Nuclear UI");
	title.SetFontSize(32);
	title.SetColor({.4f, .8f, 1, 1});
	auto& description =
		column.Add<TextBlock>("Retained widgets rendered directly by Diligent. Use Tab / Shift+Tab to focus, Enter "
							  "or Space to activate, and arrow keys to adjust the slider.");
	description.SetWrap(true);
	description.SetFontSize(16);

	auto& row = column.Add<HorizontalBox>();
	row.SetSpacing(12);
	auto& image = row.Add<Image>();
	image.SetImageSize({48, 48});
	image.SetBrush({Fallbacks::FallbacksModule::Get().GetDefaultWhiteImage(), {.15f, .7f, .55f, 1}});
	auto& status = row.Add<TextBlock>("Health / progress");
	Slot statusSlot;
	statusSlot.Fill = 1;
	statusSlot.Vertical = Alignment::Center;
	row.SetSlot(status.GetId(), statusSlot);
	auto& progress = column.Add<ProgressBar>();
	progress.SetValue(.65f);
	auto& slider = column.Add<Slider>();
	slider.SetValue(.65f);
	slider.OnChanged = [&progress](float value) { progress.SetValue(value); };

	auto& check = column.Add<CheckBox>();
	check.SetChecked(true);
	check.Add<TextBlock>("Enable slider");
	check.OnChanged = [&slider](bool checked) { slider.SetEnabled(checked); };
	auto& buttons = column.Add<HorizontalBox>();
	buttons.SetSpacing(12);
	auto& count = buttons.Add<TextBlock>("Clicks: 0");
	Slot labelSlot;
	labelSlot.Fill = 1;
	labelSlot.Vertical = Alignment::Center;
	buttons.SetSlot(count.GetId(), labelSlot);
	auto& button = buttons.Add<Button>();
	button.Add<TextBlock>("Click me");
	button.OnClicked = [this, &count]() { count.SetText("Clicks: " + std::to_string(++mClicks)); };
	auto& exit = column.Add<Button>();
	exit.Add<TextBlock>("Close UI sample");
	exit.OnClicked = []() { Core::Engine::Get().EndClient(); };

	auto& hud = canvas.Add<TextBlock>("Viewport-anchored HUD");
	hud.SetFontSize(16);
	Slot hudSlot;
	hudSlot.AnchorMin = hudSlot.AnchorMax = {1, 0};
	hudSlot.Offsets = {-20, 20, 220, 24};
	hudSlot.Pivot = {1, 0};
	canvas.SetSlot(hud.GetId(), hudSlot);
	SetContent(std::move(overlay));
}
