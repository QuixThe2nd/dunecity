$minimumAndroidNdkMajor = 26

function Get-AndroidNdkMajor([string]$Path) {
    $sourceProperties = Join-Path $Path "source.properties"
    if (Test-Path -LiteralPath $sourceProperties) {
        $revision = Select-String -LiteralPath $sourceProperties -Pattern '^Pkg\.Revision\s*=\s*([0-9]+)' |
            Select-Object -First 1
        if ($null -ne $revision) {
            return [int]$revision.Matches[0].Groups[1].Value
        }
    }
    $directoryName = Split-Path -Leaf $Path
    if ($directoryName -match '^([0-9]+)(?:\.|$)') {
        return [int]$Matches[1]
    }
    return 0
}

function Test-SupportedAndroidNdk([string]$Path) {
    return (Test-Path -LiteralPath (Join-Path $Path "build\cmake\android.toolchain.cmake")) -and
        ((Get-AndroidNdkMajor $Path) -ge $minimumAndroidNdkMajor)
}

if ([string]::IsNullOrWhiteSpace($env:JAVA_HOME)) {
    throw "Set JAVA_HOME to your JDK before running this script."
}

if ([string]::IsNullOrWhiteSpace($env:ANDROID_HOME)) {
    $env:ANDROID_HOME = $env:ANDROID_SDK_ROOT
}
if ([string]::IsNullOrWhiteSpace($env:ANDROID_HOME)) {
    throw "Set ANDROID_HOME or ANDROID_SDK_ROOT to your Android SDK before running this script."
}
$env:ANDROID_SDK_ROOT = $env:ANDROID_HOME

if ([string]::IsNullOrWhiteSpace($env:ANDROID_NDK_HOME)) {
    $ndkRoots = @(
        (Join-Path $env:ANDROID_HOME "ndk"),
        (Join-Path $env:LOCALAPPDATA "Android\Sdk\ndk")
    ) | Select-Object -Unique
    foreach ($ndkRoot in $ndkRoots) {
        if (Test-Path -LiteralPath $ndkRoot) {
            $latestNdk = Get-ChildItem -LiteralPath $ndkRoot -Directory |
                Where-Object { Test-SupportedAndroidNdk $_.FullName } |
                Sort-Object Name -Descending |
                Select-Object -First 1
            if ($null -ne $latestNdk) {
                $env:ANDROID_NDK_HOME = $latestNdk.FullName
                break
            }
        }
    }
}
if ([string]::IsNullOrWhiteSpace($env:ANDROID_NDK_HOME)) {
    throw "Set ANDROID_NDK_HOME or install a complete Android NDK 26 or newer under an Android SDK ndk directory."
}
if (-not (Test-SupportedAndroidNdk $env:ANDROID_NDK_HOME)) {
    $detectedMajor = Get-AndroidNdkMajor $env:ANDROID_NDK_HOME
    throw "Android NDK 26 or newer is required; '$env:ANDROID_NDK_HOME' reports major version $detectedMajor or is incomplete."
}

$env:Path = "$env:JAVA_HOME\bin;$env:ANDROID_HOME\platform-tools;$env:ANDROID_HOME\cmdline-tools\latest\bin;$env:Path"

Write-Host "JAVA_HOME=$env:JAVA_HOME"
Write-Host "ANDROID_HOME=$env:ANDROID_HOME"
Write-Host "ANDROID_NDK_HOME=$env:ANDROID_NDK_HOME"
