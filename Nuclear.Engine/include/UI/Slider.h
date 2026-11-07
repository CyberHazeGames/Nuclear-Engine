#pragma once

#include <UI/Widget.h>
#include <functional>

namespace Nuclear::UI
{
	/// Horizontal normalized slider with pointer capture and focused arrow-key adjustment.
	class NEAPI Slider : public Widget
	{
	public:
		void SetValue(float value); ///< Clamps to [0, 1] without firing callbacks.
		/// Returns the normalized slider value in [0, 1].
		float GetValue() const
		{
			return mValue;
		}
		std::function<void(float)> OnChanged; ///< Invoked only when user input changes the value.
		bool IsFocusable() const override
		{
			return true;
		}
		EventReply OnInput(const InputEvent& event) override;

	protected:
		Vec2 OnMeasure(Vec2) override
		{
			return {180, 24};
		}
		void OnPaint(DrawList& list, Rect clip) const override;

	private:
		void ChangeValue(float value); ///< Applies user value and invokes a copied callback.
		float mValue = 0; ///< Normalized value.
		bool mDragging = false; ///< True while this control owns pointer capture.
	};
} // namespace Nuclear::UI
