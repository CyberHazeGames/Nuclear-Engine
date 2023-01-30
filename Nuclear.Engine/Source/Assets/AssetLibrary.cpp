#include <Assets/AssetLibrary.h>
#include <Platform/FileSystem.h>
#include <filesystem>
#include <Utilities/Logger.h>

namespace Nuclear
{
	namespace Assets
	{
		AssetLibrary::AssetLibrary()
		{
		}
	
		

		void AssetLibrary::Initialize(const std::string& path)
		{
			if (path.back() != '/')
			{
				mPath = path + "/";
			}
			else
			{
				mPath = path;
			}

			std::string assets[9] = { "Textures",				//Importing & Loading done
				"Meshes",
				"Materials",
				"Animations",
				"AudioClips",
				"Fonts",
				"Shaders",
				"Scripts" };

			for (int i = 0; i < 9; i++)
			{
				Platform::FileSystem::GetInstance().CreateDir(mPath + assets[i]);
			}
		}

		const std::string& AssetLibrary::GetPath() const
		{
			return mPath;
		}

		AssetLibrary& AssetLibrary::GetInstance()
		{
			static AssetLibrary instance;

			return instance;
		}
	}
}