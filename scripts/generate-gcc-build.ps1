param(
    [string]$GccPath = "c:\Program Files\gcc-15.2.0-gdb-16.3.90.20250511-binutils-2.45-mingw-w64-v13.0.0-ucrt\bin\gcc.exe",
    [string]$SourceRoot = "Src",
    [string]$OutputPath = "Src\main.exe",
    [switch]$Execute
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $GccPath)) {
    throw "GCC executable not found at: $GccPath"
}

if (-not (Test-Path $SourceRoot)) {
    throw "Source root not found: $SourceRoot"
}

$sources = Get-ChildItem -Path $SourceRoot -Recurse -Filter "*.c" | ForEach-Object { $_.FullName }

if (-not $sources -or $sources.Count -eq 0) {
    throw "No C source files found under: $SourceRoot"
}

# Keep build deterministic and ensure entry file comes first if present.
$mainPath = (Resolve-Path (Join-Path $SourceRoot "main.c") -ErrorAction SilentlyContinue)
if ($mainPath) {
    $mainFull = $mainPath.Path
    $sources = @($mainFull) + ($sources | Where-Object { $_ -ne $mainFull } | Sort-Object)
} else {
    $sources = $sources | Sort-Object
}

$resolvedOut = (Join-Path (Get-Location) $OutputPath)
$args = @("-fdiagnostics-color=always", "-g") + $sources + @("-o", $resolvedOut)

$quotedArgs = $args | ForEach-Object { '"{0}"' -f $_ }
$commandString = '"{0}" {1}' -f $GccPath, ($quotedArgs -join " ")

Write-Host "Generated build command:"
Write-Host $commandString

if ($Execute) {
    Write-Host "`nExecuting build command..."
    & $GccPath @args
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
    Write-Host "Build completed successfully."
}
