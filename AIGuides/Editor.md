# Retained editor

Nuclear.Editor uses `Nuclear::UI` throughout. Its startup sets `EnableImGui=false`;
no editor source includes or calls ImGui, and the old file dialog and `imgui.ini`
have been removed. The engine still builds its optional legacy ImGui backend for
samples and debug clients. This migration does not remove that separate facility.

## Workspace and ownership

The main editor and detached windows opt into `WindowDesc::CustomTitleBar`.
`EditorTitleBar` paints the original README radiation emblem on the left, followed
by File, Edit, View, Project, Window and Help in the main window. Document tabs sit
below the caption. Minimize, maximize/restore and close use geometric retained
buttons on the right. The emblem uses a UV region of the original wordmark stored
at `Assets/NuclearEngine/Editor/NuclearLogo.png`.

Dropdown commands reuse scene/project/selection/workspace actions. Edit offers
Undo and Redo, enabled when their respective scene-history stacks have entries.
Saving and project settings remain disabled because they are not implemented.
Hover switches open menus; outside click, Escape and focus loss dismiss. Left/Right switches
headers, Up/Down selects enabled commands, and Enter executes. Window / Manage
panels opens the active document's recovery menu.

The title bar borrows its native window and clears its physical-pixel caption
hit-test callback on removal. Empty caption space uses native dragging,
double-click maximize/restore and the system menu. Controls, menus, active popups,
disabled modal workspaces and captured gestures stay client input. The platform
owns resize edges, work-area maximization and native commands. Close follows the
existing event flow and resource teardown. Runtime/sample windows keep OS chrome
unless they opt in. No gameplay or networking state is affected.

[NuclearEditor](../Nuclear.Editor/include/NuclearEditor.h) owns the session
[Project](../Nuclear.Editor/include/Project.h) and the
[EditorUI](../Nuclear.Editor/include/EditorUI.h) controller. The compatibility
`Nuclear.Editor.h` header contains only the client include. Implementations live
in matching files; native projects explicitly register all new source files.

The viewport context owns the retained shell and dock surface. The creation toolbar
appears only while **Main viewport** is active and exposes project/scene creation,
an **Add** menu and VSync. Add offers **Empty Entity**, **Shapes > Box / Plane /
Sphere** and **Skybox**. It collapses on asset documents,
which reclaim its vertical space, and does not appear in detached asset windows.
A filtered hierarchy
selects entities; the inspector edits names, position, degree-valued rotation,
scale, mesh visibility/shadow flags and light properties. Numeric fields validate
finite values and commit on Enter. The inspector's **Add component** dropdown starts
collapsed, lists only missing components and closes after a component is added.
Callbacks resolve ECS handles again before editing. The protected editor camera
does not appear in the authoring hierarchy. Deletion clears selection before the
next inspector rebuild.

New-project creation is a modal form: disabling the workspace isolates pointer
and Tab input until Create or Cancel removes the modal. Panel rebuilds are
requested by callbacks and performed from Update. The shell persists across
selection changes, so text input is not rebuilt every frame. Scroll views keep
hierarchy entries, inspector properties, asset lists and diagnostics reachable.
The logical 1440x900 layout scales down on smaller windows; draggable splitters
control the main workspace proportions.

## Document tabs and docking

[EditorWorkspace](../Nuclear.Editor/include/EditorWorkspace.h) coordinates the
browser-style document strip above the toolbar and a separate retained dock layout
for every document. Each layout has five optional dock groups.
**Main viewport** is a pinned document. **Open / ...** actions in the Asset Browser
open texture, mesh and material detail documents; opening an asset again activates
its existing tab. Texture documents preview ready resources, mesh documents expose
geometry counts and an Add to scene action, and material documents show shader and
texture-set information. These are resource details, not shader/material authoring.

