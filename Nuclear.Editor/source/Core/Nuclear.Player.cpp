#include <ProjectPlayer.h>
#include <Nuclear.Platform/Platform.h>
#include <fstream>
#include <iostream>

int wmain()
{
    const auto root = Nuclear::Platform::GetExecutablePath().parent_path();
    try
    {
        return Nuclear::Editor::ProjectPlayer::RunGame(root / "Game.nuclearproject", root / "Assets");
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << std::endl;
        std::ofstream("play-error.txt") << error.what() << '\n';
        return 1;
    }
}
