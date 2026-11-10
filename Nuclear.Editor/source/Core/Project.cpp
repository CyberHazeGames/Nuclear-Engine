#include <Project.h>
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
		auto& scene = Assets::AssetLibrary::Get().mImportedScenes.AddAsset();
		scene.SetName("Untitled Scene");
		Core::Scene::Get().LoadScene(&scene);
		return &scene;
	}
} // namespace Nuclear::Editor
