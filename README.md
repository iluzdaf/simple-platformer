# Simple Platformer

Simple Platformer is a C++17 teaching engine and example game built from independently
testable systems. The current implementation includes platformer movement, tile collision,
scrolling, composed actors, NPC finite state machines, flying and platformer pathfinding,
360-degree projectiles, animation, inventory, automatic pickups, a three-level game loop,
and ImGui debugging tools.

## Documentation

| Document | What it covers |
| --- | --- |
| [START_HERE.md](docs/START_HERE.md) | A recommended route through the code, and which details can wait until later. |
| [ARCHITECTURE.md](docs/ARCHITECTURE.md) | Design, ownership rules, runtime flow, and the reasons behind the main decisions. |
| [CONTENT.md](docs/CONTENT.md) | The authoring reference for the JSON level and definition files under `assets`. |
| [FUTURE_WORK.md](docs/FUTURE_WORK.md) | Designs the repository deliberately does not implement. |

New to the project? Start with START_HERE.md.

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

Run the example game:

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

## Playing the example game

These controls apply on both platforms. Use A and D or the left and right arrow keys
to move, W, Up, or Space to jump, the mouse to aim, the left mouse button to fire, and
Escape to close the window. Press F1 to toggle the debug overlay, including colliders,
NPC sensing, patrol points, and navigation paths. The hearts at the top-left show
the player's current and maximum health.

Walk over items to collect them. Click the bag at the bottom-left or press Q to pause
and open the inventory, then click a health potion to drink it. Click the bag or press Q
again to resume. Find each level's key and reach its bunker door to unlock the exit.
Each door consumes one key; the third exit completes the example campaign. Press R
at the completion message to restart from the configured starting level.

## Continuous integration

GitHub Actions runs three jobs. The names below are the ones shown on a pull request.

| Job | Runner | What it does | Runs on |
| --- | --- | --- | --- |
| macOS / Apple Clang | `macos-latest` | Configures, builds, and runs the whole test suite. | pushes to `main` and pull requests |
| Windows / Visual Studio 2022 | `windows-2022` | Generates the same solution as `setup-windows.bat`, builds the `.sln` with MSBuild, then builds its `run_tests` project. | pushes to `main` and pull requests |
| Formatting and static analysis | `ubuntu-24.04` | Checks C++ and JSON formatting, runs clang-tidy, and verifies that every public header compiles on its own. | pull requests only |

The quality job is skipped on pushes because branch protection already ran it on the
pull request. Its checks add no tools to the macOS or Visual Studio build, and Linux is
not a supported platform for local work. No job launches the graphical game.

The macOS and Windows jobs use a pinned `sccache` release backed by GitHub Actions'
cache service. Only compiler outputs are cached; generated build directories are not.
On Windows, only `cl.exe` is replaced with a cache wrapper; the solution itself is still
built as generated. None of this affects local builds.

## Formatting

`.clang-format` defines the C and C++ style and `.prettierrc` the JSON style.
`.editorconfig` supplies the indentation and line endings shared by both.

| | Config | Tool | VS Code | Visual Studio |
| --- | --- | --- | --- | --- |
| C and C++ | `.clang-format` | clang-format 18 | on save, through clangd | **Format Document** (`Ctrl+K`, `Ctrl+D`) |
| JSON | `.prettierrc` | Prettier 3.9.8 | on save, through the Prettier extension | not supported, use the command line |

Both editors read `.clang-format` and `.editorconfig` without an extension. Visual
Studio does not read `.prettierrc`, so JSON there is formatted from the command line
or caught by CI.

Format first-party C++, or check it without changing files:

```sh
cmake --build --preset mac-debug --target format
cmake --build --preset mac-debug --target format-check
```

Format first-party JSON, or check it without changing files:

```sh
cmake --build --preset mac-debug --target format-json
cmake --build --preset mac-debug --target format-json-check
```

The C++ targets skip `external/`; the JSON targets cover `assets/` and
`tests/fixtures/`. CMake looks for both tools while configuring and reports any it
cannot find, leaving those targets unavailable. Use `-DCLANG_FORMAT_EXECUTABLE=` or
`-DPRETTIER_EXECUTABLE=` to choose a specific one.

CI runs clang-format 18 and Prettier 3.9.8, and a pull request cannot merge until both
checks pass. Local versions do not have to match. If yours formats differently, CI
fails and you reformat with the commands above.

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
app/           application shell, graphics, UI, and debug tools
  game/        game flow, level transitions, and level composition
  content/     JSON loaders, catalogues, and content validators
assets/        runtime sprite atlas, content catalogues, and editable level JSON
include/       public core headers
src/           core implementations
tests/         Catch2 tests for core systems and testable application code
docs/          reading route, architecture, content format, and future work
external/      fixed third-party source releases
.github/       continuous-integration workflow
```

## License

Simple Platformer is available under the [MIT License](LICENSE). Third-party
dependencies retain their own licenses as documented in
[THIRD_PARTY.md](THIRD_PARTY.md).
