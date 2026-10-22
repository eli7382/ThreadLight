# Threadlight: The Briar Circuit

A small, self-contained top-down survival game built in C++20 with raylib 5.5. Weave a fading silk wake through the hunters: recent thread cuts them, while sparks let you choose upgrades. Survive five waves.

## Play

- **WASD** or **arrow keys**: move
- **Space**: dash (briefly invulnerable)
- **P** or **Esc**: pause and resume
- During an evolution choice, press **1 / 2 / 3** (or **A / D / S**) and then **Enter**
- **Enter** starts and retries; **R** restarts from pause; **Esc** returns to title after a run

All visuals and sound effects are generated at runtime. No external art or audio assets are needed.

## Windows build

A prebuilt Windows x64 executable is included as `Threadlight.exe`. To build from source, install Visual Studio 2022 C++ tools, CMake, and raylib 5.5 with a CMake package configuration (for example, the `x64-windows` raylib package from vcpkg). From this directory, configure and build:

```powershell
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build --config Release
```

Then run `build\Release\Threadlight.exe`. If raylib is installed outside vcpkg, configure `CMAKE_PREFIX_PATH` to its package prefix instead.

The included executable was built with MSVC 19.44.35229.0, Windows x64, and raylib 5.5. Raylib's license is included in `RAYLIB-LICENSE.txt`.