Hierarchy, Inspector, Scene settings, Asset Browser and Performance belong to **Main viewport**.
Asset viewers start with only their own central content and fill the available
workspace; they do not inherit viewport panels. Switching documents preserves each
layout, tool state and split proportions. Future asset-specific tools can be
registered with `EditorWorkspace::Add` using the owning document ID. Dock transfers
cannot move a tool into another document's layout.

Every asset tab has a nonempty display title. `AssetDocument::GetTitle` trims a
meaningful name and otherwise supplies **Unnamed Texture**, **Unnamed Mesh**,
**Unnamed Material**, or the corresponding asset type. The Asset Browser actions,
new viewer headings and native document captions use the same policy. Reopening
an existing document updates its tab title while keeping its UUID and contents.

Click a tab to switch, drag tabs along a header to reorder, and use each tab's **x**
to close it. Headers scroll with the wheel when tabs overflow; focused headers also
support Left/Right navigation. Inactive contents remain collapsed and keep their
widgets and control state. Switching away from the viewport cancels its input.
The pinned viewport cannot be closed or detached.

The viewport's tools start in separate dock groups. Drag a tool tab onto a dock
region within its owning document to group it with other tools. A drop
preview marks the destination. Empty groups collapse, and dragging the borders
resizes adjacent regions. Drag a tab outside the editor, or use the header's **window icon**
control, to create an independent native window. Its custom title bar moves the window;
its borders resize it. Tabs can be dragged back from a native window onto the main
layout. A detached browser document carries its entire layout and custom tools;
redocking it returns that workspace to the browser. In a detached header the window
icon returns the active tab to its default group.
Closing a native window redocks its contents and keeps the editor running.

The **Window** menu lists only the active document's tools. **Reset layout** restores
those tools and that document's split proportions, preserving the active document
and other documents' layouts/native windows. Switching documents closes an open
Window menu. Layouts and document history last for the session; disk layout
persistence is not implemented. Closed contents remain owned for reopening until
editor shutdown.

[EditorDockArea](../Nuclear.Editor/include/EditorDockArea.h) owns each attached tab
tree; [EditorTabStrip](../Nuclear.Editor/include/EditorTabStrip.h) owns only its header
metadata and gestures. Ownership-changing callbacks queue work for Update.
`PanelWidget::TakeChild` cancels subtree input, detaches the tree and preserves widget
IDs. The browser's outer dock area owns complete document layouts, and each
[EditorDockLayout](../Nuclear.Editor/include/EditorDockLayout.h) owns its document's
groups and splitters. [EditorFloatingWindow](../Nuclear.Editor/include/EditorFloatingWindow.h)
owns a native window, a separate UIContext/renderer and an additional swap chain on
the existing graphics device. Main-window modals disable input in every detached
window. Teardown clears callbacks before either context/controller can disappear.

`Client::OnAuxiliaryWindowEvent` receives secondary-window events before the engine
skips its normal main-window routing. `GraphicsModule::CreateWindowSwapChain`
supports the existing D3D11/D3D12/Vulkan factories; OpenGL tear-off is unavailable
and creation failure leaves the original panel in place. Only D3D11 has been run.
The floating renderer targets its own buffer and presents before the main editor's
retained UI draw. UI, swap chains and native windows are released before engine
graphics/platform shutdown. Per-monitor automatic DPI scaling remains unimplemented.

## Scene preview

[GameViewPort](../Nuclear.Editor/source/GameViewPort.cpp) samples the active
camera's offscreen color target directly through the retained draw list. Right-drag
within the surface to look; wheel moves the camera forward/backward. Click the
viewport to give it keyboard focus, then hold WASD to fly forward/left/back/right
relative to the camera. Movement uses elapsed frame time and the camera's configured
speed, with normalized diagonals. Releasing a key, Escape or losing viewport/window
focus stops movement; inspector text input and project modals do not move the camera.
WASD also works during right-button look, and pauses during transform drags. Frame Selected
places the camera six world units behind the selected entity along the current
view direction. Pointer capture ends on release/cancellation.

