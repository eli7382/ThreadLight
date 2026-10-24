# Phase 2.2 provisional Windows test build

This is an **unverified test build** of ThreadLight Phase 2.2 from `codex/phase2-visuals`. It is not a stable release and Phase 2.2 has not been accepted. Extract `Threadlight.exe` and `RAYLIB-LICENSE.txt` to a writable folder, then launch the executable. No development tools or separately installed raylib runtime are required on compatible Windows x64 systems; ordinary Windows graphics, audio, and system components are still required.

The included build uses raylib 5.5 and statically links the MSVC runtime. Its imported DLL dependencies are Windows system components: `WINMM.dll`, `KERNEL32.dll`, `USER32.dll`, `GDI32.dll`, and `SHELL32.dll`.

## Controls and settings

- **WASD / arrow keys** move; **Space** dashes.
- **P** pauses or resumes. During active play, **Esc** does nothing. While paused, **Esc** returns to the title.
- Use **Up / Down** and **Enter** in menus. Settings are available from the title and pause menus; **Tab** switches Video and Audio. **Esc** returns to the prior screen.
- Video settings select 30 / 60 / 120 / Unlimited FPS and Show FPS. Unlimited removes the game's limiter; the display or driver may still cap output.
- Audio settings control SFX volume. Mouse dragging or Left / Right keys adjust it, and changes apply immediately.
- Title-screen **Esc** opens an exit confirmation; **No** is selected by default.

Settings persist in `%APPDATA%\ThreadLight\settings.ini`. If that file is absent or invalid, defaults are restored.

## Manual checks requested

Please test maximized and resized windows at available display scaling settings, especially arena edges/player movement, and try each FPS limit. Also check settings persistence, menu/Escape behavior, sound balance, crowded combat readability, and victory/defeat flow. A successful automated build or startup does not constitute visual, audio, or gameplay acceptance.

The archive's `SHA256SUMS.txt` contains the executable checksum. The GitHub release page also lists the ZIP checksum for download verification.
