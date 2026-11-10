#pragma once
#include <Assets/IAsset.h>
#include <UI/Widget.h>
#include <UI/TextBlock.h>
#include <functional>

namespace Nuclear::Editor
{
	class MeshPreviewViewport;
	/// Builds session asset-detail documents; referenced resources remain owned by the asset library.
	class AssetDocument
	{
	public:
		/// Returns a trimmed asset name, or a nonempty type-specific title such as Unnamed Texture.
		static std::string GetTitle(Assets::AssetType type, const std::string& name);
		/// Resolves a texture, mesh or material UUID on demand; null means absent/unsupported.
		static Assets::IAsset* Resolve(Assets::AssetType type, const Core::UUID& id);
		/// Returns a full-height preview on the left and scrollable resource properties on the right.
		/// instantiate is an optional main-thread mesh action; callbacks must re-resolve the asset UUID.
		/// heading optionally receives a borrowed title label, valid for the returned tree's lifetime.
		/// preview optionally receives the document-owned mesh/material sphere surface for Update/Render before UI painting.
		static std::unique_ptr<UI::Widget> Build(Assets::AssetType type, const Core::UUID& id,
			std::function<void()> instantiate = {}, UI::TextBlock** heading = nullptr,
			MeshPreviewViewport** preview = nullptr);
	};
}
