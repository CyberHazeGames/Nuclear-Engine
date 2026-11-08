# Retained editor

Nuclear.Editor uses `Nuclear::UI` throughout. Its startup sets `EnableImGui=false`;
no editor source includes or calls ImGui, and the old file dialog and `imgui.ini`
have been removed. The engine still builds its optional legacy ImGui backend for
samples and debug clients. This migration does not remove that separate facility.

## Workspace and ownership

[NuclearEditor](../Nuclear.Editor/include/NuclearEditor.h) owns the session
[Project](../Nuclear.Editor/include/Project.h) and the
[EditorUI](../Nuclear.Editor/include/EditorUI.h) controller. The compatibility
`Nuclear.Editor.h` header contains only the client include. Implementations live
in matching files; native projects explicitly register all new source files.

The viewport context owns the retained shell and dock surface. The toolbar exposes
project/scene creation, empty entities, box/sphere/plane creation and VSync. A filtered hierarchy
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

Hierarchy, Inspector, Asset Browser and Performance belong to **Main viewport**.
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
control, to create an independent native window. Its OS title bar moves the window;
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
mode, undo history and selecting meshes by clicking the scene are not implemented.

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

Build with [BuildAndValidation.md](BuildAndValidation.md). Launch
`../Bin/DebugX64/Nuclear.Editor.exe` from `Nuclear.Editor/` so `../Assets/` resolves.
No `EditorClient.dll`, sample model library, audio device or managed runtime is
required by this editor startup configuration. Graphics/UI/rendering/fallbacks
remain enabled. Projects and scenes are held in memory; disk project persistence,
undo/redo, disk layout persistence, play mode and shader/material authoring remain absent.

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
