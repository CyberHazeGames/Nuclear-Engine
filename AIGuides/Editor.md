# Retained editor

Nuclear.Editor uses `Nuclear::UI` throughout. Its startup sets `EnableImGui=false`;
no editor source includes or calls ImGui. File selection uses the independent
platform's native picker; the old ImGui dialog and `imgui.ini` have been removed.
The engine still builds its optional legacy ImGui backend for
samples and debug clients. This migration does not remove that separate facility.

The editor and its focused test executable embed `Editor.manifest` with
PerMonitorV2 DPI awareness. Main and detached contexts use their native window's
`GetDpiScale()`; resizing a window preserves the monitor's text size instead of
shrinking the entire workspace to a 1440x900 design. `EditorToolbar` wraps viewport
actions onto additional rows at narrow widths. Bottom tools keep at least 180
logical units (capped at 60% of workspace height) so full-size controls retain a
usable scroll viewport. Moving between monitors applies
the OS-suggested bounds through the platform's DPI-change resize event. The shared
SDF shader uses a one-pixel antialiasing band to keep small letter edges sharper.

## Workspace and ownership

The main editor and detached windows opt into `WindowDesc::CustomTitleBar`.
`EditorTitleBar` paints the original README radiation emblem on the left, followed
by File, Edit, View, Project, Window and Help in the main window. Document tabs sit
below the caption. Minimize, maximize/restore and close use geometric retained
buttons on the right. The emblem uses a UV region of the original wordmark stored
at `Assets/NuclearEngine/Editor/NuclearLogo.png`.

Dropdown commands reuse scene/project/selection/workspace actions. Edit offers
Undo, Redo, Duplicate, Copy, Paste, Delete selected, and Editor Settings... (also accessible from Window / Editor Settings...).
Saving and project settings remain disabled because they are not implemented.
Hover switches open menus; outside click, Escape and focus loss dismiss. Left/Right switches
headers, Up/Down selects enabled commands, and Enter executes. Window / Manage
panels opens the active document's recovery menu. Edit / Editor Settings... opens
[EditorSettingsDocument](../Nuclear.Editor/include/Documents/EditorSettingsDocument.h) featuring a two-pane layout
with a left vertical navigation menu and a right main view for settings pages (Theme, General, Viewport).
The Theme page exposes a clickable accent-color swatch and presets that control
the editor's live accent color via [EditorSettings](../Nuclear.Editor/include/Settings/EditorSettings.h); the General
page provides VSync swap chain presentation controls, DPI scaling, and session settings.
Help / About Nuclear Engine opens the About document showing engine and editor build information, repository
link with a browser launcher, and engine and third-party license notices.

The title bar borrows its native window and clears its physical-pixel caption
hit-test callback on removal. Empty caption space uses native dragging,
double-click maximize/restore and the system menu. Controls, menus, active popups,
disabled modal workspaces and captured gestures stay client input. The platform
owns resize edges, work-area maximization and native commands. Close follows the
existing event flow and resource teardown. Runtime/sample windows keep OS chrome
unless they opt in. No gameplay or networking state is affected.

[NuclearEditor](../Nuclear.Editor/include/Core/NuclearEditor.h) owns the session
[Project](../Nuclear.Editor/include/Core/Project.h) and the
[EditorUI](../Nuclear.Editor/include/Panels/EditorUI.h) controller. The compatibility
`Nuclear.Editor.h` header contains only the client include. Implementations live
in matching files; native projects explicitly register all new source files.

The viewport context owns the retained shell and dock surface. The creation toolbar
appears only while **Main viewport** is active and exposes project/scene creation and
an **Add** menu. Add offers **Empty Entity**, **Shapes > Box / Plane /
Sphere** and **Skybox**. It collapses on asset documents,
which reclaim its vertical space, and does not appear in detached asset windows.
A filtered hierarchy
selects entities; the inspector edits names, position, degree-valued rotation,
scale, mesh visibility/shadow flags and light properties (including a segmented
radio button selecting Directional, Point, or Spot light type). Numeric fields validate
finite values and commit on Enter. The inspector's **Add component** dropdown starts
collapsed, lists only missing components and closes after a component is added.
Callbacks resolve ECS handles again before editing. The protected editor camera
does not appear in the authoring hierarchy. Deletion clears selection before the
next inspector rebuild.

