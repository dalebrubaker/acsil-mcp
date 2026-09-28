# Static x64 triplet pinned to the v143 toolset the projects build with. Without the pin, vcpkg
# compiles dependencies with the newest installed MSVC (e.g. v145 from Visual Studio 2026), and
# linking them into v143 projects fails on missing STL symbols.
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_PLATFORM_TOOLSET v143)
