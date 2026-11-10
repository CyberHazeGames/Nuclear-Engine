#include "EditorHistoryTests.h"
#include "ComponentInspectorTests.h"
#include "AssetPropertyTests.h"
#include "AssetViewerTests.h"
#include "SceneSettingsTests.h"
#include "ColorPickerTests.h"
#include "MeshPreviewTests.h"
#include "AssetDragTests.h"
#include "HierarchyMenuTests.h"
#include "MeshMaterialImportTests.h"
#include <NuclearEditor.h>
#include <GameViewPort.h>
#include <EditorTitleBar.h>
#include <EditorMenuPopup.h>
#include <EditorWindowButton.h>
#include <EditorComponentButton.h>
#include <EditorToolbar.h>
#include <EditorAddMenu.h>
#include <SkyboxComponent.h>
#include "TransformGizmoTests.h"
#include "WorkspaceTests.h"
#include "ViewportSelectionTests.h"
#include <Nuclear.Platform/Platform.h>
#include <Profiling/ProfilingModule.h>
#include <fstream>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <chrono>
#include <thread>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <dlgs.h>

using namespace Nuclear::UI;

namespace
{
	void Check(bool condition, const char* message)
	{
		if (!condition)
			throw std::runtime_error(message);
	}
	UIContext& Context()
	{
		return UIModule::Get().GetContext();
	}
	void Key(Platform::Input::KeyCode key, bool pressed = true)
	{
		Platform::Event event;
		event.mType = Platform::EventType::Key;
		event.mKey = key;
		event.mPressed = pressed;
		Context().ProcessEvent(event);
	}
	void Text(const std::string& value)
	{
		for (unsigned char character : value)
		{
			Platform::Event event;
			event.mType = Platform::EventType::TextInput;
			event.mCodepoint = character;
			Context().ProcessEvent(event);
		}
	}
	void Click(Widget& widget)
	{
		Context().Layout();
		for (auto* parent = widget.GetParent(); parent; parent = parent->GetParent())
			if (auto* scroll = dynamic_cast<ScrollBox*>(parent))
			{
				scroll->ScrollIntoView(widget);
				Context().Layout();
			}
		auto bounds = widget.GetBounds();
		Platform::Event event;
		event.mType = Platform::EventType::MouseButton;
		event.mMouseX = int((bounds.x + bounds.Width * .5f) * Context().GetScale());
		event.mMouseY = int((bounds.y + bounds.Height * .5f) * Context().GetScale());
		event.mButton = MOUSE_BUTTON_LEFT;
		event.mPressed = true;
		Context().ProcessEvent(event);
		event.mPressed = false;
		Context().ProcessEvent(event);
	}
	void Edit(TextBox& input, const std::string& value)
	{
		Click(input);
		Key(Platform::Input::KEYCODE_LCTRL);
		Key(Platform::Input::KEYCODE_A);
		Key(Platform::Input::KEYCODE_LCTRL, false);
		Text(value);
		Key(Platform::Input::KEYCODE_RETURN);
	}
	void PointerAt(Platform::EventType type, Vec2 position, bool pressed = false,
				   int button = MOUSE_BUTTON_LEFT)
	{
		Platform::Event event;
		event.mType = type;
		event.mMouseX = static_cast<int>(std::lround(position.x * Context().GetScale()));
		event.mMouseY = static_cast<int>(std::lround(position.y * Context().GetScale()));
		event.mButton = button;
		event.mPressed = pressed;
		Context().ProcessEvent(event);
	}
	// Stable IDs let this integration check locate semantic labels without application test hooks.
	template <class T, class Predicate> T* Find(Predicate predicate)
	{
		Widget marker;
		for (WidgetId id = 1; id < marker.GetId(); ++id)
			if (auto* widget = dynamic_cast<T*>(Context().Find(id)); widget && predicate(*widget))
				return widget;
		return nullptr;
	}
	void Action(const std::string& title)
	{
		std::cout << "Action: " << title << std::endl;
		auto* label = Find<TextBlock>([&title](const TextBlock& label) {
			return label.GetText() == title;
		});
		Check(label && label->GetParent(), "Expected action label");
		Click(*label->GetParent());
	}
	HWND FindImportDialog(DWORD thread)
	{
		HWND dialog = nullptr;
		EnumThreadWindows(thread, [](HWND window, LPARAM output) -> BOOL {
			wchar_t title[128]{};
			GetWindowTextW(window, title, 128);
			if (std::wcscmp(title, L"Import asset") == 0 && IsWindowVisible(window))
			{
				*reinterpret_cast<HWND*>(output) = window;
				return FALSE;
			}
			return TRUE;
		}, reinterpret_cast<LPARAM>(&dialog));
		return dialog;
	}
	// Drive the real native picker on a helper thread while the UI thread runs its modal loop.
	void ImportFile(const std::filesystem::path& path, const std::function<void()>& frame)
	{
		const auto uiThread = GetCurrentThreadId();
		Action("Import...");
		Check(!FindImportDialog(uiThread), "Picker is deferred beyond input dispatch");
		bool found = false;
		bool modal = false;
		bool closed = false;
		std::jthread picker([&]() {
			HWND dialog = nullptr;
			for (int attempt = 0; attempt < 1000 && !dialog; ++attempt)
			{
				dialog = FindImportDialog(uiThread);
				if (!dialog)
					std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
			if (!dialog)
				return;
			found = true;
			const auto owner = GetWindow(dialog, GW_OWNER);
			modal = owner == Core::Engine::Get().GetMainWindow()->GetNativeHandle() && !IsWindowEnabled(owner);
			if (path.empty())
				PostMessageW(dialog, WM_COMMAND, IDCANCEL, 0);
			else
			{
				const auto filename = path.wstring();
				EnumChildWindows(dialog, [](HWND child, LPARAM text) -> BOOL {
					wchar_t name[16]{};
					GetClassNameW(child, name, 16);
					const int id = GetDlgCtrlID(child);
					if ((id == cmb13 || id == edt1) && std::wcscmp(name, L"Edit") == 0)
					{
						SendMessageW(child, WM_SETTEXT, 0, text);
						return FALSE;
					}
					return TRUE;
				}, reinterpret_cast<LPARAM>(filename.c_str()));
				PostMessageW(dialog, WM_COMMAND, IDOK, 0);
			}
			for (int attempt = 0; attempt < 1000 && IsWindow(dialog); ++attempt)
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			closed = !IsWindow(dialog);
			if (!closed)
				PostMessageW(dialog, WM_COMMAND, IDCANCEL, 0);
		});
		frame();
		picker.join();
		if (!found || !modal || !closed)
			std::cerr << "Import picker: found=" << found << " modal=" << modal << " closed=" << closed << '\n';
		Check(found && modal && closed, "Native import picker opens modally and accepts selection/cancellation");
	}
	void CheckViewportToolbar()
	{
		Context().Layout();
		auto* surface = Find<Nuclear::Editor::GameViewPort>([](const auto&) { return true; });
		Check(surface != nullptr, "Viewport exists for toolbar layout check");
		const auto viewport = surface->GetBounds();
		Rect previous;
		for (const char* title : {"Select", "Move", "Rotate", "Scale", "Frame selected", "Delete selected"})
		{
			auto* label = Find<TextBlock>([title](const TextBlock& label) {
				return label.GetText() == title;
			});
			Check(label && label->IsVisible(), "Viewport toolbar action is visible");
			const auto bounds = label->GetParent()->GetBounds();
			Check(bounds.y + bounds.Height <= viewport.y && bounds.x >= viewport.x &&
					  bounds.x + bounds.Width <= viewport.x + viewport.Width,
				  "Viewport actions fit in a bar above the scene surface");
			Check(previous.Width == 0 || bounds.y >= previous.y + previous.Height ||
				(bounds.y == previous.y && bounds.x >= previous.x + previous.Width),
				"Viewport actions wrap in order without overlap");
			Check(label->GetBounds().Width + .01f >= label->GetDesiredSize().x,
				"Viewport action text remains fully readable");
			previous = bounds;
		}
	}
	entt::entity Entity(const std::string& name)
	{
		auto& registry = Core::Scene::Get().GetRegistry();
		for (auto entity : registry.view<Components::EntityInfoComponent>())
			if (registry.get<Components::EntityInfoComponent>(entity).mName == name)
				return entity;
		return entt::null;
	}
	std::array<unsigned char, 4> CameraCorner()
	{
		using namespace Diligent;
		auto& graphics = Graphics::GraphicsModule::Get();
		auto* source = Core::Scene::Get().GetMainCamera()->GetColorRT().GetRTV()->GetTexture();
		auto desc = source->GetDesc();
		desc.Usage = USAGE_STAGING;
		desc.BindFlags = BIND_NONE;
		desc.CPUAccessFlags = CPU_ACCESS_READ;
		RefCntAutoPtr<ITexture> staging;
		graphics.GetDevice()->CreateTexture(desc, nullptr, &staging);
		Check(staging != nullptr, "Skybox readback allocation");
		auto* context = graphics.GetContext();
		context->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);
		CopyTextureAttribs copy;
		copy.pSrcTexture = source;
		copy.pDstTexture = staging;
		copy.SrcTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
		copy.DstTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
		context->CopyTexture(copy);
		context->WaitForIdle();
		MappedTextureSubresource mapped;
		context->MapTextureSubresource(staging, 0, 0, MAP_READ, MAP_FLAG_NONE, nullptr, mapped);
		Check(mapped.pData != nullptr, "Skybox readback mapping");
		const auto* pixel = static_cast<const unsigned char*>(mapped.pData);
		const std::array<unsigned char, 4> result = {pixel[0], pixel[1], pixel[2], pixel[3]};
		context->UnmapTextureSubresource(staging, 0, 0);
		return result;
	}
	void Readback(const char* path, Diligent::ITexture* source = nullptr, bool checkScene = true)
	{
		using namespace Diligent;
		auto& graphics = Graphics::GraphicsModule::Get();
		auto* context = graphics.GetContext();
		if (!source) source = graphics.GetSwapChain()->GetCurrentBackBufferRTV()->GetTexture();
		auto desc = source->GetDesc();
		desc.Usage = USAGE_STAGING;
		desc.BindFlags = BIND_NONE;
		desc.CPUAccessFlags = CPU_ACCESS_READ;
		RefCntAutoPtr<ITexture> staging;
		graphics.GetDevice()->CreateTexture(desc, nullptr, &staging);
		Check(staging != nullptr, "Readback allocation");
		context->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);
		CopyTextureAttribs copy;
		copy.pSrcTexture = source;
		copy.pDstTexture = staging;
		copy.SrcTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
		copy.DstTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
		context->CopyTexture(copy);
		context->WaitForIdle();
		MappedTextureSubresource mapped;
		context->MapTextureSubresource(staging, 0, 0, MAP_READ, MAP_FLAG_NONE, nullptr, mapped);
		Check(mapped.pData != nullptr, "Readback mapping");
		std::ofstream image(path, std::ios::binary);
		image << "P6\n" << desc.Width << ' ' << desc.Height << "\n255\n";
		unsigned coloredPixels = 0;
		unsigned scenePixels = 0;
		auto* scene = Find<Nuclear::Editor::GameViewPort>([](const auto&) {
			return true;
		});
		Check(scene != nullptr, "Scene surface exists");
		auto sceneBounds = scene->GetBounds();
		const float scale = Context().GetScale();
		for (unsigned y = 0; y < desc.Height; ++y)
			for (unsigned x = 0; x < desc.Width; ++x)
			{
				auto* pixel = static_cast<const unsigned char*>(mapped.pData) + y * mapped.Stride + x * 4;
				image.write(reinterpret_cast<const char*>(pixel), 3);
				if (pixel[0] > 80 || pixel[1] > 100 || pixel[2] > 100)
					++coloredPixels;
				if (sceneBounds.Contains({x / scale, y / scale}) && pixel[0] > 40 && pixel[0] < 210 &&
					std::abs(int(pixel[0]) - int(pixel[1])) < 8 &&
					std::abs(int(pixel[0]) - int(pixel[2])) < 8)
					++scenePixels;
			}
		context->UnmapTextureSubresource(staging, 0, 0);
		Check(image.good() && coloredPixels > 4000, "Editor labels produce visible framebuffer coverage");
		const float minimumScenePixels = std::max(20.f,
			sceneBounds.Width * sceneBounds.Height * scale * scale * .001f);
		Check(
			!checkScene || scenePixels > minimumScenePixels,
			"Scene framebuffer contains neutral shaded geometry, independently of labels and colored gizmos");
	}
} // namespace