The controller owns a RenderSystem and GeometryPass, with a forward BlinnPhong
shader and a library-owned default material. New scenes contain an editor camera
and directional key light; the initial workspace also creates a box. The camera
uses an RGBA8 sRGB target matching the shader PSO and a separate depth target.
Rendering sets the GPU viewport to the physical scene-surface size, restores the
swap-chain target afterward, and lets the engine draw retained UI and present.
Resizing updates camera resources/projection without rebaking the UI renderer.

Structural component/deletion changes rebuild the renderer's borrowed light lists
before the next scene draw. ShaderPipeline rebaking releases previous GPU objects
and reflection while keeping variant addresses stable for mesh controllers.
Preview rendering has no shadow pass, physics simulation, audio playback or script
execution. Adding runtime-service components only creates authoring data. Additional
camera components start inactive without render targets.

## Add menu and skybox

[EditorAddMenu](../Nuclear.Editor/include/EditorAddMenu.h) is a retained popup
anchored to the scene toolbar's **Add** button. Click or hover **Shapes** for the
Box, Plane and Sphere submenu. Up/Down choose commands, Right/Enter open Shapes,
Left returns to the root menu, and Enter creates the selected entity. Escape,
outside click, focus loss, scene replacement and leaving Main viewport dismiss it.
The popup blocks underlying workspace clicks and removes itself before creation.

**Add / Skybox** creates a hierarchy entity with the editor-only
[SkyboxComponent](../Nuclear.Editor/include/SkyboxComponent.h). Adding it again
selects the existing background. The inspector exposes **Visible**; translation,
scale and gizmos are omitted because the sky surrounds the camera. Creation,
visibility, deletion and restoration participate in normal entity undo/redo.
Removing/hiding the sky restores the scene clear color. New scenes start without it.

[EditorSkybox](../Nuclear.Editor/include/EditorSkybox.h) owns a generated blue sky,
horizon and ground cubemap, so creation needs no external texture assets. GPU
creation is queued for Update. It reuses the engine
[Skybox](../Nuclear.Engine/include/Rendering/Skybox.h) renderer after scene geometry,
with the camera's actual offscreen color/depth formats and current camera constants,
including in scenes without meshes. Far-depth drawing preserves foreground geometry
and ignores camera translation. GPU resources are released before graphics shutdown;
ECS/history snapshots hold only the visibility marker. The generated background is
session authoring data; custom cubemap assignment and disk persistence are not provided.

## Scene settings

Open **Window / Scene settings** or **View / Scene settings** to show the
[SceneSettings](../Nuclear.Editor/include/SceneSettings.h) tool. It starts closed,
docks beside Inspector and uses the same float, redock, close/reopen and panel
recovery controls as the other viewport tools. Asset documents do not inherit it.

The panel edits the scene asset's display name and the preview camera's clear
color. Red, Green, Blue and Alpha are linear values in the inclusive range 0 to 1;
Enter commits a field, and invalid/nonfinite values leave scene data unchanged.
A swatch previews the color. Blank scene names become **Untitled Scene**. Changes
refresh the scene heading and settings controls outside input dispatch.

[EditorSceneState](../Nuclear.Editor/include/EditorSceneState.h) captures these
settings separately from entities, using the borrowed active scene identity and
resolving the current camera on restore. Scene settings share the same 128-action
undo/redo order as entity edits and preserve entity selection during replay.
Snapshots from another scene are rejected. New scenes/projects restore the
default name and clear color, including in an open detached settings panel;
project modals disable its input. These settings are held in memory; the existing
scene binary format and disk persistence remain unchanged.

## Transform gizmos

