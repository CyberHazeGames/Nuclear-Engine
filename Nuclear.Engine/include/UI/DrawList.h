#pragma once

#include <UI/Types.h>
#include <string_view>
#include <vector>

namespace Nuclear::UI
{
	/// CPU draw list shared by widgets and renderer; no ImGui objects are involved.
	class NEAPI DrawList
	{
	public:
		std::vector<DrawQuad> Quads; ///< Painter order, back to front.
		/// Adds a solid or textured rectangle, skipping empty/clipped bounds.
		void Add(Rect bounds, Rect clip, Color tint, Diligent::ITextureView* texture = nullptr, Rect uv = {0, 0, 1, 1},
				 bool sdf = false);
		/// Measures UTF-8 text with the same kerning and wrapping used by Text().
		static Vec2 MeasureText(const Assets::Font* font, std::string_view text, float size, float wrapWidth = 0);
		/// Appends glyphs; unsupported codepoints use '?'. Size and width are logical units.
		void Text(const Assets::Font* font, std::string_view text, Rect bounds, Rect clip, float size, Color color,
				  Alignment alignment, bool wrap);
	};
} // namespace Nuclear::UI
