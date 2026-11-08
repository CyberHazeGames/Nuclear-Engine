#include <NuclearEditor.h>
#pragma comment(lib, "Nuclear.Engine.lib")

int main()
{
	Core::EngineStartupDesc desc;
	desc.mEngineWindowDesc.WindowWidth = 1440;
	desc.mEngineWindowDesc.WindowHeight = 900;
	desc.mEngineWindowDesc.Title = "Nuclear Editor";
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