Right-click a hierarchy entity for **Duplicate**, **Copy** or **Delete**. The row
under the pointer becomes selected before commands execute; each command resolves
its versioned entity handle again. Duplicate creates a fresh entity at the same
transform with a unique **Copy** name. Copy captures a session snapshot without
changing scene history; **Ctrl+C**, **Ctrl+V** and **Edit / Paste** use this clipboard
outside focused text inputs. New scenes/projects and shutdown clear the entity
clipboard. This clipboard is separate from the platform text clipboard.

Duplicate and Paste preserve the supported authored values from
`EditorEntityState`, including mesh/material references, transform, mesh flags,
light values and supported component presence. Runtime physics/audio/script
instances and preview camera targets are excluded, matching undo snapshots.
Duplicate, Paste and Delete each enter scene history as one action. The protected
editor camera is excluded from hierarchy commands. Copy remains usable after
the source is changed or deleted within the same scene.

[EditorHierarchyRow](../Nuclear.Editor/include/Widgets/EditorHierarchyRow.h) reuses
the context-owned EditorMenuPopup in main or detached hierarchy windows. The
popup survives selection-driven row rebuilding. Outside click, Escape, focus
loss, scene replacement, invalid targets or native-context removal dismiss it;
context pointers are checked against live windows before access.

Every inspector component section has a small disclosure arrow before its name
and an **X** at the right. The arrow points down when expanded and right when
collapsed. Disclosure hides only the property body; it preserves component data,
does not add history entries, and persists per versioned entity/component through
inspector rebuilds, selection changes and undo/redo. New scenes/projects and
shutdown clear these disclosure states. Header controls retain normal pointer and
keyboard activation, including when the Inspector is detached.

**X** removes an optional component through the existing entity history, requests
new renderer bindings and returns that type to the **Add component** menu. It also
works while the section is collapsed. Undo restores supported authored values;
runtime services remain inactive. Transform belongs to the required
`EntityInfoComponent`, so its **X** is disabled and its body explains that it is
required. The protected preview camera stays outside the authoring inspector.
[EditorComponentButton](../Nuclear.Editor/include/Widgets/EditorComponentButton.h) paints
font-independent arrow/cross glyphs; [EntityEditor](../Nuclear.Editor/source/Panels/EntityEditor.cpp)
owns section construction and disclosure state.

The **Mesh** component has separate **Mesh** and **Material** asset fields. Each shows
the current name (or **None**), a 64-unit thumbnail and a **...** button.
[EditorAssetProperty](../Nuclear.Editor/include/Widgets/EditorAssetProperty.h) opens
[EditorAssetPicker](../Nuclear.Editor/include/Widgets/EditorAssetPicker.h) in the property's
current UI context, including detached inspectors. The dropdown offers **Clear**,
case-insensitive name search and a scrollable list from the matching AssetLibrary map.
Uncreated entries are disabled; a choice resolves the UUID again before assigning it.
Changing either field preserves the other, records one undoable entity edit and
requests renderer rebaking. Unmatched mesh texture-set indices use the chosen
material's first set. Clearing either reference stops mesh drawing.

Escape, outside click and native focus loss dismiss the chooser; rebuilding or
removing its owning inspector removes it. The dropdown clamps to its context in
logical units and can open above the field when space below is limited.
[MeshPreviewCache](../Nuclear.Editor/include/Rendering/MeshPreviewCache.h) owns the
cached GPU thumbnails until scene reset/shutdown. Mesh thumbnails use staging readback
to frame geometry after CPU upload data is released. Material thumbnails use the same
private forward shader and studio-lit sphere as the material viewer, with the first
texture set and defaults for missing standard maps. They are cached after rendering.

New-project creation is a modal form: disabling the workspace isolates pointer
and Tab input until Create or Cancel removes the modal. Panel rebuilds are
requested by callbacks and performed from Update. The shell persists across
selection changes, so text input is not rebuilt every frame. Scroll views keep
hierarchy entries, inspector properties, asset lists and diagnostics reachable.
The workspace keeps monitor-scaled text at smaller window sizes; draggable
splitters control the main workspace proportions.

## Document tabs and docking

