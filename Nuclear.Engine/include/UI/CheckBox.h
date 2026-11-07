#pragma once

#include <UI/Button.h>
#include <functional>

namespace Nuclear::UI
{
	/// Toggle control with a built-in check indicator and optional child content.
	class NEAPI CheckBox : public Button
	{
	public:
		CheckBox();
		void SetChecked(bool checked); ///< Sets state silently without firing callbacks.
		/// Returns the current toggle value.
		bool IsChecked() const
		{
			return mChecked;
		}
		std::function<void(bool)> OnChanged; ///< Receives new state after user activation.
	protected:
		void Activate() override;
		void OnPaint(DrawList& list, Rect clip) const override;

	private:
		bool mChecked = false; ///< Toggle state.
	};
} // namespace Nuclear::UI
