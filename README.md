# StrafeHelper Remake

A clean-room Windows C++20 movement-training utility inspired by the general idea of configurable movement timing tools.

## Scope

This implementation is intentionally standalone. It generates and visualizes training events internally; it does **not** inject keyboard input into Apex Legends or any other process, use kernel drivers, bypass anti-cheat, or conceal itself.

## Requirements

- Windows 10/11
- Visual Studio 2022 with **Desktop development with C++**
- C++ CMake tools for Windows

Microsoft documents native CMake integration in Visual Studio. Open the folder containing `CMakeLists.txt`, select the x64 configuration, and build/run from the IDE.

## Build with Visual Studio

1. Install Visual Studio with Desktop development with C++.
2. Open this folder in Visual Studio.
3. Select `windows-x64-debug` or `windows-x64-release`.
4. Build `StrafeHelper`.
5. Run it.

## Command line

From a Developer Command Prompt:

```bat
cmake --preset windows-x64-release
cmake --build --preset build-release
ctest --test-dir out/build/windows-x64-release -C Release --output-on-failure
```

## Controls

- **F8** — toggle training
- **ESC** — exit
- **Start/Stop Training** — toggle from the UI
- **Reset Statistics** — reset counters

Configuration is stored under `%APPDATA%\StrafeHelperRemake\config.ini`.

## Project structure

```text
StrafeHelper_Remake/
  CMakeLists.txt
  CMakePresets.json
  README.md
  src/
    App.cpp
    App.h
    Config.cpp
    Config.h
    MovementTrainer.cpp
    MovementTrainer.h
    main.cpp
  tests/
    MovementTrainerTests.cpp
```

The project deliberately avoids third-party runtime dependencies, so it can build with the normal Windows/MSVC toolchain.


## One-command Windows release

From a **Visual Studio 2022 Developer PowerShell**:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\build-release.ps1
```

The script:

1. Locates the Visual Studio 2022 C++ toolchain.
2. Configures an x64 Release build with Ninja.
3. Builds the application.
4. Runs the unit tests.
5. Stages only `StrafeHelper.exe` and its README.
6. Creates:

```text
dist\StrafeHelper-Portable-v1.0.0-win-x64.zip
```

### Optional installer

Install Inno Setup, then run:

```powershell
.\build-release.ps1 -BuildInstaller
```

This additionally creates:

```text
dist\installer\StrafeHelper-Setup-v1.0.0.exe
```

The portable package requires no installer and can simply be extracted and run.

## Build the EXE without installing Visual Studio

You can let GitHub compile the Windows executable for you:

1. Create a GitHub repository and upload this project's files.
2. Open the repository's **Actions** tab.
3. Select **Build Windows EXE**.
4. Click **Run workflow**.
5. When it finishes, open that workflow run and download the **StrafeHelper-Windows-x64** artifact.
6. Extract it to get `StrafeHelper.exe`.

Nothing needs to be installed locally to perform the build; compilation happens on GitHub's Windows runner.