[EditorWorkspace](../Nuclear.Editor/include/Workspace/EditorWorkspace.h) coordinates the
browser-style document strip above the toolbar and a separate retained dock layout
for every document. Each layout has five optional dock groups.
**Main viewport** is a pinned document. In the Asset Browser, selecting a texture, mesh or material thumbnail
tile opens its detail document; opening an asset again
activates its existing tab. Texture documents preview ready resources, mesh documents expose
geometry counts, a flyable mesh preview and an Add to scene action, and material documents show a flyable
material sphere with shader/texture-set information. These are resource details, not shader/material authoring.

All three viewers use [AssetViewerLayout](../Nuclear.Editor/include/Widgets/AssetViewerLayout.h):
the preview fills the left side at full document height and a scrollable **Properties**
panel fills the right. Drag the vertical divider to resize them; Escape/focus loss
cancels an unfinished drag. The split remains with the document through tab switches
and native transfer. [TexturePreviewViewport](../Nuclear.Editor/include/Widgets/TexturePreviewViewport.h)
fits textures to the left pane without stretching and shows transparency over a checkerboard.

Material documents render the built-in sphere with
[MaterialPreviewRenderer](../Nuclear.Editor/include/Rendering/MaterialPreviewRenderer.h),
which owns a copy of the source shader's forward build description, private camera/light
buffers and private texture bindings. Two directional studio lights illuminate the
sphere against the generated sky. The library shader, material sets, authored entities and main
camera stay untouched. **Frame sphere** restores the camera; **Preview set / ...** buttons
choose among source texture sets. Static forward material shaders are supported; deferred-only
materials, scene shadows, IBL variants and animated material/mesh variants are excluded.

Ready mesh documents include a private lit preview with the generated default
skybox. Click the preview and use WASD to fly, right-drag to look, wheel to travel,
and F or **Frame mesh** to frame the asset. Escape/focus loss stops navigation.
Each document keeps its own camera through tab switches and native docking; no
preview entities or camera are registered in the authored scene. Mesh bounds are
measured once, using staging vertex-buffer readback when upload released CPU data.
Movement speed and clipping planes follow mesh size. The neutral diffuse preview
shows static geometry rather than imported materials or animated poses.

[MeshPreviewViewport](../Nuclear.Editor/include/Widgets/MeshPreviewViewport.h) owns
[MeshPreviewScene](../Nuclear.Editor/include/Rendering/MeshPreviewScene.h). The controller
updates/renders visible surfaces before main/detached UI painting, using each
context's DPI. Closed documents retain their resources until workspace shutdown;
their input and rendering stay inactive. Targets are capped at 4096 pixels per axis.

Hierarchy, Inspector, Scene settings, Asset Browser and Performance belong to **Main viewport**.
Asset viewers fill the available workspace with their own preview/properties split;
they do not inherit viewport panels. Switching documents preserves each
layout, tool state and split proportions. Future asset-specific tools can be
registered with `EditorWorkspace::Add` using the owning document ID. Dock transfers
cannot move a tool into another document's layout.

Every asset tab has a nonempty display title. `AssetDocument::GetTitle` trims a
meaningful name and otherwise supplies **Unnamed Texture**, **Unnamed Mesh**,
**Unnamed Material**, or the corresponding asset type. The Asset Browser actions,
new viewer headings and native document captions use the same policy. Reopening
an existing document updates its tab title while keeping its UUID and contents.

Tab captions reserve space for **x** and use measured middle truncation when
needed, for example **WBP_Cente...ilityBar**. Both ends remain visible, UTF-8
characters stay intact, and text is clipped to its own label rectangle. Main,
tool and detached tab headers share this behavior; full titles and IDs remain
unchanged. `Nuclear.Editor.Tests.exe --tab-titles` checks the example, glyph
clipping, close activation, DPI scaling and UTF-8 preservation.

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

[EditorDockArea](../Nuclear.Editor/include/Workspace/EditorDockArea.h) owns each attached tab
tree; [EditorTabStrip](../Nuclear.Editor/include/Workspace/EditorTabStrip.h) owns only its header
metadata and gestures. Ownership-changing callbacks queue work for Update.
`PanelWidget::TakeChild` cancels subtree input, detaches the tree and preserves widget
IDs. The browser's outer dock area owns complete document layouts, and each
[EditorDockLayout](../Nuclear.Editor/include/Workspace/EditorDockLayout.h) owns its document's
groups and splitters. [EditorFloatingWindow](../Nuclear.Editor/include/Workspace/EditorFloatingWindow.h)
owns a native window, a separate UIContext/renderer and an additional swap chain on
the existing graphics device. Main-window modals disable input in every detached
window. Teardown clears callbacks before either context/controller can disappear.

