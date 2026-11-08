#include <EditorSettingsDocument.h>
#include <EditorSettings.h>
#include <EditorWidgets.h>
#include <UI/Border.h>
#include <UI/BoxPanel.h>
#include <UI/Button.h>
#include <UI/CheckBox.h>
#include <UI/ContentWidget.h>
#include <UI/HorizontalBox.h>
#include <UI/ScrollBox.h>
#include <UI/SizeBox.h>
#include <UI/TextBlock.h>
#include <UI/VerticalBox.h>
#include <vector>

namespace Nuclear::Editor
{
	namespace
	{
		struct NavItem
		{
			std::string Title;
			UI::Button* pButton = nullptr;
			UI::TextBlock* pLabel = nullptr;
		};

		std::unique_ptr<UI::Widget> BuildThemePage(std::function<void()> refreshPage)
		{
			auto scroll = std::make_unique<UI::ScrollBox>();
			auto& page = scroll->Add<UI::VerticalBox>();
			page.SetPadding({24, 20, 24, 20});
			page.SetSpacing(16);

			auto& header = page.Add<UI::VerticalBox>();
			header.SetSpacing(4);
			auto& title = Label(header, "THEME & APPEARANCE", 18);
			title.SetColor(EditorSettings::Get().GetAccentColor());
			auto& subtitle = Label(header, "Customize the editor visual styling, color scheme, and accent palette.", 12);
			subtitle.SetColor({.55f, .62f, .70f, 1.0f});

			auto& accentSection = Section(page, "ACCENT COLOR");
			auto& desc = Label(accentSection,
				"The accent color is applied to active tabs, highlights, focus outlines, and section headings across the editor.", 12);
			desc.SetWrap(true);
			desc.SetColor({.65f, .72f, .80f, 1.0f});

			// Preview swatch row
			auto& previewRow = accentSection.Add<UI::HorizontalBox>();
			previewRow.SetSpacing(10);
			auto& previewLabelBox = previewRow.Add<UI::SizeBox>();
			previewLabelBox.SetSizeLimits({90, 0}, {90, 1000000});
			Label(previewLabelBox, "Live Preview", 12);
			auto previewSlot = previewRow.GetSlot(previewLabelBox.GetId());
			previewSlot.Vertical = UI::Alignment::Center;
			previewRow.SetSlot(previewLabelBox.GetId(), previewSlot);

			auto& swatchBorder = previewRow.Add<UI::Border>();
			swatchBorder.SetSizeLimits({120, 26}, {120, 26});
			swatchBorder.SetBrush({nullptr, EditorSettings::Get().GetAccentColor()});

			// Vector RGB Property
			const auto currentAccent = EditorSettings::Get().GetAccentColor();
			Vector(accentSection, "Accent RGB", {currentAccent.r, currentAccent.g, currentAccent.b},
				[&swatchBorder, &title, refreshPage](int axis, float value) {
					auto col = EditorSettings::Get().GetAccentColor();
					float rgb[3] = {col.r, col.g, col.b};
					rgb[axis] = value;
					EditorSettings::Get().SetAccentColor({rgb[0], rgb[1], rgb[2], 1.0f});
					swatchBorder.SetBrush({nullptr, EditorSettings::Get().GetAccentColor()});
					title.SetColor(EditorSettings::Get().GetAccentColor());
				}, 0.0f, 1.0f, 90.0f);

			// Presets row
			Label(accentSection, "Color Presets", 12);
			auto& presetsRow = accentSection.Add<UI::HorizontalBox>();
			presetsRow.SetSpacing(6);

			struct Preset
			{
				const char* Name;
				UI::Color Color;
			};
			const Preset presets[] = {
				{"Cyber Teal", {.18f, .70f, .62f, 1.0f}},
				{"Electric Cyan", {.15f, .65f, .95f, 1.0f}},
				{"Emerald", {.18f, .78f, .42f, 1.0f}},
				{"Amber Gold", {.95f, .60f, .15f, 1.0f}},
				{"Solar Crimson", {.90f, .22f, .28f, 1.0f}},
				{"Violet", {.65f, .35f, .90f, 1.0f}}
			};

			for (const auto& preset : presets)
			{
				Action(presetsRow, preset.Name, [preset, refreshPage]() {
					EditorSettings::Get().SetAccentColor(preset.Color);
					if (refreshPage)
						refreshPage();
				});
			}

			Action(accentSection, "Reset to Default Accent", [refreshPage]() {
				EditorSettings::Get().ResetAccentColor();
				if (refreshPage)
					refreshPage();
			});

			return scroll;
		}

