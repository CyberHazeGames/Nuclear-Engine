#include <UI/DrawList.h>
#include <UI/TextBlock.h>
#include <cmath>

namespace Nuclear::UI
{
	TextBlock::TextBlock(std::string text) : mText(std::move(text)) {}

	void TextBlock::SetText(std::string text)
	{
		mText = std::move(text);
		InvalidateLayout();
	}

	void TextBlock::SetFont(const Assets::Font* font)
	{
		pFont = font;
		InvalidateLayout();
	}

	void TextBlock::SetFontSize(float size)
	{
		mSize = std::isfinite(size) ? std::max(1.0f, size) : 20;
		InvalidateLayout();
	}

	void TextBlock::SetWrap(bool wrap)
	{
		mWrap = wrap;
		InvalidateLayout();
	}

	void TextBlock::SetTextAlignment(Alignment alignment)
	{
		mAlignment = alignment;
	}

	Vec2 TextBlock::OnMeasure(Vec2 available)
	{
		return DrawList::MeasureText(ResolveFont(pFont), mText, mSize, mWrap ? available.x : 0);
	}

	void TextBlock::OnPaint(DrawList& list, Rect clip) const
	{
		list.Text(ResolveFont(pFont), mText, ContentBounds(), clip, mSize, mColor, mAlignment, mWrap);
	}
} // namespace Nuclear::UI
