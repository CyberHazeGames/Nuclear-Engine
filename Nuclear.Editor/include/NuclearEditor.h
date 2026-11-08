#pragma once
#include <Common.h>
#include <EditorUI.h>

/// Editor client owning its session project and retained workspace controllers.
class NuclearEditor : public Core::Client
{
  public:
	NuclearEditor();	  ///< Creates the client; graphics resources are initialized in Load.
	void Load() override; ///< Attaches the retained editor.
	void Update(float deltaTime) override; ///< Applies pending edits and tool refreshes.
	void Render(float deltaTime) override; ///< Renders the scene preview; engine draws retained UI afterward.
	void OnWindowResize(int width, int height) override; ///< Resizes the swap chain and workspace scale.
	void Shutdown() override; ///< Releases workspace resources before engine module shutdown.
	void OnAuxiliaryWindowEvent(const Platform::Event& event) override; ///< Routes events to detached panels.
	Nuclear::Editor::EditorWorkspace* GetWorkspace() const { return GUI.GetWorkspace(); }
	///< Borrowed session workspace controller, or null outside Load/Shutdown.
  private:
	Nuclear::Editor::Project mActiveProject; ///< Session project model, outlives GUI.
	Nuclear::Editor::EditorUI GUI;			 ///< Retained tool controller.
};