namespace
{
	void RunTabTitleTests()
	{
		UIContext context;
		const auto* font = Context().GetDefaultFont();
		Check(font != nullptr, "Tab title tests use the editor font");
		context.SetDefaultFont(font);
		auto strip = std::make_unique<Nuclear::Editor::EditorTabStrip>();
		auto* tabs = strip.get();
		context.AddToViewport(std::move(strip));
		const std::string title = "WBP_CenterAbilityBar";
		const std::string expected = "WBP_Cente...ilityBar";
		const float width = DrawList::MeasureText(font, expected, 14).x + 71.05f;
		context.SetViewport(width, 34);
		tabs->SetTabs({{"asset", title}}, "asset");
		context.Layout();
		Check(tabs->GetDisplayTitle("asset") == expected, "Long captions retain balanced beginning and end around dots");
		const auto bounds = tabs->GetTabBounds("asset");
		bool paintedLabel = false;
		for (const auto& quad : context.BuildDrawList().Quads)
			if (quad.SDF && std::abs(quad.Tint.r - .8f) < .001f)
			{
				paintedLabel = true;
				Check(quad.Clip.x >= bounds.x + 10 && quad.Clip.x + quad.Clip.Width <= bounds.x + bounds.Width - 28.0f + .01f,
					"Title glyphs are clipped before the close-button area");
			}
		Check(paintedLabel, "Fitted tab caption emits glyphs");
		std::string closed;
		tabs->OnClosed = [&](const std::string& id) { closed = id; };
		Platform::Event click;
		click.mType = Platform::EventType::MouseButton;
		click.mButton = MOUSE_BUTTON_LEFT;
		click.mMouseX = int(bounds.x + bounds.Width - 12);
		click.mMouseY = 16;
		click.mPressed = true;
		context.ProcessEvent(click);
		click.mPressed = false;
		context.ProcessEvent(click);
		Check(closed == "asset", "Truncation preserves close-button activation and full tab identity");
		context.SetViewport(width * 1.25f, 34 * 1.25f, 1.25f);
		context.Layout();
		Check(tabs->GetDisplayTitle("asset") == expected, "Logical caption fitting follows DPI scaling");
		context.SetViewport(400, 34);
		tabs->SetTabs({{"asset", "Short"}}, "asset");
		context.Layout();
		Check(tabs->GetDisplayTitle("asset") == "Short", "Short names remain unchanged");
		const std::string unicode = "Caf\xC3\xA9_\xE4\xB8\xAD_LongAssetTitle_\xE4\xB8\xAD_\xC3\xA9";
		tabs->SetTabs({{"asset", unicode}, {"other", "Other"}, {"third", "Third"}}, "asset");
		context.Layout();
		const auto fitted = tabs->GetDisplayTitle("asset");
		Check(fitted.find("...") != std::string::npos &&
			MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, fitted.data(), int(fitted.size()), nullptr, 0) > 0,
			"Truncation preserves complete UTF-8 characters");
		Check(fitted.starts_with("Caf") && fitted.ends_with("\xC3\xA9"), "Unicode titles retain both ends");
		std::cout << "Tab title fitting, clipping, close activation, DPI and UTF-8 passed.\n";
	}
}