`Client::OnAuxiliaryWindowEvent` receives secondary-window events before the engine
skips its normal main-window routing. `GraphicsModule::CreateWindowSwapChain`
supports the existing D3D11/D3D12/Vulkan factories; OpenGL tear-off is unavailable
and creation failure leaves the original panel in place. Only D3D11 has been run.
The floating renderer targets its own buffer and presents before the main editor's
retained UI draw. UI, swap chains and native windows are released before engine
graphics/platform shutdown. Each window's context follows its own monitor DPI;
tab drags convert between logical and physical coordinates using both contexts.

## Scene preview

[GameViewPort](../Nuclear.Editor/source/Panels/GameViewPort.cpp) samples the active
camera's offscreen color target directly through the retained draw list. Right-drag
within the surface to look; wheel moves the camera forward/backward. Click the
viewport to give it keyboard focus, then hold WASD to fly forward/left/back/right
relative to the camera. Movement uses elapsed frame time and the camera's configured
speed, with normalized diagonals. Releasing a key, Escape or losing viewport/window
focus stops movement; inspector text input and project modals do not move the camera.
WASD also works during right-button look, and pauses during transform drags. Frame Selected
places the camera six world units behind the selected entity along the current
view direction. Pointer capture ends on release/cancellation.

The controller owns a RenderSystem and GeometryPass, with forward BlinnPhong for
primitives and a static forward PBR shader for imported mesh materials. Both shaders
are registered again when scene lights or other structural changes rebuild the
renderer. The library owns shaders/materials. New scenes contain an editor camera
and directional key light; the initial workspace also creates a box. The camera
uses an RGBA8 sRGB target matching the shader PSO and a separate depth target.
Rendering sets the GPU viewport to the physical scene-surface size, restores the
swap-chain target afterward, and lets the engine draw retained UI and present.
Resizing updates camera resources/projection without rebaking the UI renderer.

In **Select** mode, left-click a visible mesh to select the frontmost entity.
The hierarchy highlights and reveals the same row, clearing a filter that would
hide it, and the inspector shows its properties. Clicking background clears
selection. Selection keeps viewport keyboard focus, changes no transforms and
does not create scene-history entries. Closed/detached hierarchy panels retain
the selected identity and reveal it when visible again.

[EditorSelectionRenderer](../Nuclear.Editor/include/Rendering/EditorSelectionRenderer.h)
owns a depth-tested R32_UINT entity-ID target, separate D32 depth, pipelines and
a one-pixel staging texture. It uses current mesh GPU buffers/transforms, including
imported meshes with released CPU arrays. Clicks queue a fresh ID render/readback
for Update, avoiding GPU work inside input dispatch. Resize/mode/modal changes
can invalidate a queued click. IDs map to live versioned ECS identities and skip
hidden/unready geometry; lights/empty entities and the sky background have no
pickable surface. The editor currently previews static opaque meshes; animated
deformation and alpha-cutout selection are not implemented.

After scene geometry/sky, the same visible IDs draw a yellow silhouette border
around the selected mesh, including hierarchy-selected meshes in transform modes.
Its width follows UI scale. It preserves interior colors, camera depth and other
meshes' pixels, and never draws a fully occluded selection through foreground
geometry. UI/gizmos render afterward. Resources are released before graphics
shutdown; selection adds no asset ownership or scene serialization fields.

Structural component/deletion changes rebuild the renderer's borrowed light lists
before the next scene draw. ShaderPipeline rebaking releases previous GPU objects
and reflection while keeping variant addresses stable for mesh controllers.
Preview rendering has no shadow pass, physics simulation, audio playback or script
execution. Adding runtime-service components only creates authoring data. Additional
camera components start inactive without render targets.

## Add menu and skybox

[EditorAddMenu](../Nuclear.Editor/include/Widgets/EditorAddMenu.h) is a retained popup
anchored to the scene toolbar's **Add** button. Click or hover **Shapes** for the
Box, Plane and Sphere submenu. Up/Down choose commands, Right/Enter open Shapes,
Left returns to the root menu, and Enter creates the selected entity. Escape,
outside click, focus loss, scene replacement and leaving Main viewport dismiss it.
The popup blocks underlying workspace clicks and removes itself before creation.

