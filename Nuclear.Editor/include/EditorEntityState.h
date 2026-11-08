#pragma once
#include <Common.h>
#include <optional>

namespace Nuclear::Editor
{
	/// In-memory authoring snapshot for one entity. Assets are borrowed from the session library.
	/// Runtime physics actors, script instances, audio handles and preview render targets are excluded.
	class EditorEntityState
	{
	public:
		/// Captures supported editor data; a missing handle produces an absent-entity snapshot.
		static EditorEntityState Capture(entt::entity entity);
		/// Restores authoring data at the same ECS identity, or destroys it for an absent snapshot.
		/// Returns false if another live entity occupies the slot. Call only in the authoring registry.
		bool Restore(entt::entity entity) const;
		/// Compares authored values, ignoring renderer caches and runtime-only state.
		bool operator==(const EditorEntityState& other) const;
		bool Exists = false; ///< Whether the captured entity existed.
		ECS::Transform Transform; ///< Local position, quaternion and scale; no parent ownership.
	private:
		/// Library references and editable mesh flags, without shader-controller caches.
		struct MeshData
		{
			Assets::Mesh* Mesh = nullptr; ///< Borrowed geometry, valid for the session.
			Assets::Material* Material = nullptr; ///< Borrowed material, valid for the session.
			bool Visible = true; ///< Whether the preview draws this mesh.
			bool CastShadows = true; ///< Authored shadow casting flag.
			bool ReceiveShadows = true; ///< Authored shadow reception flag.
			bool operator==(const MeshData&) const = default; ///< Compares authored mesh data.
		};
		/// Editable light values; GPU bindings and derived positions are rebuilt separately.
		struct LightData
		{
			Components::LightComponent::Type Type; ///< Authored light kind.
			Math::Vector3 Direction; ///< World direction used by the light.
			Math::Vector3 Color; ///< Linear RGB color.
			Math::Vector3 Attenuation; ///< Constant, linear and quadratic factors.
			Math::Vector2 Cone; ///< Inner and outer cone cosines.
			float Intensity; ///< Authored intensity multiplier.
			float NearPlane; ///< Shadow projection near distance, in world units.
			float FarPlane; ///< Shadow projection far distance, in world units.
			float Fov; ///< Shadow projection field of view, in degrees.
			bool CastShadows; ///< Authored shadow casting flag.
			bool operator==(const LightData&) const = default; ///< Compares editable light values.
		};
		std::string mName; ///< Entity display name.
		std::optional<MeshData> mMesh; ///< Missing when the entity has no mesh component.
		std::optional<LightData> mLight; ///< Missing when the entity has no light component.
		std::optional<bool> mKinematic; ///< Body presence and authored kinematic flag; no actor.
		std::optional<bool> mSkybox; ///< Editor skybox presence and visibility; GPU resources remain controller-owned.
		bool mCamera = false; ///< Presence of an inactive, targetless authoring camera.
		bool mCollider = false; ///< Presence of an uninitialized authoring collider.
		bool mScript = false; ///< Presence of an uninitialized authoring script component.
		bool mListener = false; ///< Presence of an authoring audio listener.
		bool mSource = false; ///< Presence of an uninitialized authoring audio source.
	};
}
