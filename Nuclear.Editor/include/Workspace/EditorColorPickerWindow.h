#pragma once
#include <EditorFloatingWindow.h>
#include <Math/Math.h>
#include <functional>
#include <optional>

namespace Nuclear::Editor
{
	class EditorColorPicker;
	/// Workspace-owned, single native color chooser. Creates/destroys windows only from Update/Shutdown.
	/// All methods run on the editor thread; this service must outlive properties borrowing it.
	class EditorColorPickerWindow
	{
	public:
		/// Releases the chooser before the workspace's graphics resources disappear.
		~EditorColorPickerWindow();
		/// Queues a chooser for a still-live source property. The lifetime token protects the borrowed source/callback.
		/// A new request replaces the prior chooser without applying it. Alpha selects vec4 versus vec3 editors.
		void Open(UI::Widget& source, std::weak_ptr<void> lifetime, const std::string& title,
			Math::Vector4 color, bool alpha, float maximum, std::function<void(Math::Vector4)> committed);
		/// Applies queued open/close requests and closes if the source disappears, is hidden, disabled or detached.
		void Update(bool enabled = true);
		/// Draws/presents the chooser on the shared graphics device, restoring the main presentation target.
		void Render();
		/// Routes only chooser events. Close and Escape cancel; returns true if the event belongs to this chooser.
		bool ProcessEvent(const Platform::Event& event);
		void Close(); ///< Queues cancellation, safe during input dispatch.
		void Shutdown(); ///< Immediately releases callback, UI, GPU and native resources; call outside dispatch.
		/// Borrowed chooser host, null until the next Update after a property is activated.
		EditorFloatingWindow* GetWindow() const { return pWindow.get(); }
	private:
		/// Copied request values; only source is borrowed and guarded by Lifetime.
		struct Request
		{
			UI::Widget* Source = nullptr; ///< Borrowed only while Lifetime is valid.
			std::weak_ptr<void> Lifetime; ///< Expires when the property tree is replaced/destroyed.
			std::string Title; ///< Native caption and property label.
			Math::Vector4 Color{1}; ///< Color at open; model remains unchanged until Apply.
			bool Alpha = false; ///< True for RGBA controls.
			float Maximum = 1; ///< Inclusive linear RGB/value maximum.
			std::function<void(Math::Vector4)> Committed; ///< Copied setter, used only while the source is alive.
		};
		bool IsSourceValid() const; ///< Validates the weak lifetime before touching the borrowed property.
		void Apply(); ///< Queues close and invokes one copied callback with the current local picker color.
		std::optional<Request> mPending; ///< Next open request, processed outside input dispatch.
		std::optional<Request> mActive; ///< Current chooser target; reset before resource teardown.
		std::unique_ptr<EditorFloatingWindow> pWindow; ///< Owns the independent native/UI/GPU host.
		EditorColorPicker* pPicker = nullptr; ///< Borrowed chooser-owned full editor.
		bool mClose = false; ///< Deferred cancellation/application teardown.
	};
}
