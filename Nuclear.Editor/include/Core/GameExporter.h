#pragma once
#include <ProjectDocument.h>

namespace Nuclear::Editor
{
    /// Produces a relocatable Windows game folder. All writes are staged before publication.
    class GameExporter
    {
    public:
        /// Packages a parsed startup document, named levels, content and binaries into a new directory.
        /// The renderer must be initialized for model dependency discovery. Existing directories are preserved.
        static void Build(Project& project, ProjectDocument startup, const std::filesystem::path& binaries,
            const std::filesystem::path& assets, const std::filesystem::path& destination);
    };
}
