#include <UI/TextBox.h>
#include <UI/DrawList.h>
#include <Nuclear.Platform/Input.h>
#include <Nuclear.Platform/Platform.h>

namespace Nuclear::UI
{
	namespace
	{
		size_t Previous(const std::string& text, size_t cursor)
		{
			if (cursor)
				--cursor;
			while (cursor && (static_cast<unsigned char>(text[cursor]) & 0xc0) == 0x80)
				--cursor;
			return cursor;
		}
		size_t Next(const std::string& text, size_t cursor)
		{
			if (cursor < text.size())
				++cursor;
			while (cursor < text.size() && (static_cast<unsigned char>(text[cursor]) & 0xc0) == 0x80)
				++cursor;
			return cursor;
		}
	} // namespace
	TextBox::TextBox(std::string text)
	{
		SetPadding({8, 7, 8, 7});
		SetText(std::move(text));
	}

	void TextBox::SetText(std::string text)
	{
		mText = std::move(text);
		mCursor = mText.size();
		mSelectAll = false;
		InvalidateLayout();
	}

	Vec2 TextBox::OnMeasure(Vec2)
	{
		return {120, 20};
	}

	EventReply TextBox::OnInput(const InputEvent& event)
	{
		using Key = Platform::Input;
		if (event.EventType == InputEvent::Type::Cancel)
		{
			mControl = false;
			mSelectAll = false;
			return {};
		}
		if (event.EventType == InputEvent::Type::PointerDown && event.Button == MOUSE_BUTTON_LEFT)
		{
			auto bounds = ContentBounds();
			const auto* font = ResolveFont(nullptr);
			float caret = DrawList::MeasureText(font, std::string_view(mText).substr(0, mCursor), 16).x;
			float localX = event.Position.x - bounds.x + std::max(0.0f, caret - bounds.Width + 2);
			mCursor = 0;
			while (mCursor < mText.size())
			{
				auto next = Next(mText, mCursor);
				float left = DrawList::MeasureText(font, std::string_view(mText).substr(0, mCursor), 16).x;
				float right = DrawList::MeasureText(font, std::string_view(mText).substr(0, next), 16).x;
				if (localX < (left + right) * .5f)
					break;
				mCursor = next;
			}
			mSelectAll = false;
			return {true, false, false, true};
		}
		if (event.EventType == InputEvent::Type::KeyUp || event.EventType == InputEvent::Type::KeyDown)
		{
			if (event.Key == Key::KEYCODE_LCTRL || event.Key == Key::KEYCODE_RCTRL)
			{
				mControl = event.EventType == InputEvent::Type::KeyDown;
				return {true};
			}
		}
		bool changed = false;
		if (event.EventType == InputEvent::Type::Text && !mControl)
		{
			auto cp = event.Codepoint;
			if (cp < 32 || cp == 127 || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff))
				return {true};
			std::string encoded;
			if (cp <= 0x7f)
				encoded += static_cast<char>(cp);
			else if (cp <= 0x7ff)
			{
				encoded += static_cast<char>(0xc0 | (cp >> 6));
				encoded += static_cast<char>(0x80 | (cp & 63));
			}
			else
			{
				if (cp > 0xffff)
				{
					encoded += static_cast<char>(0xf0 | (cp >> 18));
					encoded += static_cast<char>(0x80 | ((cp >> 12) & 63));
				}
				else
					encoded += static_cast<char>(0xe0 | (cp >> 12));
				encoded += static_cast<char>(0x80 | ((cp >> 6) & 63));
				encoded += static_cast<char>(0x80 | (cp & 63));
			}
			if (mSelectAll)
			{
				mText.clear();
				mCursor = 0;
				mSelectAll = false;
			}
			if (mText.size() + encoded.size() <= 4096)
			{
				mText.insert(mCursor, encoded);
				mCursor += encoded.size();
				changed = true;
			}
		}
		else if (event.EventType == InputEvent::Type::KeyDown)
		{
			if (event.Key == Key::KEYCODE_A && mControl)
			{
				mSelectAll = true;
				return {true};
			}
			if (mControl && (event.Key == Key::KEYCODE_C || event.Key == Key::KEYCODE_X))
			{
				if (!mSelectAll)
					return {true};
				if (!Platform::SetClipboardText(mText) || event.Key == Key::KEYCODE_C)
					return {true};
				mText.clear();
				mCursor = 0;
				changed = true;
			}
			else if (mControl && event.Key == Key::KEYCODE_V)
			{
				auto paste = Platform::GetClipboardText();
				std::erase_if(paste, [](unsigned char c) {
					return c < 32 || c == 127;
				});
				if (paste.empty())
					return {true};
				if (mSelectAll)
				{
					mText.clear();
					mCursor = 0;
				}
				const size_t capacity = 4096 - std::min<size_t>(4096, mText.size());
				if (paste.size() > capacity)
				{
					size_t length = capacity;
					while (length && (static_cast<unsigned char>(paste[length]) & 0xc0) == 0x80)
						--length;
					paste.resize(length);
				}
				mText.insert(mCursor, paste);
				mCursor += paste.size();
				changed = true;
			}
			else if (event.Key == Key::KEYCODE_RETURN)
			{
				if (!event.Repeat && OnCommitted)
					OnCommitted(mText);
				return {true};
			}
			else if (event.Key == Key::KEYCODE_LEFT)
				mCursor = Previous(mText, mCursor);
			else if (event.Key == Key::KEYCODE_RIGHT)
				mCursor = Next(mText, mCursor);
			else if (event.Key == Key::KEYCODE_HOME)
				mCursor = 0;
			else if (event.Key == Key::KEYCODE_END)
				mCursor = mText.size();
			else if (event.Key == Key::KEYCODE_BACKSPACE || event.Key == Key::KEYCODE_DELETE)
			{
				if (mSelectAll)
				{
					mText.clear();
					mCursor = 0;
					changed = true;
				}
				else
				{
					auto first = event.Key == Key::KEYCODE_BACKSPACE ? Previous(mText, mCursor) : mCursor;
					auto last = event.Key == Key::KEYCODE_DELETE ? Next(mText, mCursor) : mCursor;
					changed = last > first;
					mText.erase(first, last - first);
					mCursor = first;
				}
			}
			else
				return {};
			mSelectAll = false;
		}
		else
			return {};
		InvalidateLayout();
		if (changed && OnChanged)
			OnChanged(mText);
		return {true};
	}

	void TextBox::OnPaint(DrawList& list, Rect clip) const
	{
		list.Add(GetBounds(), clip, HasFocus() ? Color{.08f, .15f, .2f, 1} : Color{.04f, .055f, .075f, 1});
		auto bounds = ContentBounds();
		const auto* font = ResolveFont(nullptr);
		float caret = DrawList::MeasureText(font, std::string_view(mText).substr(0, mCursor), 16).x;
		float shift = std::max(0.0f, caret - bounds.Width + 2);
		auto textBounds = bounds;
		textBounds.x -= shift;
		textBounds.Width += shift;
		if (mSelectAll && HasFocus())
			list.Add(bounds, clip, {.06f, .3f, .4f, 1});
		list.Text(font, mText, textBounds, clip.Intersect(bounds), 16, {.85f, .9f, .95f, 1}, Alignment::Start,
				  false);
		if (HasFocus())
			list.Add({bounds.x + caret - shift, bounds.y, 1, bounds.Height}, clip, {.2f, .8f, .7f, 1});
	}
} // namespace Nuclear::UI
