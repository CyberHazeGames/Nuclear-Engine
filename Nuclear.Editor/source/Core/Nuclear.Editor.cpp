#include <NuclearEditor.h>
#include <ProjectPlayer.h>
#include <GameExporter.h>
#include <iostream>
#include <Nuclear.Platform/Platform.h>
#include <fstream>
#pragma comment(lib, "Nuclear.Engine.lib")

int wmain(int argc, wchar_t** argv)
{
    if (argc == 6 && std::wstring_view(argv[1]) == L"--build-game")
    {
        Core::EngineStartupDesc desc;
        desc.mAssetsLibraryPath = std::filesystem::absolute(argv[4]).generic_string() + "/";
        desc.AutoInitAudioModule = desc.AutoInitPhysXModule = desc.AutoInitScriptingModule = false;
        desc.AutoInitThreadingModule = desc.EnableImGui = false;
        auto& engine = Core::Engine::Get();
        if (!engine.Start(desc)) return 1;
        int result = 0;
        try
        {
            if (!Nuclear::Editor::ProjectDocument::IsProject(argv[2])) throw std::runtime_error("Build a main .nuclearproject file");
            Nuclear::Editor::Project project;
            project.InitializeMaterials();
            Nuclear::Editor::GameExporter::Build(project, Nuclear::Editor::ProjectDocument::Read(argv[2]),
                argv[3], argv[4], std::filesystem::absolute(argv[5]));
        }
        catch (const std::exception& error) { std::cerr << error.what() << std::endl; result = 1; }
        engine.Shutdown();
        return result;
    }
    if (argc == 4 && std::wstring_view(argv[1]) == L"--play")
	{
		try { return Nuclear::Editor::ProjectPlayer::RunGame(argv[2], argv[3]); }
		catch (const std::exception& e)
		{
			std::ofstream("play-error.txt") << e.what() << '\n';
			return 1;
		}
	}
	Core::EngineStartupDesc desc;
	desc.mEngineWindowDesc.WindowWidth = 1440;
	desc.mEngineWindowDesc.WindowHeight = 900;
	desc.mEngineWindowDesc.Title = "Nuclear Editor";
	desc.mEngineWindowDesc.CustomTitleBar = true;
	desc.mAssetsLibraryPath = "../Assets/";
	desc.EnableImGui = false;
	desc.AutoInitAudioModule = false;
	desc.AutoInitPhysXModule = false;
	desc.AutoInitScriptingModule = false;
	desc.AutoInitThreadingModule = false;
	auto& engine = Core::Engine::Get();
	if (!engine.Start(desc))
		return 1;
	NuclearEditor editor;
	engine.LoadClient(&editor);
	engine.EndClient();
	engine.Shutdown();
	return 0;
}
