#include <Project.h>
#include <Assets/Animations.h>
#include <Assets/DefaultMeshes.h>
#include <Threading/ThreadingModule.h>
#include <Fallbacks/FallbacksModule.h>
namespace Nuclear::Editor
{
	Project::Project() : mPath(std::filesystem::current_path()) {}
	void Project::Initalize(const ProjectInfo& info)
	{
		mInfo = info;
	}

	ProjectInfo Project::GetProjectInfo() const
	{
		return mInfo;
	}

	Assets::Material* Project::GetDefaultMaterial()
	{
		return mDefaultMaterial;
	}

	Assets::Material* Project::GetMeshMaterial(Assets::Mesh& mesh)
	{
		auto* material = mesh.GetImportedMaterial();
		if (material && material->GetState() == Assets::IAsset::State::Created && material->GetShader())
			return material;
		return mDefaultMaterial;
	}

	void Project::SetDefaultMaterial(Assets::Material* material)
	{
		mDefaultMaterial = material;
	}

	void Project::SetPath(const std::filesystem::path& path)
	{
		mPath = path.lexically_normal();
	}

	const std::filesystem::path& Project::GetPath() const
	{
		return mPath;
	}

	Assets::Scene* Project::AddNewScene()
	{
		// Core retains the active asset across Project lifetimes. Only the current map needs storage.
		static Assets::Scene scene;
		scene.mBinaryBuffer.clear();
		scene.SetUUID(Core::UUID::CreateNewUUID());
		scene.SetName("Untitled Scene");
		Core::Scene::Get().LoadScene(&scene);
		return &scene;
	}

	void Project::InitializeMaterials()
	{
		Assets::ShaderImportingDesc desc;
		desc.mCommonOptions.mAsyncImport = false;
		desc.mCommonOptions.mLoadOnly = true;
		desc.mExcludedVariants = {"NE_DEFFERED", "NE_SHADOWS", "NE_ANIMATION"};
		pShader = Assets::AssetManager::Get().Import<Assets::Shader>(
			"@NuclearAssets@/Shaders/BlinnPhong.NuclearShader", desc);
		if (!pShader || pShader->GetState() != Assets::IAsset::State::Loaded)
			throw std::runtime_error("Editor preview shader could not be imported");
		desc.mExcludedVariants = {"NE_DEFFERED", "NE_SHADOWS", "NE_ANIMATION", "NE_SSAO", "IBL_ENABLED"};
		pImportedShader = Assets::AssetManager::Get().Import<Assets::Shader>(
			"@NuclearAssets@/Shaders/PBR/PBR.NuclearShader", desc);
		if (!pImportedShader || pImportedShader->GetState() != Assets::IAsset::State::Loaded)
			throw std::runtime_error("Editor imported-material shader could not be imported");
		auto& material = Assets::AssetLibrary::Get().mImportedMaterials.AddAsset();
		material.SetName("Editor / Default material");
		mDefaultMaterial = &material;
	}

	std::unique_ptr<Systems::RenderSystem> Project::BuildRenderer(Rendering::GeometryPass& geometry)
	{
		auto pRenderer = std::make_unique<Systems::RenderSystem>();
		pRenderer->RegisterShader(pShader);
		pRenderer->RegisterShader(pImportedShader);
		if (pAnimatedShader) pRenderer->RegisterShader(pAnimatedShader);
		std::unordered_set<Assets::Shader*> shaders = {pShader, pImportedShader, pAnimatedShader};
		for (auto entity : Core::Scene::Get().GetRegistry().view<Components::MeshComponent>())
		{
			auto* material = Core::Scene::Get().GetRegistry().get<Components::MeshComponent>(entity).GetMaterial();
			auto* shader = material ? material->GetShader() : nullptr;
			if (shader && shaders.insert(shader).second) pRenderer->RegisterShader(shader);
		}
		Systems::RenderSystemBakingDesc desc;
		desc.RTWidth = 800;
		desc.RTHeight = 600;
		desc.mRenderToScreen = false;
		pRenderer->Bake(desc);
		pRenderer->AddRenderPass(&geometry);
		if (mDefaultMaterial->GetTextures().empty())
		{
			Assets::MaterialTextureSet set;
			set.mData.push_back(Fallbacks::FallbacksModule::Get().GetDefaultDiffuseTex());
			mDefaultMaterial->GetTextures().push_back(set);
		}
		if (mDefaultMaterial->GetState() != Assets::IAsset::State::Created)
			mDefaultMaterial->Create(pShader);
		for (auto entity : Core::Scene::Get().GetRegistry().view<Components::MeshComponent>())
		{
			auto& mesh = Core::Scene::Get().GetRegistry().get<Components::MeshComponent>(entity);
			mesh.SetMaterial(mesh.GetMaterial());
		}
		return pRenderer;
	}

