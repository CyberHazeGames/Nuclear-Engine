#include "MeshMaterialImportTests.h"
#include <NuclearEditor.h>
#include <Fallbacks/FallbacksModule.h>
#include <EditorAssetButton.h>
#include <GameViewPort.h>
#include <Threading/ThreadingModule.h>
#include <fstream>
#include <array>
#include <iostream>
#include <stdexcept>

namespace Nuclear::Editor::Tests
{
	namespace
	{
		void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
		template<class T, class Predicate> T* Find(UI::UIContext& context, Predicate predicate)
		{
			UI::Widget marker;
			for (UI::WidgetId id = 1; id < marker.GetId(); ++id)
				if (auto* widget = dynamic_cast<T*>(context.Find(id)); widget && predicate(*widget)) return widget;
			return nullptr;
		}
		void WriteImage(const std::filesystem::path& path)
		{
			std::array<unsigned char, 70> bitmap{};
			bitmap[0] = 'B'; bitmap[1] = 'M'; bitmap[2] = 70;
			bitmap[10] = 54; bitmap[14] = 40; bitmap[18] = 2; bitmap[22] = 2;
			bitmap[26] = 1; bitmap[28] = 24; bitmap[34] = 16;
			bitmap[54] = bitmap[58] = bitmap[64] = 255;
			bitmap[65] = bitmap[66] = bitmap[67] = 255;
			std::ofstream file(path, std::ios::binary);
			file.write(reinterpret_cast<const char*>(bitmap.data()), bitmap.size());
		}
		void Drain()
		{
			auto& tasks = Threading::ThreadingModule::Get();
			while (!tasks.GetMainTasks().empty()) tasks.ExecuteMainThreadTasks(1);
		}
		void CheckMaterial(Assets::Mesh* mesh)
		{
			Check(mesh && mesh->GetState() == Assets::IAsset::State::Created, "Imported mesh has ready GPU geometry");
			auto* material = mesh->GetImportedMaterial();
			Check(material && !material->GetTextures().empty(), "Default mesh import retains an associated material");
			for (const auto& set : material->GetTextures())
			{
				std::array<bool, 4> roles{};
				for (const auto& texture : set.mData)
				{
					const auto& defaults = Fallbacks::FallbacksModule::Get();
					const bool fallback = texture.pTexture == defaults.GetDefaultNormalTex().pTexture ||
						texture.pTexture == defaults.GetDefaultSpecularTex().pTexture;
					Check(texture.pTexture && (texture.pTexture->GetState() == Assets::IAsset::State::Created || fallback) && texture.pTexture->GetTextureView(),
						"Material textures are ready without worker threads");
					if (texture.mUsageType < 4) roles[texture.mUsageType] = true;
				}
				Check(roles[0] && roles[1] && roles[2] && roles[3], "Albedo, metallic, normal and roughness roles are imported");
			}
		}
	}

