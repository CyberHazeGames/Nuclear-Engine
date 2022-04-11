﻿#include "Common.h"
#include "SampleSelector.h"
#include <iostream>
#include "Engine\Scripting\ScriptingEngine.h"

#pragma comment(lib,"Nuclear.Core.lib")
#pragma comment(lib,"glfw3.lib")

//Core::RenderAPI SelectRenderer()
//{
//	std::cout << "Select Renderer: \n"
//		<< "1) DirectX 11 \n"
//		<< "2) DirectX 12 \n"
//		<< "3) OpenGL 4 \n"
//		<< "4) Vulkan \n";
//
//	int i;
//	std::cin >> i;
//
//	switch (i)
//	{
//	case 1:
//		return Core::RenderAPI::DirectX11;
//	case 2:
//		return Core::RenderAPI::DirectX12;
//	case 3:
//		return Core::RenderAPI::OpenGL;
//	case 4:
//		return Core::RenderAPI::Vulkan;
//	}
//
//	return Core::RenderAPI::DirectX12;
//}

int main(int argc, char* argv[])
{

	//std::cout << "Auto Initialize? \n"
	//	<< "1) Yes \n"
	//	<< "2) No \n";

	//int i;
	//std::cin >> i;
	//if (i == 2)
	//{
	//	Core::EngineStartupDesc desc;
	//	desc.mAppdesc.Renderer = SelectRenderer();
	//	Core::Engine::GetInstance()->Start(desc);

	//}
	//else
	//{
	//	Core::EngineStartupDesc desc;
	//	Core::Engine::GetInstance()->Start(desc);
	//}

	Core::EngineStartupDesc desc;
	desc.mEngineWindowDesc.WindowWidth = 1280;
	desc.mEngineWindowDesc.WindowHeight = 720;

	Core::Engine::GetInstance()->Start(desc);
	Core::Path::mReservedPaths["@CommonAssets@"] = "Assets/Common";
	Core::Path::mReservedPaths["@NuclearAssets@"] = "Assets/NuclearEngine";

	while (!Core::Engine::GetInstance()->ShouldClose())
	{
		SampleSelector selector;
		Core::Engine::GetInstance()->SetGame(&selector);
		Core::Engine::GetInstance()->LoadGame();
		Core::Engine::GetInstance()->RunGame();
	}
	return 0;
}