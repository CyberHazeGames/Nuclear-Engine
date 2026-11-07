# Runtime UI

`Nuclear::UI` is a retained screen-space widget system inspired by UMG's widget trees,
panel slots, and reusable compositions. It renders directly through Diligent. It does
not use ImGui types, font atlases, drawing, or input state. Existing editor/debug
ImGui remains a separate optional overlay.

## Source and lifetime

- Public facade: [UI.h](../Nuclear.Engine/include/UI/UI.h).
- Geometry, brushes, portable UI events, and quad values: [Types.h](../Nuclear.Engine/include/UI/Types.h); text layout and CPU drawing: [DrawList.h](../Nuclear.Engine/include/UI/DrawList.h), [DrawList.cpp](../Nuclear.Engine/Source/UI/DrawList.cpp).
- Ownership and composition: [Widget.h](../Nuclear.Engine/include/UI/Widget.h), [PanelWidget.h](../Nuclear.Engine/include/UI/PanelWidget.h), [ContentWidget.h](../Nuclear.Engine/include/UI/ContentWidget.h), [UserWidget.h](../Nuclear.Engine/include/UI/UserWidget.h); slot values: [Slot.h](../Nuclear.Engine/include/UI/Slot.h).
- Basic controls and panels: [Widgets.h](../Nuclear.Engine/include/UI/Widgets.h) is an include-only aggregate. Each widget owns a matching header/source pair, such as [Button.h](../Nuclear.Engine/include/UI/Button.h), [Button.cpp](../Nuclear.Engine/Source/UI/Button.cpp), [CanvasPanel.h](../Nuclear.Engine/include/UI/CanvasPanel.h), and [CanvasPanel.cpp](../Nuclear.Engine/Source/UI/CanvasPanel.cpp).
- Viewport, hit testing, focus, and event routing: [UIContext.h](../Nuclear.Engine/include/UI/UIContext.h), [UIContext.cpp](../Nuclear.Engine/Source/UI/UIContext.cpp).
- GPU backend: [UIRenderer.cpp](../Nuclear.Engine/Source/UI/UIRenderer.cpp) and [UI shaders](../Assets/NuclearEngine/Shaders/UI).
- Module integration: [UIModule.cpp](../Nuclear.Engine/Source/UI/UIModule.cpp), [Engine.cpp](../Nuclear.Engine/Source/Core/Engine.cpp).
- Sample client: [UISample.h](../Samples/UISample.h), [UISample.cpp](../Samples/UISample.cpp); composite widget: [UIShowcaseWidget.h](../Samples/UIShowcaseWidget.h), [UIShowcaseWidget.cpp](../Samples/UIShowcaseWidget.cpp).

Each engine UI class has its own same-named `.h`/`.cpp` pair, including the panel,
content, and user-widget bases and the lightweight Overlay and SizeBox wrappers.
Shared control colors live in [ControlStyle.h](../Nuclear.Engine/include/UI/ControlStyle.h).
`Types.h` contains shared value types, while private free drawing helpers live in
`Source/UI/WidgetUtilities.h` and `.cpp`. Template definitions and small inline
accessors remain with their owning class. `UI.h` and `Widgets.h` contain includes
only. New native compilation units are explicitly registered in the engine project;
the two sample implementation files are also compiled by the sample and UI test
projects. See the class-per-file rule in [AGENTS.md](../AGENTS.md).

After the file split on 2026-11-08, all 26 public UI headers compiled independently,
the full Debug x64 solution built, and the existing headless UI checks passed.
D3D11 UI readback/lifecycle tests and retained-selector round trips passed with
ImGui both disabled and enabled. This validates the source organization change
on the existing Windows Debug path; other rendering backends and a full Release
engine build were not checked by this refactor.

The viewport context owns roots through `std::unique_ptr`; each panel exclusively
owns its children. Widget references returned by `Add`, `AddChild`, and
`AddToViewport` are borrowed until removal. Parent/context references are borrowed.
Stable nonzero `WidgetId` values can be resolved through `UIContext::Find`; removed
IDs resolve to null. Widgets are separate from ECS entities and scene snapshots.
Use all UI operations on the engine/window thread.

`UserWidget::Construct` builds a reusable single-content composition on first
attachment. Attachment/removal hooks run with the context available. Removal,
content replacement, and viewport clearing requested during input callbacks are
deferred until event bubbling completes. Focus and capture use IDs, and removal
cancels pending presses before destruction. Callbacks should request destruction
through the context or owning panel rather than deleting borrowed widget pointers.
Custom measure/arrange/paint hooks should not mutate the tree.

`AutoInitUIModule` defaults true and registers UI only when automatic Graphics is
enabled. UI declares dependencies on Graphics and Assets; it is deliberately not
added to the list of modules that Assets itself depends on. The module creates its
renderer and default font after asset initialization. `EnableImGui` independently
controls the existing ImGui context. Client teardown clears viewport widgets before
client shutdown can release borrowed assets. Engine shutdown also clears widgets
before clearing the asset library, then releases UI GPU resources before Graphics.

## Layout and basic widgets

