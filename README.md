# Threadlight: The Briar Circuit

A small, self-contained top-down survival game built in C++20 with raylib 5.5. Weave a fading silk wake through the hunters: recent thread cuts them, while sparks let you choose upgrades. Survive five waves.

## Play

- **WASD** or **arrow keys**: move
- **Space**: dash (briefly invulnerable)
- **P** or **Esc**: pause and resume
- During an evolution choice, press **1 / 2 / 3** (or **A / D / S**) and then **Enter**
- **Enter** starts and retries; **R** restarts from pause; **Esc** returns to title after a run

All visuals and sound effects are generated at runtime. No external art or audio assets are needed.

## Audio (Phase 2.1)

Effects are synthesized at startup and cover dash, silk hits and kills, spark collection, player damage, evolution, victory, and defeat. Each cue uses a short shaped envelope, layered tones, and slight pitch variation. The mixer keeps at most eight effects active; important feedback can replace a lower-priority sound, and simultaneous voices are attenuated to leave mix headroom. There is no background music.

To change the master SFX volume, pause with **P** or **Esc**, then use the **Left / Right arrow keys** to lower or raise it in 5% steps. The pause screen shows the current percentage. The title and play HUD point to the pause control. Arrow keys work regardless of keyboard layout.

The Windows x64 Phase 2.1 build in this workspace is `build/Phase2.1-accepted-20261024/Threadlight-Phase2.1-accepted.exe`.

### Manual listening check

Listen at a comfortable system volume, then try the master control at low, default, and maximum settings using the arrow keys while paused. Check that dash has a soft descending airy attack; enemy hits stay light while kills have a fuller resolving cue; collected sparks sound warm and understated without disappearing; player damage is lower and darker; evolution rises; and victory and defeat are clearly different. Trigger several events close together and listen for clipping, abrupt volume pumping, or a cue that masks player damage. Also confirm the red impact pulse and expanding ring are clear without flashing, while the dash remains visibly cyan. These perceptual checks need a human listener and are not covered by the automated synthesis tests.

## Windows build

A prebuilt Windows x64 executable is included as `Threadlight.exe`. To build from source, install Visual Studio 2022 C++ tools, CMake, and raylib 5.5 with a CMake package configuration (for example, the `x64-windows` raylib package from vcpkg). From this directory, configure and build:

```powershell
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build --config Release
```

Then run `build\Release\Threadlight.exe`. If raylib is installed outside vcpkg, configure `CMAKE_PREFIX_PATH` to its package prefix instead.

## Logic regression tests

The focused state and spawn tests do not need raylib. Configure them with:

```powershell
cmake -S . -B build-tests -DTHREADLIGHT_BUILD_GAME=OFF
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

The included executable was built with MSVC 19.44.35229.0, Windows x64, and raylib 5.5. Raylib's license is included in `RAYLIB-LICENSE.txt`.

