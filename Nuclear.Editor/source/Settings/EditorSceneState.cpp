#include <EditorSceneState.h>

namespace Nuclear::Editor
{
	EditorSceneState EditorSceneState::Capture()
	{
		EditorSceneState result;
		auto& scene = Core::Scene::Get();
		if (!scene.GetSceneAsset() || !scene.GetMainCamera())
			return result;
		result.mSceneId = scene.GetSceneAsset()->GetUUID();
		result.Name = scene.GetSceneAsset()->GetName();
		result.ClearColor = scene.GetMainCamera()->mRTClearColor.Get4f();
		return result;
	}

	bool EditorSceneState::IsValid() const
	{
		return mSceneId.isValid();
	}

	bool EditorSceneState::Restore() const
	{
		auto& scene = Core::Scene::Get();
		auto* camera = scene.GetMainCamera();
		if (!IsValid() || !scene.GetSceneAsset() || scene.GetSceneAsset()->GetUUID() != mSceneId || !camera)
			return false;
		scene.GetSceneAsset()->SetName(Name);
		camera->mRTClearColor = Graphics::Color(ClearColor.r, ClearColor.g, ClearColor.b, ClearColor.a);
		return true;
	}
}
