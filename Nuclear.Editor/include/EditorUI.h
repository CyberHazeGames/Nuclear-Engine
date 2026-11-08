#pragma once
#include <Common.h>
#include <Project.h>
#include <EntityEditor.h>
#include <AssetLibraryViewer.h>
#include <Systems/RenderSystem.h>
#include <Rendering/RenderPasses/GeometryPass.h>
#include <UILayers/LoggerView.h>
#include <array>
#include <EditorWorkspace.h>
namespace Nuclear::Editor
{
	class GameViewPort;
	/// Retained editor workspace. Owns session tools; viewport context owns all widgets.
	class EditorUI
	{
	  public:
		explicit EditorUI(Project& project); ///< Borrows the client's session project.
		void Initialize();					 ///< Creates a default session scene and attaches the workspace.
		void Update(float deltaTime); ///< Applies requested panel changes, imports and diagnostic sampling.
		void Render(float deltaTime); ///< Renders the scene to its offscreen camera target.
		void Resize(int width, int height); ///< Updates logical workspace scaling after native resize.
		void Shutdown(); ///< Drops borrowed widgets and clears scene resources before client destruction.
		void OnAuxiliaryWindowEvent(const Platform::Event& event); ///< Routes detached-window input/resize/close.
		EditorWorkspace* GetWorkspace() const { return pWorkspace.get(); } ///< Borrowed workspace until Shutdown.
	  private:
		void BuildWorkspace();	 ///< Attaches the persistent shell and tool regions.
		void RefreshHierarchy(); ///< Replaces hierarchy content outside dispatch.
		void RefreshInspector(); ///< Replaces inspector after selection/component changes.
		void NewScene();		 ///< Resets renderer, selection and registry, then creates an editor camera.
		void CreateEntity(int shape); ///< Adds an empty entity or a default primitive and selects it.
		void RebuildRenderer();	  ///< Refreshes renderer light pointers and shader baking after structural
								  ///< changes.
		void UpdateDiagnostics(); ///< Samples completed profiler snapshots with explicit unavailable states.
		void OpenAssetDocument(Assets::AssetType type, const Core::UUID& id); ///< Opens or activates a stable asset tab.
		Project& mProject;		  ///< Borrowed client-owned session model.
		EntityEditor mInspector;  ///< Inspector builder.
		AssetLibraryViewer mAssets; ///< Filesystem/library builder.
		LoggerView mLogs;			///< Bounded file-log tail model.
		std::unique_ptr<EditorWorkspace> pWorkspace; ///< Owns native windows and tab transfer commands.
		std::vector<std::pair<Assets::AssetType, Core::UUID>> mPendingDocuments; ///< Asset-open requests from input.
		std::unique_ptr<Systems::RenderSystem>
			pRenderer;							 ///< Editor-owned renderer; never runs physics/scripts/audio.
		Rendering::GeometryPass mGeometry;		 ///< Borrowed by pRenderer for this object's lifetime.
		Assets::Shader* pShader = nullptr;		 ///< Library-owned preview shader.
		Assets::Material* pMaterial = nullptr;	 ///< Library-owned preview primitive material.
		entt::entity mSelected = entt::null;	 ///< Selected ECS handle; validated before editing.
		entt::entity mCamera = entt::null;		 ///< Protected editor preview camera handle.
		UI::WidgetId mModal = 0;				 ///< Active modal identity, or zero.
		UI::WidgetId mRoot = 0;					 ///< Attached workspace identity.
		UI::ContentWidget* pHierarchy = nullptr; ///< Borrowed persistent content region.
		UI::ScrollBox* pInspector = nullptr;	 ///< Borrowed persistent inspector viewport.
		UI::ContentWidget* pAssets = nullptr;	 ///< Borrowed persistent browser region.
		GameViewPort* pViewport = nullptr;		 ///< Borrowed scene surface.
		std::array<UI::Button*, 4>
			mGizmoButtons{}; ///< Borrowed Select/Move/Rotate/Scale buttons, synchronized with viewport hotkeys.
		UI::TextBlock* pDiagnostics = nullptr; ///< Borrowed profiler summary label.
		UI::TextBlock* pLogText = nullptr;	   ///< Borrowed log label.
		UI::TextBlock* pStatus = nullptr;	   ///< Borrowed bottom status label.
		UI::TextBlock* pSceneTitle = nullptr;  ///< Borrowed active scene heading.
		bool mHierarchyDirty = true;		   ///< Pending hierarchy refresh.
		bool mInspectorDirty = true;		   ///< Pending inspector refresh.
		bool mAssetsDirty = true;			   ///< Pending asset refresh.
		bool mRendererDirty = false;		   ///< Pending light-list and pipeline refresh.
		float mDiagnosticTimer = 0;			   ///< Seconds until next bounded diagnostic refresh.
		std::string mFilter;				   ///< Hierarchy substring filter.
		std::string mNotice = "Ready / session changes are held in memory"; ///< Current action feedback.
	};
} // namespace Nuclear::Editor