**Add / Skybox** creates a hierarchy entity with the editor-only
[SkyboxComponent](../Nuclear.Editor/include/Rendering/SkyboxComponent.h). Adding it again
selects the existing background. The inspector exposes **Visible**; translation,
scale and gizmos are omitted because the sky surrounds the camera. Creation,
visibility, deletion and restoration participate in normal entity undo/redo.
Removing/hiding the sky restores the scene clear color. New scenes start without it.

[EditorSkybox](../Nuclear.Editor/include/Rendering/EditorSkybox.h) owns a generated blue sky,
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
[SceneSettings](../Nuclear.Editor/include/Panels/SceneSettings.h) tool. It starts closed,
docks beside Inspector and uses the same float, redock, close/reopen and panel
recovery controls as the other viewport tools. Asset documents do not inherit it.

The panel edits the scene asset's display name and the preview camera's clear
color through a compact swatch that opens the external RGBA picker. Channels are linear values in the inclusive
range 0 to 1. Blank scene names become **Untitled Scene**. Changes
refresh the scene heading and settings controls outside input dispatch.

[EditorSceneState](../Nuclear.Editor/include/Settings/EditorSceneState.h) captures these
settings separately from entities, using the borrowed active scene identity and
resolving the current camera on restore. Scene settings share the same 128-action
undo/redo order as entity edits and preserve entity selection during replay.
Snapshots from another scene are rejected. New scenes/projects restore the
default name and clear color, including in an open detached settings panel;
project modals disable its input. These settings are held in memory; the existing
scene binary format and disk persistence remain unchanged.

## Color picker

[EditorColorProperty](../Nuclear.Editor/include/Widgets/EditorColorProperty.h) is a compact
swatch button; the full color editor never appears inline. Clicking the color
property (or activating it with the keyboard) requests a separate native window,
managed by the workspace-owned
[EditorColorPickerWindow](../Nuclear.Editor/include/Workspace/EditorColorPickerWindow.h).
Only one chooser is open at a time, independently from dockable panel windows.
It reuses EditorFloatingWindow's platform window, private context, renderer and
shared-device swap chain. Creation and destruction run outside input dispatch.

[EditorColorPicker](../Nuclear.Editor/include/Widgets/EditorColorPicker.h) is the owning
retained editor inside that window, with vec3 and vec4 constructors and copied callbacks.
Light color and theme accent use vec3; scene clear color uses vec4. The editor
contains [EditorColorWheel](../Nuclear.Editor/include/Widgets/EditorColorWheel.h),
a hue/saturation disk, a value strip, an optional alpha strip, and a checkerboard
preview. Numeric RGB(A), HSV and HEX controls stay synchronized. Hue uses degrees,
saturation and alpha use 0 to 1, and RGB/value use linear channels. HEX deliberately
encodes those linear channels as 8-bit #RRGGBB or #RRGGBBAA, with an optional hash
and case-insensitive input; it does not perform an sRGB conversion.

Enter validates text locally. Invalid, nonfinite or out-of-range values restore
the last valid text. Captured wheel/strip gestures preview within the chooser and
finish on release. **Apply** commits the resulting color to the property as one
scene/entity undo action and closes the chooser. **Cancel**, native close or
Escape outside a captured gesture close without changing the model. Escape during
a captured gesture restores its starting value; focus loss also cancels that gesture.
The chooser tracks a weak property lifetime and closes when the source is removed,
hidden, disabled or detached. New scenes, project modals and shutdown cancel it.
Arrow keys adjust focused wheel hue/saturation; numeric fields support keyboard
entry for all channels. Achromatic colors retain their chosen hue, and black
retains saturation, allowing value to rise back to the selected color.

The light picker preserves the existing 0 to 100000 HDR RGB/value range; the value
strip spans 0 to at least 1 (or the current HDR value). HDR HEX previews clamp to
8-bit range; entering HEX replaces the color with the represented 0-to-1 values.
Theme accent remains opaque. The picker borrows no model pointers; the window
validates source lifetime before invoking its copied setter, and callers resolve
the current light/scene at Apply through existing history boundaries.

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