int main(int argc, char** argv)
{
	try
	{
		const bool skipNativePicker = argc == 2 && std::string(argv[1]) == "--skip-native-picker";
		Nuclear::Editor::Tests::RunTransformGizmoTests();
		Core::EngineStartupDesc desc;
		desc.EnableImGui = false;
		desc.AutoInitAudioModule = false;
		desc.AutoInitPhysXModule = false;
		desc.AutoInitScriptingModule = false;
		desc.AutoInitThreadingModule = false;
		desc.mAssetsLibraryPath = "../Assets/";
		desc.mEngineWindowDesc.WindowWidth = 1440;
		desc.mEngineWindowDesc.WindowHeight = 900;
		desc.mEngineWindowDesc.CustomTitleBar = true;
		auto& engine = Core::Engine::Get();
		Check(engine.Start(desc), "Editor engine startup");
		engine.GetMainWindow()->Display(false);
		if (argc == 2 && std::string(argv[1]) == "--color-picker")
		{
			Nuclear::Editor::Tests::RunColorPickerTests();
			NuclearEditor editor;
			editor.Load();
			auto frame = [&]() {
				engine.BeginFrame();
				editor.Update(1.f / 60);
				editor.Render(1.f / 60);
				engine.EndFrame();
			};
			for (int index = 0; index < 4; ++index) frame();
			Action("Window");
			Action("Scene settings");
			frame();
			UIModule::Get().Render();
			Readback("Build/DebugX64/editor-color-picker.ppm");
			Action("Clear color...");
			frame();
			auto& colors = editor.GetWorkspace()->GetColorPicker();
			Check(colors.GetWindow() != nullptr, "Clicking the property opens a separate native chooser");
			colors.GetWindow()->GetWindow().Display(false);
			colors.GetWindow()->Render();
			Readback("Build/DebugX64/editor-color-window.ppm",
				colors.GetWindow()->GetSwapChain()->GetCurrentBackBufferRTV()->GetTexture(), false);
			colors.Close();
			editor.GetWorkspace()->Update();
			Nuclear::Editor::Tests::RunSceneSettingsTests(editor, frame);
			editor.Shutdown();
			engine.Shutdown();
			return 0;
		}
		if (argc == 2 && std::string(argv[1]) == "--mesh-material")
		{
			Nuclear::Editor::Tests::RunMeshMaterialImportTests();
			engine.Shutdown();
			return 0;
		}
		if (argc == 2 && std::string(argv[1]) == "--asset-properties")
		{
			Nuclear::Editor::Tests::RunAssetPropertyTests();
			engine.Shutdown();
			return 0;
		}
		if (argc == 2 && std::string(argv[1]) == "--asset-viewers")
		{
			Nuclear::Editor::Tests::RunAssetViewerTests();
			engine.Shutdown();
			return 0;
		}
		if (argc == 2 && std::string(argv[1]) == "--hierarchy-menu")
		{
			Nuclear::Editor::Tests::RunHierarchyMenuTests();
			engine.Shutdown();
			return 0;
		}
		if (argc == 2 && std::string(argv[1]) == "--asset-drag")
		{
			Nuclear::Editor::Tests::RunAssetDragTests();
			engine.Shutdown();
			return 0;
		}
		if (argc == 2 && std::string(argv[1]) == "--tab-titles")
		{
			RunTabTitleTests();
			engine.Shutdown();
			return 0;
		}
		if (argc == 2 && std::string(argv[1]) == "--mesh-preview")
		{
			Nuclear::Editor::Tests::RunMeshPreviewTests();
			engine.Shutdown();
			return 0;
		}
		Nuclear::Editor::Tests::RunEditorHistoryTests();
		Nuclear::Editor::Tests::RunColorPickerTests();
		Nuclear::Editor::Tests::RunComponentInspectorTests();
		{
			NuclearEditor editor;
			editor.Load();
			auto frame = [&editor, &engine]() {
				engine.BeginFrame();
				editor.Update(1.f / 60);
				editor.Render(1.f / 60);
				engine.EndFrame();
			};
			auto historyKey = [](Platform::Input::KeyCode key, bool shift = false) {
				Key(Platform::Input::KEYCODE_LCTRL);
				if (shift)
					Key(Platform::Input::KEYCODE_LSHIFT);
				Key(key);
				Key(key, false);
				if (shift)
					Key(Platform::Input::KEYCODE_LSHIFT, false);
				Key(Platform::Input::KEYCODE_LCTRL, false);
			};
			auto historyMenu = [&frame](const std::string& command) {
				Action("Edit");
				Action(command);
				frame();
			};
			for (int index = 0; index < 4; ++index)
				frame();
			Check(editor.GetWorkspace()->GetArea("scene-settings") == nullptr,
				"Scene settings starts closed until opened from a menu");
			Context().Layout();
			Check(std::abs(Context().GetScale() - engine.GetMainWindow()->GetDpiScale()) < .001f,
				"Editor uses monitor DPI instead of fitting the window to a design resolution");
			auto* titleBar = Find<Nuclear::Editor::EditorTitleBar>([](const auto&) { return true; });
			Check(titleBar && titleBar->GetBounds().x == 0 && titleBar->GetBounds().y == 0 &&
				titleBar->GetBounds().Height == 40, "Custom title bar occupies the top of the client area");
			auto* nativeWindow = engine.GetMainWindow();
			Check(nativeWindow->HasCustomTitleBar() && nativeWindow->IsTitleBarAt(1000, 20), "Empty caption space supports native dragging");
			Check(!nativeWindow->IsTitleBarAt(70, 20) && !nativeWindow->IsTitleBarAt(1420, 20), "Menus and caption controls remain client input");
			Action("File");
			auto menu = []() { return Find<Nuclear::Editor::EditorMenuPopup>([](const auto&) { return true; }); };
			Check(menu() && Context().GetFocusedWidget() == menu()->GetId(), "File opens a keyboard-focused dropdown");
			Check(!nativeWindow->IsTitleBarAt(1000, 20), "Open menu blocks caption dragging");
			UIModule::Get().Render();
			Readback("Build/DebugX64/editor-file-menu.ppm");
			Key(Platform::Input::KEYCODE_RIGHT);
			Check(Find<TextBlock>([](const auto& label) { return label.GetText() == "Undo"; }),
				"Right switches to the next menu");
			Check(!Find<TextBlock>([](const auto& label) { return label.GetText() == "Undo"; })->GetParent()->IsEnabled(),
				"Initial scene has no undoable user edits");
			Key(Platform::Input::KEYCODE_LEFT);
			Check(Find<TextBlock>([](const auto& label) { return label.GetText() == "Save scene (unavailable)"; }),
				"Left switches to the previous menu");
			Key(Platform::Input::KEYCODE_ESCAPE);
			Check(!menu(), "Escape dismisses the menu");
			Action("Help");
			Key(Platform::Input::KEYCODE_DOWN);
			Key(Platform::Input::KEYCODE_RETURN);
			Check(!menu(), "Keyboard selection executes a command and dismisses its menu");
			Action("Help");
			Action("About Nuclear Engine");
			Check(!menu(), "Pointer commands survive focus moving into the popup and dismiss it");
			if (editor.GetWorkspace()->Contains("about"))
			{
				editor.GetWorkspace()->Close("about");
				frame();
			}
			Action("Edit");
			auto* projectHeader = Find<TextBlock>([](const auto& label) { return label.GetText() == "Project"; });
			const auto projectBounds = projectHeader->GetBounds();
			PointerAt(Platform::EventType::MouseMove, {projectBounds.x + 5, projectBounds.y + 5});
			Check(Find<TextBlock>([](const auto& label) { return label.GetText() == "Project settings (unavailable)"; }),
				"Hover switches open dropdowns");
			Platform::Event focus;
			focus.mType = Platform::EventType::FocusChanged;
			Context().ProcessEvent(focus);
			Check(!menu(), "Window focus loss dismisses dropdowns");
			focus.mFocused = true;
			Context().ProcessEvent(focus);
			Action("Help");
			auto* about = Find<TextBlock>([](const auto& label) { return label.GetText() == "About Nuclear Engine"; });
			const auto aboutBounds = about->GetBounds();
			PointerAt(Platform::EventType::MouseButton, {aboutBounds.x + 5, aboutBounds.y + 5}, true);
			focus.mFocused = false;
			Context().ProcessEvent(focus);
			frame();
			Check(!menu(), "Losing window focus during a command press also closes its popup");
			focus.mFocused = true;
			Context().ProcessEvent(focus);
			Action("Edit");
			PointerAt(Platform::EventType::MouseButton, {1000, 70}, true);
			PointerAt(Platform::EventType::MouseButton, {1000, 70}, false);
			Check(!menu(), "Outside click dismisses without invoking underlying controls");
			auto* maximize = Find<Nuclear::Editor::EditorWindowButton>([](const auto& button) {
				return button.GetAction() == Nuclear::Editor::EditorWindowButton::Action::Maximize;
			});
			Click(*maximize);
			Check(nativeWindow->IsMaximized(), "Retained maximize button invokes the native window command");
			Click(*maximize);
			Check(!nativeWindow->IsMaximized(), "Retained restore button invokes the native window command");
			nativeWindow->Display(false);
			auto& previewMesh =
				Core::Scene::Get().GetRegistry().get<Components::MeshComponent>(Entity("Box"));
			auto* camera = Core::Scene::Get().GetMainCamera();
			auto clip = camera->GetProjectionMatrix() * camera->mCameraData.View * Math::Vector4(0, 0, 0, 1);
			std::cout << "Preview queue=" << previewMesh.GetRenderQueue()
					  << " mesh=" << int(previewMesh.GetMesh()->GetState())
					  << " material=" << int(previewMesh.GetMaterial()->GetState()) << " clip=" << clip.x
					  << ',' << clip.y << ',' << clip.z << ',' << clip.w << std::endl;
			Check(std::abs(clip.x / clip.w) < .05f && std::abs(clip.y / clip.w) < .05f,
				  "Preview camera frames the origin");
			UIModule::Get().Render();
			CheckViewportToolbar();
			Readback("Build/DebugX64/editor-1440.ppm");
			Check(Entity("Box") != entt::null, "Initial scene contains a preview primitive");
			auto* surface = Find<Nuclear::Editor::GameViewPort>([](const auto&) {
				return true;
			});
			Check(surface != nullptr, "Gizmo scene surface exists");
			Nuclear::Editor::Tests::RunViewportSelectionTests(editor, frame);
			const char* modes[] = {"Move", "Rotate", "Scale"};
			for (int mode = 0; mode < 3; ++mode)
			{
				Action(modes[mode]);
				frame();
				Nuclear::Editor::TransformGizmo handles;
				handles.SetMode(static_cast<Nuclear::Editor::GizmoMode>(mode));
				Check(surface->GetGizmoMode() == handles.GetMode(), "Toolbar changes gizmo operation");
				auto& target = Core::Scene::Get()
								   .GetRegistry()
								   .get<Components::EntityInfoComponent>(Entity("Box"))
								   .mTransform;
				const auto originalPosition = target.GetLocalPosition();
				const auto originalScale = target.GetLocalScale();
				const auto originalRotation = target.GetLocalRotation();
				Vec2 start, end;
				Check(handles.GetHandlePosition(target, *camera, surface->GetBounds(), 0, start),
					  "Visible editor gizmo handle");
				if (mode == 1)
					Check(handles.GetHandlePosition(target, *camera, surface->GetBounds(), 0, end, 1.1f),
						  "Visible rotation destination");
				else
				{
					Vec2 center;
					handles.GetHandlePosition(target, *camera, surface->GetBounds(), 3, center);
					end = {start.x + (start.x - center.x) * .4f, start.y + (start.y - center.y) * .4f};
				}
				PointerAt(Platform::EventType::MouseButton, start, true);
				PointerAt(Platform::EventType::MouseMove, end);
				frame();
				Check(Context().GetCapturedWidget() == surface->GetId(),
					  "Inspector rebuilding preserves gizmo capture");
				Check(mode == 0	  ? target.GetLocalPosition().x > originalPosition.x + .1f
					  : mode == 1 ? target.GetLocalRotation() != originalRotation
								  : target.GetLocalScale().x > originalScale.x,
					  "Gizmo input updates the live scene transform");
				Key(Platform::Input::KEYCODE_ESCAPE);
				frame();
				Check(target.GetLocalPosition() == originalPosition &&
						  target.GetLocalScale() == originalScale &&
						  target.GetLocalRotation() == originalRotation && !Context().GetCapturedWidget(),
					  "Editor Escape restores the entire starting transform");
				UIModule::Get().Render();
				if (mode == 1)
					Readback("Build/DebugX64/editor-rotate.ppm");
				if (mode == 2)
					Readback("Build/DebugX64/editor-scale.ppm");
			}
			Key(Platform::Input::KEYCODE_G);
			Check(surface->GetGizmoMode() == Nuclear::Editor::GizmoMode::Move,
				  "Focused viewport G selects move");
			Key(Platform::Input::KEYCODE_E);
			Check(surface->GetGizmoMode() == Nuclear::Editor::GizmoMode::Rotate,
				  "Focused viewport E selects rotation");
			Key(Platform::Input::KEYCODE_R);
			Check(surface->GetGizmoMode() == Nuclear::Editor::GizmoMode::Scale,
				  "Focused viewport R selects scale");
			Key(Platform::Input::KEYCODE_Q);
			Check(surface->GetGizmoMode() == Nuclear::Editor::GizmoMode::Select,
				  "Focused viewport Q selects without transform handles");
			Key(Platform::Input::KEYCODE_G);
			frame();
			Nuclear::Editor::TransformGizmo moveHandles;
			auto& movedBox = Core::Scene::Get()
								 .GetRegistry()
								 .get<Components::EntityInfoComponent>(Entity("Box"))
								 .mTransform;
			Vec2 moveStart, moveCenter;
			moveHandles.GetHandlePosition(movedBox, *camera, surface->GetBounds(), 0, moveStart);
			moveHandles.GetHandlePosition(movedBox, *camera, surface->GetBounds(), 3, moveCenter);
			PointerAt(Platform::EventType::MouseButton, moveStart, true);
			PointerAt(Platform::EventType::MouseMove, {moveStart.x + (moveStart.x - moveCenter.x) * .4f,
													   moveStart.y + (moveStart.y - moveCenter.y) * .4f});
			frame();
			PointerAt(Platform::EventType::MouseButton, {-20, -20});
			frame();
			const auto committed = movedBox.GetLocalPosition();
			Check(committed.x > .1f && !Context().GetCapturedWidget(),
				  "Release outside the viewport commits and releases capture");
			historyKey(Platform::Input::KEYCODE_Z);
			frame();
			Check(movedBox.GetLocalPosition() == Math::Vector3(0), "Ctrl+Z undoes a whole committed gizmo drag");
			historyKey(Platform::Input::KEYCODE_Z);
			frame();
			Check(Entity("Box") != entt::null && movedBox.GetLocalPosition() == Math::Vector3(0),
				"Canceled drags and initial scene setup do not enter history");
			historyKey(Platform::Input::KEYCODE_Y);
			frame();
			Check(movedBox.GetLocalPosition() == committed, "Ctrl+Y redoes the gizmo drag");
			historyKey(Platform::Input::KEYCODE_Z);
			frame();
			historyKey(Platform::Input::KEYCODE_Z, true);
			frame();
			Check(movedBox.GetLocalPosition() == committed, "Ctrl+Shift+Z also redoes the gizmo drag");
			Key(Platform::Input::KEYCODE_LCTRL);
			Platform::Event repeated;
			repeated.mType = Platform::EventType::Key;
			repeated.mKey = Platform::Input::KEYCODE_Z;
			repeated.mPressed = repeated.mRepeat = true;
			Context().ProcessEvent(repeated);
			Key(Platform::Input::KEYCODE_LCTRL, false);
			Check(movedBox.GetLocalPosition() == committed, "Auto-repeat does not replay additional history steps");
			moveHandles.GetHandlePosition(movedBox, *camera, surface->GetBounds(), 0, moveStart);
			moveHandles.GetHandlePosition(movedBox, *camera, surface->GetBounds(), 3, moveCenter);
			PointerAt(Platform::EventType::MouseButton, moveStart, true);
			PointerAt(Platform::EventType::MouseMove, {moveStart.x + (moveStart.x - moveCenter.x) * .4f,
				moveStart.y + (moveStart.y - moveCenter.y) * .4f});
			frame();
			Check(Context().GetCapturedWidget() == surface->GetId(), "History cancellation check starts a live drag");
			historyKey(Platform::Input::KEYCODE_Z);
			frame();
			Check(!Context().GetCapturedWidget() && movedBox.GetLocalPosition() == Math::Vector3(0),
				"Undo cancels a pending drag before reverting the previous committed action");
			historyKey(Platform::Input::KEYCODE_Y);
			frame();
			PointerAt(Platform::EventType::MouseButton, moveStart);
			Check(movedBox.GetLocalPosition() == committed, "A release after history cancellation cannot commit a stale drag");
			Key(Platform::Input::KEYCODE_ESCAPE);
			Check(movedBox.GetLocalPosition() == committed,
				  "Escape after commit does not undo a completed drag");
			const auto previousFront = camera->GetFrontView();
			const auto bounds = surface->GetBounds();
			const Vec2 lookStart = {bounds.x + 20, bounds.y + 20};
			PointerAt(Platform::EventType::MouseButton, lookStart, true, MOUSE_BUTTON_RIGHT);
			PointerAt(Platform::EventType::MouseMove, {lookStart.x + 20, lookStart.y + 10});
			PointerAt(Platform::EventType::MouseButton, lookStart, false, MOUSE_BUTTON_RIGHT);
			frame();
			Check(Math::length(camera->GetFrontView() - previousFront) > .01f &&
					  movedBox.GetLocalPosition() == committed,
				  "Right drag navigates the camera without editing the selected entity");
			Action("Select");
			frame();
			Check(surface->GetGizmoMode() == Nuclear::Editor::GizmoMode::Select,
				  "Select toolbar button disables the transform tool");
			UIModule::Get().Render();
			Readback("Build/DebugX64/editor-select.ppm");
			PointerAt(Platform::EventType::MouseButton, lookStart, true, MOUSE_BUTTON_RIGHT);
			const auto cameraPosition = camera->GetPosition();
			Key(Platform::Input::KEYCODE_W);
			frame();
			Key(Platform::Input::KEYCODE_W, false);
			PointerAt(Platform::EventType::MouseButton, lookStart, false, MOUSE_BUTTON_RIGHT);
			Check(Math::dot(camera->GetPosition() - cameraPosition, camera->GetFrontView()) > .03f &&
					  surface->GetGizmoMode() == Nuclear::Editor::GizmoMode::Select &&
					  movedBox.GetLocalPosition() == committed,
				  "WASD runs through editor updates during right-look and preserves Select mode");
			Action("Add");
			Action("Shapes  >");
			Action("Sphere");
			frame();
			auto sphere = Entity("Sphere");
			Check(sphere != entt::null, "Retained toolbar creates a sphere");
			historyMenu("Undo");
			Check(!Core::Scene::Get().GetRegistry().valid(sphere), "Edit menu undoes primitive creation");
			historyMenu("Redo");
			Check(Entity("Sphere") == sphere, "Edit menu redoes creation with the same entity identity");
			auto* name = Find<TextBox>([](const TextBox& input) {
				return input.GetText() == "Sphere";
			});
			Check(name != nullptr, "Selected entity has an editable name");
			Click(*name);
			historyKey(Platform::Input::KEYCODE_Z);
			Check(Entity("Sphere") == sphere, "Scene history shortcuts leave focused text editing alone");
			const auto inspectorCameraPosition = camera->GetPosition();
			Key(Platform::Input::KEYCODE_W);
			Key(Platform::Input::KEYCODE_G);
			frame();
			Key(Platform::Input::KEYCODE_W, false);
			Key(Platform::Input::KEYCODE_G, false);
			Check(camera->GetPosition() == inspectorCameraPosition &&
					  surface->GetGizmoMode() == Nuclear::Editor::GizmoMode::Select,
				  "Inspector keyboard focus isolates camera movement and transform shortcuts");
			Key(Platform::Input::KEYCODE_LCTRL);
			Key(Platform::Input::KEYCODE_A);
			Key(Platform::Input::KEYCODE_LCTRL, false);
			Text("Test Sphere");
			Key(Platform::Input::KEYCODE_RETURN);
			frame();
			Check(Entity("Test Sphere") == sphere, "Name commits through retained text input");
			auto* position = Find<TextBox>([](const TextBox& input) {
				return input.GetText() == "0.000";
			});
			Check(position != nullptr, "Selected entity has an editable position");
			Edit(*position, "2.5");
			frame();
			Check(Core::Scene::Get()
						  .GetRegistry()
						  .get<Components::EntityInfoComponent>(sphere)
						  .mTransform.GetLocalPosition()
						  .x == 2.5f,
				  "Numeric position commits to ECS");
			auto* lightOption = Find<TextBlock>([](const TextBlock& label) {
				return label.GetText() == "Light";
			});
			Check(lightOption && !lightOption->IsVisible(), "Component dropdown starts collapsed");
			Action("Add component  v");
			Check(lightOption->IsVisible(), "Component dropdown opens on click");
			Action("Add component  v");
			Check(!lightOption->IsVisible(), "Component dropdown closes on a second click");
			Key(Platform::Input::KEYCODE_RETURN);
			Key(Platform::Input::KEYCODE_RETURN, false);
			Check(lightOption->IsVisible(), "Component dropdown opens from the keyboard");
			frame();
			UIModule::Get().Render();
			Readback("Build/DebugX64/editor-components.ppm");
			Action("Light");
			frame();
			Check(Core::Scene::Get().GetRegistry().all_of<Components::LightComponent>(sphere),
				  "Component action updates ECS");
			Check(!Find<TextBlock>([](const TextBlock& label) { return label.GetText() == "Light"; }),
				  "Added components are removed from the dropdown choices");
			auto componentControl = [](const std::string& title, Nuclear::Editor::EditorComponentButton::Action action)
				-> Nuclear::Editor::EditorComponentButton& {
				auto* heading = Find<TextBlock>([&](const auto& label) { return label.GetText() == title; });
				Check(heading != nullptr, "Rendered inspector has the component heading");
				auto* button = Find<Nuclear::Editor::EditorComponentButton>([&](const auto& button) {
					return button.GetParent() == heading->GetParent() && button.GetAction() == action;
				});
				Check(button != nullptr, "Rendered inspector has component header controls");
				return *button;
			};
			using ComponentAction = Nuclear::Editor::EditorComponentButton::Action;
			Click(componentControl("TRANSFORM", ComponentAction::Collapse));
			UIModule::Get().Render();
			Readback("Build/DebugX64/editor-component-headers.ppm");
			for (const auto& title : {"LIGHT", "MESH"})
			{
				Click(componentControl(title, ComponentAction::Remove));
				frame();
				auto& registry = Core::Scene::Get().GetRegistry();
				Check(std::string(title) == "LIGHT" ? !registry.all_of<Components::LightComponent>(sphere)
					: !registry.all_of<Components::MeshComponent>(sphere), "Inspector X removes the selected component");
				historyMenu("Undo");
				Check(registry.all_of<Components::LightComponent, Components::MeshComponent>(sphere),
					"Undo restores component data and preview bindings");
				historyMenu("Redo");
				historyMenu("Undo");
				Check(!Find<TextBlock>([](const auto& label) { return label.GetText() == "POSITION"; })->IsVisible(),
					"Disclosure persists across removal and undo/redo inspector rebuilds");
			}
			Click(componentControl("TRANSFORM", ComponentAction::Collapse));
			auto* cameraOption = Find<TextBlock>([](const TextBlock& label) {
				return label.GetText() == "Camera";
			});
			Check(cameraOption && !cameraOption->IsVisible(), "Component selection closes the dropdown");
			Action("Add component  v");
			Action("Camera");
			Check(Core::Scene::Get().GetMainCamera() ==
				&Core::Scene::Get().GetRegistry().get<Components::CameraComponent>(Entity("Editor Camera")),
				"Adding an authoring camera preserves the preview camera before the next frame");
			frame();
			historyMenu("Undo");
			Check(!Core::Scene::Get().GetRegistry().all_of<Components::CameraComponent>(sphere), "Undo removes an authoring camera");
			historyMenu("Redo");
			Check(Core::Scene::Get().GetRegistry().all_of<Components::CameraComponent>(sphere) &&
				!Core::Scene::Get().GetRegistry().get<Components::CameraComponent>(sphere).GetIsActive(),
				"Redo restores an inactive authoring camera");
			historyMenu("Undo");
			Action("Delete selected");
			frame();
			Check(!Core::Scene::Get().GetRegistry().valid(sphere),
				  "Delete invalidates selected handle safely");
			historyMenu("Undo");
			Check(Entity("Test Sphere") == sphere &&
				Core::Scene::Get().GetRegistry().all_of<Components::MeshComponent, Components::LightComponent>(sphere) &&
				Core::Scene::Get().GetRegistry().get<Components::EntityInfoComponent>(sphere).mTransform.GetLocalPosition().x == 2.5f,
				"Undo deletion restores identity, name, transform and components");
			historyKey(Platform::Input::KEYCODE_Z);
			frame();
			Check(!Core::Scene::Get().GetRegistry().all_of<Components::LightComponent>(sphere), "Undo removes the added light");
			historyKey(Platform::Input::KEYCODE_Z);
			frame();
			Check(Core::Scene::Get().GetRegistry().get<Components::EntityInfoComponent>(sphere).mTransform.GetLocalPosition().x == 0,
				"Undo restores the committed numeric field");
			historyKey(Platform::Input::KEYCODE_Z);
			frame();
			Check(Entity("Sphere") == sphere, "Undo restores the previous entity name");
			historyKey(Platform::Input::KEYCODE_Z);
			frame();
			Check(!Core::Scene::Get().GetRegistry().valid(sphere), "Earlier creation remains undoable after deletion restoration");
			for (int index = 0; index < 5; ++index)
			{
				historyKey(Platform::Input::KEYCODE_Y);
				frame();
			}
			Check(!Core::Scene::Get().GetRegistry().valid(sphere), "Redo replays creation, edits, component addition and deletion in order");
			Action("New project");
			frame();
			historyKey(Platform::Input::KEYCODE_Z);
			frame();
			Check(!Core::Scene::Get().GetRegistry().valid(sphere), "Project modal blocks history shortcuts");
			Key(Platform::Input::KEYCODE_TAB);
			auto* focused = Context().Find(Context().GetFocusedWidget());
			Check(dynamic_cast<TextBox*>(focused) != nullptr, "Modal focus excludes the disabled workspace");
			Action("Cancel");
			frame();
			Action("Add");
			Action("Empty Entity");
			frame();
			Check(Entity("Entity") != entt::null, "Cancel restores workspace input");
			Action("New project");
			frame();
			Action("Create project");
			frame();
			Check(Entity("Entity") == entt::null && Entity("Editor Camera") != entt::null,
				  "Project creation resets scene and camera");
			Action("Edit");
			Check(!Find<TextBlock>([](const auto& label) { return label.GetText() == "Undo"; })->GetParent()->IsEnabled() &&
				!Find<TextBlock>([](const auto& label) { return label.GetText() == "Redo"; })->GetParent()->IsEnabled(),
				"New project clears undo and redo history");
			Key(Platform::Input::KEYCODE_ESCAPE);
			Action("Add");
			Action("Shapes  >");
			Action("Box");
			frame();
			engine.GetMainWindow()->SetSize(960, 600);
			editor.OnWindowResize(960, 600);
			for (int index = 0; index < 4; ++index)
				frame();
			UIModule::Get().Render();
			CheckViewportToolbar();
			Check(std::abs(Context().GetScale() - nativeWindow->GetDpiScale()) < .001f,
				"Small workspace retains the monitor's text size");
			Readback("Build/DebugX64/editor-960.ppm");
			const float dpiScale = Context().GetScale();
			const int captionX = int((titleBar->GetBounds().Width - 150) * dpiScale);
			Check(nativeWindow->IsTitleBarAt(captionX, int(20 * dpiScale)) &&
				!nativeWindow->IsTitleBarAt(int(70 * dpiScale), int(20 * dpiScale)),
				"Caption hit testing follows the smaller workspace's physical UI scale");
			Action("Capture");
			for (int index = 0; index < 8; ++index)
				frame();
			Check(Profiling::ProfilingModule::Get().GetSnapshot().mEnabled,
				  "Retained capture toggle enables profiler");
			Action("Clear history");
			Check(Profiling::ProfilingModule::Get().GetSnapshot().mFrames.empty(),
				  "Retained profiler history action");
			Action("Textures");
			frame();
			Action("Files");
			frame();
			const auto fixture = std::filesystem::absolute("Build/DebugX64/editorfixtures");
			std::filesystem::create_directories(fixture);
			{
				std::ofstream mesh(fixture / "triangle.obj");
				mesh << "v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nvt 1 0\nvt 0 1\nvn 0 0 1\nf 1/1/1 2/2/1 3/3/1\n";
			}
			auto* path = Find<TextBox>([](const TextBox& input) {
				return input.GetText() ==
					   std::filesystem::absolute(Assets::AssetLibrary::Get().GetPath()).string();
			});
			Check(path != nullptr, "Asset directory field exists");
			Edit(*path, fixture.string());
			frame();
			const auto directoryBeforeImport = std::filesystem::current_path();
			const auto meshCountBeforeImport = Assets::AssetLibrary::Get().mImportedMeshes.mData.size();
			if (skipNativePicker)
			{
				std::cout << "Native picker checks skipped; importing through the browser file row.\n";
				Action("Import / triangle.obj");
				frame();
			}
			else
			{
				ImportFile({}, frame);
				Check(Assets::AssetLibrary::Get().mImportedMeshes.mData.size() == meshCountBeforeImport,
					"Cancelling the picker creates no asset");
				ImportFile(fixture / "triangle.obj", frame);
			}
			Check(std::filesystem::current_path() == directoryBeforeImport,
				"Native file selection preserves the engine working directory");
			Action("Meshes");
			frame();
			Action("Add to scene / editorfixtures_triangle");
			frame();
			Check(Entity("editorfixtures_triangle") != entt::null,
				  "Imported geometry is ready and can be instantiated");
			const auto importedEntity = Entity("editorfixtures_triangle");
			historyMenu("Undo");
			Check(!Core::Scene::Get().GetRegistry().valid(importedEntity), "Undo removes an instantiated library mesh");
			historyMenu("Redo");
			Check(Entity("editorfixtures_triangle") == importedEntity &&
				Core::Scene::Get().GetRegistry().get<Components::MeshComponent>(importedEntity).GetMesh()->GetState() == Assets::IAsset::State::Created,
				"Redo instantiates the same library-owned ready mesh");
			Nuclear::Editor::Tests::RunWorkspaceTests(editor, frame);
			Nuclear::Editor::Tests::RunSceneSettingsTests(editor, frame);
			auto addMenu = []() { return Find<Nuclear::Editor::EditorAddMenu>([](const auto&) { return true; }); };
			Action("Add");
			Check(addMenu() && Context().GetFocusedWidget() == addMenu()->GetId(), "Add opens a keyboard-focused creation popup");
			Key(Platform::Input::KEYCODE_ESCAPE);
			Check(!addMenu(), "Escape dismisses Add");
			Action("Add");
			PointerAt(Platform::EventType::MouseButton, {1000, 70}, true);
			PointerAt(Platform::EventType::MouseButton, {1000, 70});
			Check(!addMenu(), "Outside clicks dismiss Add without activating the workspace");
			Action("Add");
			Platform::Event addFocus;
			addFocus.mType = Platform::EventType::FocusChanged;
			Context().ProcessEvent(addFocus);
			frame();
			Check(!addMenu(), "Window focus loss dismisses the creation popup");
			addFocus.mFocused = true;
			Context().ProcessEvent(addFocus);
			Action("Add");
			Action("Skybox");
			frame();
			const auto emptySky = CameraCorner();
			Check(Core::Scene::Get().GetRegistry().view<Components::MeshComponent>().empty() &&
				emptySky[2] > emptySky[0] + 25, "Skybox initializes and renders in a scene without meshes");
			historyMenu("Undo");
			Action("Add");
			Key(Platform::Input::KEYCODE_RETURN);
			frame();
			Check(Entity("Entity") != entt::null, "Keyboard Enter creates an Empty Entity");
			historyMenu("Undo");
			Action("Add");
			Key(Platform::Input::KEYCODE_DOWN);
			Key(Platform::Input::KEYCODE_RIGHT);
			Check(Find<TextBlock>([](const auto& label) { return label.GetText() == "Plane" && label.IsVisible(); }),
				"Right opens Shapes as a submenu");
			Key(Platform::Input::KEYCODE_LEFT);
			Check(!Find<TextBlock>([](const auto& label) { return label.GetText() == "Plane" && label.IsVisible(); }),
				"Left returns from Shapes to the Add menu");
			Key(Platform::Input::KEYCODE_RIGHT);
			Key(Platform::Input::KEYCODE_DOWN);
			Key(Platform::Input::KEYCODE_RETURN);
			frame();
			Check(Entity("Plane") != entt::null, "Keyboard submenu navigation creates a plane");
			historyMenu("Undo");
			Action("Add");
			Action("Shapes  >");
			Action("Box");
			frame();
			Check(Entity("Box") != entt::null, "Add / Shapes / Box creates geometry");
			const auto clearCorner = CameraCorner();
			Action("Add");
			auto* shapesLabel = Find<TextBlock>([](const auto& label) { return label.GetText() == "Shapes  >"; });
			const auto shapesBounds = shapesLabel->GetBounds();
			PointerAt(Platform::EventType::MouseMove, {shapesBounds.x + 5, shapesBounds.y + 5});
			Check(Find<TextBlock>([](const auto& label) { return label.GetText() == "Sphere" && label.IsVisible(); }),
				"Hover opens Shapes without creating an entity");
			UIModule::Get().Render();
			Readback("Build/DebugX64/editor-add-menu.ppm");
			Action("Skybox");
			frame();
			const auto sky = Entity("Skybox");
			Check(sky != entt::null && Core::Scene::Get().GetRegistry().all_of<Nuclear::Editor::SkyboxComponent>(sky),
				"Add creates a selectable skybox entity");
			const auto skyCorner = CameraCorner();
			Check(skyCorner[2] > skyCorner[0] + 25 && skyCorner[2] > clearCorner[2] + 30,
				"The default cubemap renders behind the scene");
			Check(!Find<TextBlock>([](const auto& label) { return label.GetText() == "TRANSFORM" && label.IsVisible(); }),
				"Skybox inspection offers visibility without misleading spatial transforms");
			UIModule::Get().Render();
			Readback("Build/DebugX64/editor-skybox.ppm");
			auto* skyCamera = Core::Scene::Get().GetMainCamera();
			const auto skyCameraPosition = skyCamera->GetPosition();
			skyCamera->SetPosition(skyCameraPosition + Math::Vector3(20, 0, 0));
			frame();
			const auto translatedCorner = CameraCorner();
			Check(std::abs(int(translatedCorner[0]) - int(skyCorner[0])) <= 1 &&
				std::abs(int(translatedCorner[2]) - int(skyCorner[2])) <= 1,
				"Skybox follows camera translation without shifting its horizon");
			skyCamera->SetPosition(skyCameraPosition);
			frame();
			Action("Add");
			Action("Skybox");
			frame();
			int skyCount = 0;
			for (auto entity : Core::Scene::Get().GetRegistry().view<Nuclear::Editor::SkyboxComponent>())
				++skyCount;
			Check(skyCount == 1, "Adding Skybox again selects the existing background");
			historyMenu("Undo");
			Check(!Core::Scene::Get().GetRegistry().valid(sky) && CameraCorner() == clearCorner,
				"Undo removes the skybox and restores clear-color pixels, including after duplicate Add");
			historyMenu("Redo");
			Check(Entity("Skybox") == sky && CameraCorner() == skyCorner, "Redo restores skybox identity and rendering");
			Action("Visible");
			frame();
			Check(!Core::Scene::Get().GetRegistry().get<Nuclear::Editor::SkyboxComponent>(sky).Enabled && CameraCorner() == clearCorner,
				"Skybox visibility toggles the rendered background");
			historyMenu("Undo");
			Check(CameraCorner() == skyCorner, "Skybox visibility is undoable");
			Action("Delete selected");
			frame();
			Check(!Core::Scene::Get().GetRegistry().valid(sky) && CameraCorner() == clearCorner,
				"Deleting the skybox restores scene clear color");
			historyMenu("Undo");
			Check(Core::Scene::Get().GetRegistry().all_of<Nuclear::Editor::SkyboxComponent>(sky) && CameraCorner() == skyCorner,
				"Undo deletion restores the skybox component and background");
			Action("New scene");
			frame();
			Check(Core::Scene::Get().GetRegistry().view<Nuclear::Editor::SkyboxComponent>().empty(),
				"New scene removes the skybox authoring entity");
			Action("Edit");
			Check(!Find<TextBlock>([](const auto& label) { return label.GetText() == "Undo"; })->GetParent()->IsEnabled() &&
				!Find<TextBlock>([](const auto& label) { return label.GetText() == "Redo"; })->GetParent()->IsEnabled(),
				"New scene clears both history directions while retaining the workspace");
			Key(Platform::Input::KEYCODE_ESCAPE);
			Context().Clear();
			editor.Shutdown();
			Check(Context().BuildDrawList().Quads.empty(), "Editor teardown clears all widgets");
			editor.Load();
			for (int index = 0; index < 4; ++index)
				frame();
			editor.Shutdown();
		}
		engine.Shutdown();
		Check(!engine.GetMainWindow()->GetNativeHandle(), "Engine shutdown destroys editor window");
		std::cout << "Retained editor lifecycle, actions, text, modal focus, ECS edits, profiler, resize and "
					 "readback passed.\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << "Editor test failed: " << error.what() << '\n';
		return 1;
	}
}
