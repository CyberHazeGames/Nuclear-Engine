#pragma once
#include <NE_Common.h>
#include <Assets\Mesh.h>
#include <Assets\Material.h>
#include <Assets/ImportingDescs.h>
#include "Animation\Bone.h"
#include <Assets/Animations.h>

struct aiMaterial;
struct aiMesh;
struct aiScene;
struct aiNode;
struct aiAnimation;
struct aiNodeAnim;
namespace Assimp {
	class Importer;
}
namespace Nuclear
{
	namespace Assets
	{
		namespace Importers
		{
			class NEAPI AssimpImporter
			{
			public:
				AssimpImporter();
				~AssimpImporter();

				bool isBusy();

				bool Load(const Assets::ModelImportingDesc& desc, const std::string& Path, Assets::Mesh* mesh, Assets::MaterialData* material, Assets::Animations* anim);
				bool IsExtensionSupported(const std::string& extension);
				Assimp::Importer* GetImporter();
			private:
				bool busy = false;
				Assimp::Importer* pImporter;
			};


			class NEAPI AssimpLoader
			{
				Assets::TextureSet ProcessMaterialTexture(aiMaterial* mat, int type);
				void ProcessMesh(aiMesh* mesh, const aiScene* scene);
				Uint32 ProcessMaterial(aiMesh* mesh, const aiScene* scene);
				void ProcessNode(aiNode* node, const aiScene* scene);

			protected:
				friend class AssimpImporter;
				std::vector<std::string> TexturePaths;

				Assets::MaterialData* pMaterialData = nullptr;
				Assets::Animations* pAnimation = nullptr;
				Assets::Mesh* pMesh = nullptr;
				const aiScene* scene;
				std::string mDirectory;

				void LoadAnimations();
				void ExtractBoneWeightForVertices(Assets::Mesh::SubMesh::SubMeshData* meshdata, aiMesh* mesh, const aiScene* scene);
				void ReadMissingBones(const aiAnimation* animation, Animation::AnimationClip* clip);
				void ReadHeirarchyData(Animation::ClipNodeData* dest, const aiNode* src);

				void InitBoneData(const aiNodeAnim* channel, Animation::BoneData& data);
				Assets::ModelImportingDesc mLoadingDesc;
			};


			class AssimpManager
			{
			public:

				std::vector<AssimpImporter> mImporters;

			};

			//bool AssimpLoadMesh(const MeshImporterDesc& desc, Assets::Mesh* mesh, Assets::MaterialData* material, Assets::Animations* anim);
		}
	}
}
