#include <GameExporter.h>
#include <fstream>
#include <Assimp/contrib/rapidjson/include/rapidjson/document.h>
#include <Assimp/contrib/rapidjson/include/rapidjson/istreamwrapper.h>
#include <set>

namespace Nuclear::Editor
{
    namespace fs = std::filesystem;
    namespace
    {
        void Copy(const fs::path& source, const fs::path& target)
        {
            if (!fs::is_regular_file(source)) throw std::runtime_error("Missing export dependency: " + source.string());
            fs::create_directories(target.parent_path());
            fs::copy_file(source, target);
        }

        fs::path CopyModel(Project& project, const fs::path& model, const fs::path& folder)
        {
            auto* mesh = project.ImportMesh(model);
            std::set<fs::path> files{model};
            fs::path base = model.parent_path();
            for (const auto& source : mesh->SourceFiles)
            {
                auto file = fs::absolute(source).lexically_normal();
                files.insert(file);
                if (file.root_name() != model.root_name())
                    throw std::runtime_error("Model references another drive: " + file.string());
                while (base != base.root_path() && file.lexically_relative(base).begin()->string() == "..")
                    base = base.parent_path();
            }
            for (const auto& file : files) Copy(file, folder / file.lexically_relative(base));
            return folder / model.lexically_relative(base);
        }

        void CopyAssemblyDependencies(const fs::path& source, const fs::path& target)
        {
            auto manifest = source.parent_path() / (source.stem().string() + ".deps.json");
            if (!fs::exists(manifest)) return;
            std::ifstream input(manifest);
            rapidjson::IStreamWrapper stream(input);
            rapidjson::Document json;
            json.ParseStream(stream);
            if (json.HasParseError() || !json.IsObject() || !json.HasMember("targets") || !json["targets"].IsObject()
                || !json.HasMember("runtimeTarget") || !json["runtimeTarget"].IsObject() ||
                !json["runtimeTarget"].HasMember("name") || !json["runtimeTarget"]["name"].IsString())
                throw std::runtime_error("Invalid .NET dependency manifest: " + manifest.string());
            const auto runtime = json["runtimeTarget"]["name"].GetString();
            if (!json["targets"].HasMember(runtime) || !json["targets"][runtime].IsObject())
                throw std::runtime_error("Missing gameplay dependency target");
            std::set<fs::path> copied;
            for (auto& library : json["targets"][runtime].GetObject())
            {
                if (!library.value.IsObject()) continue;
                for (const char* group : {"runtime", "native", "runtimeTargets"})
                {
                    if (!library.value.HasMember(group) || !library.value[group].IsObject()) continue;
                    for (auto& asset : library.value[group].GetObject())
                    {
                        if (asset.value.IsObject() && asset.value.HasMember("rid") &&
                            (!asset.value["rid"].IsString() || std::string(asset.value["rid"].GetString()) != "win-x64")) continue;
                        fs::path declared(asset.name.GetString());
                        if (declared.empty() || declared.is_absolute() || declared.lexically_normal().begin()->string() == "..")
                            throw std::runtime_error("Invalid path in assembly dependencies");
                        auto filename = declared.filename();
                        if (filename == "_._" || filename == source.filename() || filename == "Nuclear.ScriptCore.dll"
                            || filename == "Nuclear.Managed.dll" || !copied.insert(filename).second) continue;
                        auto dependency = source.parent_path() / declared;
                        if (!fs::is_regular_file(dependency)) dependency = source.parent_path() / filename;
                        Copy(dependency, target.parent_path() / filename);
                    }
                }
            }
        }

        void CopyRuntime(const fs::path& binaries, const fs::path& assets, const fs::path& stage, bool audio, bool scripts)
        {
            Copy(binaries / "Nuclear.Player.exe", stage / "Game.exe");
            for (const auto* file : {"Nuclear.Engine.dll", "Nuclear.Platform.dll", "FreeImage.dll", "sndfile.dll",
                "assimp-vc145-mt.dll", "PhysX_64.dll", "PhysXFoundation_64.dll", "PhysXCommon_64.dll"})
                Copy(binaries / file, stage / file);
            const char suffix = fs::exists(binaries / "GraphicsEngineD3D11_64r.dll") ? 'r' : 'd';
            for (const auto* backend : {"D3D11", "D3D12", "OpenGL", "Vk"})
            {
                auto file = std::string("GraphicsEngine") + backend + "_64" + suffix + ".dll";
                Copy(binaries / file, stage / file);
            }
            if (audio)
                for (const auto* file : {"Nuclear.Audio.XAudio2.dll", "Nuclear.Audio.OpenAL.dll", "OpenAL32.dll"})
                    Copy(binaries / file, stage / file);
            if (scripts)
                for (const auto* assembly : {"Nuclear.Managed", "Nuclear.ScriptCore"})
                    for (const auto* extension : {".dll", ".deps.json", ".runtimeconfig.json"})
                    {
                        const auto file = std::string(assembly) + extension;
                        Copy(binaries / file, stage / file);
                    }
            const auto content = stage / "Assets" / "NuclearEngine";
            Copy(assets / "NuclearEngine/Fonts/Roboto-Medium.ttf", content / "Fonts/Roboto-Medium.ttf");
            fs::copy(assets / "NuclearEngine/Shaders", content / "Shaders", fs::copy_options::recursive);
            std::ofstream requirements(stage / "Runtime.txt");
            requirements.exceptions(std::ios::failbit | std::ios::badbit);
            requirements << "Windows x64 with Direct3D 11 and the Microsoft Visual C++ x64 runtime.\n";
            if (scripts) requirements << "Install the x64 Microsoft .NET 10 runtime before launching Game.exe.\n";
        }
    }

