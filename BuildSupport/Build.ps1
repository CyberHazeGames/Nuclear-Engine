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

# Some launch environments contain both PATH and Path, which breaks MSBuild tasks.
$buildPath = $env:Path
[Environment]::SetEnvironmentVariable('PATH', $null, 'Process')
[Environment]::SetEnvironmentVariable('Path', $null, 'Process')
[Environment]::SetEnvironmentVariable('Path', $buildPath, 'Process')

Push-Location $repoRoot
try {
    # One worker also avoids silent worker-start failures in restricted launch environments.
    & $msbuild 'Nuclear Engine.sln' /restore /m:1 /nr:false "/p:Configuration=$Configuration" /p:Platform=x64 /v:minimal /nologo
    if ($LASTEXITCODE) { throw "Solution build failed with exit code $LASTEXITCODE." }
}
finally { Pop-Location }