	void Project::PrepareImportedMaterial(Assets::Mesh& mesh)
	{
		auto* material = mesh.GetImportedMaterial();
		if (!material || (material->GetState() == Assets::IAsset::State::Created && material->GetShader()))
			return;

		auto& sets = material->GetTextures();
		// Even untextured submeshes need a complete set; keep every existing index stable.
		for (const auto& part : mesh.mSubMeshes)
			if (sets.size() <= part.data.TexSetIndex)
				sets.resize(size_t(part.data.TexSetIndex) + 1);

		auto& fallbacks = Fallbacks::FallbacksModule::Get();
		const std::array<Assets::MaterialTexture, 5> defaults = {{
			{fallbacks.GetDefaultWhiteImage(), Assets::TextureUsageType::Albedo},
			{fallbacks.GetDefaultBlackImage(), Assets::TextureUsageType::Metallic},
			fallbacks.GetDefaultNormalTex(),
			{fallbacks.GetDefaultGreyImage(), Assets::TextureUsageType::Roughness},
			{fallbacks.GetDefaultWhiteImage(), Assets::TextureUsageType::AO}}};
		for (auto& set : sets)
		{
			for (const auto& fallback : defaults)
			{
				auto found = std::find_if(set.mData.begin(), set.mData.end(), [&fallback](const auto& texture) {
					return texture.mUsageType == fallback.mUsageType;
				});
				if (found == set.mData.end())
					set.mData.push_back(fallback);
				else if (!found->pTexture || !found->pTexture->GetTextureView())
					*found = fallback;
			}
		}
		if (mesh.GetImportedAnimations() && !mesh.GetImportedAnimations()->mClips.empty())
        {
            if (!pAnimatedShader)
            {
                Assets::ShaderImportingDesc desc;
                desc.mCommonOptions.mAsyncImport = false;
                desc.mCommonOptions.mLoadOnly = true;
                desc.mExcludedVariants = {"NE_DEFFERED", "NE_SHADOWS", "NE_SSAO", "IBL_ENABLED"};
                pAnimatedShader = Assets::AssetManager::Get().Import<Assets::Shader>("@NuclearAssets@/Shaders/PBR/PBR.NuclearShader", desc);
                if (!pAnimatedShader) throw std::runtime_error("Could not load animated PBR shader");
            }
            material->Create(pAnimatedShader);
        }
        else material->Create(pImportedShader);
	}


	Assets::Mesh* Project::ImportMesh(const std::filesystem::path& file)
	{
		const auto path = std::filesystem::weakly_canonical(file);
		if (!std::filesystem::is_regular_file(path)) throw std::runtime_error("Missing model: " + path.string());
		Assets::MeshImportingDesc desc;
		desc.mImportMaterial = true;
		desc.mSaveMaterialNames = true;
		desc.mImportAnimations = true;
		desc.mCommonOptions.mAsyncImport = false;
		desc.mCommonOptions.mLoadOnly = true;
		auto* result = Assets::AssetManager::Get().Import<Assets::Mesh>(path.string(), desc);
		// Synchronous mesh decoding still queues its GPU creation on the main thread.
		auto& threading = Threading::ThreadingModule::Get();
		while (!threading.GetMainTasks().empty())
			threading.ExecuteMainThreadTasks(1);
		if (!result || result->GetState() != Assets::IAsset::State::Created)
			throw std::runtime_error("Could not import " + path.string());
		mSources[result->GetUUID()] = path;
		if (auto* material = result->GetImportedMaterial()) mSources[material->GetUUID()] = path;
		PrepareImportedMaterial(*result);
		return result;
	}

	std::string Project::ResourceReference(Assets::IAsset* asset, const std::filesystem::path& directory) const
	{
		if (!asset) return {};
		if (asset == mDefaultMaterial) return "default";
		if (asset == Assets::DefaultMeshes::GetCubeAsset()) return "cube";
		if (asset == Assets::DefaultMeshes::GetSphereAsset()) return "sphere";
		if (asset == Assets::DefaultMeshes::GetPlaneAsset()) return "plane";
		auto source = mSources.find(asset->GetUUID());
		if (source == mSources.end()) throw std::runtime_error("Unknown asset origin: " + asset->GetName());
		auto path = source->second.lexically_relative(directory);
		if (path.empty()) path = source->second; // Separate volumes have no relative path.
		return "file:" + path.generic_string();
	}

	Assets::IAsset* Project::ResolveResource(const std::string& ref, bool material, const std::filesystem::path& directory,
		std::map<std::filesystem::path, Assets::Mesh*>& imports)
	{
		if (ref.empty()) return nullptr;
		if (material && ref == "default") return mDefaultMaterial;
		if (!material && ref == "cube") return Assets::DefaultMeshes::GetCubeAsset();
		if (!material && ref == "sphere") return Assets::DefaultMeshes::GetSphereAsset();
		if (!material && ref == "plane") return Assets::DefaultMeshes::GetPlaneAsset();
		if (!ref.starts_with("file:")) throw std::runtime_error("Invalid asset reference: " + ref);
		const auto path = std::filesystem::weakly_canonical(directory / ref.substr(5));
		auto& mesh = imports[path];
		if (!mesh) mesh = ImportMesh(path);
		if (!material) return mesh;
		auto* result = mesh->GetImportedMaterial();
		if (!result) throw std::runtime_error("Model material missing: " + ref);
		return result;
	}
} // namespace Nuclear::Editor
