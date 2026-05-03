# Stasis

Stasis is a Windows x64 C++/JNI research project for experimenting with an injected OpenGL overlay in Minecraft 1.8.9. It contains a small module system, JNI wrappers around selected Minecraft classes, an ImGui click GUI, and a bundled injector that embeds the built DLL as a resource.

This repository is intended for local research, private testing, and learning about Windows DLL injection, JNI attachment, and OpenGL render hooks. Do not use it on servers or environments where you do not have permission.

## Features

- CMake/MSVC project for a 64-bit DLL and companion injector.
- JNI attachment helpers for interacting with a running JVM.
- OpenGL hook using MinHook.
- ImGui-based click GUI.
- Minimal module manager and keybind handler.

## Requirements

- Windows x64.
- Visual Studio 2022 with the Desktop development with C++ workload.
- CMake 3.20 or newer.
- A 64-bit JDK. `JAVA_HOME` must point to this JDK.

## Build

From the repository root:

```bat
cmake -B build -A x64
cmake --build build --config Release
```

Outputs:

- `build/Release/stasis.dll`
- `build/Release/StasisInjector.exe`

During the build, CMake copies `stasis.dll` into `injector/stasis.dll` so the resource compiler can embed it in `StasisInjector.exe`. That copied DLL is generated and intentionally ignored by Git.

## Project Layout

```text
injector/     Windows injector executable and resource definition
src/          DLL source code
vendor/imgui Dear ImGui source used by the overlay
vendor/minhook MinHook source, headers, and x64 import library
```

## Controls

| Key | Action |
| --- | --- |
| `M` | Toggle the ClickGUI |
| `End` | Unload the DLL |

## Credits

- Injection architecture reference: [misery1x/Flame](https://github.com/misery1x/Flame).
- GUI library: [Dear ImGui](https://github.com/ocornut/imgui), by Omar Cornut and contributors.
- Hooking library: [MinHook](https://github.com/TsudaKageyu/minhook), by Tsuda Kageyu and contributors.
- Original stasis module: [Raven BS](https://codeberg.org/strangerrrrs/raven-bS/)

Dependency licenses are kept in their respective `vendor/` folders.

## Notes

- The code targets Minecraft 1.8.9 obfuscated names. Different jars or mappings may require SDK updates.
- Build outputs, local logs, previous chat history, and embedded DLL payloads are excluded from version control.
