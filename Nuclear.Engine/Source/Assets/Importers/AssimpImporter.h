#pragma once
#include <NE_Common.h>
#include <Assets/ImportingDescs.h>
#include <Assets/LoadingDescs.h>
#include <Assets/Importers/Model.h>
#include <Assets\Mesh.h>
#include <Assets\Material.h>
#include <Assets/Animations.h>
#include "Animation\Bone.h"

struct aiMaterial;
struct aiMesh;
struct aiScene;
struct aiNode;
struct aiAnimation;
struct aiNodeAnim;
namespace Nuclear
{
	namespace Assets
	{
		namespace Importers
		{
			//TODO!!!:
			//Each thread should have its instance... / or remove assimp completely for gltf loading
			class NEAPI AssimpImporter
			{
			public:
				AssimpImporter();
				~AssimpImporter();

				bool Import(const std::string& importPath, const std::string& exportPath, const Model& model, const Assets::MeshImportingDesc& desc);

				bool Load(const std::string& Path, const Model& model, const Assets::MeshLoadingDesc& desc);
				static bool IsExtensionSupported(const std::string& extension);

			protected:
				friend class AssimpImporter;

				Assets::MaterialTextureSet ProcessMaterialTexture(aiMaterial* mat, int type);
				void ProcessMesh(aiMesh* mesh, const aiScene* scene);
				Uint32 ProcessMaterial(aiMesh* mesh, const aiScene* scene);
				void ProcessNode(aiNode* node, const aiScene* scene);

				std::vector<std::string> TexturePaths;

				Model mModel;
				const aiScene* scene;
				std::string mDirectory;
				std::string exportPath;

				void LoadAnimations();
				void ExtractBoneWeightForVertices(Assets::Mesh::SubMesh::SubMeshData* meshdata, aiMesh* mesh, const aiScene* scene);
				void ReadMissingBones(const aiAnimation* animation, Animation::AnimationClip* clip);
				void ReadHeirarchyData(Animation::ClipNodeData* dest, const aiNode* src);

				void InitBoneData(const aiNodeAnim* channel, Animation::BoneData& data);
				bool save_materials_names;
				CommonImportingOptions mTextureOptions; ///< Parent import policy propagated to material textures; texture names remain independent.
				bool mDiscoverCompanions = false; ///< Enables bounded companion discovery for model imports only.
				std::string mAlbedoPath; ///< Resolved external albedo path for the material currently being processed.
				std::unordered_map<std::string, Assets::Texture*> mTextureCache; ///< Borrowed texture results keyed by resolved path and color space within this import.
				/// Resolves absolute/relative references, then model-local filenames and Textures directories.
				std::string ResolveTexturePath(const std::string& reference) const;
				Assets::Texture* ImportExternalTexture(const std::string& path, Assets::TextureUsageType usage); ///< Imports once using the parent policy.
				void ImportCompanions(Assets::MaterialTextureSet& textures); ///< Adds missing conventional sibling maps without replacing explicit slots.
			};
		}
	}
}
