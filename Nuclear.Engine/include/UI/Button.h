#pragma once

#include <UI/ContentWidget.h>
#include <UI/ControlStyle.h>
#include <functional>

namespace Nuclear::UI
{
	/// Single-child button; activation occurs on release inside or focused Enter/Space release.
	class NEAPI Button : public ContentWidget
	{
	public:
		Button();
		std::function<void()> OnClicked; ///< Optional activation callback; may request removal safely.
		/// Copies the control state colors.
		void SetStyle(const ControlStyle& style)
		{
			mStyle = style;
		}
		/// Buttons participate in keyboard focus navigation.
		bool IsFocusable() const override
		{
			return true;
		}
		EventReply OnInput(const InputEvent& event) override;

	protected:
		void OnPaint(DrawList& list, Rect clip) const override;
		virtual void Activate(); ///< Invokes OnClicked; subclasses may update state first.
	private:
		ControlStyle mStyle; ///< State palette.
		bool mPressed = false; ///< A pointer or activation key press is pending.
		int mActivationKey = 0; ///< Zero for pointer presses, otherwise matching physical key.
	};
} // namespace Nuclear::UI
