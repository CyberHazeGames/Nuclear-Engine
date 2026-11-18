#include <VisualScriptDocument.h>
#include <VisualScriptCanvas.h>
#include <EditorToolbar.h>
#include <EditorWidgets.h>
#include <Nuclear.Platform/Platform.h>

namespace Nuclear::Editor
{
	VisualScriptDocument::VisualScriptDocument()
	{
		using Kind = VisualScriptGraph::Kind;
		auto& bar = Add<EditorToolbar>();
		bar.SetPadding({12, 8, 12, 8});
		auto& body = Add<UI::HorizontalBox>();
		Fill(*this, body);
		auto& rail = body.Add<UI::Border>();
		rail.SetBrush({nullptr, {.055f, .068f, .085f, 1}});
		rail.SetSizeLimits({196, 0}, {196, 1000000});
		auto& scroll = rail.Add<UI::ScrollBox>();
		auto& palette = scroll.Add<UI::VerticalBox>();
		palette.SetPadding({12, 16, 12, 16});
		palette.SetSpacing(8);
		auto& canvas = body.Add<VisualScriptCanvas>();
		Fill(body, canvas);

		Label(palette, "ADD NODE", 12);
		for (Kind kind : {Kind::Begin, Kind::Branch, Kind::Print, Kind::Boolean})
			Action(palette, VisualScriptGraph::Title(kind), [&canvas, kind]() { canvas.AddNode(kind); });

		auto& divider = palette.Add<UI::Border>();
		divider.SetBrush({nullptr, {.14f, .17f, .20f, 1}});
		divider.SetSizeLimits({0, 1}, {1000000, 1});
		Label(palette, "SELECTION", 12);
		auto& title = Label(palette, "", 16);
		auto& literal = palette.Add<UI::VerticalBox>();
		Label(literal, "Message", 13);
		auto& message = literal.Add<UI::TextBox>();
		message.OnChanged = [&canvas](const std::string& value) {
			if (auto* node = canvas.Selected(); node && node->NodeKind == Kind::Print) node->Message = value;
		};
		auto& boolean = palette.Add<UI::CheckBox>();
		Label(boolean, "Value", 14);
		boolean.OnChanged = [&canvas](bool value) {
			if (auto* node = canvas.Selected(); node && node->NodeKind == Kind::Boolean) node->Value = value;
		};
		auto& remove = Action(palette, "Delete node", [&canvas]() { canvas.DeleteSelected(); });
		canvas.OnSelection = [&]() {
			const auto* node = canvas.Selected();
			title.SetText(node ? VisualScriptGraph::Title(node->NodeKind) : "No node selected");
			remove.SetEnabled(node != nullptr);
			const bool print = node && node->NodeKind == Kind::Print;
			const bool flag = node && node->NodeKind == Kind::Boolean;
			literal.SetVisibility(print ? UI::Visibility::Visible : UI::Visibility::Collapsed);
			boolean.SetVisibility(flag ? UI::Visibility::Visible : UI::Visibility::Collapsed);
			if (print) message.SetText(node->Message);
			if (flag) boolean.SetChecked(node->Value);
		};
		canvas.OnSelection();

		auto& hint = Label(*this, "Drag pins to connect. Right-click a pin to disconnect. Drag the grid to pan. Scroll to zoom.", 13);
		hint.SetPadding({12, 5, 12, 5});
		hint.SetWrap(true);
		Action(bar, "Frame graph", [&canvas]() { canvas.FrameGraph(); });
		Action(bar, "Copy JSON", [&canvas]() {
			canvas.OnNotice(Platform::SetClipboardText(canvas.ToJson()) ? "Graph copied." : "Clipboard unavailable.");
		});
		Action(bar, "Paste JSON", [&canvas]() {
			canvas.OnNotice(canvas.LoadJson(Platform::GetClipboardText()) ? "Graph imported." : "Clipboard needs a valid version 1 graph.");
		});
		auto& notice = Label(bar, "", 13);
		notice.SetVisibility(UI::Visibility::Collapsed);
		canvas.OnNotice = [&notice](const std::string& text) {
			notice.SetText(text);
			notice.SetVisibility(text.empty() ? UI::Visibility::Collapsed : UI::Visibility::Visible);
		};
	}
}
