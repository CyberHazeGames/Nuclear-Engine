#include <EditorAssetProperty.h>
#include <EditorAssetPicker.h>
#include <EditorWidgets.h>
#include <AssetDocument.h>
#include <MeshPreviewCache.h>
#include <Assets/Material.h>

namespace Nuclear::Editor
{
	EditorAssetProperty::EditorAssetProperty(Assets::AssetType type, Assets::IAsset* current,
		MeshPreviewCache& previews, std::function<void(Assets::IAsset*)> committed)
		: mType(type), mCommitted(std::move(committed))
	{
		SetSpacing(8);
		auto& thumbnail = Add<UI::Image>();
		thumbnail.SetImageSize({64, 64});
		thumbnail.SetSizeLimits({64, 64}, {64, 64});
		auto* preview = type == Assets::AssetType::Mesh
			? previews.GetPreview(static_cast<Assets::Mesh*>(current))
			: previews.GetMaterialPreview(static_cast<Assets::Material*>(current));
		thumbnail.SetBrush(preview ? UI::Brush{preview} : UI::Brush{nullptr, {.12f, .15f, .19f, 1}});

		auto& description = Add<UI::VerticalBox>();
		description.SetSpacing(4);
		Fill(*this, description);
		Label(description, type == Assets::AssetType::Mesh ? "Mesh" : "Material", 12);
		Label(description, current ? AssetDocument::GetTitle(type, current->GetName()) : "None", 13).SetWrap(true);
		if (current && current->GetState() != Assets::IAsset::State::Created)
			Label(description, "Not ready", 12);
		auto& browse = Action(*this, "...", [this]() { OpenPicker(); });
		browse.SetSizeLimits({32, 28}, {32, 28});
		auto slot = GetSlot(browse.GetId());
		slot.Vertical = UI::Alignment::Center;
		SetSlot(browse.GetId(), slot);
	}

	void EditorAssetProperty::OpenPicker()
	{
		auto* context = GetContext();
		if (!context) return;
		if (context->Find(mPicker))
		{
			context->RemoveFromViewport(mPicker);
			mPicker = 0;
			return;
		}
		auto picker = std::make_unique<EditorAssetPicker>(GetId(), mType, mCommitted);
		mPicker = picker->GetId();
		context->AddToViewport(std::move(picker), 450);
		context->Defer([context, id = mPicker]() {
			if (auto* picker = dynamic_cast<EditorAssetPicker*>(context->Find(id))) picker->FocusSearch();
		});
	}

	void EditorAssetProperty::OnRemoved()
	{
		if (mPicker && GetContext()) GetContext()->RemoveFromViewport(mPicker);
		mPicker = 0;
	}
}
