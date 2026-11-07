#pragma once

#include <UI/Widget.h>
#include <string>

namespace Nuclear::UI
{
	/// UTF-8 label using an explicit library-owned font or the context's default font.
	class NEAPI TextBlock : public Widget
	{
	public:
		explicit TextBlock(std::string text = {}); ///< Copies UTF-8 text.
		void SetText(std::string text); ///< Changes text and invalidates layout.
		/// Borrowed text until next SetText.
		const std::string& GetText() const
		{
			return mText;
		}
		void SetFont(const Assets::Font* font); ///< Borrowed; null restores the context default.
		void SetFontSize(float size); ///< Positive logical em size.
		/// Linear text color.
		void SetColor(Color color)
		{
			mColor = color;
		}
		void SetWrap(bool wrap); ///< Enables width-constrained glyph wrapping.
		void SetTextAlignment(Alignment alignment); ///< Start, Center, or End per line.
	protected:
		Vec2 OnMeasure(Vec2 available) override;
		void OnPaint(DrawList& list, Rect clip) const override;

	private:
		std::string mText; ///< UTF-8 source string.
		const Assets::Font* pFont = nullptr; ///< Borrowed override.
		float mSize = 20; ///< Logical em size.
		Color mColor; ///< Linear glyph tint.
		bool mWrap = false; ///< Wraps at available width.
		Alignment mAlignment = Alignment::Start; ///< Horizontal line alignment.
	};
} // namespace Nuclear::UI
