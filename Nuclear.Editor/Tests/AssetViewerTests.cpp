#include "AssetViewerTests.h"
#include <NuclearEditor.h>
#include <AssetDocument.h>
#include <AssetViewerLayout.h>
#include <TexturePreviewViewport.h>
#include <MeshPreviewViewport.h>
#include <Assets/TextureDesc.h>
#include <Fallbacks/FallbacksModule.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <array>

namespace Nuclear::Editor::Tests
{
	namespace
	{
		void Check(bool value, const char* message)
		{
			if (!value) throw std::runtime_error(message);
		}
		template<class T, class Predicate> T* Find(UI::UIContext& context, Predicate predicate)
		{
			UI::Widget marker;
			for (UI::WidgetId id = 1; id < marker.GetId(); ++id)
				if (auto* widget = dynamic_cast<T*>(context.Find(id)); widget && predicate(*widget)) return widget;
			return nullptr;
		}
		void Click(UI::Widget& widget)
		{
			auto& context = *widget.GetContext();
			context.Layout();
			for (auto* parent = widget.GetParent(); parent; parent = parent->GetParent())
				if (auto* scroll = dynamic_cast<UI::ScrollBox*>(parent))
				{
					scroll->ScrollIntoView(widget);
					context.Layout();
				}
			const auto bounds = widget.GetBounds();
			Platform::Event event;
			event.mType = Platform::EventType::MouseButton;
			event.mButton = MOUSE_BUTTON_LEFT;
			event.mMouseX = int((bounds.x + bounds.Width * .5f) * context.GetScale());
			event.mMouseY = int((bounds.y + bounds.Height * .5f) * context.GetScale());
			event.mPressed = true;
			context.ProcessEvent(event);
			event.mPressed = false;
			context.ProcessEvent(event);
		}
		void Action(UI::UIContext& context, const std::string& name)
		{
			auto* label = Find<UI::TextBlock>(context, [&](const auto& text) { return text.IsVisible() && text.GetText() == name; });
			UI::Button* button = nullptr;
			for (auto* parent = label ? label->GetParent() : nullptr; parent; parent = parent->GetParent())
				if ((button = dynamic_cast<UI::Button*>(parent))) break;
			Check(button != nullptr, ("Viewer action exists: " + name).c_str());
			Click(*button);
		}
		Math::Vector3 Readback(Diligent::ITexture* source, const char* path)
		{
			using namespace Diligent;
			auto& graphics = Graphics::GraphicsModule::Get();
			auto* context = graphics.GetContext();
			auto desc = source->GetDesc();
			desc.Usage = USAGE_STAGING;
			desc.BindFlags = BIND_NONE;
			desc.CPUAccessFlags = CPU_ACCESS_READ;
			RefCntAutoPtr<ITexture> staging;
			graphics.GetDevice()->CreateTexture(desc, nullptr, &staging);
			Check(staging != nullptr, "Viewer readback allocation succeeds");
			context->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);
			CopyTextureAttribs copy;
			copy.pSrcTexture = source;
			copy.pDstTexture = staging;
			copy.SrcTextureTransitionMode = copy.DstTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
			context->CopyTexture(copy);
			context->WaitForIdle();
			MappedTextureSubresource mapped;
			context->MapTextureSubresource(staging, 0, 0, MAP_READ, MAP_FLAG_NONE, nullptr, mapped);
			Check(mapped.pData != nullptr, "Viewer framebuffer can be read back");
			std::ofstream image(path, std::ios::binary);
			image << "P6\n" << desc.Width << ' ' << desc.Height << "\n255\n";
			Math::Vector3 center(0);
			size_t samples = 0;
			for (Uint32 y = 0; y < desc.Height; ++y)
				for (Uint32 x = 0; x < desc.Width; ++x)
				{
					const auto* pixel = static_cast<const unsigned char*>(mapped.pData) + y * mapped.Stride + x * 4;
					image.write(reinterpret_cast<const char*>(pixel), 3);
					if (x > desc.Width * .45f && x < desc.Width * .55f && y > desc.Height * .45f && y < desc.Height * .55f)
					{
						center += Math::Vector3(pixel[0], pixel[1], pixel[2]);
						++samples;
					}
				}
			context->UnmapTextureSubresource(staging, 0, 0);
			return center / float(std::max<size_t>(1, samples));
		}
		void CheckPanes(UI::UIContext& context)
		{
			auto* layout = Find<AssetViewerLayout>(context, [](const auto& layout) { return layout.IsVisible(); });
			Check(layout != nullptr, "Asset document owns a common preview/properties layout");
			const auto bounds = layout->GetBounds();
			const auto preview = layout->GetPreview().GetBounds();
			const auto properties = layout->GetProperties().GetBounds();
			Check(preview.Width > properties.Width && preview.x + preview.Width < properties.x &&
				std::abs(preview.y - properties.y) < .01f && std::abs(preview.Height - bounds.Height) < .01f &&
				std::abs(properties.Height - bounds.Height) < .01f, "Preview fills the left side and properties fill the right side");
		}
	}

	void RunAssetViewerTests()
	{
		NuclearEditor editor;
		editor.Load();
		auto frame = [&]() {
			auto& engine = Core::Engine::Get();
			engine.BeginFrame(); editor.Update(1.f / 60); editor.Render(1.f / 60); engine.EndFrame();
		};
		frame();
		auto& context = UI::UIModule::Get().GetContext();
		auto* workspace = editor.GetWorkspace();
		auto& library = Assets::AssetLibrary::Get();
		Assets::Shader* shader = nullptr;
		for (auto& [id, asset] : library.mImportedShaders.mData)
			if (asset.GetName() == "PBR") shader = &asset;
		Check(shader != nullptr, "Editor has a ready PBR shader for the material fixture");
		auto* originalPipeline = shader->GetShaderPipeline().GetVariant(0)->mPipeline.RawPtr();
		const auto originalVariants = shader->GetShaderPipeline().GetVariants().size();
		auto makeTexture = [](const char* name, Byte red, Byte blue) {
			std::array<Byte, 32> pixels{};
			for (size_t index = 0; index < pixels.size(); index += 4)
			{
				pixels[index] = red;
				pixels[index + 2] = blue;
				pixels[index + 3] = 255;
			}
			Assets::TextureDesc desc;
			desc.mPath = name;
			desc.mType = Diligent::RESOURCE_DIM_TEX_2D;
			desc.mWidth = 4;
			desc.mHeight = 2;
			desc.mBitsPerPixel = 32;
			desc.mNumComponents = 4;
			desc.mRowStride = 16;
			desc.mComponentType = Diligent::VT_UINT8;
			desc.mMipLevels = 1;
			desc.mData = pixels.data();
			return Assets::Importer::Get().ImportTexture(desc);
		};
		auto* red = makeTexture("Viewer red texture", 255, 0);
		auto* blue = makeTexture("Viewer blue texture", 0, 255);
		Check(red && blue && red->GetTextureView() && blue->GetTextureView(), "Fixture color maps have GPU resources");
		red->SetName("Viewer red texture");
		blue->SetName("Viewer blue texture");
		// The raw-descriptor helper leaves Loaded after a completed GPU upload.
		// These fixture views were checked above, so mark them ready for retained image sampling.
		red->SetState(Assets::IAsset::State::Created);
		blue->SetState(Assets::IAsset::State::Created);
		auto& material = library.mImportedMaterials.AddAsset();
		material.SetName("Viewer PBR material");
		for (auto* texture : {red, blue})
		{
			Assets::MaterialTextureSet set;
			set.mName = texture == red ? "Red" : "Blue";
			set.mData.push_back({texture, Assets::TextureUsageType::Albedo});
			material.GetTextures().push_back(set);
		}
		material.Create(shader);
		auto* authoredCamera = Core::Scene::Get().GetMainCamera();
		const auto authoredPosition = authoredCamera->GetPosition();
		const auto authoredEntities = Core::Scene::Get().GetRegistry().view<Components::EntityInfoComponent>().size();
		Action(context, "Materials");
		frame();
		bool materialThumbnail = false;
		for (const auto& quad : context.BuildDrawList().Quads)
			if (quad.pTexture && quad.Bounds.Width == 72 && quad.Bounds.Height == 72) materialThumbnail = true;
		Check(materialThumbnail, "Materials category renders sphere thumbnail images");
		UI::UIModule::Get().Render();
		Readback(Graphics::GraphicsModule::Get().GetSwapChain()->GetCurrentBackBufferRTV()->GetTexture(), "Build/DebugX64/material-browser.ppm");
		Action(context, material.GetName());
		frame();
		CheckPanes(context);
		auto* surface = Find<MeshPreviewViewport>(context, [](const auto& surface) { return surface.IsVisible(); });
		Check(surface != nullptr, "Clicking a material thumbnail opens its interactive sphere scene");
		auto first = Readback(surface->GetScene().GetTexture().GetTextureView()->GetTexture(), "Build/DebugX64/material-sphere-red.ppm");
		Check(first.r > first.b + 30, "PBR preview sphere samples the first source texture set");
		Action(context, "Preview set / Blue");
		frame();
		auto second = Readback(surface->GetScene().GetTexture().GetTextureView()->GetTexture(), "Build/DebugX64/material-sphere-blue.ppm");
		Check(second.b > second.r + 30, "Choosing a texture set changes the showcased sphere material");
		Check(shader->GetShaderPipeline().GetVariant(0)->mPipeline.RawPtr() == originalPipeline &&
			shader->GetShaderPipeline().GetVariants().size() == originalVariants, "Sphere rendering preserves the library shader's baked scene pipeline");
		Check(Core::Scene::Get().GetMainCamera() == authoredCamera && authoredCamera->GetPosition() == authoredPosition &&
			Core::Scene::Get().GetRegistry().view<Components::EntityInfoComponent>().size() == authoredEntities,
			"Material previews add no authored entities or camera changes");
		UI::UIModule::Get().Render();
		Readback(Graphics::GraphicsModule::Get().GetSwapChain()->GetCurrentBackBufferRTV()->GetTexture(), "Build/DebugX64/material-viewer.ppm");

		auto* viewer = Find<AssetViewerLayout>(context, [](const auto& viewer) { return viewer.IsVisible(); });
		const auto initialWidth = viewer->GetPreview().GetBounds().Width;
		UI::InputEvent pointer;
		pointer.EventType = UI::InputEvent::Type::PointerDown;
		pointer.Button = MOUSE_BUTTON_LEFT;
		pointer.Position = {viewer->GetPreview().GetBounds().x + initialWidth + 2, viewer->GetBounds().y + 50};
		viewer->OnInput(pointer);
		pointer.EventType = UI::InputEvent::Type::PointerMove;
		pointer.Position.x -= 80;
		viewer->OnInput(pointer);
		context.Layout();
		Check(viewer->GetPreview().GetBounds().Width < initialWidth - 60, "Asset divider resizes the preview and property panes");
		UI::InputEvent cancel;
		cancel.EventType = UI::InputEvent::Type::Cancel;
		viewer->OnInput(cancel);
		context.Layout();
		Check(std::abs(viewer->GetPreview().GetBounds().Width - initialWidth) < 1, "Cancelling divider movement restores its starting proportions");

		const auto materialId = workspace->GetArea("viewport")->GetActive();
		workspace->Detach(materialId);
		frame();
		Check(workspace->GetWindows().size() == 1, "Material document can move into a native window");
		auto& floating = *workspace->GetWindows().front();
		floating.GetWindow().Display(false);
		CheckPanes(floating.GetContext());
		Check(surface->GetContext() == &floating.GetContext(), "Native transfer preserves the material preview instance");
		const auto pixels = floating.GetContext().GetPixelSize();
		floating.GetContext().SetViewport(pixels.x, pixels.y, 1.5f);
		floating.GetContext().Layout();
		surface->Render();
		Check(surface->GetScene().GetCamera().GetColorRT().GetDimensions().x == Uint32(surface->GetBounds().Width * 1.5f),
			"Detached material target follows its context's DPI");
		floating.Render();
		Readback(floating.GetSwapChain()->GetCurrentBackBufferRTV()->GetTexture(), "Build/DebugX64/material-viewer-detached.ppm");
		workspace->Dock(materialId, EditorDockRegion::Documents);
		frame();
		workspace->Show("viewport");
		frame();
		Action(context, "Textures");
		frame();
		Action(context, red->GetName());
		frame();
		CheckPanes(context);
		auto* textureSurface = Find<TexturePreviewViewport>(context, [](const auto& surface) { return surface.IsVisible(); });
		Check(textureSurface != nullptr, "Texture document dedicates its left pane to the texture");
		bool fitted = false;
		for (const auto& quad : context.BuildDrawList().Quads)
			if (quad.pTexture == red->GetTextureView() && quad.Bounds.Width > 200)
				fitted = std::abs(quad.Bounds.Width / quad.Bounds.Height - 2) < .01f;
		Check(fitted, "Texture preview fits the pane while preserving its two-to-one aspect ratio");
		UI::UIModule::Get().Render();
		Readback(Graphics::GraphicsModule::Get().GetSwapChain()->GetCurrentBackBufferRTV()->GetTexture(), "Build/DebugX64/texture-viewer.ppm");

		UI::UIContext meshContext;
		meshContext.SetViewport(960, 640, 1.25f);
		auto& mesh = library.mImportedMeshes.AddAsset();
		Assets::Mesh::CreateCube(&mesh);
		meshContext.AddToViewport(AssetDocument::Build(Assets::AssetType::Mesh, mesh.GetUUID()));
		meshContext.Layout();
		CheckPanes(meshContext);
		meshContext.Clear();
		editor.Shutdown();
		std::cout << "Asset viewer thumbnails, left/right panes, material shader/set isolation, texture fit, divider, docking and DPI passed.\n";
	}
}