[TransformGizmo](../Nuclear.Editor/include/Rendering/TransformGizmo.h) owns only drag math and
snapshots. [GameViewPort](../Nuclear.Editor/source/Panels/GameViewPort.cpp) validates the
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
The first version supports one selected entity and world axes; snapping and
local-axis mode are not implemented. Select-mode mesh picking is described above.

## Undo and redo

[EditorHistory](../Nuclear.Editor/include/Settings/EditorHistory.h) owns up to 128 scene
authoring transactions. Use **Edit / Undo**, **Edit / Redo**, **Ctrl+Z**, **Ctrl+Y**
or **Ctrl+Shift+Z**. Shortcuts work in the main workspace and detached windows;
focused text boxes retain their uncommitted text and do not invoke scene history.
Project modals block history actions. Native focus changes reset held modifiers,
and repeated key-down events do not replay additional steps.

History covers toolbar entity/primitive creation, mesh instantiation from the
browser and asset documents, entity deletion, committed inspector names and
transforms, light properties, mesh visibility/shadow flags, kinematic flags and
component addition/removal, scene names and clear color. One released gizmo drag is one transaction, regardless of how
many live updates occurred. Cancellation adds nothing. Replaying history first
cancels a pending viewport gesture, then restores selection, hierarchy, inspector
and renderer bindings. Invalid numeric input and unchanged values add no entries.
A new committed edit after Undo drops Redo; no-op edits preserve it.

[EditorEntityState](../Nuclear.Editor/include/Settings/EditorEntityState.h) stores authored
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

[AssetLibraryViewer](../Nuclear.Editor/source/Panels/AssetLibraryViewer.cpp) has Files,
Textures, Meshes, Materials and Scenes categories. The filesystem browser supports
parent navigation and an editable directory path, and reports filesystem errors.
An **Import...** button sits on the right side of the asset browser tabs bar across all categories and opens a native
single-file picker in the current browsing directory. Its supported-assets filter
covers the existing texture, geometry, NuclearShader and TTF formats; All files
allows other selections, which receive the existing unsupported-format feedback.
The picker is owned by the focused main/detached editor window, preserves the
process working directory and queues selection outside retained input dispatch.
Cancel creates no asset. Selection reuses the existing load-only import flow:
resources belong to the current project session; no source copying or disk project
persistence is added. Existing file-row Import actions remain available.
Explicit Import actions queue work for Update. Texture, geometry, shader and TTF
imports use existing importers; model imports include materials and their textures
by default and omit animations. After successful GPU creation,
`EditorUI::PrepareImportedMaterial` assigns the engine PBR shader to the associated
library material, retaining named texture sets and submesh indices. Its editor
shader excludes deferred, shadow, animation, SSAO and IBL variants. Missing maps use
white albedo/AO, black metallic, flat normal and grey roughness; provided GPU-backed
maps are preserved. Untextured source slots remain distinct from textured slots.
The Materials category lists these library materials with shader and source-set
names, and opens their material documents. One model material asset contains the
source texture sets; the editor does not split it into unrelated per-submesh assets.
Mesh GPU creation
still uses the importer's main-thread task queue, which the editor drains after
synchronous decoding. Readiness is checked separately from decoding; failed texture
decoding must not report the fallback texture as a successful import. Textures and meshes
display in an Unreal-style grid of thumbnail icon buttons with asset names, dimensions or
triangle counts, and direct document-opening actions; mesh cards also retain an **Add to scene**
action button. Mesh previews are rendered and cached on-demand by
[MeshPreviewCache](../Nuclear.Editor/include/Rendering/MeshPreviewCache.h) using an offscreen depth-tested
render target and directional Blinn-Phong lighting.

Material texture imports inherit the mesh's synchronous/load-only policy, so the
editor does not leave them queued on its disabled worker pool or export source
textures as a side effect. Model-relative references are resolved first, with
model-local filename/Textures fallbacks for stale author-machine paths. Referenced
`*_A` images also discover existing sibling `_N`, `_M`, `_R` and `_AO` maps of the
same extension by default. Explicit material slots take precedence; shared files
are reused within a model import and nested Raw directories are not scanned.
`MeshImportingDesc::mDiscoverCompanionTextures=false` disables this discovery.

For the local Cerberus bundle, the FBX references the albedo through an obsolete
absolute authoring path; the normal, metallic and roughness maps are companion
files rather than FBX material references. Reimport the FBX to populate the
Textures category and assign its material. Scene instances use that PBR material.
Private mesh documents and cached thumbnail renderers still show neutral geometry;
they do not evaluate the imported material shader or animated poses.

