# Nuclear Engine
![Alt text](http://media.indiedb.com/images/engines/1/1/764/NELOgo.png)
A Fast Cross-Platform Multi-Renderer 3D Game Engine using OpenGL 3.3+ , DirectX 11

## Features:-
- __Core__
  - Crossplatform ready
  - Modern Fast code using C++
  
- __Renderer3D__
  - Support for Forward Shading with Phong Light Model.
  
  
- __ResourceManager__
  - Support for PNG, JPEG, BMP, DDS, GIF, HDR, PSD and many more image formats (using stb_image library).
  - Support for OBJ, FBX, Blend, 3DS, CAD, and many more 3D model formats (using Assimp library).

- __OneAPI__
  - Support for DirectX 11 "Feature Level 10.1" and OpenGL 3.3 Core Renderers.
  - Support for Point, Bilinear, Trilinear, Anisotropic "X2, X4, X8, X16", Filtering for Textures.
  - Support for HLSL Shaders which can be Cross Compiled to GLSL.
  
## Current Platforms
  - Windows 7+     

## Current Milestones
  - Finish the OneAPI:
    - Finish RenderTarget class (also known as Framebuffer in OpenGL).
    - Implement TextureCube class (required for Skybox).
  - Correctly Implement Model Loading with Assimp.
  - Implement Blinn-Phong & Gourard Light Model.
  ![Alt text](http://media.indiedb.com/images/engines/1/1/764/shit.png "The Current MileStone")
  - Implement Skybox.

## Roadmap
  - Support Android & Linux & UWP Platforms.
  - Support PhysX or Bullet Physics Library.
  - Support A Uber Shader instead of GLSL , HLSL.
  - Support Text Renderering With immediate mode GUI.
  - Support 2D Renderer and its classes.
  - There are more but these are the most important...

### Building
You need Visual Studio 2017 15.4.2 to build it and look at Deps section

### Required Deps
  - Glew
  - OpenAL
  - Assimp
  - Windows API (Which has DirectX & XAudio)