		std::unique_ptr<UI::Widget> BuildGeneralPage()
		{
			auto scroll = std::make_unique<UI::ScrollBox>();
			auto& page = scroll->Add<UI::VerticalBox>();
			page.SetPadding({24, 20, 24, 20});
			page.SetSpacing(16);

			auto& header = page.Add<UI::VerticalBox>();
			header.SetSpacing(4);
			auto& title = Label(header, "GENERAL PREFERENCES", 18);
			title.SetColor(EditorSettings::Get().GetAccentColor());
			auto& subtitle = Label(header, "Global editor configuration and interface parameters.", 12);
			subtitle.SetColor({.55f, .62f, .70f, 1.0f});

			auto& displaySection = Section(page, "DISPLAY & SCALING");
			auto& vsync = displaySection.Add<UI::CheckBox>();
			vsync.SetChecked(EditorSettings::Get().GetVSync());
			Label(vsync, "VSync");
			vsync.OnChanged = [](bool enabled) {
				EditorSettings::Get().SetVSync(enabled);
			};
			auto& vsyncDesc = Label(displaySection,
				"Synchronizes frame presentation with the display refresh rate to eliminate screen tearing.", 12);
			vsyncDesc.SetColor({.65f, .72f, .80f, 1.0f});
			vsyncDesc.SetWrap(true);

			Label(displaySection, "PerMonitorV2 DPI awareness is enabled for main and detached windows.", 13);
			Label(displaySection, "SDF font rendering antialiasing: 1-pixel band.", 12);

			auto& sessionSection = Section(page, "SESSION & WORKSPACE");
			Label(sessionSection, "Session changes are held in-memory; project file saving is disabled.", 13);
			Label(sessionSection, "Retained UI renderer: Nuclear::UI using Diligent Engine graphics.", 12);

			return scroll;
		}

		std::unique_ptr<UI::Widget> BuildViewportPage()
		{
			auto scroll = std::make_unique<UI::ScrollBox>();
			auto& page = scroll->Add<UI::VerticalBox>();
			page.SetPadding({24, 20, 24, 20});
			page.SetSpacing(16);

			auto& header = page.Add<UI::VerticalBox>();
			header.SetSpacing(4);
			auto& title = Label(header, "VIEWPORT SETTINGS", 18);
			title.SetColor(EditorSettings::Get().GetAccentColor());
			auto& subtitle = Label(header, "3D scene viewport navigation and camera controls.", 12);
			subtitle.SetColor({.55f, .62f, .70f, 1.0f});

			auto& navSection = Section(page, "CAMERA NAVIGATION");
			Label(navSection, "Flight Controls: WASD to navigate camera forward, back, left, and right.", 13);
			Label(navSection, "Mouse Look: Hold right mouse button inside the viewport to rotate camera view.", 13);
			Label(navSection, "Frame Selected: Places the camera 6 units behind the selected entity along current view direction.", 13);

			auto& renderSection = Section(page, "RENDER PASSES");
			Label(renderSection, "Forward Blinn-Phong shading pass with directional shadows.", 13);
			Label(renderSection, "Selection outline: Depth-tested silhouette outline on selected meshes.", 12);

			return scroll;
		}
	} // namespace

	std::string EditorSettingsDocument::GetTitle()
	{
		return "Editor Settings";
	}

