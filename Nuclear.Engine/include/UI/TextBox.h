#pragma once
#include <UI/Widget.h>
#include <functional>
#include <string>

namespace Nuclear::UI
{
	/// Single-line UTF-8 editor. Cursor offsets are codepoint boundaries; text is owned.
	class NEAPI TextBox : public Widget
	{
	  public:
		/// Copies initial text; cursor starts at the end.
		explicit TextBox(std::string text = {});
		/// Replaces text without invoking callbacks.
		void SetText(std::string text);
		/// Borrowed until the next edit.
		const std::string& GetText() const
		{
			return mText;
		}
		std::function<void(const std::string&)> OnChanged; ///< Called after user edits; may remove this widget.
		std::function<void(const std::string&)> OnCommitted; ///< Called on Enter; may remove this widget.
		/// Participates in Tab navigation.
		bool IsFocusable() const override
		{
			return true;
		}
		/// Handles text, cursor keys, deletion, whole-line selection and clipboard shortcuts.
		EventReply OnInput(const InputEvent& event) override;
	  protected:
		Vec2 OnMeasure(Vec2 available) override; ///< Reserves a stable editor width and line height.
		void OnPaint(DrawList& list, Rect clip) const override; ///< Draws background, text and caret.
	  private:
		std::string mText;		 ///< Owned UTF-8, limited to 4096 bytes by user input.
		size_t mCursor = 0;		 ///< Byte index at a codepoint boundary.
		bool mControl = false;	 ///< Ctrl modifier for select all.
		bool mSelectAll = false; ///< Whole-line selection, replaced on the next insertion.
	};
} // namespace Nuclear::UI
