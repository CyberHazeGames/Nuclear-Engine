#pragma once
#include <Common.h>
#include <Project.h>
#include <EntityEditor.h>
#include <AssetLibraryViewer.h>
#include <Systems/RenderSystem.h>
#include <Rendering/RenderPasses/GeometryPass.h>
#include <LoggerView.h>
#include <array>
#include <EditorWorkspace.h>
#include <EditorHistory.h>
#include <optional>
namespace Nuclear::Editor
{
	class GameViewPort;
	class EditorTitleBar;
	class SceneSettings;
	class EditorSkybox;
	class EditorSelectionRenderer;
	class MeshPreviewViewport;
	class MeshPlacementPreview;
	/// Retained editor workspace. Owns session tools; viewport context owns all widgets.
	class EditorUI
	{
	  public:
		explicit EditorUI(Project& project); ///< Borrows the client's session project.
		~EditorUI(); ///< Destroys controller-owned resources; Shutdown releases them before graphics teardown.
		void Initialize();					 ///< Creates a default session scene and attaches the workspace.
		void Update(float deltaTime); ///< Applies requested panel changes, imports and diagnostic sampling.
		void Render(float deltaTime); ///< Renders the scene to its offscreen camera target.
		void Resize(int width, int height); ///< Sets physical viewport dimensions and the main window's monitor DPI scale.
		void Shutdown(); ///< Drops borrowed widgets and clears scene resources before client destruction.
		void OnAuxiliaryWindowEvent(const Platform::Event& event); ///< Routes detached-window input/resize/close.
		EditorWorkspace* GetWorkspace() const { return pWorkspace.get(); } ///< Borrowed workspace until Shutdown.
	  private:
		void BuildWorkspace();	 ///< Attaches the persistent shell and tool regions.
		void RefreshHierarchy(); ///< Replaces hierarchy content outside dispatch.
		void OpenHierarchyMenu(entt::entity entity, UI::UIContext& context, UI::Vec2 point); ///< Selects a live row and opens commands in its owning context.
		void CloseHierarchyMenu(); ///< Dismisses through a still-live context, including after native-window removal.
		UI::UIContext* GetHierarchyMenuContext() const; ///< Resolves the borrowed popup context against currently owned windows; null if gone.
		void DeleteEntity(entt::entity entity); ///< Deletes a live authored entity as one history action; protects the preview camera.
		void CopyEntity(entt::entity entity); ///< Stores a session snapshot without adding scene history.
		void DuplicateEntity(entt::entity entity); ///< Creates a unique named copy from the current authored values.
		void PasteEntity(const EditorEntityState& state); ///< Creates/selects a snapshot copy and records one undoable creation.
		void RefreshInspector(); ///< Replaces inspector after selection/component changes.
		void PickEntity(); ///< Applies a queued visible-mesh pick, synchronizing hierarchy and inspector without history edits.
		void NewScene();		 ///< Resets renderer, selection and registry, then creates an editor camera.
		void CreateEntity(int shape); ///< Adds an empty entity or a default primitive and selects it.
		void CreateSkybox(); ///< Creates or selects the scene's skybox; GPU creation runs from Update.
		void OpenAddMenu(UI::Rect anchor); ///< Opens the retained creation menu below the toolbar button.
		void CloseAddMenu(); ///< Safely dismisses the creation popup through the main context.
		void RebuildRenderer();	  ///< Refreshes renderer light pointers and shader baking after structural
								  ///< changes.
		/// Assigns the editor PBR shader and missing-map defaults to a successfully imported mesh material.
		/// Runs on the graphics thread after texture/mesh creation, preserving submesh texture-set indices.
		void PrepareImportedMaterial(Assets::Mesh& mesh);
		void UpdateDiagnostics(); ///< Samples completed profiler snapshots with explicit unavailable states.
		void OpenAssetDocument(Assets::AssetType type, const Core::UUID& id); ///< Opens or activates a stable asset tab.
		void ReplayHistory(bool redo); ///< Cancels live gestures, then replays one scene action.
		void DragMesh(const Core::UUID& mesh, UI::UIContext& source, UI::Vec2 point, bool dropped);
		///< Maps a captured asset drag between native contexts and queues viewport placement outside dispatch.
		void UpdateMeshPlacement(); ///< Resolves depth/ground placement and commits one entity/history action on release.
		void CancelMeshPlacement(); ///< Discards the transient ghost and any queued drop.
		bool ProcessShortcut(const Platform::Event& event, UI::UIContext& context); ///< Tracks modifiers and consumes scene-history keys outside text input/modals.
		Project& mProject;		  ///< Borrowed client-owned session model.
		EntityEditor mInspector;  ///< Inspector builder.
		EditorHistory mHistory; ///< Session scene history; cleared before scene replacement/shutdown.
		std::optional<EditorEntityState> mCopiedEntity; ///< Session clipboard; cleared before scene replacement or asset shutdown.
		UI::WidgetId mHierarchyMenu = 0; ///< Context-owned hierarchy popup identity, or zero while closed.
		UI::UIContext* pHierarchyMenuContext = nullptr; ///< Borrowed identity; resolve against live windows before dereferencing.
		entt::entity mHierarchyMenuEntity = entt::null; ///< Versioned command target, revalidated before execution.
		std::array<bool, 4> mHistoryModifiers{}; ///< Left/right Ctrl and Shift; cleared on native focus changes.
		AssetLibraryViewer mAssets; ///< Filesystem/library builder.
		std::map<std::string, UI::TextBlock*> mAssetHeadings; ///< Borrowed document headings; workspace retains closed trees until Shutdown.
		std::map<std::string, MeshPreviewViewport*> mMeshPreviews; ///< Borrowed mesh surfaces; document trees own GPU/input lifetime until Shutdown.
		std::unique_ptr<MeshPlacementPreview> pMeshPlacement; ///< Owns transient ghost rendering and contact readback resources.
		std::optional<UI::Vec2> mMeshPlacementPoint; ///< Latest logical point in the scene viewport's context.
		UI::Rect mMeshPlacementBounds; ///< Target geometry when the latest pointer sample was converted.
		UI::UIContext* pMeshPlacementContext = nullptr; ///< Borrowed target identity; compared with the current viewport context before use.
		bool mMeshPlacementDropped = false; ///< Release queued for the next Update.
		LoggerView mLogs;			///< Bounded file-log tail model.
		std::unique_ptr<EditorWorkspace> pWorkspace; ///< Owns native windows and tab transfer commands.
		std::unique_ptr<EditorSkybox> pSkybox; ///< Owns generated sky resources until Shutdown; components hold authoring data only.
		std::unique_ptr<EditorSelectionRenderer> pSelectionRenderer; ///< Owns mesh ID/outline resources until graphics teardown.
		std::optional<UI::Vec2> mPendingPick; ///< Logical Select-mode click queued for the next Update, or empty.
		UI::Rect mPickBounds; ///< Viewport allocation at the queued press; stale geometry cancels the pick.
		bool mRevealSelection = false; ///< Scrolls the next selected hierarchy row into view after a viewport pick.
		UI::WidgetId mAddMenu = 0; ///< Context-owned creation popup identity, or zero.
		bool mPendingSkybox = false; ///< Queued skybox creation from menu input.
		std::vector<std::pair<Assets::AssetType, Core::UUID>> mPendingDocuments; ///< Asset-open requests from input.
		std::unique_ptr<Systems::RenderSystem>
			pRenderer;							 ///< Editor-owned renderer; never runs physics/scripts/audio.
		Rendering::GeometryPass mGeometry;		 ///< Borrowed by pRenderer for this object's lifetime.
		Assets::Shader* pShader = nullptr;		 ///< Library-owned preview shader.
		Assets::Shader* pImportedShader = nullptr; ///< Library-owned static forward PBR shader for imported meshes.
		Assets::Material* pMaterial = nullptr;	 ///< Library-owned preview primitive material.
		entt::entity mSelected = entt::null;	 ///< Selected ECS handle; validated before editing.
		entt::entity mCamera = entt::null;		 ///< Protected editor preview camera handle.
		UI::WidgetId mModal = 0;				 ///< Active modal identity, or zero.
		UI::WidgetId mRoot = 0;					 ///< Attached workspace identity.
		UI::ScrollBox* pHierarchy = nullptr; ///< Borrowed hierarchy scroll viewport, including while its panel is closed.
		UI::TextBox* pHierarchyFilter = nullptr; ///< Context-owned filter, cleared when it hides a viewport selection.
		UI::ScrollBox* pInspector = nullptr;	 ///< Borrowed persistent inspector viewport.
		UI::ContentWidget* pAssets = nullptr;	 ///< Borrowed persistent browser region.
		GameViewPort* pViewport = nullptr;		 ///< Borrowed scene surface.
		UI::Widget* pSceneToolbar = nullptr; ///< Context-owned creation row; collapsed outside Main viewport.
		SceneSettings* pSceneSettings = nullptr; ///< Workspace-owned scene settings, valid while attached or parked.
		bool mSceneSettingsDirty = true; ///< Refresh settings after scene replacement, commit or scene-history replay.
		EditorTitleBar* pTitleBar = nullptr; ///< Context-owned main title bar, valid until Shutdown.
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
