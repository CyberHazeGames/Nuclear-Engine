#include "Common.h"
#include "SampleSelector.h"
#include <iostream>

Diligent::RENDER_DEVICE_TYPE SelectRenderer()
{
	std::cout << "Select Renderer: \n"
		<< "1) DirectX 11 \n"
		<< "2) DirectX 12 \n"
		<< "3) OpenGL 4 \n"
		<< "4) Vulkan \n";

	int i;
	std::cin >> i;

	switch (i)
	{
	case 1:
		return Diligent::RENDER_DEVICE_TYPE_D3D11;
	case 2:
		return Diligent::RENDER_DEVICE_TYPE_D3D12;
	case 3:
		return Diligent::RENDER_DEVICE_TYPE_GL;
	case 4:
		return Diligent::RENDER_DEVICE_TYPE_VULKAN;
	}

	return Diligent::RENDER_DEVICE_TYPE_D3D11;
}

int main(int argc, char* argv[])
{
	Core::EngineStartupDesc desc;
	const bool uiOnly = argc > 1 && std::string(argv[1]) == "--ui";
	if (uiOnly)
	{
		desc.EnableImGui = false;
		desc.AutoInitAudioModule = false;
		desc.AutoInitPhysXModule = false;
		desc.AutoInitScriptingModule = false;
		desc.AutoInitThreadingModule = false;
		desc.AutoInitRenderingModule = false;
	}

	/*std::cout << "Auto Initialize? \n"
		<< "1) Yes \n"
		<< "2) No \n";
	*/
//	int i;
//	std::cin >> i;
//	if (i == 2)
	//{
	//	desc.Renderer = SelectRenderer();
	//	Core::Engine::Get().Start(desc);

	//}
	//else
	//{
	//	ModuleStartupDesc desc;
	//	Core::Engine::Get().Start(desc);
	//}

	//ModuleStartupDesc desc;


	desc.mEngineWindowDesc.WindowWidth = 1280;
	desc.mEngineWindowDesc.WindowHeight = 720;
	desc.mScriptingClientDllName = "SamplesScripts.dll";
	desc.mScriptingAssemblyNamespace = "Samples";
	desc.mAssetsLibraryPath = "../Assets/";
	if (!Core::Engine::Get().Start(desc)) return 1;
	Core::Path::mReservedPaths["@CommonAssets@"] = "../Assets/Common";
	if (uiOnly)
	{
		UISample ui;
		Core::Engine::Get().LoadClient(&ui);
		Core::Engine::Get().EndClient();
		Core::Engine::Get().Shutdown();
		return 0;
	}

	while (!Core::Engine::Get().ShouldClose())
	{
		SampleSelector selector;
		Core::Engine::Get().LoadClient(&selector);
		Core::Engine::Get().EndClient();
		if (Core::Engine::Get().ShouldClose()) break;
		if (auto sample = selector.CreateSelectedSample())
		{
			Core::Engine::Get().LoadClient(sample.get());
			Core::Engine::Get().EndClient();
		}
	}
	Core::Engine::Get().Shutdown();
	return 0;
}
