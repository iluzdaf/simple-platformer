# Simple Platformer

Simple Platformer is a C++17 teaching engine and example game built from independently
testable systems. The current implementation includes platformer movement, tile collision,
scrolling, composed actors, NPC finite state machines, flying and platformer pathfinding,
360-degree projectiles, animation, inventory, automatic pickups, a three-level game loop,
and ImGui debugging tools.

New to the project? Start with [START_HERE.md](START_HERE.md). It gives a recommended
route through the code and points out which details can wait until later.

After that, read [ARCHITECTURE.md](ARCHITECTURE.md) for the detailed design, ownership
rules, runtime flow, and reasons behind the main decisions.

## Requirements

- CMake 3.21 or newer
- A C++17 compiler:
  - Apple Clang supplied with current Xcode on macOS
  - Visual Studio 2022 with the **Desktop development with C++** workload on Windows
- **C++ CMake tools for Windows** when installing Visual Studio

All third-party source required by the project is vendored under `external/`.

The supported development platforms are macOS and Windows, where development and
graphical testing take place. Linux is used solely for CI quality checks and is not a
supported local development workflow.

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

## Running focused tests

Build before running CTest so the test executable includes your changes. On macOS,
list test names or run only tests whose names contain `Pickup` with:

```sh
ctest --preset mac-debug -N
ctest --preset mac-debug -R "Pickup" --output-on-failure
```

On Windows, use the `windows-debug` test preset. Use your personal preset name if
configured. Omit `-R "Pickup"` to run the complete suite.

## Playing the supplied game

These controls apply on both platforms. Use A and D or the left and right arrow keys
to move, W, Up, or Space to jump, the mouse to aim, the left mouse button to fire, and
Escape to close the window. Press F1 to toggle the debug overlay, including colliders,
NPC sensing, patrol points, and navigation paths. The hearts at the top-left show
the player's current and maximum health.

Walk over items to collect them. Click the bag at the bottom-left or press Q to pause
and open the inventory, then click a health potion to drink it. Click the bag or press Q
again to resume. Find each level's key and reach its bunker door to unlock the exit.
Each door consumes one key; the third exit completes the supplied campaign. Press R
at the completion message to restart from the configured starting level.

To change the game, use the [development loop](START_HERE.md#everyday-development-loop)
and [project starting points](START_HERE.md#starting-project-work). For level layouts
and shared definitions, see the
[content-file guide](ARCHITECTURE.md#content-files-at-a-glance).

## Continuous integration

GitHub Actions configures, builds, and runs all tests on both macOS with Apple Clang
and Windows with Visual Studio 2022. The workflow runs for pushes to `main` and for
pull requests. The Windows job generates the same solution as `setup-windows.bat`,
builds the actual `.sln` with MSBuild, and builds its generated `run_tests` project. CI
does not launch the graphical game.

The macOS and Windows jobs use a pinned `sccache` release backed by GitHub Actions'
cache service. Only compiler outputs are cached; generated build directories are not.
On Windows, CI still builds the generated Visual Studio solution and only replaces
`cl.exe` with a cache wrapper for that build. This does not affect local student builds.

A separate Linux quality job runs on pull requests. It checks source formatting, runs clang-tidy, and verifies
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

These command-line targets use the `CLANG_FORMAT_EXECUTABLE` found during CMake
configuration (or set explicitly) and deliberately exclude `external/`. CI uses
clang-format 18. Editor formatting uses the editor's selected tool, which may be a
different version even when a CMake preset selects LLVM 18.

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

For matching local quality tools, set `CLANG_FORMAT_EXECUTABLE` and
`CLANG_TIDY_EXECUTABLE` to LLVM 18 executables in a personal `CMakeUserPresets.json`
preset, then configure and build using that preset. These variables select quality
tools, not the C++ compiler. A personal preset such as `mac-debug-llvm18` is not part
of the shared checkout. Compiler and SDK differences can still produce different
diagnostics from CI.

The `header_self_containment` target verifies that public headers include everything
they need themselves:

```sh
cmake --build --preset mac-debug --target header_self_containment
```

## Repository layout

```text
app/        application shell, example game, graphics, UI, and debug tools
assets/     runtime sprite atlas, level/tile/actor/item/pickup catalogues, and editable level JSON
include/    public core headers
src/        core implementations
tests/      Catch2 tests for core systems and testable application code
external/   fixed third-party source releases
.github/    continuous-integration workflow
```

The recommended code-reading route is in `START_HERE.md`; detailed design decisions are
in `ARCHITECTURE.md`.

## License

Simple Platformer is available under the [MIT License](LICENSE). Third-party
dependencies retain their own licenses as documented in
[THIRD_PARTY.md](THIRD_PARTY.md).
