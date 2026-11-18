#pragma once
#include <Assimp/include/assimp/DefaultIOSystem.h>
#include <string>
#include <vector>

namespace Nuclear::Assets::Importers
{
    /// Delegates to Assimp's filesystem and remembers files actually opened by its decoder.
    class ModelSourceIO final : public Assimp::IOSystem
    {
    public:
        explicit ModelSourceIO(std::vector<std::string>& files); ///< Borrows the mesh-owned output until import returns.
        bool Exists(const char* file) const override; ///< Tests the same path used by the default decoder.
        char getOsSeparator() const override; ///< Native path separator.
        Assimp::IOStream* Open(const char* file, const char* mode) override; ///< Records successful reads.
        void Close(Assimp::IOStream* stream) override; ///< Releases a stream returned by Open.
    private:
        Assimp::DefaultIOSystem mFiles; ///< Assimp's ordinary filesystem implementation.
        std::vector<std::string>& mSources; ///< Unique files used by this model.
    };
}
