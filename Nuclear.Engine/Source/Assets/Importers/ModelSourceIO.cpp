#include "ModelSourceIO.h"
#include <algorithm>
#include <filesystem>

namespace Nuclear::Assets::Importers
{
    ModelSourceIO::ModelSourceIO(std::vector<std::string>& files) : mSources(files) {}
    bool ModelSourceIO::Exists(const char* file) const { return mFiles.Exists(file); }
    char ModelSourceIO::getOsSeparator() const { return mFiles.getOsSeparator(); }
    void ModelSourceIO::Close(Assimp::IOStream* stream) { mFiles.Close(stream); }

    Assimp::IOStream* ModelSourceIO::Open(const char* file, const char* mode)
    {
        auto* stream = mFiles.Open(file, mode);
        if (stream && mode[0] == 'r')
        {
            const auto path = std::filesystem::absolute(file).lexically_normal().string();
            if (std::find(mSources.begin(), mSources.end(), path) == mSources.end())
                mSources.push_back(path);
        }
        return stream;
    }
}
