#include <EditorAssetPicker.h>
#include <EditorWidgets.h>
#include <AssetDocument.h>
#include <Assets/AssetLibrary.h>
#include <Nuclear.Platform/Input.h>
#include <algorithm>
#include <cctype>
#include <unordered_map>

namespace Nuclear::Editor
{
	namespace
	{
		std::unordered_map<UI::UIContext*, UI::WidgetId> ActivePickers;
		std::string Lower(std::string text)
		{
			std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return char(std::tolower(c)); });
			return text;
		}
	}

	void EditorAssetPicker::OnAttached()
	{
		auto* context = GetContext();
		const auto previous = ActivePickers[context];
		ActivePickers[context] = GetId();
		if (previous) context->RemoveFromViewport(previous);
	}

	void EditorAssetPicker::OnRemoved()
	{
		auto found = ActivePickers.find(GetContext());
		if (found != ActivePickers.end() && found->second == GetId()) ActivePickers.erase(found);
	}

	bool EditorAssetPicker::AcceptsPointer() const
	{
		auto* owner = GetContext() ? GetContext()->Find(mOwner) : nullptr;
		return owner && owner->IsVisible() && owner->IsEnabled();
	}

	bool EditorAssetPicker::ProcessEvent(UI::UIContext& context, const Platform::Event& event)
	{
		auto found = ActivePickers.find(&context);
		if (found == ActivePickers.end()) return false;
		auto* picker = dynamic_cast<EditorAssetPicker*>(context.Find(found->second));
		if (!picker) return false;
		if (!picker->AcceptsPointer() || event.mType == Platform::EventType::FocusChanged)
		{
			picker->Close();
			return false;
		}
		if (event.mType == Platform::EventType::Key && event.mPressed)
		{
			if (event.mKey == Platform::Input::KEYCODE_ESCAPE)
			{
				picker->Close();
				return true;
			}
		}
		return false;
	}

	void EditorAssetPicker::Update(UI::UIContext& context)
	{
		auto found = ActivePickers.find(&context);
		if (found == ActivePickers.end()) return;
		auto* picker = dynamic_cast<EditorAssetPicker*>(context.Find(found->second));
		if (!picker) return;
		bool focused = false;
		for (auto* widget = context.Find(context.GetFocusedWidget()); widget; widget = widget->GetParent())
			focused = focused || widget == picker;
		if (!focused || !picker->AcceptsPointer()) picker->Close();
	}

	EditorAssetPicker::EditorAssetPicker(UI::WidgetId owner, Assets::AssetType type,
		std::function<void(Assets::IAsset*)> committed)
		: mOwner(owner), mType(type), mCommitted(std::move(committed))
	{
		auto& column = Add<UI::VerticalBox>();
		column.SetPadding({8, 8, 8, 8});
		column.SetSpacing(6);
		Label(column, type == Assets::AssetType::Mesh ? "Select mesh" : "Select material", 14);
		Action(column, "Clear", [this]() { Commit(nullptr); });
		pSearch = &column.Add<UI::TextBox>();
		Label(column, "Search assets by name", 12);
		pResults = &column.Add<UI::ScrollBox>();
		Fill(column, *pResults);
		pSearch->OnChanged = [this](const std::string& text) { Filter(text); };
		Filter("");
	}

	void EditorAssetPicker::Filter(const std::string& text)
	{
		const auto query = Lower(text);
		std::vector<Assets::IAsset*> matches;
		auto gather = [&](auto& map) {
			for (auto& [id, asset] : map.mData)
				if (Lower(AssetDocument::GetTitle(mType, asset.GetName())).find(query) != std::string::npos)
					matches.push_back(&asset);
		};
		auto& library = Assets::AssetLibrary::Get();
		if (mType == Assets::AssetType::Mesh) gather(library.mImportedMeshes);
		else gather(library.mImportedMaterials);
		std::sort(matches.begin(), matches.end(), [](auto* a, auto* b) {
			const auto first = Lower(a->GetName());
			const auto second = Lower(b->GetName());
			return first == second ? a->GetUUID().str() < b->GetUUID().str() : first < second;
		});
		auto results = std::make_unique<UI::VerticalBox>();
		results->SetSpacing(3);
		if (matches.empty()) Label(*results, "No matching assets", 13);
		for (auto* asset : matches)
		{
			const auto id = asset->GetUUID();
			auto& button = Action(*results, AssetDocument::GetTitle(mType, asset->GetName()), [this, id]() {
				auto& library = Assets::AssetLibrary::Get();
				Assets::IAsset* asset = mType == Assets::AssetType::Mesh
					? static_cast<Assets::IAsset*>(library.mImportedMeshes.GetAsset(id))
					: static_cast<Assets::IAsset*>(library.mImportedMaterials.GetAsset(id));
				if (asset && asset->GetState() == Assets::IAsset::State::Created) Commit(asset);
			});
			button.SetEnabled(asset->GetState() == Assets::IAsset::State::Created);
		}
		pResults->SetContent(std::move(results));
		pResults->SetOffset(0);
	}

	void EditorAssetPicker::FocusSearch()
	{
		GetContext()->SetFocus(pSearch->GetId());
	}

	void EditorAssetPicker::Close()
	{
		if (auto* context = GetContext()) context->RemoveFromViewport(GetId());
	}

	void EditorAssetPicker::Commit(Assets::IAsset* asset)
	{
		auto* owner = GetContext() ? GetContext()->Find(mOwner) : nullptr;
		auto committed = owner && owner->IsVisible() && owner->IsEnabled() ? mCommitted : nullptr;
		Close();
		if (committed) committed(asset);
	}

	void EditorAssetPicker::OnArrange(UI::Rect content)
	{
		auto* owner = GetContext()->Find(mOwner);
		if (!owner || !owner->IsVisible() || !owner->IsEnabled())
		{
			mPopup = {};
			mChildren.front().Child->SetVisibility(UI::Visibility::Collapsed);
			return;
		}
		mChildren.front().Child->SetVisibility(UI::Visibility::Visible);
		const auto anchor = owner->GetBounds();
		const float width = std::min(300.f, content.Width);
		const float height = std::min(360.f, content.Height);
		float top = anchor.y + anchor.Height;
		if (top + height > content.y + content.Height) top = anchor.y - height;
		mPopup = {std::clamp(anchor.x, content.x, content.x + content.Width - width),
			std::clamp(top, content.y, content.y + content.Height - height), width, height};
		mChildren.front().Child->Arrange(mPopup);
	}

	void EditorAssetPicker::OnPaint(UI::DrawList& list, UI::Rect clip) const
	{
		if (AcceptsPointer()) list.Add(mPopup, clip, {.065f, .08f, .10f, 1});
	}

	UI::EventReply EditorAssetPicker::OnInput(const UI::InputEvent& event)
	{
		if (event.EventType == UI::InputEvent::Type::KeyDown && event.Key == Platform::Input::KEYCODE_ESCAPE)
		{
			Close();
			return {true};
		}
		if (event.EventType == UI::InputEvent::Type::PointerDown && !mPopup.Contains(event.Position))
		{
			Close();
			return {true};
		}
		if (event.EventType == UI::InputEvent::Type::Cancel)
		{
			auto* context = GetContext();
			context->Defer([context, id = GetId()]() {
				auto* popup = context->Find(id);
				if (!popup) return;
				for (auto* focus = context->Find(context->GetFocusedWidget()); focus; focus = focus->GetParent())
					if (focus == popup) return;
				context->RemoveFromViewport(id);
			});
		}
		return {};
	}
}
