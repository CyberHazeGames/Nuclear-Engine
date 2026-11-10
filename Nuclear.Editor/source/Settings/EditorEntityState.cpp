#include <EditorEntityState.h>
#include <SkyboxComponent.h>

namespace Nuclear::Editor
{
	namespace
	{
		template<class T> void RestorePresence(entt::registry& registry, entt::entity entity, bool present)
		{
			if (present)
				registry.get_or_emplace<T>(entity);
			else
				registry.remove<T>(entity);
		}
	}

	EditorEntityState EditorEntityState::Capture(entt::entity entity)
	{
		EditorEntityState result;
		auto& registry = Core::Scene::Get().GetRegistry();
		if (!registry.valid(entity))
			return result;
		auto* info = registry.try_get<Components::EntityInfoComponent>(entity);
		if (!info)
			return result;
		result.Exists = true;
		result.mName = info->mName;
		result.Transform = info->mTransform;
		if (auto* mesh = registry.try_get<Components::MeshComponent>(entity))
			result.mMesh = MeshData{mesh->GetMesh(), mesh->GetMaterial(), mesh->GetEnableRendering(),
				mesh->GetCastShadow(), mesh->GetReceiveShadows()};
		if (auto* light = registry.try_get<Components::LightComponent>(entity))
		{
			const auto color = light->GetColor();
			result.mLight = LightData{light->GetType(), light->GetDirection(), {color.r, color.g, color.b},
				light->GetAttenuation(), light->GetSpotlightCone(), light->GetIntensity(),
				light->GetNearPlane(), light->GetFarPlane(), light->GetFOV(), light->mCastShadows};
		}
		if (auto* body = registry.try_get<Components::RigidBodyComponent>(entity))
			result.mKinematic = body->isKinematic;
		if (auto* sky = registry.try_get<SkyboxComponent>(entity))
			result.mSkybox = sky->Enabled;
		result.mCamera = registry.all_of<Components::CameraComponent>(entity);
		result.mCollider = registry.all_of<Components::ColliderComponent>(entity);
		result.mScript = registry.all_of<Components::ScriptComponent>(entity);
		result.mListener = registry.all_of<Components::AudioListenerComponent>(entity);
		result.mSource = registry.all_of<Components::AudioSourceComponent>(entity);
		return result;
	}

	bool EditorEntityState::Restore(entt::entity entity) const
	{
		auto& scene = Core::Scene::Get();
		auto& registry = scene.GetRegistry();
		if (!Exists)
		{
			if (registry.valid(entity))
				registry.destroy(entity);
			return true;
		}
		if (!registry.valid(entity))
		{
			// EnTT accepts a versioned hint only when its slot is free.
			const auto restored = registry.create(entity);
			if (restored != entity)
			{
				registry.destroy(restored);
				return false;
			}
		}
		auto& info = registry.get_or_emplace<Components::EntityInfoComponent>(entity);
		info.mName = mName;
		info.mTransform.SetPosition(Transform.GetLocalPosition());
		info.mTransform.SetRotation(Transform.GetLocalRotation());
		info.mTransform.SetScale(Transform.GetLocalScale());
		RestorePresence<Components::MeshComponent>(registry, entity, mMesh.has_value());
		if (mMesh)
		{
			auto& mesh = registry.get<Components::MeshComponent>(entity);
			mesh.SetMesh(mMesh->Mesh);
			mesh.SetMaterial(mMesh->Material);
			mesh.SetEnableRendering(mMesh->Visible);
			mesh.SetCastShadow(mMesh->CastShadows);
			mesh.SetReceiveShadows(mMesh->ReceiveShadows);
		}
		RestorePresence<Components::LightComponent>(registry, entity, mLight.has_value());
		if (mLight)
		{
			auto& light = registry.get<Components::LightComponent>(entity);
			light.SetType(mLight->Type);
			light.SetDirection(mLight->Direction);
			light.SetColor(Graphics::Color(mLight->Color.x, mLight->Color.y, mLight->Color.z, 1.f));
			light.SetAttenuation(mLight->Attenuation);
			light.SetSpotlightCone(mLight->Cone);
			light.SetIntensity(mLight->Intensity);
			light.SetNearPlane(mLight->NearPlane);
			light.SetFarPlane(mLight->FarPlane);
			light.SetFOV(mLight->Fov);
			light.mCastShadows = mLight->CastShadows;
		}
		RestorePresence<Components::RigidBodyComponent>(registry, entity, mKinematic.has_value());
		if (mKinematic)
			registry.get<Components::RigidBodyComponent>(entity).isKinematic = *mKinematic;
		RestorePresence<Components::ColliderComponent>(registry, entity, mCollider);
		RestorePresence<SkyboxComponent>(registry, entity, mSkybox.has_value());
		if (mSkybox)
			registry.get<SkyboxComponent>(entity).Enabled = *mSkybox;
		RestorePresence<Components::ScriptComponent>(registry, entity, mScript);
		RestorePresence<Components::AudioListenerComponent>(registry, entity, mListener);
		RestorePresence<Components::AudioSourceComponent>(registry, entity, mSource);
		if (mCamera && !registry.all_of<Components::CameraComponent>(entity))
		{
			Components::CameraComponentDesc desc;
			desc.mInitRTs = false;
			desc.mIsActiveCamera = false;
			desc.mProjection = Math::perspective(Math::radians(45.f), 1.f, .1f, 100.f);
			registry.emplace<Components::CameraComponent>(entity, desc);
		}
		else if (!mCamera)
			registry.remove<Components::CameraComponent>(entity);
		return true;
	}

	entt::entity EditorEntityState::CreateCopy(const std::string& name) const
	{
		if (!Exists) return entt::null;
		auto& scene = Core::Scene::Get();
		const auto entity = scene.CreateEntity(name).GetEntityID();
		if (!Restore(entity))
		{
			scene.GetRegistry().destroy(entity);
			return entt::null;
		}
		scene.GetRegistry().get<Components::EntityInfoComponent>(entity).mName = name;
		return entity;
	}

	bool EditorEntityState::operator==(const EditorEntityState& other) const
	{
		if (!Exists || !other.Exists)
			return Exists == other.Exists;
		return mName == other.mName && Transform.GetLocalPosition() == other.Transform.GetLocalPosition() &&
			Transform.GetLocalRotation() == other.Transform.GetLocalRotation() &&
			Transform.GetLocalScale() == other.Transform.GetLocalScale() && mMesh == other.mMesh &&
			mLight == other.mLight && mKinematic == other.mKinematic && mCamera == other.mCamera &&
			mCollider == other.mCollider && mScript == other.mScript && mListener == other.mListener &&
			mSource == other.mSource && mSkybox == other.mSkybox;
	}
}
