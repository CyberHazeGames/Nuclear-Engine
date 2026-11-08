#include <AssetDocument.h>
#include <EditorWidgets.h>
#include <Assets/AssetLibrary.h>
#include <Assets/Mesh.h>
#include <Assets/Material.h>
#include <Assets/Texture.h>

namespace Nuclear::Editor
{
	std::string AssetDocument::GetTitle(Assets::AssetType type, const std::string& name)
	{
		const auto first = name.find_first_not_of(" \t\r\n");
		if (first != std::string::npos)
			return name.substr(first, name.find_last_not_of(" \t\r\n") - first + 1);

		switch (type)
		{
		case Assets::AssetType::Texture: return "Unnamed Texture";
		case Assets::AssetType::Mesh: return "Unnamed Mesh";
		case Assets::AssetType::Material: return "Unnamed Material";
		case Assets::AssetType::Scene: return "Unnamed Scene";
		case Assets::AssetType::Shader: return "Unnamed Shader";
		case Assets::AssetType::Font: return "Unnamed Font";
		case Assets::AssetType::AudioClip: return "Unnamed Audio Clip";
		case Assets::AssetType::Animations: return "Unnamed Animation";
		default: return "Unnamed Asset";
		}
	}

	Assets::IAsset* AssetDocument::Resolve(Assets::AssetType type, const Core::UUID& id)
	{
		auto& library = Assets::AssetLibrary::Get();
		if (type == Assets::AssetType::Texture) return library.mImportedTextures.GetAsset(id);
		if (type == Assets::AssetType::Mesh) return library.mImportedMeshes.GetAsset(id);
		if (type == Assets::AssetType::Material) return library.mImportedMaterials.GetAsset(id);
		return nullptr;
	}

	std::unique_ptr<UI::Widget> AssetDocument::Build(Assets::AssetType type, const Core::UUID& id,
		std::function<void()> instantiate)
	{
		auto scroll = std::make_unique<UI::ScrollBox>();
		auto& root = scroll->Add<UI::VerticalBox>();
		root.SetPadding({20, 18, 20, 18});
		root.SetSpacing(14);
		auto* asset = Resolve(type, id);
		if (!asset)
		{
			Label(root, "This asset is no longer available in the session.").SetWrap(true);
			return scroll;
		}
		Label(root, GetTitle(type, asset->GetName()), 25).SetWrap(true);
		const char* names[] = {"Unknown", "Deserialized", "Queued", "Loaded", "Created"};
		const auto state = static_cast<size_t>(asset->GetState());
		Label(root, std::string("Resource / ") + (state < 5 ? names[state] : "Unknown"), 13);
		Label(root, "UUID / " + id.str(), 12).SetWrap(true);
		if (type == Assets::AssetType::Texture)
		{
			auto* texture = static_cast<Assets::Texture*>(asset);
			Label(root, "TEXTURE DETAILS", 14);
			Label(root, std::to_string(texture->GetWidth()) + " x " + std::to_string(texture->GetHeight()) + " pixels");
			if (asset->GetState() == Assets::IAsset::State::Created && texture->GetTextureView())
			{
				auto& image = root.Add<UI::Image>();
				const float ratio = texture->GetHeight() ? float(texture->GetWidth()) / texture->GetHeight() : 1;
				image.SetBrush({texture});
				image.SetImageSize(ratio >= 1 ? UI::Vec2{560, 560 / ratio} : UI::Vec2{420 * ratio, 420});
				auto slot = root.GetSlot(image.GetId());
				slot.Horizontal = UI::Alignment::Start;
				root.SetSlot(image.GetId(), slot);
			}
			else Label(root, "Texture preview is waiting for a ready GPU resource.").SetWrap(true);
		}
		else if (type == Assets::AssetType::Mesh)
		{
			auto* mesh = static_cast<Assets::Mesh*>(asset);
			Label(root, "MESH DETAILS", 14);
			size_t vertices = 0;
			size_t indices = 0;
			for (const auto& part : mesh->mSubMeshes)
			{
				// Upload releases decoded arrays; created buffers retain the geometry counts.
				vertices += part.mVB ? part.mVB->GetDesc().Size / sizeof(Assets::Mesh::Vertex)
					: part.data.Vertices.size();
				indices += part.mIndicesCount ? part.mIndicesCount : part.data.indices.size();
			}
			Label(root, "Submeshes / " + std::to_string(mesh->mSubMeshes.size()));
			Label(root, "Vertices / " + std::to_string(vertices));
			Label(root, "Triangles / " + std::to_string(indices / 3));
			Label(root, "Bones / " + std::to_string(mesh->mBoneInfoMap.size()));
			if (instantiate && asset->GetState() == Assets::IAsset::State::Created)
				Action(root, "Add mesh to scene", std::move(instantiate));
		}
		else if (type == Assets::AssetType::Material)
		{
			auto* material = static_cast<Assets::Material*>(asset);
			Label(root, "MATERIAL DETAILS", 14);
			if (asset->GetState() == Assets::IAsset::State::Created && material->GetShader())
				Label(root, "Shader / " + material->GetShader()->GetName()).SetWrap(true);
			Label(root, "Texture sets / " + std::to_string(material->GetTextures().size()));
			for (size_t index = 0; index < material->GetTextures().size(); ++index)
			{
				auto& set = material->GetTextures()[index];
				Label(root, "Set " + std::to_string(index) + " / " + std::to_string(set.mData.size()) + " textures", 13);
			}
		}
		return scroll;
	}
}
