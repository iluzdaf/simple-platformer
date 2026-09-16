# Simple Platformer

Simple Platformer is a C++17 teaching engine and example game built in independently
testable phases. The current implementation includes platformer movement, tile collision,
scrolling, composed actors, NPC finite state machines, flying and platformer pathfinding,
360-degree projectiles, animation, and ImGui debugging tools.

Read [ARCHITECTURE.md](ARCHITECTURE.md) for the agreed design and implementation plan.

## Requirements

- CMake 3.21 or newer
- A C++17 compiler:
  - Apple Clang supplied with current Xcode on macOS
  - Visual Studio 2022 with the **Desktop development with C++** workload on Windows
- **C++ CMake tools for Windows** when installing Visual Studio

All third-party source required by the project is vendored under `external/`.

## macOS: configure, build, and test

The shared macOS preset uses the build tools supplied with Xcode.

```sh
cmake --preset mac-debug
cmake --build --preset mac-debug
ctest --preset mac-debug
```

Run the example application:

```sh
cd build/mac-debug
./simple_platformer
```

Use the left and right arrow keys to move, Space to jump, the mouse to aim, the left
mouse button to fire, and Escape to close the window. Press F1 to show or hide the
debug overlay text and the in-game sprite, collider, camera, dead-zone, and NPC path
overlays.

## Windows: create and use the Visual Studio solution

Install Visual Studio 2022 with **Desktop development with C++** and **C++ CMake
tools for Windows** selected in the Visual Studio Installer. Then double-click:

```text
setup-windows.bat
```

The script finds CMake, generates `build/windows-vs/SimplePlatformer.sln`, and opens
the solution. The `simple_platformer` project is already selected as the startup
project, so build the solution and press **F5** to run the game.

To run all automated tests from Visual Studio, find the `run_tests` project in
Solution Explorer, right-click it, and choose **Build**. This first builds the test
executable and then displays the CTest results in Visual Studio's Output window.

The solution is generated from `CMakeLists.txt` and `CMakePresets.json`. It belongs in
the ignored `build/` directory and should not be committed. Run `setup-windows.bat`
again after changing the CMake configuration.

Anyone comfortable with the terminal can perform the same steps with:

```powershell
cmake --preset windows-vs
cmake --build --preset windows-debug
ctest --preset windows-debug
```

The Windows executable is:

```text
build\windows-vs\Debug\simple_platformer.exe
```

`CMakePresets.json` contains the shared macOS and Windows configurations.
`CMakeUserPresets.json` is ignored and is available for personal configuration that
should not be shared.

## Continuous integration

GitHub Actions configures, builds, and runs all tests on both macOS with Apple Clang
and Windows with Visual Studio 2022. The workflow runs for every push and pull request.
The Windows job generates the same solution as `setup-windows.bat`, builds the actual
`.sln` with MSBuild, and builds its generated `run_tests` project. CI does not launch
the graphical game.

A separate Linux quality job checks source formatting, runs clang-tidy, and verifies
that every public header can compile on its own. These checks do not add any tools to
the normal macOS or Visual Studio build.

## Formatting

The checked-in `.clang-format` defines the shared C and C++ style, while
`.editorconfig` provides basic editor settings such as indentation and line endings.
Both Visual Studio and VS Code discover these files automatically.

In Visual Studio, use **Format Document** (`Ctrl+K`, `Ctrl+D`) to format the current
file. No additional formatting extension is required.

In VS Code, the recommended clangd extension formats C and C++ files automatically
when they are saved.

To format every first-party C++ file from the command line, use:

```sh
cmake --build --preset mac-debug --target format
```

To check formatting without changing files, use:

```sh
cmake --build --preset mac-debug --target format-check
```

These command-line targets require `clang-format` on `PATH` and deliberately exclude
`external/`.

## Static analysis

The checked-in `.clang-tidy` checks naming, unused and missing includes, common bugs,
and performance mistakes. VS Code's recommended clangd extension reports unused and
missing includes while editing. Treat include-cleaner suggestions as findings to
review; do not automatically remove headers without rebuilding and running the tests.

Static analysis is enforced by CI using LLVM 18, but remains optional for local
builds. Developers with clang-tidy installed can run it with:

```sh
cmake --build --preset mac-debug --target tidy
```

The `header_self_containment` target verifies that public headers include everything
they need themselves:

```sh
cmake --build --preset mac-debug --target header_self_containment
```

## Repository layout

```text
app/        executable entry point
assets/     finished runtime sprite atlas supplied with the example game
include/    public core headers
src/        core implementations
tests/      Catch2 tests mirroring the core subjects
external/   fixed third-party source releases
.github/    continuous-integration workflow
```

## License

Simple Platformer is available under the [MIT License](LICENSE). Third-party
dependencies retain their own licenses as documented in
[THIRD_PARTY.md](THIRD_PARTY.md).
