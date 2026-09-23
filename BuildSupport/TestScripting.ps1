param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug'
)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vsRoot = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
if (!$vsRoot) { throw 'Visual Studio with MSBuild and the v145 C++ toolset is required.' }
$msbuild = Join-Path $vsRoot 'MSBuild/Current/Bin/amd64/MSBuild.exe'

# Some launch environments contain both PATH and Path; MSBuild rejects that.
$buildPath = $env:Path
[Environment]::SetEnvironmentVariable('PATH', $null, 'Process')
[Environment]::SetEnvironmentVariable('Path', $null, 'Process')
[Environment]::SetEnvironmentVariable('Path', $buildPath, 'Process')

Push-Location $repoRoot
try {
    & dotnet build SamplesScripts/SamplesScripts.csproj -c $Configuration --nologo /m:1 /nr:false
    if ($LASTEXITCODE) { throw 'Sample scripts build failed.' }
    & dotnet build Tests/ScriptingSmoke/Managed/ScriptingSmoke.Managed.csproj -c $Configuration --nologo /m:1 /nr:false
    if ($LASTEXITCODE) { throw 'Managed smoke fixture build failed.' }
    & $msbuild Tests/ScriptingSmoke/ScriptingSmoke.vcxproj /m:1 /nr:false "/p:Configuration=$Configuration" /p:Platform=x64 /v:minimal /nologo
    if ($LASTEXITCODE) { throw 'Native scripting smoke build failed.' }
    & "./Bin/${Configuration}X64/ScriptingSmoke.exe" "./Bin/${Configuration}X64"
    if ($LASTEXITCODE) { throw 'Scripting smoke check failed.' }
}
finally { Pop-Location }
