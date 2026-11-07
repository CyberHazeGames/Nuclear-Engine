#pragma once

#include <NE_Common.h>
#include <algorithm>
#include <cstddef>

namespace Nuclear::Assets
{
	class Font;
	class Texture;
} // namespace Nuclear::Assets
namespace Diligent
{
	struct ITextureView;
}

namespace Nuclear::UI
{
	/// A position or size in logical UI units, measured from the viewport's top left.
	struct Vec2
	{
		float x = 0; ///< Horizontal coordinate or width.
		float y = 0; ///< Vertical coordinate or height.
	};

	/// Axis-aligned rectangle; right and bottom edges are excluded from hit tests.
	struct NEAPI Rect
	{
		float x = 0; ///< Left edge in logical units.
		float y = 0; ///< Top edge in logical units.
		float Width = 0; ///< Nonnegative width.
		float Height = 0; ///< Nonnegative height.
		bool Contains(Vec2 point) const
		{
			return point.x >= x && point.y >= y && point.x < x + Width && point.y < y + Height;
		}
		/// Returns the overlap, or an empty rectangle when the inputs do not overlap.
		Rect Intersect(const Rect& other) const;
	};

	/// Unassociated RGBA color. RGB values are linear; alpha is coverage in [0, 1].
	struct Color
	{
		float r = 1; ///< Red channel.
		float g = 1; ///< Green channel.
		float b = 1; ///< Blue channel.
		float a = 1; ///< Opacity.
	};

	/// Insets around content, in logical units.
	struct Margin
	{
		float Left = 0; ///< Left inset.
		float Top = 0; ///< Top inset.
		float Right = 0; ///< Right inset.
		float Bottom = 0; ///< Bottom inset.
	};

	/// Fill uses the allocated space; other modes use the child's desired size.
	enum class Alignment
	{
		Start,
		Center,
		End,
		Fill
	};
	/// Hidden occupies layout space; Collapsed occupies none.
	enum class Visibility
	{
		Visible,
		Hidden,
		Collapsed
	};
	/// Controls engine-side delivery without altering physical platform input state.
	enum class InputMode
	{
		GameOnly,
		UIOnly,
		GameAndUI
	};
	/// Stable identity within a context; zero is an invalid handle.
	using WidgetId = Uint64;

	/// Portable UI event after client-pixel coordinates have been scaled to logical units.
	struct InputEvent
	{
		enum class Type
		{
			PointerMove,
			PointerDown,
			PointerUp,
			KeyDown,
			KeyUp,
			Cancel
		};
		Type EventType = Type::Cancel; ///< Payload discriminator.
		Vec2 Position; ///< Logical viewport position.
		int Key = 0; ///< Platform physical key identifier.
		int Button = 0; ///< Platform mouse button identifier.
		bool Repeat = false; ///< Keyboard auto-repeat.
	};

	/// A widget's reply; capture/focus apply to the widget that handled the event.
	struct EventReply
	{
		bool Handled = false; ///< Stops bubbling when true.
		bool Capture = false; ///< Captures subsequent pointer events.
		bool Release = false; ///< Releases this widget's pointer capture.
		bool Focus = false; ///< Requests keyboard focus.
	};

	/// Brush resources are borrowed from the asset library and must outlive the widget.
	struct Brush
	{
		Assets::Texture* pTexture = nullptr; ///< Null draws a solid tinted rectangle.
		Color Tint; ///< Multiplies the sampled texture or solid white.
		Rect UV = {0, 0, 1, 1}; ///< Normalized source region.
	};

	/// One ordered quad. Resource references are borrowed for the current frame only.
	struct DrawQuad
	{
		Rect Bounds; ///< Destination in logical units.
		Rect Clip; ///< Effective ancestor clip.
		Rect UV; ///< Normalized source region.
		Color Tint; ///< Linear vertex color.
		Diligent::ITextureView* pTexture = nullptr; ///< Null selects renderer-owned white.
		bool SDF = false; ///< Sample red as a signed-distance font atlas.
	};

} // namespace Nuclear::UI
