#include "ViewportSelectionTests.h"
#include <NuclearEditor.h>
#include <GameViewPort.h>
#include <Assets/DefaultMeshes.h>
#include <Diligent/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace Nuclear::Editor::Tests
{
	using namespace UI;

	namespace
	{
		void Check(bool condition, const char* message)
		{
			if (!condition)
				throw std::runtime_error(message);
		}

		UIContext& Context() { return UIModule::Get().GetContext(); }

		template <class T, class Predicate> T* Find(Predicate predicate)
		{
			Widget marker;
			for (WidgetId id = 1; id < marker.GetId(); ++id)
				if (auto* widget = dynamic_cast<T*>(Context().Find(id)); widget && predicate(*widget))
					return widget;
			return nullptr;
		}

		void Click(Vec2 point, int button = MOUSE_BUTTON_LEFT)
		{
			Platform::Event event;
			event.mType = Platform::EventType::MouseButton;
			event.mMouseX = int(std::lround(point.x * Context().GetScale()));
			event.mMouseY = int(std::lround(point.y * Context().GetScale()));
			event.mButton = button;
			event.mPressed = true;
			Context().ProcessEvent(event);
			event.mPressed = false;
			Context().ProcessEvent(event);
		}

		void Action(const char* title)
		{
			auto* label = Find<TextBlock>([title](const auto& value) { return value.GetText() == title; });
			Check(label && label->GetParent(), "Selection test action exists");
			const auto bounds = label->GetParent()->GetBounds();
			Click({bounds.x + bounds.Width * .5f, bounds.y + bounds.Height * .5f});
		}

		Vec2 Project(Math::Vector3 world, Components::CameraComponent& camera, Rect viewport)
		{
			const auto point = camera.GetProjectionMatrix() * camera.mCameraData.View * Math::Vector4(world, 1);
			Check(point.w > 0, "Selection target is in front of the camera");
			return {viewport.x + (point.x / point.w + 1) * viewport.Width * .5f,
				viewport.y + (1 - point.y / point.w) * viewport.Height * .5f};
		}

		std::vector<std::array<Byte, 4>> ReadPixels(Diligent::ITexture* source)
		{
			using namespace Diligent;
			auto desc = source->GetDesc();
			desc.Usage = USAGE_STAGING;
			desc.BindFlags = BIND_NONE;
			desc.CPUAccessFlags = CPU_ACCESS_READ;
			RefCntAutoPtr<ITexture> staging;
			auto& graphics = Graphics::GraphicsModule::Get();
			graphics.GetDevice()->CreateTexture(desc, nullptr, &staging);
			Check(staging != nullptr, "Selection pixel readback allocation");
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
			Check(mapped.pData != nullptr, "Selection pixel readback mapping");
			std::vector<std::array<Byte, 4>> pixels(size_t(desc.Width) * desc.Height);
			for (unsigned y = 0; y < desc.Height; ++y)
				std::memcpy(pixels.data() + size_t(y) * desc.Width,
					static_cast<const Byte*>(mapped.pData) + y * mapped.Stride, size_t(desc.Width) * 4);
			context->UnmapTextureSubresource(staging, 0, 0);
			return pixels;
		}

		unsigned YellowPixels(Components::CameraComponent& camera)
		{
			unsigned count = 0;
			for (const auto& pixel : ReadPixels(camera.GetColorRT().GetRTV()->GetTexture()))
				if (pixel[0] > 240 && pixel[1] > 180 && pixel[2] < 20)
					++count;
			return count;
		}

		void Selected(const char* name)
		{
			const std::string row = std::string("> ") + name;
			auto* label = Find<TextBlock>([&row](const auto& value) { return value.GetText() == row; });
			Check(label && label->IsVisible(), "Viewport click selects the matching hierarchy row");
			Check(Find<TextBox>([name](const auto& value) { return value.GetText() == name; }),
				"Viewport click selects the same entity in the inspector");
		}

		void SaveFrame(const char* path)
		{
			UIModule::Get().Render();
			auto* texture = Graphics::GraphicsModule::Get().GetSwapChain()->GetCurrentBackBufferRTV()->GetTexture();
			const auto pixels = ReadPixels(texture);
			std::ofstream output(path, std::ios::binary);
			output << "P6\n" << texture->GetDesc().Width << ' ' << texture->GetDesc().Height << "\n255\n";
			for (const auto& pixel : pixels)
				output.write(reinterpret_cast<const char*>(pixel.data()), 3);
			Check(output.good(), "Selection framebuffer saved");
		}
	}

	void RunViewportSelectionTests(NuclearEditor& editor, const std::function<void()>& frame)
	{
		auto& scene = Core::Scene::Get();
		auto& registry = scene.GetRegistry();
		auto* surface = Find<GameViewPort>([](const auto&) { return true; });
		Check(surface != nullptr, "Selection viewport exists");
		auto* camera = scene.GetMainCamera();
		Assets::Material* material = nullptr;
		entt::entity box = entt::null;
		for (auto entity : registry.view<Components::MeshComponent, Components::EntityInfoComponent>())
			if (registry.get<Components::EntityInfoComponent>(entity).mName == "Box")
			{
				box = entity;
				material = registry.get<Components::MeshComponent>(entity).GetMaterial();
			}
		Check(material && box != entt::null, "Selection test borrows the preview material");
		const auto original = registry.get<Components::EntityInfoComponent>(box).mTransform;
		const auto frontPosition = camera->GetPosition() + camera->GetFrontView() * 4.f;
		auto front = scene.CreateEntity("Selection Front");
		const auto frontId = front.GetEntityID();
		front.GetComponent<Components::EntityInfoComponent>().mTransform.SetPosition(frontPosition);
		front.AddComponent<Components::MeshComponent>(Assets::DefaultMeshes::GetCubeAsset(), material);
		Action("Select");
		frame();
		const auto bounds = surface->GetBounds();
		const auto center = Project(frontPosition, *camera, bounds);
		Check(YellowPixels(*camera) == 0, "A foreground mesh completely occludes the rear selection outline");
		const auto before = ReadPixels(camera->GetColorRT().GetRTV()->GetTexture());
		Click(center);
		frame();
		Selected("Selection Front");
		Check(surface->HasFocus() && !Context().GetCapturedWidget(),
			"Click selection retains viewport keyboard focus without starting a transform drag");
		Check(YellowPixels(*camera) > 40, "Selected mesh has a yellow silhouette in the camera framebuffer");
		const auto after = ReadPixels(camera->GetColorRT().GetRTV()->GetTexture());
		const auto size = camera->GetColorRT().GetDimensions();
		const size_t centerIndex = size_t(size.y / 2) * size.x + size.x / 2;
		Check(before[centerIndex] == after[centerIndex], "Outline preserves the selected mesh's interior color");
		SaveFrame("Build/DebugX64/editor-selection.ppm");

		Click({bounds.x + 4, bounds.y + 4});
		frame();
		Check(Find<TextBlock>([](const auto& value) {
			return value.GetText() == "Select an entity in the hierarchy.";
		}), "Background click clears hierarchy and inspector selection");
		Check(YellowPixels(*camera) == 0, "Background click removes the yellow outline");
		Action("Frame selected");
		frame();
		Check(YellowPixels(*camera) == 0, "Framing an empty selection is a safe no-op");
		editor.GetWorkspace()->Close("hierarchy");
		frame();
		Click(Project(frontPosition, *camera, surface->GetBounds()));
		frame();
		Check(Find<TextBox>([](const auto& value) { return value.GetText() == "Selection Front"; }),
			"Viewport selection remains usable while the hierarchy is closed");
		editor.GetWorkspace()->Show("hierarchy");
		frame();
		Selected("Selection Front");

		// A hierarchy filter must not hide a mesh selected directly from the viewport.
		auto* filter = Find<TextBox>([](const auto& value) { return value.GetText().empty(); });
		Check(filter && filter->OnChanged, "Hierarchy filter exists");
		filter->SetText("does not match a scene entity");
		filter->OnChanged(filter->GetText());
		frame();
		Click(center);
		frame();
		Selected("Selection Front");
		Check(filter->GetText().empty(), "Viewport selection clears an excluding hierarchy filter");

		registry.get<Components::MeshComponent>(frontId).SetEnableRendering(false);
		frame();
		Click(center);
		frame();
		Selected("Box");
		Check(YellowPixels(*camera) > 20, "Picking ignores a hidden foreground mesh and outlines the rear mesh");
		registry.get<Components::MeshComponent>(frontId).SetEnableRendering(true);
		frame();
		Check(YellowPixels(*camera) == 0, "Occluded selected meshes never outline through foreground geometry");

		// Fractional scaling changes logical viewport geometry while picking stays pixel accurate.
		const auto pixels = Context().GetPixelSize();
		const float originalScale = Context().GetScale();
		Context().SetViewport(pixels.x, pixels.y, 1.25f);
		frame();
		Click(Project(frontPosition, *camera, surface->GetBounds()));
		frame();
		Selected("Selection Front");
		Check(YellowPixels(*camera) > 40, "Picking and outlines follow fractional viewport scaling");
		Context().SetViewport(pixels.x, pixels.y, originalScale);
		frame();

		Action("Move");
		frame();
		const auto empty = surface->GetBounds();
		Click({empty.x + 4, empty.y + 4});
		frame();
		Selected("Selection Front");
		Check(registry.get<Components::EntityInfoComponent>(box).mTransform.GetLocalPosition() == original.GetLocalPosition(),
			"Viewport selection does not modify scene transforms");
		registry.destroy(frontId);
		frame();
		Action("Select");
		frame();
		Click(Project(original.GetLocalPosition(), *camera, surface->GetBounds()));
		frame();
		Selected("Box");
		Action("Edit");
		auto* undo = Find<TextBlock>([](const auto& value) { return value.GetText() == "Undo"; });
		Check(undo && !undo->GetParent()->IsEnabled(), "Picking creates no scene history transactions");
		Platform::Event escape;
		escape.mType = Platform::EventType::Key;
		escape.mKey = Platform::Input::KEYCODE_ESCAPE;
		escape.mPressed = true;
		Context().ProcessEvent(escape);
		Action("Move");
		frame();
		std::cout << "Viewport picking, hierarchy selection, yellow outlines, occlusion and scaling passed.\n";
	}
}
