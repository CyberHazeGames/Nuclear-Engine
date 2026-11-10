#pragma once
#include <UI/Widget.h>
#include <Core/UUID.h>

namespace Nuclear::Editor
{
	/// Fits a ready library texture into its full preview pane without changing its aspect ratio.
	class TexturePreviewViewport : public UI::Widget
	{
	public:
		explicit TexturePreviewViewport(const Core::UUID& texture); ///< Stores a registry identity; does not own the texture.
	protected:
		void OnPaint(UI::DrawList& list, UI::Rect clip) const override; ///< Resolves readiness and centers the texture over a checkerboard.
	private:
		Core::UUID mTexture; ///< Library identity resolved for each paint, including after a session deletion.
	};
}
