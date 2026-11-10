#include "MeshPreviewTests.h"
#include <AssetDocument.h>
#include <MeshPreviewViewport.h>
#include <Core/Scene.h>
#include <Assets/AssetLibrary.h>
#include <Graphics/GraphicsModule.h>
#include <UI/UIContext.h>
#include <UI/DrawList.h>
#include <Nuclear.Platform/Input.h>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace Nuclear::Editor::Tests
{
	void RunMeshPreviewTests()
	{
		auto check = [](bool value, const char* message) { if (!value) throw std::runtime_error(message); };
		auto* sceneCamera = Core::Scene::Get().GetMainCamera();
		auto& mesh = Assets::AssetLibrary::Get().mImportedMeshes.AddAsset();
		mesh.SetName("Preview test cube");
		Assets::Mesh::CreateCube(&mesh, 10, 10, 10);
		mesh.SetState(Assets::IAsset::State::Created);
		UI::UIContext context;
		context.SetViewport(960, 720, 1);
		MeshPreviewViewport* surface = nullptr;
		context.AddToViewport(AssetDocument::Build(Assets::AssetType::Mesh, mesh.GetUUID(), {}, nullptr, &surface));
		context.Layout();
		check(surface && surface->GetBounds().Height >= 180, "Mesh document allocates an interactive preview");
		surface->Render();
		auto& camera = surface->GetScene().GetCamera();
		check(camera.GetColorRT().GetSRV() != nullptr, "Mesh preview creates an output texture");
		check(camera.GetPosition().z > 20, "Framing measures the uploaded mesh dimensions");
		check(Core::Scene::Get().GetMainCamera() == sceneCamera, "Preview preserves the authored scene camera");

		using namespace Diligent;
		auto* gpu = Graphics::GraphicsModule::Get().GetContext();
		auto* source = camera.GetColorRT().GetRTV()->GetTexture();
		auto desc = source->GetDesc();
		desc.Usage = USAGE_STAGING;
		desc.BindFlags = BIND_NONE;
		desc.CPUAccessFlags = CPU_ACCESS_READ;
		RefCntAutoPtr<ITexture> staging;
		Graphics::GraphicsModule::Get().GetDevice()->CreateTexture(desc, nullptr, &staging);
		check(staging != nullptr, "Preview readback allocation");
		CopyTextureAttribs copy;
		copy.pSrcTexture = source;
		copy.pDstTexture = staging;
		copy.SrcTextureTransitionMode = copy.DstTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
		gpu->CopyTexture(copy);
		gpu->WaitForIdle();
		MappedTextureSubresource pixels;
		gpu->MapTextureSubresource(staging, 0, 0, MAP_READ, MAP_FLAG_NONE, nullptr, pixels);
		size_t geometry = 0, sky = 0, borderGeometry = 0;
		std::ofstream image("Build/DebugX64/mesh-preview.ppm", std::ios::binary);
		image << "P6\n" << desc.Width << ' ' << desc.Height << "\n255\n";
		for (Uint32 y = 0; y < desc.Height; ++y)
			for (Uint32 x = 0; x < desc.Width; ++x)
			{
				auto* pixel = static_cast<const unsigned char*>(pixels.pData) + y * pixels.Stride + x * 4;
				if (pixel[0] > 100 && pixel[0] > pixel[2] * .75f)
				{
					++geometry;
					if (x == 0 || y == 0 || x + 1 == desc.Width || y + 1 == desc.Height) ++borderGeometry;
				}
				if (pixel[2] > 60 && pixel[0] < pixel[2] * .75f) ++sky;
				image.write(reinterpret_cast<const char*>(pixel), 3);
			}
		gpu->UnmapTextureSubresource(staging, 0, 0);
		check(geometry > 100 && sky > 100, "Preview draws lit mesh geometry and the default blue sky");
		check(borderGeometry == 0, "Automatic framing fits all mesh geometry inside the preview");
		context.SetFocus(surface->GetId());
		Platform::Event key;
		key.mType = Platform::EventType::Key;
		key.mKey = Platform::Input::KEYCODE_W;
		key.mPressed = true;
		context.ProcessEvent(key);
		const auto start = camera.GetPosition();
		surface->Update(.25f);
		check(Math::length(camera.GetPosition() - start) > 1, "Focused WASD flies around the preview");
		context.SetFocus(0);
		const auto stopped = camera.GetPosition();
		surface->Update(.25f);
		check(Math::length(camera.GetPosition() - stopped) < .001f, "Focus cancellation stops held movement");
		const auto bounds = surface->GetBounds();
		UI::InputEvent pointer;
		pointer.EventType = UI::InputEvent::Type::PointerDown;
		pointer.Button = MOUSE_BUTTON_RIGHT;
		pointer.Position = {bounds.x + 20, bounds.y + 20};
		surface->OnInput(pointer);
		const auto direction = camera.GetFrontView();
		pointer.EventType = UI::InputEvent::Type::PointerMove;
		pointer.Position.x += 120;
		surface->OnInput(pointer);
		check(Math::length(camera.GetFrontView() - direction) > .1f, "Right drag turns only the preview camera");
		surface->FrameMesh();
		context.SetViewport(800, 600, 1.25f);
		context.Layout();
		surface->Render();
		check(camera.GetColorRT().GetDimensions().x == Uint32(surface->GetBounds().Width * 1.25f), "Preview render target follows document DPI");
		context.Clear();
		check(Core::Scene::Get().GetMainCamera() == sceneCamera, "Preview teardown preserves the authored scene camera");
		std::cout << "Mesh preview sky, geometry, framing, fly camera, cancellation, DPI and teardown passed.\n";
	}
}
