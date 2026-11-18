#pragma once

#include <VisualScriptGraph.h>
#include <UI/Widget.h>
#include <functional>

namespace Nuclear::Editor
{
	/// Retained graph surface. The document and transient gestures travel with the widget when docked.
	class VisualScriptCanvas : public UI::Widget
	{
	public:
		/// Returns the selected node for the inspector, or null after deselection.
		VisualScriptGraph::Node* Selected();
		void AddNode(VisualScriptGraph::Kind kind); ///< Adds and selects a palette entry near the view center.
		void DeleteSelected(); ///< Deletes the current selection and its connections.
		void FrameGraph(); ///< Fits the current graph in the arranged viewport.
		std::string ToJson() const; ///< Exports the current graph independent of pan and zoom.
		bool LoadJson(const std::string& text); ///< On success, clears selection and frames the imported document.
		std::function<void()> OnSelection; ///< Updates sibling controls after selection or document replacement.
		std::function<void(const std::string&)> OnNotice; ///< Reports connection errors and authoring limits.
		bool IsFocusable() const override { return true; } ///< Receives Escape and Delete in the canvas.
		UI::EventReply OnInput(const UI::InputEvent& event) override; ///< Routes scaled pointer and keyboard gestures.

	protected:
		void OnArrange(UI::Rect content) override; ///< Frames the example on the first usable layout.
		void OnPaint(UI::DrawList& list, UI::Rect clip) const override; ///< Draws grid, wires and node cards in graph order.
		void OnRemoved() override; ///< Clears transient gestures on document transfer or closure.

	private:
		enum class Drag { None, Pan, Move, Wire }; ///< Captured pointer operation.
		VisualScriptGraph mGraph = VisualScriptGraph::Example(); ///< Session-owned editable data.
		bool mNeedsInitialFrame = true; ///< Set until the first nonempty arrangement.
		int mSelected = 0; ///< Selected card ID, or zero for empty space.
		float mZoom = 1; ///< Logical pixels per graph unit.
		UI::Vec2 mPan; ///< Canvas-relative offset in logical pixels.
		Drag mDrag = Drag::None; ///< Current pointer gesture.
		int mButton = 0; ///< Only this button can finish the active drag.
		UI::Vec2 mStart, mPointer; ///< Pointer positions at press and most recent update.
		UI::Vec2 mOriginal; ///< Initial node position or pan offset for rollback.
		VisualScriptGraph::Endpoint mWire; ///< Anchored pin for a preview wire.

		UI::Vec2 Screen(UI::Vec2 point) const; ///< Converts a graph point into logical viewport coordinates.
		UI::Vec2 World(UI::Vec2 point) const; ///< Converts a logical viewport point into graph coordinates.
		int HitNode(UI::Vec2 world) const; ///< Returns the frontmost card at a graph point.
		VisualScriptGraph::Endpoint HitPin(int id, UI::Vec2 world) const; ///< Picks a pin within the card's hit margin.
		void Select(int id); ///< Updates selection and inspector together.
		void CancelDrag(); ///< Restores the gesture's starting state.
	};
}
