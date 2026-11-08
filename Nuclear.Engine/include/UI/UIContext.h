#pragma once

#include <Nuclear.Platform/Event.h>
#include <UI/DrawList.h>
#include <UI/Widget.h>
#include <functional>
#include <vector>

namespace Nuclear::UI
{
	/// Owns viewport roots and input state. Can be used without graphics for layout/input tests.
	class NEAPI UIContext
	{
	public:
		UIContext() = default;
		~UIContext();
		UIContext(const UIContext&) = delete;
		UIContext& operator=(const UIContext&) = delete;
		/// Transfers a detached root. Roots paint by stable ZOrder; returned reference is borrowed.
		Widget& AddToViewport(std::unique_ptr<Widget> root, int zOrder = 0);
		/// Removes a root by ID; destruction during dispatch is deferred until bubbling finishes.
		void RemoveFromViewport(WidgetId id);
		/// Clears roots, draw data, focus, capture, and restores GameAndUI mode.
		void Clear();
		/// Looks up a still-attached widget; null means the handle has expired.
		Widget* Find(WidgetId id) const;
		/// Physical client dimensions and logical-to-pixel scale; zero dimensions suspend UI.
		void SetViewport(float width, float height, float scale = 1);
		/// Invalidates cached geometry; normal widget setters call this automatically.
		void InvalidateLayout()
		{
			mLayoutDirty = true;
		}
		/// Resolves dirty measure/arrange state; input also calls this before hit testing.
		void Layout();
		/// Rebuilds ordered draw data from the current tree, ready for a renderer.
		const DrawList& BuildDrawList();
		/// Processes one platform event; true means gameplay should not receive it.
		bool ProcessEvent(const Platform::Event& event);
		/// Optional application shortcut filter, called before widget routing inside the dispatch guard.
		/// Return true to consume the event. Focus notifications still reset context input normally.
		/// Captured objects must outlive this context or clear the callback; Clear resets it.
		std::function<bool(const Platform::Event&)> OnPreviewEvent;
		/// Selects input routing; GameOnly cancels focus/capture. Physical input remains untouched.
		void SetInputMode(InputMode mode);
		/// Cancels presses/focus/capture when another UI layer takes input ownership.
		void ResetInput();
		/// Current routing policy.
		InputMode GetInputMode() const
		{
			return mInputMode;
		}
		bool WantsMouse() const; ///< UIOnly, captured pointer, or hovered interactive widget.
		bool WantsKeyboard() const; ///< UIOnly or focused interactive widget.
		/// Validated on event/layout changes.
		WidgetId GetFocusedWidget() const
		{
			return mFocus;
		}
		/// Selects an attached, visible, enabled focusable widget by ID; zero clears focus.
		/// Cancels the previous focus and reveals scroll ancestors. Call on the UI/event thread.
		void SetFocus(WidgetId id);
		/// Zero when uncaptured.
		WidgetId GetCapturedWidget() const
		{
			return mCapture;
		}
		/// Borrowed default font; must outlive this context or be cleared before destruction.
		void SetDefaultFont(const Assets::Font* font);
		/// Borrowed default.
		const Assets::Font* GetDefaultFont() const
		{
			return pDefaultFont;
		}
		/// Pixels per logical unit.
		float GetScale() const
		{
			return mScale;
		}
		/// Physical viewport dimensions.
		Vec2 GetPixelSize() const
		{
			return mPixelSize;
		}
		/// Runs a tree mutation after current dispatch; executes immediately outside dispatch.
		void Defer(std::function<void()> mutation);
		/// Cancels input targeting a widget/subtree before visibility, enablement, or removal changes.
		void CancelSubtree(Widget& widget);
		/// True while dispatching input or deferred tree mutations; ownership transfers must wait for Update.
		bool IsDispatching() const { return mDispatchDepth != 0; }

	private:
		/// Owned root with stable paint ordering.
		struct RootEntry
		{
			std::unique_ptr<Widget> Root; ///< Exclusive root ownership.
			int ZOrder = 0; ///< Higher roots paint last.
		};
		void CancelInput(); ///< Sends Cancel and drops focus/capture/hover.
		bool Dispatch(WidgetId target, const InputEvent& event); ///< Bubbles along a snapshot of IDs.
		void UpdateHover(Vec2 position); ///< Recomputes topmost interactive hover target.
		void FlushDeferred(); ///< Applies queued mutations after dispatch.
		void CollectFocusable(Widget& widget, std::vector<WidgetId>& ids) const; ///< Visible enabled order.
		std::vector<RootEntry> mRoots; ///< Stable-sorted viewport tree roots.
		std::vector<std::function<void()>> mDeferred; ///< Pending safe tree mutations.
		DrawList mDrawList; ///< Current frame CPU quads.
		const Assets::Font* pDefaultFont = nullptr; ///< Borrowed default typeface.
		Vec2 mPixelSize; ///< Physical client size.
		float mScale = 1; ///< Pixels per logical unit.
		Vec2 mPointer; ///< Last logical pointer position.
		WidgetId mHover = 0; ///< Hovered interactive widget.
		WidgetId mFocus = 0; ///< Keyboard target.
		WidgetId mCapture = 0; ///< Pointer capture target.
		InputMode mInputMode = InputMode::GameAndUI; ///< Routing policy.
		bool mLayoutDirty = true; ///< Needs measure/arrange.
		bool mWindowFocused = true; ///< Suppresses input when the native window loses focus.
		bool mShiftDown = false; ///< Tracks reverse Tab traversal.
		unsigned mDispatchDepth = 0; ///< Defers destruction through nested callbacks.
	};
} // namespace Nuclear::UI
