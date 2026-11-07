#pragma once

#include <UI/Types.h>
#include <memory>
#include <vector>

namespace Nuclear::UI
{
	class UIContext;
	class DrawList;

	/// Base retained widget. Tree ownership belongs to its parent or viewport context.
	/// All widget operations run on the engine/window thread.
	class NEAPI Widget
	{
	public:
		Widget();
		virtual ~Widget();
		Widget(const Widget&) = delete;
		Widget& operator=(const Widget&) = delete;
		/// Stable ID; look it up through UIContext instead of retaining pointers after removal.
		WidgetId GetId() const
		{
			return mId;
		}
		/// Borrowed owning context, or null while detached.
		UIContext* GetContext() const
		{
			return pContext;
		}
		/// Borrowed parent, or null for detached widgets and viewport roots.
		Widget* GetParent() const
		{
			return pParent;
		}
		/// Current logical viewport bounds, valid after layout.
		Rect GetBounds() const
		{
			return mBounds;
		}
		/// Last measured size, including this widget's padding.
		Vec2 GetDesiredSize() const
		{
			return mDesiredSize;
		}
		/// Changes drawing/layout participation and cancels input when hidden or collapsed.
		void SetVisibility(Visibility visibility);
		/// Current visibility.
		Visibility GetVisibility() const
		{
			return mVisibility;
		}
		bool IsVisible() const; ///< True only when this widget and all ancestors are visible.
		/// Disables input throughout this subtree; stateful controls cancel pending presses.
		void SetEnabled(bool enabled);
		bool IsEnabled() const; ///< Includes inherited parent enabled state.
		/// Insets content; negative values are clamped to zero.
		void SetPadding(Margin padding);
		/// Sets desired-size limits; maximums are expanded to at least minimums.
		void SetSizeLimits(Vec2 minimum, Vec2 maximum);
		/// Enables rectangular clipping of this widget and descendants (default true).
		void SetClipToBounds(bool clip)
		{
			mClipToBounds = clip;
			InvalidateLayout();
		}
		/// Marks cached layout dirty up to the owning context.
		void InvalidateLayout();
		/// Pointer is over this control.
		bool IsHovered() const
		{
			return mHovered;
		}
		bool HasFocus() const; ///< True when this context's keyboard focus targets this widget.
		/// Measure and arrange are public for custom panels and deterministic layout tests.
		void Measure(Vec2 available);
		void Arrange(Rect bounds);
		/// Paints the visible subtree with an inherited clip rectangle.
		void Paint(DrawList& list, Rect clip) const;
		/// Returns the topmost eligible widget at a logical position, or null.
		Widget* HitTest(Vec2 position, Rect clip);
		/// Override for interactive widgets. Handled replies stop ancestor bubbling.
		virtual EventReply OnInput(const InputEvent& event);
		/// Override to participate in Tab navigation.
		virtual bool IsFocusable() const
		{
			return false;
		}

	protected:
		friend class UIContext;
		friend class PanelWidget;
		friend class ContentWidget;
		/// Computes content size without this widget's padding; panels measure their children here.
		virtual Vec2 OnMeasure(Vec2 available);
		/// Arranges children within padded content bounds.
		virtual void OnArrange(Rect content);
		/// Emits this widget's visuals before its children.
		virtual void OnPaint(DrawList& list, Rect clip) const;
		/// True for pointer targets; noninteractive leaves let input reach controls behind them.
		virtual bool AcceptsPointer() const
		{
			return IsFocusable();
		}
		/// Called once on each viewport attachment/removal, with context valid during removal.
		virtual void OnAttached() {}
		virtual void OnRemoved() {}
		/// Child iteration for custom owning panels; empty on leaves.
		virtual std::vector<Widget*> Children() const;
		/// Resolves an explicit font, otherwise the context's default font.
		const Assets::Font* ResolveFont(const Assets::Font* font) const;
		Rect ContentBounds() const; ///< Arranged bounds inset by padding.
		Margin mPadding; ///< Nonnegative content insets.

	private:
		/// Updates attachment recursively and invokes hooks when the context changes.
		void Attach(UIContext* context, Widget* parent);
		WidgetId mId; ///< Process-unique nonzero identity.
		UIContext* pContext = nullptr; ///< Borrowed context.
		Widget* pParent = nullptr; ///< Borrowed parent.
		Visibility mVisibility = Visibility::Visible; ///< Layout/drawing participation.
		bool mEnabled = true; ///< Local enabled state.
		bool mHovered = false; ///< Set by context hit testing.
		bool mClipToBounds = true; ///< Constrains descendants and hit testing.
		Vec2 mMinimum; ///< Minimum desired size.
		Vec2 mMaximum = {1000000, 1000000}; ///< Maximum desired size.
		Vec2 mDesiredSize; ///< Measure result.
		Rect mBounds; ///< Arrange result.
	};
} // namespace Nuclear::UI
