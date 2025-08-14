# msvc.ps1
# Usage: pwsh -ExecutionPolicy Bypass -File ./msvc.ps1 build.bat gameonly

function Save-EnvVars($path) {
    Get-ChildItem env: |
        ForEach-Object { "$($_.Name)=$($_.Value)" } |
        Set-Content -Encoding UTF8 $path
}

function Load-EnvVars($path) {
    Get-Content $path | ForEach-Object {
        if ($_ -match '^(.*?)=(.*)$') {
            Set-Item -Path "env:$($matches[1])" -Value $matches[2]
        }
    }
}

$cacheFile = "$PSScriptRoot/build/cached_env.txt"

if (Test-Path $cacheFile) {
    Load-EnvVars $cacheFile
} else {

    $vsPath = "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    if (-not (Test-Path $vsPath)) {
        $vsPath = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    }
    if (-not (Test-Path $vsPath)) {
        Write-Error "vcvars64.bat not found in Program Files or Program Files (x86)"
        exit 1
    }
    $bat = $vsPath

    # Run vcvars64.bat and capture the resulting environment
    & cmd /c "`"$bat`" && set" |
        ForEach-Object {
            if ($_ -match '^(.*?)=(.*)$') {
                Set-Item -Path "env:$($matches[1])" -Value $matches[2]
            }
        }

    Save-EnvVars $cacheFile
}

# Prepare arguments
$cmd = $args[0]
$cmdArgs = @()
if ($args.Count -gt 1) {
    $cmdArgs = $args[1..($args.Count - 1)]
}

& $cmd @cmdArgs