Coordinates use logical units from the viewport's top left, with X rightward and Y
downward. `UIContext::SetViewport(pixelWidth, pixelHeight, scale)` sets physical
dimensions and pixels per logical unit. The default scale is 1. This is explicit
application scaling, not automatic per-monitor DPI detection. The engine initializes
physical dimensions at startup and updates them on resize while preserving scale.
Zero-size resize events keep UI suspended until a nonzero resize restores it.

Layout measures desired sizes, then arranges roots over the viewport and children
within parent allocations. Text, padding, size, slot, and visibility setters
invalidate cached geometry. `SetSizeLimits` constrains desired size; fill slots can
still allocate a different size. Hidden widgets keep their layout space; collapsed
widgets consume no size or box gap. Clipping defaults on for every widget and is
shared by drawing and pointer hit testing. `SetClipToBounds(false)` allows overflow
until an ancestor clip or viewport boundary is reached.

| Type | Behavior |
| --- | --- |
| `CanvasPanel` | Anchors, offsets, pivot, auto size, and stable Z-order |
| `HorizontalBox`, `VerticalBox` | Sequential children, spacing, desired-size slots, positive fill weights |
| `Overlay` | Children share space with slot alignment; higher Z paints last |
| `SizeBox` | Single-child desired-size constraint wrapper |
| `Border` | Tinted solid/texture background with one padded content child |
| `TextBlock` | Font, em size, color, line alignment, newlines, word/glyph wrapping |
| `Image` | Borrowed texture brush, tint, UV region, and desired size |
| `Button` | Single child, state palette, focus outline, release activation |
| `CheckBox` | Button behavior, checked indicator, and `OnChanged(bool)` |
| `Slider` | Normalized value, drag capture, arrows/Home/End, and `OnChanged(float)` |
| `ProgressBar` | Normalized horizontal fill with background/fill colors |

Canvas anchors are normalized and clamped to [0, 1]. On a fixed axis, offsets
specify position and size, with pivot subtracting a fraction of the size. On a
stretched axis, offsets specify start/end margins. Slot padding applies inside
the resulting allocation. Equal Z values preserve insertion order. Slots are
copied: change them with `SetSlot`, not by editing a previous `GetSlot` copy.

## Creating widgets

```cpp
#include <UI/UI.h>

using namespace Nuclear::UI;
auto column = std::make_unique<VerticalBox>();
column->SetPadding({ 24, 24, 24, 24 });
column->SetSpacing(12);
column->Add<TextBlock>("Settings");
auto& button = column->Add<Button>();
button.Add<TextBlock>("Apply");
button.OnClicked = []() { /* application action */ };
auto& context = UIModule::Get().GetContext();
context.AddToViewport(std::move(column));
context.SetInputMode(InputMode::UIOnly);
```

`ContentWidget` accepts one child; `SetContent` replaces it, including deferred
replacement during dispatch. Explicit fonts and brush textures are borrowed and
must outlive their widgets. Asset-library resources must not be deleted by callers.

## Input

The engine forwards portable platform events to the context before gameplay mouse
callbacks. MouseMove and MouseButton positions are converted from client pixels to
logical units; Windows button events include coordinates even without prior motion.
The frontmost visible enabled interactive target receives input first. Unhandled
events bubble to its ancestors along a snapshot of widget IDs. Noninteractive
labels/images do not prevent controls underneath from receiving pointer input.

Buttons capture on left press and activate only when released over the same control.
Sliders retain capture outside their bounds and clamp values. Tab and Shift+Tab
traverse visible enabled focusable widgets; focused buttons activate on Enter/Space
release. Focus loss, hiding/disabling a subtree, removal, and minimized dimensions
cancel active input. Other-window events are not sent to the main viewport context.

`GameOnly` bypasses UI; `UIOnly` reserves mouse/keyboard for UI; `GameAndUI` reserves
input when a control is hovered, captured, or focused. These policies do not change
physical `Platform::Input` state or cursor locking. Gameplay that polls raw keys
must check `WantsKeyboard()`; camera movement should respect `WantsMouse()`.
`SampleBase` demonstrates those guards. Applications select normal/locked cursor
mode explicitly when opening menus.

Existing ImGui input capture takes precedence when no retained widget owns pointer
capture. The engine resets retained input when ImGui takes ownership. ImGui capture
flags reflect its frame state; this is coexistence support, not a replacement
editor designer or a general gameplay input-action system.

## Rendering and fonts

Widgets produce painter-ordered CPU quads with effective clipping. `UIRenderer`
uploads dynamic vertex/index buffers and batches adjacent quads with matching
textures and scissors, preserving transparency order. It draws to the swap-chain
back buffer without clearing, after client scene/postprocessing rendering and before
optional ImGui/presentation. The UI PSO disables depth, uses straight-alpha color
blending, and owns a white texture for solid brushes. Colors are linear RGBA;
texture sRGB decoding and render-target encoding follow Diligent texture formats.