Drag a ready mesh thumbnail into the scene viewport to place it. Movement must
exceed six logical units; a normal click still opens the mesh viewer. A translucent
green ghost follows the pointer. Release in the viewport creates and selects one
entity, recorded as one undoable creation. `Project::GetMeshMaterial` selects the
mesh's created shader-backed imported material, falling back to the primitive
default only when unavailable. Browser Add to scene, mesh-document insertion and
drag placement all use this resolver; undo/redo retains the material reference.
Escape, source focus loss/removal, a project modal or releasing outside the scene
cancels placement. Unready meshes and non-mesh categories do not create entities.

[EditorAssetButton](../Nuclear.Editor/include/Widgets/EditorAssetButton.h) retains
source input capture while dragging. The controller maps points between native
client coordinates and each context's DPI, including detached Asset Browsers.
[MeshPlacementPreview](../Nuclear.Editor/include/Rendering/MeshPlacementPreview.h)
reads the last rendered depth to resolve a surface and estimates its normal from
neighboring pixels. Local mesh bounds offset the upright asset against that
surface; background falls back to ground Y=0, or ten units along the pointer ray
when the ground is behind/parallel. Bounds use GPU staging if CPU vertices were
released. The ghost creates no ECS entity and never writes scene depth, preventing
self-snapping. Readback and resource creation run in Update/Render, outside input
dispatch. Placement does not rotate to the surface, use physics collision, or
implement grid snapping; screen-edge/unsupported-depth contacts fall back to up.

Right-click an imported texture, mesh, material or scene card for **Add to scene**,
**Delete** and **Rename**. Add to scene is enabled only for created meshes and uses
the existing entity creation/history path. Rename opens a browser form; Enter or
Save name commits a trimmed, nonblank session display name, preserving the UUID
and updating any document caption and heading. Cancel rename leaves the original name intact.
Delete removes the entry from the session library and closes its open document.
Neither action changes source files or serialized assets on disk, and these asset
edits do not enter scene undo history.

[EditorAssetItem](../Nuclear.Editor/include/Widgets/EditorAssetItem.h) wraps each card
and reuses EditorMenuPopup in the card's own context, including detached browsers.
Outside click, Escape and focus loss dismiss the menu; removing the card closes
its overlay. Left-click still opens the asset document. Deleted library map nodes
remain owned by AssetLibraryViewer until editor shutdown, preserving borrowed
resource addresses used by existing entities, history snapshots and previews.
Shutdown clears preview caches and retired assets after widgets, history and scene
entities are gone, before engine graphics teardown.

The synchronous texture importer preserves the Created state assigned after a
successful GPU upload. It must not downgrade an uploaded resource to Loaded:
retained image brushes deliberately require Created before sampling the view.

The retained performance panel reads ProfilingModule snapshots: capture/history,
completed CPU/GPU timing, inclusive CPU scopes, pipeline counts, process/GPU memory,
explicit allocation categories and dropped/invalid results. Unavailable and pending
values stay explicit. Diagnostics refresh twice per second. LoggerView reads only
the latest 16 KiB of `NuclearEngine.log`; it does not change the log or add a sink.

## Launch and validation

Color-picker validation on 2026-11-09: Debug x64 editor and focused editor-test
builds passed. From `Nuclear.Editor/`, `../Bin/DebugX64/Nuclear.Editor.Tests.exe --color-picker`
checks vec3/vec4 RGB(A), HSV and HEX synchronization, strict invalid-input rejection,
HDR retention, hue at black, captured drags at 100%/150% DPI, cancellation, external
window creation, Apply/Cancel/native close, expired source lifetime, light history
and theme application. It also runs scene-settings integration with camera
clear-color readback, scene undo/redo, detached editing and modal isolation, and
saves `Build/DebugX64/editor-color-picker.ppm` (compact property) and
`Build/DebugX64/editor-color-window.ppm` (native chooser) for visual inspection. This validates
D3D11 and automated input; Release, other backends and manual interaction were not run.

