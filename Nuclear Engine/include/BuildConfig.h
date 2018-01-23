#pragma once

#ifdef _WIN32

#ifdef _WIN64
#define NUCLEAR_PLATFORM_WINDOWS_PC_64BIT
#else
#define NUCLEAR_PLATFORM_WINDOWS_PC_32BIT
#endif

#define NUCLEAR_PLATFORM_WINDOWS_PC

#ifdef NUCLEAR_ENGINE_MAIN_IMPLEMENTATION

#include <Windows.h>

extern int main(int argc, char* argv[]);

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, INT)
{
	return main(__argc, __argv);
}

#endif

#endif

#ifdef _DEBUG
#define NUCLEAR_CONFIG_DEBUG
#else
#define NUCLEAR_CONFIG_RELEASE
#endif
/*
Building Configs for the whole engine goes here.

Building options:
-NE_USE_CORE_OPENGL
	-Builds whole engine in OpenGL Render API (Defines NE_COMPILE_CORE_OPENGL)
	-Also uses XShaderCompiler to cross-compile HLSL to GLSL (Defines NE_COMPILE_XSHADERCOMPILER)
-NE_USE_DIRECTX11
	-Builds whole engine in DirectX Render API (Defines NE_COMPILE_DIRECTX11)
	-Also uses DirectX HLSL Compiler to compile HLSL to DXBC (Defines NE_COMPILE_D3DCOMPILER)
-NE_USE_RUNTIME_RENDER_API
	-Builds the engine to use both DirectX11 and OpenGL 3 render APIs, (May have impact on performance so not recommended for release)
-NE_USE_RENDER_API_DEBUG_LAYER
	-Enable the Render-API debug layer to trace and detect errors
*/



#ifdef NE_USE_CORE_OPENGL
#define NE_COMPILE_CORE_OPENGL
#define NE_COMPILE_XSHADERCOMPILER
#endif

#ifdef NE_USE_DIRECTX11
#define NE_COMPILE_DIRECTX11
#define NE_COMPILE_D3DCOMPILER
#endif

#define NE_USE_RUNTIME_RENDER_API
#define NE_USE_RENDER_API_DEBUG_LAYER
#ifdef NE_USE_RUNTIME_RENDER_API

#ifdef NE_USE_CORE_OPENGL
#error "Nuclear Engine: Cannot use seperate renderers with runtime renderer!"
#endif

#ifdef NE_USE_DIRECTX11
#error "Nuclear Engine: Cannot use seperate renderers with runtime renderer!"
#endif

#define NE_COMPILE_CORE_OPENGL
#define NE_COMPILE_DIRECTX11
#define NE_COMPILE_D3DCOMPILER
#define NE_COMPILE_XSHADERCOMPILER

#ifndef _DEBUG
#warning "Nuclear Engine: Usage of runtime chosable render API slows down performance!"
#endif

#else 

#endif