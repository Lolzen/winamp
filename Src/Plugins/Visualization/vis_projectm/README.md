# projectM visualization plug-in

This directory contains the native projectM replacement for the legacy
DirectX/MilkDrop visualization. The Windows target is a Winamp VIS plug-in;
the Linux target is built into the main GTK application as a `GtkGLArea`
window because the current Linux plug-in loader only supports input and
output modules.

## Dependencies

Both targets use projectM 4's C API (`projectM-4/projectM.h`). projectM must
be built with desktop OpenGL support.

On Linux, install the distribution's projectM 4 development package (the
package name varies by distribution), then build the normal Linux target:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

The CMake build looks for the `projectM-4` pkg-config module. Set
`WINAMP_PROJECTM_PRESETS` to a colon-separated list of preset directories if
the distribution installs presets somewhere non-standard.

On Windows, `install-packages.cmd` installs projectM through vcpkg for both
Win32 and x64. The Visual Studio project uses
`vcpkg\installed\<triplet>` by default. Set `PROJECTM_ROOT` to a different
vcpkg install prefix when required. The projectM runtime DLLs are copied next
to `vis_projectm.dll` after a successful build.

Set `WINAMP_PROJECTM_PRESETS` to a semicolon-separated list of preset
directories on Windows. The left and right arrow keys select the previous and
next preset in the visualizer window.
