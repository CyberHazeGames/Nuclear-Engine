#pragma once
#include <Common.h>
#include <Project.h>
#include "EntityEditor.h"
#include "AssetLibraryViewer.h"

class NuclearEditor;
	
namespace Nuclear::Editor {
	class EditorUI
	{
	public:
		EditorUI(NuclearEditor* editor);

		void SetProject(Project* project);
		void NewProjectWindow();
		void Render();
		void RenderSceneView();
		void RenderStatusBar();
	protected:
		void RenderMainMenuBar();
		EntityEditor mEditor;

		Project* pActiveProject;
		NuclearEditor* mEditorInstance;
		AssetLibraryViewer mAssetLibViewer;
		ECS::Entity mSelectedEntity;

		void RenderEntityExplorer();
		bool mNewPrjWindowOpen = false;
		bool mProfilerWindowOpen = false; ///< Visibility of the optional performance/memory tool.
	};
}
