#pragma once
#include <Assets/IAsset.h>
#include <UI/Widget.h>
#include <functional>

namespace Nuclear::Editor
{
	/// Builds session asset-detail documents; referenced resources remain owned by the asset library.
	class AssetDocument
	{
	public:
		/// Returns a trimmed asset name, or a nonempty type-specific title such as Unnamed Texture.
		static std::string GetTitle(Assets::AssetType type, const std::string& name);
		/// Resolves a texture, mesh or material UUID on demand; null means absent/unsupported.
		static Assets::IAsset* Resolve(Assets::AssetType type, const Core::UUID& id);
		/// Returns a retained details tree with texture previews or mesh/material resource information.
		/// instantiate is an optional main-thread mesh action; callbacks must re-resolve the asset UUID.
		static std::unique_ptr<UI::Widget> Build(Assets::AssetType type, const Core::UUID& id,
			std::function<void()> instantiate = {});
	};
}
