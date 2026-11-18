#pragma once
#include <Core/Path.h>
#include <Assets/Importer.h>
#include <Assets/Loader.h>
#include <Assets/Mesh.h>
#include <Assets/AssetLibrary.h>
#include <algorithm>

#include <Threading/MainThreadTask.h>
#include <Threading/ThreadingModule.h>
#include <Utilities/Logger.h>

#include <Graphics\GraphicsModule.h>

#define IMPORTER_FACTORY_TYPE 1
#define LOADER_FACTORY_TYPE 2

namespace Nuclear
{
	namespace Assets
	{
		class Mesh;
		class MeshCreateTask : public Threading::MainThreadTask
		{
		public:
			//type int : 
			//1: importer
			//2: loader
			//other
			MeshCreateTask(Mesh* mesh,const Core::Path& path, int factory_type)
				: pMesh(mesh), mPath(path), mFactoryType(factory_type)
			{
			}

			bool OnRunning() override
			{
				bool result = Graphics::GraphicsModule::Get().CreateMesh(pMesh);

				if (mFactoryType == IMPORTER_FACTORY_TYPE || mFactoryType == LOADER_FACTORY_TYPE)
				{
					auto& queued = mFactoryType == IMPORTER_FACTORY_TYPE
						? Importer::Get().GetQueuedAssets() : Loader::Get().GetQueuedAssets();
					std::erase(queued, static_cast<IAsset*>(pMesh));
					if (auto* clips = pMesh->pImportedAnimations)
					{
						std::erase(queued, static_cast<IAsset*>(clips));
						if (clips->GetState() != IAsset::State::Loaded)
						{
							pMesh->pImportedAnimations = nullptr;
							// An import allocated a private placeholder. A load may share its UUID with another mesh.
							if (mFactoryType == IMPORTER_FACTORY_TYPE)
								AssetLibrary::Get().mImportedAnimations.mData.erase(clips->GetUUID());
						}
					}
				}

				if (!result)
				{
					NUCLEAR_ERROR("[MeshCreateTask] Failed To Create Mesh: '{0}'", mPath.GetInputPath());
					return false;
				}

				if (mFactoryType == 1)
				{
					NUCLEAR_TRACE("[Assets] Imported: {0} ", mPath.GetInputPath());
				}
				else if (mFactoryType == 2)
				{
					NUCLEAR_TRACE("[Assets] Loaded: {0} ", mPath.GetInputPath());
				}
				else
				{
					NUCLEAR_TRACE("[MeshCreateTask] Created: {0} ", mPath.GetInputPath());
				}

				return result;
			}

			void OnEnd() override
			{
				delete this;
			}

		protected:
			Mesh* pMesh;
			Core::Path mPath;
			int mFactoryType;
		};
	}
}