#pragma once

#include <UI/Slot.h>
#include <UI/Widget.h>
#include <utility>
#include <vector>

namespace Nuclear::UI
{
	/// Owns children and their slots. Removal during input dispatch is deferred safely.
	class NEAPI PanelWidget : public Widget
	{
	public:
		/// Transfers an unattached widget; throws invalid_argument for null/attached children.
		virtual Widget& AddChild(std::unique_ptr<Widget> child, const Slot& slot = {});
		/// Constructs a child, transfers ownership, and returns a borrowed typed reference.
		template <class T, class... Args> T& Add(Args&&... args)
		{
			auto child = std::make_unique<T>(std::forward<Args>(args)...);
			auto& result = *child;
			AddChild(std::move(child));
			return result;
		}
		/// Removes an immediate child by ID; no-op when absent.
		void RemoveChild(WidgetId id);
		/// Replaces slot values and invalidates layout; returns false if absent.
		bool SetSlot(WidgetId id, const Slot& slot);
		/// Returns a copy of the slot, or defaults if absent.
		Slot GetSlot(WidgetId id) const;

	protected:
		/// One uniquely owned child plus its layout rules.
		struct ChildEntry
		{
			std::unique_ptr<Widget> Child; ///< Exclusive ownership.
			Slot Layout; ///< Copied slot settings.
		};
		std::vector<ChildEntry> mChildren; ///< Insertion order; paint uses stable ZOrder sorting.
		/// Returns borrowed children in stable paint order.
		std::vector<Widget*> Children() const override;
		/// Measures the maximum desired child extent including slot padding.
		Vec2 OnMeasure(Vec2 available) override;
		/// Arranges all children within the panel's padded content rectangle.
		void OnArrange(Rect content) override;
		/// Places a child using slot insets and alignment within an allocated rectangle.
		static void ArrangeChild(ChildEntry& child, Rect allocation);
	};
} // namespace Nuclear::UI