	void RunMeshMaterialImportTests()
	{
		const auto fixture = std::filesystem::absolute("Build/DebugX64/materialfixture");
		std::filesystem::create_directories(fixture / "Textures/Raw");
		for (const auto* name : {"Fixture_A.bmp", "Fixture_N.bmp", "Fixture_M.bmp", "Fixture_R.bmp", "Explicit_N.bmp"}) WriteImage(fixture / "Textures" / name);
		WriteImage(fixture / "Textures/Raw/Fixture_AO.bmp");
		{
			std::ofstream material(fixture / "material.mtl");
			material << "newmtl First\nmap_Kd C:/MissingAuthorMachine/Textures/Fixture_A.bmp\n"
				"newmtl Second\nmap_Kd Textures/Fixture_A.bmp\nmap_Bump Textures/Explicit_N.bmp\n";
			std::ofstream mesh(fixture / "material.obj");
			mesh << "mtllib material.mtl\nv 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nvt 1 0\nvt 0 1\n"
				"vn 0 0 1\nusemtl First\nf 1/1/1 2/2/1 3/3/1\nusemtl Second\nf 1/1/1 3/3/1 2/2/1\n";
		}
		NuclearEditor editor;
		editor.Load();
		auto& context = UI::UIModule::Get().GetContext();
		auto frame = [&]() {
			auto& engine = Core::Engine::Get();
			engine.BeginFrame(); editor.Update(1.f / 60); editor.Render(1.f / 60); engine.EndFrame();
		};
		frame();
		auto* path = Find<UI::TextBox>(context, [](const auto& input) {
			return input.GetText() == std::filesystem::absolute(Assets::AssetLibrary::Get().GetPath()).string();
		});
		Check(path != nullptr, "Editor file browser has a directory field");
		path->OnCommitted(fixture.string());
		frame();
		auto* action = Find<UI::TextBlock>(context, [](const auto& label) { return label.IsVisible() && label.GetText() == "Import / material.obj"; });
		Check(action != nullptr, "Fixture is importable through the editor");
		const auto before = Assets::AssetLibrary::Get().mImportedTextures.mData.size();
		dynamic_cast<UI::Button*>(action->GetParent())->OnClicked();
		frame();
		Assets::Mesh* imported = nullptr;
		for (auto& [id, mesh] : Assets::AssetLibrary::Get().mImportedMeshes.mData)
			if (mesh.GetName() == "materialfixture_material") imported = &mesh;
		CheckMaterial(imported);
		auto* importedMaterial = imported->GetImportedMaterial();
		Check(importedMaterial->GetState() == Assets::IAsset::State::Created && importedMaterial->GetShader() &&
			importedMaterial->GetShader()->GetName() == "PBR", "Editor assigns the PBR shader to the imported library material");
		Check(importedMaterial->GetShader()->GetShaderPipeline().GetVariants().size() == 1,
			"Editor bakes only the supported static forward PBR variant");
		for (const auto& part : imported->mSubMeshes)
			Check(part.data.TexSetIndex < importedMaterial->GetTextures().size(), "Every imported submesh retains a valid texture set");
		Check(Assets::AssetLibrary::Get().mImportedTextures.mData.size() == before + 5, "Shared material maps import once and Raw files are excluded");
		bool explicitNormal = false;
		for (const auto& set : imported->GetImportedMaterial()->GetTextures())
			for (const auto& texture : set.mData)
				if (texture.mUsageType == Assets::TextureUsageType::Normal && texture.pTexture->GetName().find("Explicit_N") != std::string::npos) explicitNormal = true;
		Check(explicitNormal, "An explicit normal map takes precedence over filename discovery");
		auto* textures = Find<UI::TextBlock>(context, [](const auto& label) { return label.IsVisible() && label.GetText() == "Textures"; });
		dynamic_cast<UI::Button*>(textures->GetParent())->OnClicked();
		frame();
		Check(Find<UI::TextBlock>(context, [](const auto& label) { return label.IsVisible() && label.GetText().find("Fixture_R") != std::string::npos; }) != nullptr,
			"Imported companion maps appear in the editor Textures category");
		auto actionByText = [&](const std::string& text) {
			auto* label = Find<UI::TextBlock>(context, [&text](const auto& item) { return item.IsVisible() && item.GetText() == text; });
			UI::Button* button = nullptr;
			for (auto* parent = label ? label->GetParent() : nullptr; parent; parent = parent->GetParent())
				if ((button = dynamic_cast<UI::Button*>(parent))) break;
			Check(button != nullptr, ("Action is available: " + text).c_str());
			button->OnClicked();
			frame();
		};
		actionByText("Materials");
		Check(Assets::AssetLibrary::Get().mImportedMaterials.GetAsset(importedMaterial->GetUUID()) == importedMaterial,
			"Imported material retains its browser-resolvable library identity");
		actionByText(importedMaterial->GetName());
		Check(Find<UI::TextBlock>(context, [](const auto& label) { return label.IsVisible() && label.GetText() == "Shader / PBR"; }),
			"Material properties display the automatically assigned shader");
		for (const auto* name : {"First", "Second"})
			Check(Find<UI::TextBlock>(context, [name](const auto& label) { return label.IsVisible() && label.GetText().find(std::string(" / ") + name + " / ") != std::string::npos; }),
				"Material properties display named source texture sets");
		Check(Find<UI::TextBlock>(context, [](const auto& label) { return label.IsVisible() && label.GetText() == "MATERIAL DETAILS"; }),
			"Imported material opens as a material document");
		editor.GetWorkspace()->Show("viewport");
		frame();
		actionByText("Meshes");
		actionByText("Add to scene / " + imported->GetName());
		auto& registry = Core::Scene::Get().GetRegistry();
		auto instances = [&]() {
			std::vector<entt::entity> result;
			for (auto entity : registry.view<Components::MeshComponent>())
				if (registry.get<Components::MeshComponent>(entity).GetMesh() == imported) result.push_back(entity);
			return result;
		};
		Check(instances().size() == 1 && registry.get<Components::MeshComponent>(instances()[0]).GetMaterial() == importedMaterial,
			"Browser Add to scene assigns the imported material");
		Check(registry.get<Components::MeshComponent>(instances()[0]).GetRenderQueue() > 0,
			"Imported PBR geometry reaches draw submission");

		EditorAssetButton* source = nullptr;
		auto* label = Find<UI::TextBlock>(context, [imported](const auto& text) { return text.IsVisible() && text.GetText() == imported->GetName(); });
		for (auto* parent = label ? label->GetParent() : nullptr; parent; parent = parent->GetParent())
			if (auto* button = dynamic_cast<EditorAssetButton*>(parent)) { source = button; break; }
		auto* viewport = Find<GameViewPort>(context, [](const auto& surface) { return surface.IsVisible(); });
		Check(source && viewport, "Imported mesh tile exposes viewport placement");
		const auto bounds = viewport->GetBounds();
		UI::InputEvent drag;
		drag.EventType = UI::InputEvent::Type::PointerDown;
		drag.Button = MOUSE_BUTTON_LEFT;
		drag.Position = {source->GetBounds().x + 5, source->GetBounds().y + 5};
		source->OnInput(drag);
		drag.EventType = UI::InputEvent::Type::PointerMove;
		drag.Position = {bounds.x + bounds.Width * .5f, bounds.y + bounds.Height * .5f};
		source->OnInput(drag);
		drag.EventType = UI::InputEvent::Type::PointerUp;
		source->OnInput(drag);
		frame();
		Check(instances().size() == 2, "Imported mesh can be placed by dragging");
		for (auto entity : instances())
			Check(registry.get<Components::MeshComponent>(entity).GetMaterial() == importedMaterial, "Both insertion routes use the imported material");
		// Mesh-document insertion uses the same material and participates in scene history.
		source->OnClicked();
		frame();
		actionByText("Add mesh to scene");
		Check(instances().size() == 3, "Mesh document inserts another instance");
		for (auto entity : instances())
			Check(registry.get<Components::MeshComponent>(entity).GetMaterial() == importedMaterial,
				"Mesh document preserves the imported material reference");
		auto historyKey = [&](Platform::Input::KeyCode key) {
			Platform::Event event;
			event.mType = Platform::EventType::Key;
			event.mPressed = true;
			event.mKey = Platform::Input::KEYCODE_LCTRL;
			context.ProcessEvent(event);
			event.mKey = key;
			context.ProcessEvent(event);
			event.mPressed = false;
			context.ProcessEvent(event);
			event.mKey = Platform::Input::KEYCODE_LCTRL;
			context.ProcessEvent(event);
			frame();
		};
		historyKey(Platform::Input::KEYCODE_Z);
		Check(instances().size() == 2, "Undo removes document insertion");
		historyKey(Platform::Input::KEYCODE_Y);
		Check(instances().size() == 3, "Redo restores document insertion after renderer rebaking");
		for (auto entity : instances())
		{
			auto& component = registry.get<Components::MeshComponent>(entity);
			Check(component.GetMaterial() == importedMaterial && component.GetRenderQueue() > 0,
				"Undo/redo retains the imported material and its baked rendering variant");
		}
		editor.GetWorkspace()->Show("viewport");
		frame();

		// An untextured source slot must survive beside a textured slot and receive complete defaults.
		{
			std::ofstream material(fixture / "sparse.mtl");
			material << "newmtl Textured\nmap_Kd Textures/Fixture_A.bmp\nnewmtl Untextured\nKd 1 1 1\n";
			std::ofstream mesh(fixture / "sparse.obj");
			mesh << "mtllib sparse.mtl\nv 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nvt 1 0\nvt 0 1\n"
				"vn 0 0 1\nusemtl Textured\nf 1/1/1 2/2/1 3/3/1\nusemtl Untextured\nf 1/1/1 3/3/1 2/2/1\n";
		}
		actionByText("Files");
		actionByText("Import / sparse.obj");
		Assets::Mesh* sparse = nullptr;
		for (auto& [id, mesh] : Assets::AssetLibrary::Get().mImportedMeshes.mData)
			if (mesh.GetName() == "materialfixture_sparse") sparse = &mesh;
		Check(sparse && sparse->GetImportedMaterial()->GetTextures().size() == 2,
			"Untextured and textured source materials retain separate texture sets");
		auto& sparseSets = sparse->GetImportedMaterial()->GetTextures();
		for (const auto& set : sparseSets)
		{
			std::array<bool, 5> roles{};
			for (const auto& texture : set.mData)
			{
				Check(texture.pTexture && texture.pTexture->GetTextureView(), "Every default has a ready GPU view");
				if (texture.mUsageType < 5) roles[texture.mUsageType] = true;
			}
			Check(std::all_of(roles.begin(), roles.end(), [](bool role) { return role; }), "Sparse material sets have all five PBR roles");
		}
		for (const auto& part : sparse->mSubMeshes)
			Check(part.data.TexSetIndex < sparseSets.size(), "Sparse submeshes retain their source slots");
		auto untextured = std::find_if(sparseSets.begin(), sparseSets.end(), [](const auto& set) { return set.mName == "Untextured"; });
		Check(untextured != sparseSets.end(), "Untextured source material keeps its name");
		Check(untextured->mData[0].pTexture == Fallbacks::FallbacksModule::Get().GetDefaultWhiteImage(),
			"Untextured slot receives a white albedo rather than another source material's map");
		const auto cerberus = std::filesystem::absolute("../Assets/Meshes/Cerberus_by_Andrew_Maximov/Cerberus_LP.FBX");
		if (std::filesystem::exists(cerberus))
		{
			Assets::MeshImportingDesc desc;
			desc.mImportAnimations = false;
			desc.mCommonOptions.mAsyncImport = false;
			desc.mCommonOptions.mLoadOnly = true;
			auto* mesh = Assets::AssetManager::Get().Import<Assets::Mesh>(cerberus.string(), desc);
			Drain();
			CheckMaterial(mesh);
			std::cout << "Local Cerberus FBX imported with ready A/N/M/R textures.\n";
		}
		else std::cout << "Local Cerberus content absent; fixture checks cover the import policy.\n";
		editor.Shutdown();
		std::cout << "Editor PBR assignment, material browser/documents, all insertion routes, undo/redo, sparse slots and texture import passed.\n";
	}
}
