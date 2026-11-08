#include <NuclearEditor.h>
#include <GameViewPort.h>
#include "TransformGizmoTests.h"
#include "WorkspaceTests.h"
#include <Nuclear.Platform/Platform.h>
#include <Profiling/ProfilingModule.h>
#include <fstream>
#include <cmath>
#include <iostream>
#include <stdexcept>

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
	void CheckViewportToolbar()
	{
		Context().Layout();
		auto* surface = Find<Nuclear::Editor::GameViewPort>([](const auto&) { return true; });
		Check(surface != nullptr, "Viewport exists for toolbar layout check");
		const auto viewport = surface->GetBounds();
		float rowY = -1;
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
			Check(rowY < 0 || std::abs(rowY - bounds.y) < .01f, "Viewport actions share one row");
			rowY = bounds.y;
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
	void Readback(const char* path)
	{
		using namespace Diligent;
		auto& graphics = Graphics::GraphicsModule::Get();
		auto* context = graphics.GetContext();
		auto* source = graphics.GetSwapChain()->GetCurrentBackBufferRTV()->GetTexture();
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
		Check(
			scenePixels > 200,
			"Scene framebuffer contains neutral shaded geometry, independently of labels and colored gizmos");
	}
} // namespace

int main()
{
	try
	{
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
		auto& engine = Core::Engine::Get();
		Check(engine.Start(desc), "Editor engine startup");
		engine.GetMainWindow()->Display(false);
		{
			NuclearEditor editor;
			editor.Load();
			auto frame = [&editor, &engine]() {
				engine.BeginFrame();
				editor.Update(1.f / 60);
				editor.Render(1.f / 60);
				engine.EndFrame();
			};
			for (int index = 0; index < 4; ++index)
				frame();
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
			Action("+ Sphere");
			frame();
			auto sphere = Entity("Sphere");
			Check(sphere != entt::null, "Retained toolbar creates a sphere");
			auto* name = Find<TextBox>([](const TextBox& input) {
				return input.GetText() == "Sphere";
			});
			Check(name != nullptr, "Selected entity has an editable name");
			Click(*name);
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
			auto* cameraOption = Find<TextBlock>([](const TextBlock& label) {
				return label.GetText() == "Camera";
			});
			Check(cameraOption && !cameraOption->IsVisible(), "Component selection closes the dropdown");
			Action("Delete selected");
			frame();
			Check(!Core::Scene::Get().GetRegistry().valid(sphere),
				  "Delete invalidates selected handle safely");
			Action("New project");
			frame();
			Key(Platform::Input::KEYCODE_TAB);
			auto* focused = Context().Find(Context().GetFocusedWidget());
			Check(dynamic_cast<TextBox*>(focused) != nullptr, "Modal focus excludes the disabled workspace");
			Action("Cancel");
			frame();
			Action("+ Entity");
			frame();
			Check(Entity("Entity") != entt::null, "Cancel restores workspace input");
			Action("New project");
			frame();
			Action("Create project");
			frame();
			Check(Entity("Entity") == entt::null && Entity("Editor Camera") != entt::null,
				  "Project creation resets scene and camera");
			Action("+ Box");
			frame();
			engine.GetMainWindow()->SetSize(960, 600);
			editor.OnWindowResize(960, 600);
			for (int index = 0; index < 4; ++index)
				frame();
			UIModule::Get().Render();
			CheckViewportToolbar();
			Readback("Build/DebugX64/editor-960.ppm");
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
			Action("Import / triangle.obj");
			frame();
			Action("Meshes");
			frame();
			Action("Add to scene / editorfixtures_triangle");
			frame();
			Check(Entity("editorfixtures_triangle") != entt::null,
				  "Imported geometry is ready and can be instantiated");
			Nuclear::Editor::Tests::RunWorkspaceTests(editor, frame);
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
