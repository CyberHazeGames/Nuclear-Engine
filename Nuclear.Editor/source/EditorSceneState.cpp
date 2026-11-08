#include <EditorSceneState.h>

namespace Nuclear::Editor
{
	EditorSceneState EditorSceneState::Capture()
	{
		EditorSceneState result;
		auto& scene = Core::Scene::Get();
		if (!scene.GetSceneAsset() || !scene.GetMainCamera())
			return result;
		result.pScene = scene.GetSceneAsset();
		result.Name = result.pScene->GetName();
		result.ClearColor = scene.GetMainCamera()->mRTClearColor.Get4f();
		return result;
	}

	bool EditorSceneState::IsValid() const
	{
		return pScene != nullptr;
	}

	bool EditorSceneState::Restore() const
	{
		auto& scene = Core::Scene::Get();
		auto* camera = scene.GetMainCamera();
		if (!IsValid() || scene.GetSceneAsset() != pScene || !camera)
			return false;
		pScene->SetName(Name);
		camera->mRTClearColor = Graphics::Color(ClearColor.r, ClearColor.g, ClearColor.b, ClearColor.a);
		return true;
	}
}
