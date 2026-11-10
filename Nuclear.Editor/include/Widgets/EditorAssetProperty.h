#pragma once
#include <UI/HorizontalBox.h>
#include <Assets/IAsset.h>
#include <functional>

namespace Nuclear::Editor
{
	class MeshPreviewCache;
	/// Compact asset reference with a thumbnail, typed name and searchable registry chooser.
	class EditorAssetProperty : public UI::HorizontalBox
	{
	public:
		/// Borrows the current asset and cache. The cache and commit callback must outlive this tree.
		/// Only Mesh and Material are supported; committed receives a ready registry asset or null to clear.
		EditorAssetProperty(Assets::AssetType type, Assets::IAsset* current, MeshPreviewCache& previews,
			std::function<void(Assets::IAsset*)> committed);
	protected:
		/// Removes an open chooser before this property leaves its native UI context.
		void OnRemoved() override;
	private:
		void OpenPicker(); ///< Opens the chooser in this property's current context, including detached inspectors.
		Assets::AssetType mType; ///< Registry category accepted by this property.
		std::function<void(Assets::IAsset*)> mCommitted; ///< Revalidates the ECS target at commit time.
		UI::WidgetId mPicker = 0; ///< Context-owned chooser ID, resolved before each access.
	};
}