Automatic-material assignment validation on 2026-11-09: Debug x64 engine, editor
and focused editor-test builds passed. `Nuclear.Editor.Tests.exe --mesh-material`
checks a single baked static forward PBR variant, library/material-browser identity,
named source sets and material documents, browser Add to scene, drag placement,
mesh-document insertion, and retained material/variant references through undo/redo
and renderer rebaking. A mixed textured/untextured OBJ fixture verifies independent
slots and complete GPU-backed defaults. The existing `--asset-drag` D3D11 run also
passed fallback-mesh placement, ghost/depth, cancellation, history and detached DPI.
These checks exercise draw submission; they do not establish color accuracy,
private material previews, animated materials, Release or other graphics backends.

Material-import validation on 2026-11-09: Debug x64 engine, editor and focused-test
builds passed. `Nuclear.Editor.Tests.exe --mesh-material` imported an OBJ/MTL fixture
through the editor and checked stale absolute path recovery, synchronous GPU
readiness, material association, shared-map reuse, explicit normal-map precedence,
bounded companion discovery and Textures-category visibility. The local Cerberus
FBX also imported with ready A/N/M/R textures. These checks do not validate PBR
rendering, Release, other backends or absent third-party bundles on a clean checkout.

Hierarchy-menu validation on 2026-11-09: Debug x64 editor and focused-test builds
passed. `Nuclear.Editor.Tests.exe --hierarchy-menu` checks right-click targeting,
popup persistence during row rebuilding, duplicate transforms/mesh flags/light
presence, unique naming, copied snapshots after source edits/deletion, Ctrl+V and
Edit/Paste, undo/redo identities, text-input shortcut isolation, Escape, detached
125% DPI menus, redocking with an open popup and scene/clipboard reset. This
focused D3D11 run does not establish Release or physical input behavior.

Mesh-drag validation on 2026-11-09: Debug x64 editor and focused-test builds
passed. `Nuclear.Editor.Tests.exe --asset-drag` passed ghost pixel/depth checks,
surface/ground placement, resource readiness, exactly-one entity on release,
undo/redo identity, Escape/outside/focus cancellation, detached-source 125% DPI
conversion and normal click-to-open. The ghost framebuffer was visually reviewed.
This focused check did not run Release, other graphics backends or physical input.

Mesh-preview validation on 2026-11-09: Debug x64 editor and focused-test builds
passed. The `--mesh-preview` D3D11 run passed geometry/sky readback, complete mesh
framing, WASD movement, right-drag look, focus cancellation, DPI resize and
isolated resource teardown. The framebuffer was visually reviewed. This does not
validate animated poses, imported-material rendering, Release or other backends.

Viewport-selection validation on 2026-11-08: Debug x64 editor and focused-test
builds passed. D3D11 checks cover frontmost selection, hidden/occluded meshes,
background deselection and empty Frame selected, hierarchy/inspector agreement,
filter clearing, closed-panel recovery, retained keyboard focus, no transform or
history changes and 125% UI scaling. GPU readback verifies yellow silhouette
pixels, unchanged mesh interiors and absence of outlines on fully occluded
selections. The complete existing editor lifecycle/history/settings/docking suite
also passed, and the selection framebuffer was visually reviewed. These checks
do not establish alpha-cutout/skinned picking, other graphics backends, Release
or physical mouse feel.

Text-quality validation on 2026-11-08: Debug x64 editor, focused editor-test,
UI-test and Windows platform-test builds passed. The complete D3D11 editor suite
passed with readable text at 1440x900 and 960x600, wrapped viewport actions,
small-panel imports and a detached-window redock at a synthetic 125% scale.
Headless UI checks and both 60-frame D3D11 UI GPU modes passed, including a
fractional-contour readback that rejects a two-pixel smoothing halo. Platform
checks passed for native DPI queries and OS-suggested DPI-change bounds, and the
editor executable's embedded PerMonitorV2 manifest was inspected. Main and
detached framebuffer readbacks were visually reviewed. Physical movement between
monitors with different DPI, other graphics backends and Release were not checked.

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

`Nuclear.Editor.Tests.exe --mesh-preview` runs the focused D3D11 mesh-preview
checks without native file pickers. It verifies mesh/sky pixels, complete initial
framing of uploaded geometry, focused fly navigation, right-drag look, focus
cancellation, DPI resize and isolated teardown, and writes `mesh-preview.ppm`
under the ignored editor test output directory.

`--skip-native-picker` runs the general editor suite using the browser file-row
fixture import instead of the native picker checks. This explicitly leaves picker
selection/cancellation unverified.

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
