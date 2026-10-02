# Simple Platformer

Simple Platformer is a C++17 teaching engine and example game built from independently
testable systems. The current implementation includes platformer movement, tile collision,
scrolling, composed actors, NPC states with enum tactics, flying and platformer
pathfinding, projectiles, animation, inventory, automatic pickups, a three-level game
loop, and ImGui debugging tools.

## Documentation

| Document                                | What it covers                                                                    |
| --------------------------------------- | --------------------------------------------------------------------------------- |
| [START_HERE.md](docs/START_HERE.md)     | A recommended route through the code, and which details can wait until later.     |
| [ARCHITECTURE.md](docs/ARCHITECTURE.md) | Design, ownership rules, runtime flow, and the reasons behind the main decisions. |
| [CONTENT.md](docs/CONTENT.md)           | How to author levels and definitions under `assets`.                              |
| [GLOSSARY.md](docs/GLOSSARY.md)         | The words the code and documents use, each with one meaning.                      |
| [CPP_STYLE.md](docs/CPP_STYLE.md)       | How the C++ is written, and the language features to know before reading it.      |
| [FUTURE_WORK.md](docs/FUTURE_WORK.md)   | Proposed features that are not implemented yet.                                   |

New to the project? Start with [START_HERE.md](docs/START_HERE.md).

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

