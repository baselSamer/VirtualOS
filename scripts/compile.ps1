param(
    [string]$GccPath = "",
    [string]$SourceRoot = "Src",
    [string]$OutputPath = "main.exe"
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($GccPath)) {
    $gccCmd = Get-Command gcc -ErrorAction SilentlyContinue
    if ($gccCmd -ne $null) {
        $GccPath = $gccCmd.Source
    }
}

if ([string]::IsNullOrWhiteSpace($GccPath) -or -not (Test-Path $GccPath)) {
    throw "GCC executable not found. Install a GCC toolchain and make sure 'gcc' is in PATH, or pass -GccPath explicitly."
}

if (-not (Test-Path $SourceRoot)) {
    throw "Source root not found: $SourceRoot"
}

$sources = Get-ChildItem -Path $SourceRoot -Recurse -Filter "*.c" | ForEach-Object { $_.FullName }

if (-not $sources -or $sources.Count -eq 0) {
    throw "No C source files found under: $SourceRoot"
}

$mainPath = Resolve-Path (Join-Path $SourceRoot "main.c") -ErrorAction SilentlyContinue
if ($mainPath) {
    $mainFull = $mainPath.Path
    $sources = @($mainFull) + ($sources | Where-Object { $_ -ne $mainFull } | Sort-Object)
} else {
    $sources = $sources | Sort-Object
}

$resolvedOut = Join-Path (Get-Location) $OutputPath
$args = @("-fdiagnostics-color=always", "-g") + $sources + @("-o", $resolvedOut)

$quotedArgs = $args | ForEach-Object { '"{0}"' -f $_ }
$commandString = '"{0}" {1}' -f $GccPath, ($quotedArgs -join " ")

Write-Host "Compiling with command:"
Write-Host $commandString
Write-Host ""

& $GccPath @args
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Host "Build completed successfully. Output: $resolvedOut"
