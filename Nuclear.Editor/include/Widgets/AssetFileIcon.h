#pragma once
#include <UI/Widget.h>
#include <string>

namespace Nuclear::Editor
{
	/// Scalable thumbnail for folders and unimported files in the Content Browser.
	class AssetFileIcon : public UI::Widget
	{
	public:
		/// Uses a folder silhouette or a file page marked with its short format label.
		AssetFileIcon(bool folder, std::string format, UI::Color accent);
	protected:
		/// Paints at the center of the tile in logical coordinates, clipped by the asset view.
		void OnPaint(UI::DrawList& list, UI::Rect clip) const override;
	private:
		bool mFolder; ///< Selects the folder silhouette.
		std::string mFormat; ///< Short file type displayed on the page.
		UI::Color mAccent; ///< Type color used by the icon and its card.
	};
}