	std::unique_ptr<UI::Widget> EditorSettingsDocument::Build()
	{
		auto root = std::make_unique<UI::HorizontalBox>();

		// Left vertical navigation menu
		auto& sidebarBorder = root->Add<UI::Border>();
		sidebarBorder.SetBrush({nullptr, {.052f, .065f, .082f, 1.0f}});
		sidebarBorder.SetPadding({10, 14, 10, 14});
		sidebarBorder.SetSizeLimits({170, 0}, {170, 1000000});

		auto& sidebar = sidebarBorder.Add<UI::VerticalBox>();
		sidebar.SetSpacing(4);

		auto& menuHeader = Label(sidebar, "SETTINGS", 11);
		menuHeader.SetColor({.42f, .50f, .58f, 1.0f});

		// Divider under sidebar title
		auto& divider = sidebar.Add<UI::Border>();
		divider.SetSizeLimits({0, 1}, {1000000, 1});
		divider.SetBrush({nullptr, {.10f, .13f, .17f, 1.0f}});
		divider.SetPadding({0, 4, 0, 8});

		// Vertical divider between sidebar and main view
		auto& vDivider = root->Add<UI::Border>();
		vDivider.SetSizeLimits({1, 0}, {1, 1000000});
		vDivider.SetBrush({nullptr, {.10f, .13f, .17f, 1.0f}});

		// Right main view
		auto& mainBorder = root->Add<UI::Border>();
		mainBorder.SetBrush({nullptr, {.065f, .08f, .10f, 1.0f}});
		Fill(*root, mainBorder, 1.0f);

		auto& pageHost = mainBorder.Add<UI::ContentWidget>();
		Fill(mainBorder, pageHost, 1.0f);

		const std::vector<std::string> pageTitles = {"Theme", "General", "Viewport"};
		auto navItems = std::make_shared<std::vector<NavItem>>();
		auto activePage = std::make_shared<size_t>(0);

		auto updateNavStyles = [navItems, activePage]() {
			for (size_t i = 0; i < navItems->size(); ++i)
			{
				const bool active = (i == *activePage);
				UI::ControlStyle style;
				if (active)
				{
					style.Normal = {.10f, .15f, .20f, 1.0f};
					style.Hovered = {.13f, .19f, .25f, 1.0f};
					style.Pressed = {.08f, .12f, .16f, 1.0f};
				}
				else
				{
					style.Normal = {.055f, .07f, .09f, 1.0f};
					style.Hovered = {.09f, .12f, .16f, 1.0f};
					style.Pressed = {.045f, .06f, .08f, 1.0f};
				}
				style.Focus = EditorSettings::Get().GetAccentColor();
				(*navItems)[i].pButton->SetStyle(style);
				(*navItems)[i].pLabel->SetColor(active ? EditorSettings::Get().GetAccentColor() : UI::Color{.72f, .78f, .85f, 1.0f});
			}
		};

		auto loadPage = [&pageHost, activePage](auto self) -> void {
			std::function<void()> refresh = [&pageHost, activePage, self]() {
				self(self);
			};
			if (*activePage == 0)
				pageHost.SetContent(BuildThemePage(refresh));
			else if (*activePage == 1)
				pageHost.SetContent(BuildGeneralPage());
			else
				pageHost.SetContent(BuildViewportPage());
		};

		for (size_t i = 0; i < pageTitles.size(); ++i)
		{
			auto& btn = sidebar.Add<UI::Button>();
			btn.SetPadding({10, 8, 10, 8});
			auto& lbl = Label(btn, pageTitles[i], 13);
			auto slot = btn.GetSlot(lbl.GetId());
			slot.Horizontal = UI::Alignment::Start;
			btn.SetSlot(lbl.GetId(), slot);

			navItems->push_back({pageTitles[i], &btn, &lbl});

			btn.OnClicked = [i, activePage, updateNavStyles, loadPage]() {
				*activePage = i;
				updateNavStyles();
				loadPage(loadPage);
			};
		}

		updateNavStyles();
		loadPage(loadPage);

		return root;
	}
} // namespace Nuclear::Editor