    void GameExporter::Build(Project& project, ProjectDocument startup, const fs::path& binaries,
        const fs::path& assets, const fs::path& destination)
    {
        if (fs::exists(destination)) throw std::runtime_error("Choose a new output folder: " + destination.string());
        const auto stage = fs::absolute(destination).parent_path() / (".nuclear-build-" + Core::UUID::CreateNewUUID().str());
        if (!fs::create_directory(stage)) throw std::runtime_error("Cannot create export staging folder");
        try
        {
            std::vector<std::pair<fs::path, ProjectDocument>> documents;
            bool audio = startup.NeedsAudio(), scripts = startup.NeedsScripts();
            for (auto& [name, path] : startup.Levels)
            {
                auto level = ProjectDocument::Read(path);
                if (name == startup.SceneName || (!level.Assembly.empty() && level.Assembly != startup.Assembly))
                    throw std::runtime_error("Incompatible linked level: " + name);
                level.Levels.clear();
                level.Assembly.clear();
                audio |= level.NeedsAudio();
                scripts |= level.NeedsScripts();
                path = stage / ("Level-" + std::to_string(documents.size()) + ".NMap");
                documents.emplace_back(path, std::move(level));
            }
            std::map<fs::path, fs::path> prefabs;
            auto relocate = [&](auto& table) {
                for (auto& [name, path] : table)
                {
                    if (!prefabs.contains(path))
                        prefabs.emplace(path, stage / ("Prefab-" + std::to_string(prefabs.size()) + ".nuclearprefab"));
                    path = prefabs.at(path);
                }
            };
            relocate(startup.Prefabs);
            relocate(startup.StartupPrefabs);
            for (auto& [file, doc] : documents) relocate(doc.Prefabs);
            for (const auto& [source, target] : prefabs)
            {
                auto prefab = ProjectDocument::Read(source);
                prefab.Template();
                prefab.Prefabs.clear();
                prefab.Levels.clear();
                prefab.Assembly.clear();
                audio |= prefab.NeedsAudio();
                scripts |= prefab.NeedsScripts();
                documents.emplace_back(target, std::move(prefab));
            }
            CopyRuntime(binaries, assets, stage, audio, scripts);
            if (scripts)
            {
                const auto source = startup.Assembly.empty() ? binaries / "SamplesScripts.dll" : startup.Assembly;
                auto target = startup.Assembly.empty() ? stage / source.filename() : stage / "Scripts" / source.filename();
                Copy(source, target);
                CopyAssemblyDependencies(source, target);
                for (const auto* ext : {".deps.json", ".runtimeconfig.json"})
                {
                    auto sidecar = source.parent_path() / (source.stem().string() + ext);
                    if (fs::exists(sidecar)) Copy(sidecar, target.parent_path() / sidecar.filename());
                }
                startup.Assembly = target;
            }
            if (!scripts) startup.Assembly.clear();
            startup.StartupMap = stage / "Startup.NMap";
            auto manifest = ProjectDocument{};
            manifest.ProjectName = startup.ProjectName;
            manifest.Assembly = startup.Assembly;
            manifest.StartupMap = startup.StartupMap;
            manifest.Levels = std::move(startup.Levels);
            manifest.Prefabs = std::move(startup.Prefabs);
            startup.Prefabs = std::move(startup.StartupPrefabs);
            documents.emplace_back(manifest.StartupMap, std::move(startup));
            std::map<fs::path, fs::path> models, clips;
            for (auto& [file, doc] : documents)
            {
                if (file.extension() != ".nuclearprefab" && doc.mCamera < 0) throw std::runtime_error("Select a game camera for " + doc.SceneName);
                for (auto& refs : doc.mReferences)
                    for (auto* ref : {&refs.first, &refs.second})
                    {
                        if (!ref->starts_with("file:")) continue;
                        const auto source = fs::absolute(doc.mDirectory / ref->substr(5)).lexically_normal();
                        if (!models.contains(source))
                            models[source] = CopyModel(project, source, stage / "Models" / std::to_string(models.size()));
                        *ref = "file:" + models.at(source).lexically_relative(stage).generic_string();
                    }
                for (auto& entity : doc.mEntities)
                {
                    if (entity.mClipPath.empty()) continue;
                    const fs::path source = entity.mClipPath;
                    if (!clips.contains(source))
                    {
                        auto target = stage / "Audio" / std::to_string(clips.size()) / source.filename();
                        Copy(source, target);
                        clips[source] = target;
                    }
                    entity.mClipPath = clips.at(source).string();
                }
                doc.Write(file, nullptr);
            }
            manifest.SaveManifest(stage / "Game.nuclearproject");
            fs::rename(stage, destination);
        }
        catch (...)
        {
            std::error_code ignored;
            fs::remove_all(stage, ignored);
            throw;
        }
    }
}
