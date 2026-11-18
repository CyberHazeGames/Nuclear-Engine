#pragma once
#include <Threading/MainThreadTask.h>
#include <Threading/ThreadingModule.h>
#include <Assets/ImportingDescs.h>
#include <filesystem>
#include <Assets/Loader.h>
#include <Assets/AssetLibrary.h>
#include <Assets\Importers\AssimpManager.h>

#include <Assets/AssetManager.h>

#include <Platform/FileSystem.h>
#include <Utilities/Logger.h>

#include <Serialization/SerializationModule.h>

#include <Assets/Tasks/MeshCreateTask.h>

namespace Nuclear
{
	namespace Assets
	{
		class MeshLoadTask : public Threading::Task
		{
		public:
			MeshLoadTask(const Importers::Model& model, const Core::Path& path, const Assets::AssetMetadata& metadata)
				: mResult(model), mPath(path), mAssetmeta(metadata)
			{
			}
			~MeshLoadTask()
			{

			}
			bool OnRunning() override
			{
				Assets::MeshLoadingDesc mLoadingDesc = *static_cast<Assets::MeshLoadingDesc*>(mAssetmeta.pLoadingDesc);
				delete mAssetmeta.pLoadingDesc;

				//Load Mesh & animations
				if (!Importers::AssimpManager::Get().Load(mPath.GetRealPath(), mResult, mLoadingDesc))
				{
					NUCLEAR_ERROR("[Loader] Failed to Load Model : '{0}'", mPath.GetInputPath());

					return false;
				}

				if (mResult.pMesh)
				{
					//create task
					Threading::ThreadingModule::Get().AddMainThreadTask(new MeshCreateTask(mResult.pMesh, mPath, LOADER_FACTORY_TYPE));

				}

				return true;
			}

			void OnEnd() override
			{
				delete this;
			}
		protected:
			Importers::Model mResult;
			Assets::AssetMetadata mAssetmeta;
			Core::Path mPath;
		};

	}
}