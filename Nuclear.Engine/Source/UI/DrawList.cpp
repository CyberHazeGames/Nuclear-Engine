#include <Assets/Font.h>
#include <UI/DrawList.h>
#include <cmath>

namespace Nuclear::UI
{
	void DrawList::Add(Rect bounds, Rect clip, Color tint, Diligent::ITextureView* texture, Rect uv, bool sdf)
	{
		const auto visible = bounds.Intersect(clip);
		if (visible.Width > 0 && visible.Height > 0 && tint.a > 0)
			Quads.push_back({bounds, clip, uv, tint, texture, sdf});
	}

	namespace
	{
		struct PositionedGlyph
		{
			const Assets::FontCharacter* Glyph;
			float x;
			unsigned Line;
		};
		struct TextLayout
		{
			std::vector<PositionedGlyph> Glyphs;
			std::vector<float> Widths = {0};
			float LineHeight;
		};

		// Reject overlong encodings, surrogates, incomplete sequences, and values above U+10FFFF.
		Uint32 Decode(std::string_view text, size_t& cursor)
		{
			const unsigned char lead = static_cast<unsigned char>(text[cursor++]);
			if (lead < 0x80)
				return lead;
			const int count = lead >= 0xC2 && lead <= 0xDF
								  ? 1
								  : (lead >= 0xE0 && lead <= 0xEF ? 2 : (lead >= 0xF0 && lead <= 0xF4 ? 3 : 0));
			if (!count || cursor + count > text.size())
				return '?';
			Uint32 value = lead & ((1u << (6 - count)) - 1);
			for (int index = 0; index < count; ++index)
			{
				const auto next = static_cast<unsigned char>(text[cursor + index]);
				if ((next & 0xC0) != 0x80)
					return '?';
				value = (value << 6) | (next & 0x3F);
			}
			cursor += count;
			if ((count == 1 && value < 0x80) || (count == 2 && value < 0x800) || (count == 3 && value < 0x10000) ||
				(value >= 0xD800 && value <= 0xDFFF) || value > 0x10FFFF)
				return '?';
			return value;
		}

		TextLayout LayoutText(const Assets::Font* font, std::string_view text, float size, float wrapWidth)
		{
			TextLayout result;
			result.LineHeight = (font ? font->LineHeight : 1.2f) * size;
			float x = 0;
			Uint32 previous = 0;
			auto nextLine = [&]()
			{
				result.Widths.back() = x;
				result.Widths.push_back(0);
				x = 0;
				previous = 0;
			};
			for (size_t cursor = 0; cursor < text.size();)
			{
				const size_t glyphStart = cursor;
				Uint32 codepoint = Decode(text, cursor);
				if (codepoint == '\r')
					continue;
				if (codepoint == '\n')
				{
					nextLine();
					continue;
				}
				const unsigned copies = codepoint == '\t' ? 4 : 1;
				if (codepoint == '\t')
					codepoint = ' ';
				const auto* glyph = font ? font->FindCharacter(codepoint) : nullptr;
				const Uint32 resolved = glyph ? glyph->CodePoint : codepoint;
				// Prefer a word boundary when the upcoming word fits on a line;
				// oversized words still fall back to glyph wrapping below.
				if (wrapWidth > 0 && x > 0 && previous == ' ' && codepoint != ' ')
				{
					float wordWidth = 0;
					Uint32 wordPrevious = 0;
					for (size_t lookahead = glyphStart; lookahead < text.size();)
					{
						const auto next = Decode(text, lookahead);
						if (next == ' ' || next == '\t' || next == '\r' || next == '\n')
							break;
						const auto* nextGlyph = font ? font->FindCharacter(next) : nullptr;
						const auto nextResolved = nextGlyph ? nextGlyph->CodePoint : next;
						wordWidth += (nextGlyph ? nextGlyph->Advance : .5f) * size;
						if (font && wordPrevious)
							wordWidth += font->GetKerning(wordPrevious, nextResolved) * size;
						wordPrevious = nextResolved;
					}
					if (wordWidth <= wrapWidth && x + wordWidth > wrapWidth)
						nextLine();
				}
				for (unsigned copy = 0; copy < copies; ++copy)
				{
					float kerning = font && previous ? font->GetKerning(previous, resolved) * size : 0;
					const float advance = (glyph ? glyph->Advance : .5f) * size;
					if (wrapWidth > 0 && x > 0 && x + kerning + advance > wrapWidth)
					{
						nextLine();
						kerning = 0;
						if (resolved == ' ')
							continue;
					}
					x += kerning;
					if (glyph)
						result.Glyphs.push_back({glyph, x, static_cast<unsigned>(result.Widths.size() - 1)});
					x += advance;
					previous = resolved;
				}
			}
			result.Widths.back() = x;
			return result;
		}
	} // namespace

	Vec2 DrawList::MeasureText(const Assets::Font* font, std::string_view text, float size, float wrapWidth)
	{
		if (text.empty())
			return {};
		const auto layout = LayoutText(font, text, size, wrapWidth);
		return {*std::max_element(layout.Widths.begin(), layout.Widths.end()),
				layout.LineHeight * layout.Widths.size()};
	}

	void DrawList::Text(const Assets::Font* font, std::string_view text, Rect bounds, Rect clip, float size,
						Color color, Alignment alignment, bool wrap)
	{
		if (!font || !font->mTextureView || bounds.Width <= 0 || bounds.Height <= 0)
			return;
		const auto layout = LayoutText(font, text, size, wrap ? bounds.Width : 0);
		for (const auto& positioned : layout.Glyphs)
		{
			const auto& glyph = *positioned.Glyph;
			const auto& plane = glyph.PlaneBounds;
			const auto& atlas = glyph.AtlasBounds;
			const float extra = bounds.Width - layout.Widths[positioned.Line];
			const float alignmentOffset =
				alignment == Alignment::Center ? extra * .5f : (alignment == Alignment::End ? extra : 0);
			const float baseline = bounds.y + font->Ascender * size + positioned.Line * layout.LineHeight;
			Add({bounds.x + alignmentOffset + positioned.x + plane.x * size, baseline - plane.w * size,
				 (plane.z - plane.x) * size, (plane.w - plane.y) * size},
				clip, color, font->mTextureView, {atlas.x, atlas.y, atlas.z - atlas.x, atlas.w - atlas.y}, true);
		}
	}

} // namespace Nuclear::UI