Select an entity in the hierarchy, then use **Select**, **Move**, **Rotate** or
**Scale** in the bar above the viewport, alongside **Frame selected** and
**Delete selected**. Select hides and disables transform handles while retaining
the selected entity and its inspector; switching to it cancels any pending drag.
Q selects Select; G/E/R choose Move/Rotate/Scale while the viewport has keyboard
focus. W is reserved for camera movement. Typing in an inspector field does not
change modes. Red, green and blue handles
represent world X/Y/Z. Left-drag an arrow to move along an axis, a ring to rotate
around an axis, or an endpoint box to scale one component. The center square moves
in the camera-facing plane in Move mode and scales all components in Scale mode.
Scale factors stay positive and preserve existing mirrored signs. Handles use
logical UI units so their size follows workspace UI scaling rather than object size.

[TransformGizmo](../Nuclear.Editor/include/TransformGizmo.h) owns only drag math and
snapshots. [GameViewPort](../Nuclear.Editor/source/GameViewPort.cpp) validates the
selected ECS handle on every event, owns pointer capture and paints clipped retained
UI primitives over the scene. It uses the camera's actual `mCameraData.View`, not
the existing `GetViewMatrix()` accessor that returns Model. Axis movement uses
closest ray/axis points; free movement and rotation use ray/plane intersections.
Nearly edge-on rotation rings use a screen-tangent fallback. No ImGui, ImGuizmo,
third-party dependency or engine rendering API change is involved.

Dragging updates the live ECS transform and inspector. Releasing the left button,
even outside the viewport, commits. Escape, focus loss, selection/mode changes or
viewport geometry changes cancel and restore the starting position, quaternion and
scale. Deleted targets discard drag state without accessing freed components.
Right-button camera look and wheel movement are reserved from active gizmo drags.
The first version supports one selected entity and world axes; snapping, local-axis
mode and selecting meshes by clicking the scene are not implemented.

## Undo and redo

[EditorHistory](../Nuclear.Editor/include/EditorHistory.h) owns up to 128 scene
authoring transactions. Use **Edit / Undo**, **Edit / Redo**, **Ctrl+Z**, **Ctrl+Y**
or **Ctrl+Shift+Z**. Shortcuts work in the main workspace and detached windows;
focused text boxes retain their uncommitted text and do not invoke scene history.
Project modals block history actions. Native focus changes reset held modifiers,
and repeated key-down events do not replay additional steps.

History covers toolbar entity/primitive creation, mesh instantiation from the
browser and asset documents, entity deletion, committed inspector names and
transforms, light properties, mesh visibility/shadow flags, kinematic flags and
component addition, scene names and clear color. One released gizmo drag is one transaction, regardless of how
many live updates occurred. Cancellation adds nothing. Replaying history first
cancels a pending viewport gesture, then restores selection, hierarchy, inspector
and renderer bindings. Invalid numeric input and unchanged values add no entries.
A new committed edit after Undo drops Redo; no-op edits preserve it.

[EditorEntityState](../Nuclear.Editor/include/EditorEntityState.h) stores authored
values and borrowed library mesh/material references, restoring deleted entities
at their original versioned ECS identity so earlier commands still resolve. The
editor's protected camera is excluded from editing. Runtime-service components
remain uninitialized authoring data; inactive authoring cameras are recreated
without render targets. Runtime actors, audio handles and script instances are
never captured. Future editable fields/components must extend these snapshots.

New scene, successful project creation and shutdown clear both history directions.
Canceling project creation preserves history. Initial scene setup does not enter
history. History lasts only for this editor session; asset importing, filesystem
operations, camera navigation, document/dock layouts and project replacement are
outside scene history.

## Assets and diagnostics

[AssetLibraryViewer](../Nuclear.Editor/source/AssetLibraryViewer.cpp) has Files,
Textures, Meshes, Materials and Scenes categories. The filesystem browser supports
parent navigation and an editable directory path, and reports filesystem errors.
Explicit Import actions queue work for Update. Texture, geometry, shader and TTF
imports use existing importers; model imports deliberately omit materials and
animations and use the editor default material when instantiated. Mesh GPU creation
still uses the importer's main-thread task queue, which the editor drains after
synchronous decoding. Readiness is checked separately from decoding; failed texture
decoding must not report the fallback texture as a successful import. Library mesh
cards can create scene entities, and created textures display thumbnails/dimensions.
Library assets are borrowed, never deleted by a panel.

