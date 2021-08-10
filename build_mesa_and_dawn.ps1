# Build Mesa spirv_to_dxil, copy relevant files to Dawn, then build dawn_end2end_tests.
#
# Requires:
#  - Run form "Developter Powershell for VS 2019"
#  - $env:DEPOT_TOOLS_WIN_TOOLCHAIN="0"

Param(
    [Parameter(Mandatory = $true)]
    [String] $mesa_dir,

    [Parameter(Mandatory = $true)]
    [String] $mesa_target_dir,

    [Parameter(Mandatory = $true)]
    [String] $dawn_target_dir
)

meson compile -C "$mesa_target_dir" spirv_to_dxil:shared_library
if ($LASTEXITCODE) { return; }

Copy-Item -Path "$mesa_dir\src\microsoft\spirv_to_dxil\spirv_to_dxil.h" -Destination ".\src\dawn_native\d3d12"
if ($LASTEXITCODE) { return; }

Copy-Item -Path "$mesa_target_dir\src\microsoft\spirv_to_dxil\spirv_to_dxil.dll" -Destination "$dawn_target_dir"
if ($LASTEXITCODE) { return; }

ninja -C "$dawn_target_dir" dawn_end2end_tests
if ($LASTEXITCODE) { return; }
