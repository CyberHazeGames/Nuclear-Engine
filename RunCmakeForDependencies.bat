cmake External\Assimp\ -B External\CmakeProjects\Assimp\ -G "Visual Studio 18 2026" -A x64
cmake External\Diligent\ -B External\CmakeProjects\Diligent\ -G "Visual Studio 18 2026" -A x64 -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake External\OpenAL\ -B External\CmakeProjects\OpenAL\ -G "Visual Studio 18 2026" -A x64 -DCMAKE_POLICY_VERSION_MINIMUM=3.5
pause