The synchronous texture importer preserves the Created state assigned after a
successful GPU upload. It must not downgrade an uploaded resource to Loaded:
retained image brushes deliberately require Created before sampling the view.

The retained performance panel reads ProfilingModule snapshots: capture/history,
completed CPU/GPU timing, inclusive CPU scopes, pipeline counts, process/GPU memory,
explicit allocation categories and dropped/invalid results. Unavailable and pending
values stay explicit. Diagnostics refresh twice per second. LoggerView reads only
the latest 16 KiB of `NuclearEngine.log`; it does not change the log or add a sink.

## Launch and validation

Add-menu/skybox validation on 2026-11-08: Debug x64 editor and focused editor-test
builds passed. The D3D11 integration run covers pointer and keyboard creation,
Shapes navigation/hover, dismissal, primitive creation, generated sky pixels,
foreground geometry occlusion, camera-translation invariance, duplicate Add,
skybox visibility/deletion undo/redo and new-scene cleanup. Add-menu and skybox
framebuffer readbacks were visually reviewed. The existing history, settings,
workspace/native-window and teardown suite passed in the same run. Release,
other graphics backends, physical pointer feel and custom cubemap import were
not validated by this change.

Scene settings validation on 2026-11-08: Debug x64 editor and focused editor-test
builds passed. The D3D11 integration run covers opening/reopening the panel, scene
name and RGBA commits, invalid/nonfinite/range rejection, interleaved scene/entity
history, preserved selection, no-op redo retention, native-window edits/shortcuts,
modal isolation, scene identity rejection/reset and teardown with settings floated.
A staging readback confirmed the edited clear color in the actual camera target.
The existing workspace/gizmo/import/readback suite passed in the same run. Release,
other graphics backends and physical input feel were not checked by this change.

Undo/redo validation on 2026-11-08: Debug x64 editor, editor-test and UI-test
builds passed. The editor D3D11 run covers menu/keyboard replay, primitive and
imported-mesh creation, deletion and original identity restoration, numeric/name
edits, component addition, gizmo transaction/cancellation, pending-drag undo,
auto-repeat suppression, text/modal isolation, detached-window shortcuts, scene
and project resets, and workspace teardown/reload. Focused history checks also
cover all supported component presence/data, redo branching, no-op preservation,
slot reuse and the 128-entry bound. Headless UI preview-filter checks and the
60-frame D3D11 UI run with ImGui passed. These are automated input checks; manual
input feel, Release and other graphics backends were not validated by this change.

Build with [BuildAndValidation.md](BuildAndValidation.md). Launch
`../Bin/DebugX64/Nuclear.Editor.exe` from `Nuclear.Editor/` so `../Assets/` resolves.
No `EditorClient.dll`, sample model library, audio device or managed runtime is
required by this editor startup configuration. Graphics/UI/rendering/fallbacks
remain enabled. Projects and scenes are held in memory; disk project persistence,
disk layout persistence, play mode and shader/material authoring remain absent.

The focused `Nuclear.Editor/Nuclear.Editor.Tests.vcxproj` project is separate from
the solution default build. Build it with Debug x64 MSBuild using the normalized
environment described in the build guide, then run
`../Bin/DebugX64/Nuclear.Editor.Tests.exe` from `Nuclear.Editor/`. It drives retained
input/actions and renders to a hidden D3D11 window. Readbacks are written under
ignored `Nuclear.Editor/Build/DebugX64/`. The UI test project additionally covers
Unicode editing/deletion, self-removal on commit, focused-content replacement,
scroll bubbling, focus reveal and resize clamping.

