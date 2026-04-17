$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
cmake -S "$root/lib" -B "$root/lib/stub_build" -G "Visual Studio 17 2022" -A x64
cmake --build "$root/lib/stub_build" --config Release