The existing font importer now preserves fractional advances, plane bounds, atlas
UVs, ascender/line metrics, and pair kerning. The retained text shader samples its
single-channel SDF atlas. Font creation checks upload success before setting
`Created`. `FontImportingDesc::Height` controls atlas scale, clamped to 8–256;
the initial repertoire is ASCII. UTF-8 is decoded and validated, with unsupported
codepoints replaced by `?`. Newlines, tabs, word wrapping, and oversized-word glyph
wrapping use the same layout code for measurement and painting.

The module keeps a private copy of the bundled Roboto font's metrics and ref-counted
GPU atlas so clearing the library font map does not invalidate default labels. If
that font cannot load, labels reserve approximate layout space and emit no glyphs;
shapes and controls remain usable. Null or uncreated image textures use a solid
tinted brush. Explicit borrowed fonts/textures still require caller lifetime care.

The first version has no visual designer, serialized widget assets, scene component,
C# bindings, editable text, scrolling, animation, world-space widgets, multiple native
viewports, accessibility integration, complex-script shaping, RTL layout, or automatic
Unicode font fallback. It adds no third-party dependencies.

## Validation and launch

### Retained sample selector

[SampleSelector.h](../Samples/SampleSelector.h) builds the normal Samples launcher
entirely from retained widgets. Navigation covers Runtime UI, Samples 1–5, and
Playground. Tools include VSync, a collapsible performance/memory summary, capture
and clear-history controls, and the existing models/PBR asset-library import action.
Frame timing uses engine delta time, independently of ImGui. The editor's detailed
ImGui profiler remains available separately.

Selection callbacks only record a choice. The selector ends itself from Render;
[Main.cpp](../Samples/Main.cpp) then owns and loads the selected client after the
selector loop and input dispatch have returned. Returning from a sample creates a
fresh selector. This avoids nested client loops inside widget callbacks and clears
the old widget tree before the next client starts. VSync and diagnostics visibility
persist between selector instances. Asset imports retain the existing flow; the
button presents a status frame before synchronous work and reports submitted
requests rather than claiming successful resource creation.

The launcher fits a logical 1280x720 design, reducing UI scale on smaller windows.
The focused `Samples/Samples.SelectorTests.vcxproj` project validates settings,
capture, all twelve expanded-panel focus targets, resize, native keyboard/mouse
selection, clear-history, queued asset-import cancellation, and selector → Runtime UI
→ selector round trips. Run its Debug executable
from `Samples/` as `../Bin/DebugX64/Samples.SelectorTests.exe`, optionally with
`--imgui`. On 2026-11-07 both modes passed on D3D11, the full Debug solution built,
and framebuffer readbacks at 1280x720 and 960x540 were visually reviewed. The test
disables audio, physics, scripting, worker threads, and 3D scene rendering; it does
not validate the other samples' imported content or run model/material importing.

### UI widget checks

Build with the existing normalized MSBuild environment from [BuildAndValidation.md](BuildAndValidation.md).
The focused test project is separate from the solution's default build:

```powershell
msbuild Nuclear.Engine/Nuclear.UI.Tests.vcxproj /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64
./Bin/DebugX64/Nuclear.UI.Tests.exe
Push-Location Samples
try {
    & ../Bin/DebugX64/Nuclear.UI.Tests.exe --gpu
    & ../Bin/DebugX64/Nuclear.UI.Tests.exe --gpu --imgui
}
finally { Pop-Location }
```

For the interactive demonstration, select **Runtime UI** in Samples, or run
`../Bin/DebugX64/Samples.exe --ui` from `Samples/`. The `--ui` path disables ImGui,
audio, physics, scripting, worker threads, and 3D rendering-module initialization;
it needs the engine font and UI shaders but no imported sample models or audio.

The headless checks cover anchors/stretch/scaling, resize invalidation, hidden and
collapsed layout, fill weights, Z-order hit testing, clipping, pointer capture,
focus traversal, activation repeat, self-removal/clearing from callbacks, input
modes, fractional font metrics, kerning, wrapping, and UTF-8 fallback. The GPU path
runs two clients and uses staging readback to verify rectangle color, alpha
blending, and SDF text coverage. It also exercises resizing, minimize/restore, manual scale changes,
library font-map release, platform mouse-message routing, and shutdown. A PPM
readback is written under ignored `Nuclear.Engine/Build/` for visual review.

Validation on 2026-11-07: the full Debug x64 solution build passed. The Debug
headless UI checks passed, and both GPU commands passed on D3D11 / NVIDIA GeForce
RTX 5070 Ti, each rendering two 30-frame clients. Pixel readback verified solid
color, transparency order/blending, ancestor scissor clipping, and SDF text. The
runs exercised synthetic Windows mouse-button routing, resize, minimize/restore,
manual scale changes, library font-map release, repeated client removal, and
module/window shutdown with exit code 0. The readback image was visually reviewed.
Windows platform smoke tests also passed in Debug and Release with the new
mouse-button coordinate regression check. This does not establish other rendering
backends, a full Release engine build, physical input feel, or complex-script text.
The actual `Samples.exe --ui` entry point also reached rendering and completed
client/module/window shutdown with exit code 0 after its engine window received a
normal close message. This launch check used a hidden window.