Validation on 2026-11-08: the full Debug x64 solution build, focused editor tests,
and headless UI tests passed. D3D11 editor readbacks at 1440x900 and 960x600 verified
scene geometry separately from UI text and were visually reviewed. Retained input
checks covered naming, numeric position edits, component addition, project modal
focus/cancel/create, deletion, profiler controls and OBJ import/instantiation.
Teardown/reload passed, and the actual editor executable reached rendering and
closed normally with exit code 0 in a hidden-window launch. The existing 60-frame
UI GPU test also passed with the legacy overlay enabled. See the build guide for
explicit validation limits; a full Release build and other backends were not run.

Gizmo validation on 2026-11-08: Debug editor and editor-test builds passed. The
focused tests cover all three world axes in Move/Rotate/Scale, free move, uniform
and mirrored scaling, edge-on rotation, default zero-quaternion handling, hidden
pivots, scaled input, Escape/focus/Tab/resize cancellation and deleted targets.
D3D11 checks drag each mode while the inspector rebuilds, verify outside release
commit and camera navigation, and save move/rotation/scale readbacks for visual
review. These are automated logical-input checks, not physical mouse-feel or
other-backend validation.

Viewport navigation validation on 2026-11-08: the Debug editor and focused test
project passed after adding WASD flight and Select. Tests cover all four camera
directions, key release, equal movement across frame subdivisions, normalized
diagonals, opposing keys, focus/window cancellation, inspector input isolation,
movement during right-button look and suspension during transform drags. Select
is checked for hidden/inactive handles, pending-edit rollback and toolbar/keyboard
activation. D3D11 readbacks cover Select and the toolbar at both workspace sizes.
Physical keyboard/mouse feel, Release and other backends remain unverified.

Docking validation on 2026-11-08: the full Debug x64 solution build and focused
editor tests passed. Checks cover texture/mesh/material documents, duplicate-tab
reuse, close/reopen, tab reorder, dock grouping with preserved widget identities
and text, split resizing, native tear-off, independent framebuffer rendering/text
input/resize, cross-window drag redocking, close-to-redock, Window-menu recovery,
layout reset, modal isolation and teardown/reload with a native panel still open.
Readbacks of the main workspace, mesh/texture documents and floating panel were
visually reviewed. Created-mesh counts use retained GPU buffer metadata after CPU
arrays are released. The actual Debug editor reached rendering and closed normally
with exit code 0 in a hidden launch. Headless UI ownership-transfer checks and the
60-frame D3D11 UI test with ImGui also passed; platform coordinate smoke checks
passed in Debug and Release. These are automated Windows/D3D11 checks, not a full
Release engine build, other graphics backends, physical drag feel or per-monitor DPI.

Document isolation validation on 2026-11-08: Debug editor and focused editor-test
builds passed. D3D11 checks cover full-width asset viewers without viewport tools,
document-owned custom panels and Window menus, scoped docking/reset, preserving
viewport grouping/control state across tab switches, and moving a complete asset
workspace through a native window. Empty/whitespace names for textures, meshes and
materials produce typed titles, including reopening existing document identities.
Existing viewport/gizmo, import, modal, native-window and teardown/reload checks
remain in the same passing integration run. Asset-viewer readbacks were reviewed.

Custom chrome validation on 2026-11-08: focused Debug editor, editor-test and UI-test
builds passed. D3D11 checks cover title/menu/control layout, native drag regions,
scaled hit testing, keyboard/hover menu switching, outside/Escape/focus dismissal,
pointer command activation, maximize/restore, detached caption renaming and the
existing docking/modal/teardown suite. Main-window, dropdown, small-workspace and
floating-window framebuffer readbacks were visually reviewed. Debug/Release platform
smoke checks passed for caption/resize hit tests, double-click maximize/restore,
client double-click presses, work-area maximization, minimize and deferred close.
Headless UI checks and the D3D11 UI GPU run with ImGui passed. The actual Debug
editor reached rendering and closed normally in a hidden launch. These focused
checks do not establish a full Release editor build, physical drag feel, automatic
per-monitor DPI or other graphics backends.
