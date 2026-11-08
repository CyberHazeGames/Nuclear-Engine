#pragma once
#include <EditorDockLayout.h>
#include <EditorFloatingWindow.h>
#include <map>

namespace Nuclear::Editor
{
	/// Session controller for document-owned dock layouts and native windows.
	/// Main widgets are context-owned; this controller owns detached native windows and closed tab contents.
	class EditorWorkspace
	{
	public:
		/// Borrows the main context, browser strip and document host. Creates a pinned viewport workspace.
		EditorWorkspace(UI::UIContext& context, EditorTabStrip& documents, EditorDockArea& documentHost);
		~EditorWorkspace(); ///< Closes native windows without accessing destroyed main-context widgets.
		/// Sets the viewport document's central contents during initial workspace construction.
		void SetViewportContent(std::unique_ptr<UI::Widget> content);
		/// Adds a document with its own layout, or a tool owned by ownerDocument (viewport by default).
		/// A document starts with only its central content. Tool ownership cannot change through docking.
		void Add(const std::string& id, const std::string& title, std::unique_ptr<UI::Widget> content,
			EditorDockRegion home, bool document = false, bool closable = true, bool detachable = true,
			const std::string& ownerDocument = "viewport");
		/// Updates browser, local and native-window captions without changing identity or contents.
		/// Blank titles use a generic fallback; asset callers supply a type-specific title.
		void SetTitle(const std::string& id, const std::string& title);
		/// Returns a borrowed document layout, including a closed document retained for reopening.
		EditorDockLayout* GetDocumentLayout(const std::string& id) const;
		bool Contains(const std::string& id) const; ///< Includes closed tabs retained for reopening.
		void Show(const std::string& id); ///< Queues opening/activation, preserving any existing tab contents.
		void Close(const std::string& id); ///< Queues closure; pinned main viewport remains available.
		void Dock(const std::string& id, EditorDockRegion region); ///< Queues moving a tab to a main-window group.
		void Detach(const std::string& id); ///< Queues tear-off to a movable, resizable native window.
		void ResetLayout(); ///< Resets only the active browser document's tools and split proportions.
		void ToggleWindowMenu(); ///< Shows/hides the panel recovery and layout menu in the main window.
		void SetInteractionEnabled(bool enabled); ///< Isolates detached panels while a main-window modal is open.
		void Update(); ///< Applies pending transfers outside dispatch and releases empty native windows.
		void RenderWindows(); ///< Draws all independent native windows on the existing graphics device.
		void ProcessEvent(const Platform::Event& event); ///< Routes secondary-window input/resize/close.
		void Shutdown(); ///< Releases floating and closed contents before graphics/assets shut down.
		EditorDockArea* GetArea(const std::string& id) const; ///< Borrowed current group; null for closed tabs.
		const std::vector<std::unique_ptr<EditorFloatingWindow>>& GetWindows() const { return mWindows; }
		///< Borrowed native windows; entries can disappear on the next Update.
	private:
		/// Controller metadata never owns an attached widget a second time.
		struct TabState
		{
			EditorTabStrip::Tab Header; ///< Stable identity and visible title.
			EditorDockRegion Home = EditorDockRegion::Documents; ///< Reset/reopen destination.
			EditorDockArea* Area = nullptr; ///< Borrowed current owner, or null while closed.
			EditorDockLayout* Layout = nullptr; ///< Document-owned layout, null for tool tabs.
			std::unique_ptr<UI::Widget> ClosedContent; ///< Exclusive ownership only while closed.
			std::string OwnerDocument; ///< Owning document ID for tools; the document's own ID otherwise.
			bool Document = false; ///< True for asset/viewport browser documents.
			bool Detachable = true; ///< Pinned viewport cannot leave the main document group.
		};
		/// A group belongs to one document, or is the browser document host itself.
		struct AreaState
		{
			UI::WidgetId Id = 0; ///< Stable identity used to validate pointers during teardown.
			std::string Document; ///< Owning document ID; empty for the browser host.
			Platform::Window* Window = nullptr; ///< Borrowed native event origin.
			bool Floating = false; ///< True for a native window's top-level tab group.
		};
		void Bind(EditorDockArea& area, Platform::Window* window, const std::string& document, bool floating = false);
		///< Connects gestures and records the owning document and native event origin.
		void BindDocument(const std::string& id, Platform::Window* window);
		///< Rebinds all groups when a complete document moves between native contexts.
		void Unbind(EditorDockArea& area); ///< Clears callbacks before either context or controller is destroyed.
		void BindStrip(EditorTabStrip& strip, EditorDockArea& area, Platform::Window* window);
		///< Connects an internal or top-level header to its borrowed group and origin window.
		void Refresh(); ///< Mirrors main documents into the browser strip and invalidates dock geometry.
		void ClearPreviews(); ///< Clears drop previews across live and parked document layouts.
		EditorDockArea& HomeArea(const TabState& tab) const; ///< Default browser or document-local destination.
		void ShowNow(const std::string& id); ///< Restores the owner document before opening/activating a tool.
		void MoveNow(const std::string& id, EditorDockArea& destination, size_t index = SIZE_MAX);
		///< Transfers one live/closed tab outside dispatch, preserving its widget tree and identity.
		void DetachNow(const std::string& id); ///< Creates a native host before transferring ownership.
		void Drag(const std::string& id, Platform::Window* origin, UI::Vec2 point, bool dropped);
		///< Resolves cross-window desktop coordinates, previews a target and queues the final drop.
		void CloseMenu(); ///< Removes a recovery-menu overlay through its owning context.
		UI::UIContext& mContext; ///< Borrowed main retained context.
		EditorTabStrip* pDocuments; ///< Borrowed browser strip; validated by mDocumentsId before teardown access.
		EditorDockArea* pDocumentHost; ///< Main-context-owned browser document container.
		UI::WidgetId mDocumentsId; ///< Validates the borrowed browser strip during teardown.
		std::map<std::string, TabState> mTabs; ///< Stable records for all registered tab IDs.
		std::map<EditorDockArea*, AreaState> mAreas; ///< Group ownership and validated native event origins.
		std::vector<std::unique_ptr<EditorFloatingWindow>> mWindows; ///< Owned independent native windows.
		std::vector<std::function<void()>> mPending; ///< Input callbacks enqueue ownership-changing work here.
		UI::WidgetId mMenu = 0; ///< Main-context recovery menu identity, or zero.
		std::string mMenuDocument; ///< Owner of the open recovery menu; switching documents closes it.
		bool mEnabled = true; ///< False while a project modal isolates the editor workspace.
	};
}