Press F1 in the game to open the [debug overlay](#debug-overlay). To check performance,
build and run the release preset. The debug build has no optimisation, so it may not represent the performance
players will experience.

```sh
cmake --preset mac-release
cmake --build --preset mac-release
build/mac-release/simple_platformer
```

On Windows, choose the **Release** configuration in Visual Studio, or run
`cmake --build --preset windows-release` after `setup-windows.bat` has generated the
solution. The game is then at `build\windows-vs\Release\simple_platformer.exe`.

## Windows: create and use the Visual Studio solution

Install Visual Studio 2022 with **Desktop development with C++** and **C++ CMake
tools for Windows** selected in the Visual Studio Installer. Then double-click:

```text
setup-windows.bat
```

The script finds CMake, generates `build/windows-vs/SimplePlatformer.sln`, and opens
the solution. The `simple_platformer` project is already selected as the startup
project, so build the solution and press **F5** to run the game.

To run the tests, build the `run_tests` project; the results appear in the Output
window. To debug them, set `simple_platformer_tests` as the startup project and press
**F5**, then set `simple_platformer` back to run the game.

The solution is generated from `CMakeLists.txt` and `CMakePresets.json`. It belongs in
the ignored `build/` directory and should not be committed. Run `setup-windows.bat`
again after changing the CMake configuration.

The Windows executable is:

```text
build\windows-vs\Debug\simple_platformer.exe
```

`CMakePresets.json` contains the shared macOS and Windows configurations.
`CMakeUserPresets.json` is ignored and is available for personal configuration that
should not be shared with version control.

## Running focused tests

Build before running CTest so the test executable includes your changes. On macOS,
list test names or run only tests whose names contain `Pickup` with:

```sh
ctest --preset mac-debug -N
# Omit -R "Pickup" to run the complete suite.
ctest --preset mac-debug -R "Pickup" --output-on-failure
```

On Windows, use the `windows-debug` test preset. Use your personal preset name if
configured.

## Playing the example game

| Action                                        | Controls                                  |
| --------------------------------------------- | ----------------------------------------- |
| Move                                          | A and D, or the left and right arrow keys |
| Jump                                          | W, Up, or Space                           |
| Aim                                           | Mouse                                     |
| Fire                                          | Left mouse button                         |
| Collect an item                               | Walk over it                              |
| Open or close the inventory (pauses the game) | Q, or click the bag at the bottom-left    |
| Drink a health potion                         | Click it in the open inventory            |
| Restart from the starting level               | R, at the completion message              |
| Pause or resume the simulation                | P                                         |
| Run one simulation step while paused          | . (full stop)                             |
| Toggle the debug overlay                      | F1                                        |
| Close the window                              | Escape                                    |

The hearts at the top-left show the player's current and maximum health.

Find each level's key and reach its bunker door to unlock the exit. Each door consumes
one key; the third exit completes the example campaign.

## Debug overlay

F1 opens the debug tools with the world-space and camera overlay visible. It shows
actor bounds, paths, navigation connections, projectiles, pickups, and camera regions.

| Action                                          | Controls |
| ----------------------------------------------- | -------- |
| Show or hide the world-space and camera overlay | 2        |
| Show or hide actor text                         | 3        |
| Show or hide navigation connection totals       | 4        |
| Show the next navigation profile                | N        |
| Break a labelled tile under the cursor          | B        |

Use P to pause or resume the simulation and . to run one step while paused.

## Continuous integration

GitHub Actions runs the jobs below. The names are the ones shown on a pull request.

| Job                          | Runner         | What it does                                                                                                                                                        | Runs on                            |
| ---------------------------- | -------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------- |
| macOS / Apple Clang          | `macos-latest` | Configures, builds, and runs the whole test suite.                                                                                                                  | pushes to `main` and pull requests |
| Windows / Visual Studio 2022 | `windows-2022` | Generates the same solution as `setup-windows.bat`, builds it with MSBuild, and runs the tests through `run_tests`, then again from where the debugger starts them. | pushes to `main` and pull requests |
| Formatting                   | `ubuntu-24.04` | Checks the formatting of C++, JSON, YAML, Markdown, and Python, lints the Python, and runs the tests for the repository's tools.                                    | pull requests only                 |
| Headers stand alone          | `ubuntu-24.04` | Compiles every public header on its own.                                                                                                                            | pull requests only                 |
| Static analysis (1/3 to 3/3) | `ubuntu-24.04` | Runs clang-tidy, with warnings as errors, on the files the pull request affects (see [Static analysis](#static-analysis)), split across three shards.               | pull requests only                 |
| Static analysis              | `ubuntu-24.04` | Passes only if every static analysis shard passed. This is the check branch protection requires.                                                                    | pull requests only                 |

The Linux jobs are skipped on pushes because branch protection already ran them on the
pull request.

The macOS and Windows jobs use a pinned `sccache` release backed by GitHub Actions'
cache service. Only compiler outputs are cached; generated build directories are not.
On Windows, only `cl.exe` is replaced with a cache wrapper; the solution itself is still
built as generated. None of this affects local builds.

## Formatting

`.clang-format` defines the C and C++ style; `.prettierrc` covers JSON, YAML, and
Markdown. Ruff formats and checks first-party Python.
`.editorconfig` supplies shared whitespace rules.

|           | Config          | Tool            | VS Code                                 | Visual Studio                            |
| --------- | --------------- | --------------- | --------------------------------------- | ---------------------------------------- |
| C and C++ | `.clang-format` | clang-format 18 | on save, through clangd                 | **Format Document** (`Ctrl+K`, `Ctrl+D`) |
| JSON      | `.prettierrc`   | Prettier 3.9.8  | on save, through the Prettier extension | not supported, use the command line      |
| YAML      | `.prettierrc`   | Prettier 3.9.8  | on save, through the Prettier extension | not supported, use the command line      |
| Markdown  | `.prettierrc`   | Prettier 3.9.8  | on save, through the Prettier extension | not supported, use the command line      |
| Python    | Ruff defaults   | Ruff 0.16.8     | on save, through the Ruff extension     | not supported, use the command line      |

Both editors read `.clang-format` and `.editorconfig` without an extension. Visual
Studio does not read the other formatter configs, so those files are formatted from
the command line or caught by CI.

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

Format first-party YAML, or check it without changing files:

```sh
cmake --build --preset mac-debug --target format-yaml
cmake --build --preset mac-debug --target format-yaml-check
```

Format first-party Markdown, or check it without changing files:

```sh
cmake --build --preset mac-debug --target format-markdown
cmake --build --preset mac-debug --target format-markdown-check
```

Format first-party Python, or check its formatting and lint findings:

```sh
cmake --build --preset mac-debug --target format-python
cmake --build --preset mac-debug --target format-python-check lint-python
```

Run the tests for the scripts in `tools/`, as CI does:

```sh
python3 -m unittest discover -s tools -p 'test_*.py'
```

The C++ targets skip `external/`; the JSON targets cover `assets/` and
`tests/fixtures/`; the YAML targets cover `.github/`; the Markdown targets cover the
root documentation and `docs/`; the Python targets cover `tools/`. CMake reports any unavailable tool while
configuring and omits only its targets. Use `-DCLANG_FORMAT_EXECUTABLE=`,
`-DPRETTIER_EXECUTABLE=`, or `-DRUFF_EXECUTABLE=` to choose a specific one.

CI runs clang-format 18, Prettier 3.9.8, and Ruff 0.16.8,
and a pull request cannot merge until their checks pass. Local versions do not have to
match. If yours formats differently, CI fails and you reformat with the commands above.

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

Pull-request CI checks each changed C++ file, and every first-party file that includes
a changed header, directly or through other headers. Adding a new `.cpp` file and
listing it in its manifest under `cmake/sources/` checks only the new code, but
changing only a manifest checks the whole tree. So do changes to the analysis rules,
the CI workflow, the global build configuration, or `tools/tidy_targets.py`, which
picks the files. Local `tidy` builds always check the whole tree. CMake configuration
fails with a focused error if an `app/`, `src/`, or enabled `tests/` source is missing
from its target's manifest.

To see which files CI will check for your branch, run the same script:

```sh
python3 tools/tidy_targets.py --since origin/main
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
  content/     JSON loaders, catalogs, and content validators
assets/        runtime game content
  catalogs/    shared JSON definitions
  levels/      level catalog and maps
  textures/    runtime sprite atlas
cmake/         dependencies, quality rules, and explicit target source manifests
include/       public core headers
src/           core implementations
tests/         Catch2 tests for core systems and testable application code
  app/         application tests grouped like app/ (content, debug, game, graphics, UI)
  fixtures/    example content mirroring assets/levels, and catalogs
  support/     test-only builders and simulation helpers
tools/         repository quality and maintenance scripts
docs/          reading route, architecture, content format, and future work
external/      fixed third-party source releases
.github/       continuous-integration workflow
```

## License

Simple Platformer is available under the [MIT License](LICENSE). Third-party
dependencies retain their own licenses as documented in
[THIRD_PARTY.md](THIRD_PARTY.md).
