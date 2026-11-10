#pragma once
#include <EditorDockArea.h>
#include <Nuclear.Platform/Window.h>
#include <Nuclear.Platform/Event.h>
#include <UI/UIContext.h>
#include <UI/UIRenderer.h>
#include <Diligent/Common/interface/RefCntAutoPtr.hpp>
#include <Diligent/Graphics/GraphicsEngine/interface/SwapChain.h>

namespace Nuclear::Editor
{
	class EditorTitleBar;
	/// Owns one native editor window, its independent retained input tree and its swap chain.
	/// All operations run on the engine/window thread; destroy before graphics/platform shutdown.
	class EditorFloatingWindow
	{
	public:
		~EditorFloatingWindow(); ///< Releases UI/GPU objects before the native window.
		/// Creates a shared-device swap chain and private UI context; returns false without losing any panel.
		/// size specifies the initial physical client dimensions; dock windows keep their existing default.
		bool Initialize(const std::string& title, const Assets::Font* font, bool show = true, UI::Vec2 size = {760, 620});
		void Shutdown(); ///< Idempotent ordered teardown; does not redock or run controller callbacks.
		void SetTitle(const std::string& title); ///< Sets both custom caption and native taskbar/Alt-Tab text for a live window.
		void ProcessEvent(const Platform::Event& event); ///< Routes input and resizes only this window.
		void Render(); ///< Draws this window's current back buffer; minimized windows are skipped.
		void Present(); ///< Presents the rendered buffer; contents may be discarded afterward.
		Platform::Window& GetWindow() { return mWindow; } ///< Borrowed platform window.
		UI::UIContext& GetContext() { return mContext; } ///< Borrowed independent context.
		EditorDockArea& GetArea() { return *pArea; } ///< Borrowed window-owned dock group.
		Diligent::ISwapChain* GetSwapChain() const { return pSwapChain; } ///< Borrowed presentation target until Shutdown.
	private:
		Platform::Window mWindow; ///< Owned native window, outlives the swap chain.
		UI::UIContext mContext; ///< Owns the floating panel tree and input state.
		UI::UIRenderer mRenderer; ///< Private retained renderer using the shared graphics device.
		Diligent::RefCntAutoPtr<Diligent::ISwapChain> pSwapChain; ///< Window-specific presentation resources.
		EditorDockArea* pArea = nullptr; ///< Borrowed root dock area, valid after Initialize.
		EditorTitleBar* pTitleBar = nullptr; ///< Context-owned caption, valid until Shutdown.
	};
}
