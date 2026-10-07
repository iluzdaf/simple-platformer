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

New to the project? Start with [START_HERE.md](docs/START_HERE.md).

## Requirements

- CMake 3.21 or newer
- A C++17 compiler:
  - Apple Clang supplied with current Xcode on macOS
  - Visual Studio 2022 with the **Desktop development with C++** workload on Windows
- **C++ CMake tools for Windows** when installing Visual Studio

All third-party source required by the project is vendored under `external/`.

The supported development platforms are macOS and Windows, where development and
graphical testing take place.

`CMakePresets.json` defines the shared Windows and macOS build configurations.
Use the ignored `CMakeUserPresets.json` for personal overrides.

## Windows: create and use the Visual Studio solution

Install Visual Studio 2022 with **Desktop development with C++** and **C++ CMake
tools for Windows** selected in the Visual Studio Installer. Then double-click:

```text
setup-windows.bat
```

The script finds CMake, generates `build/windows-vs/SimplePlatformer.sln`, and opens
the solution. The `simple_platformer` project is already selected as the startup
project. Choose **Build > Build Solution** (`Ctrl+Shift+B`), then press **F5** to run
the game.

To run the tests, right-click `run_tests` in **Solution Explorer** and choose **Build**;
the results appear in the **Output** window. To debug them, right-click
`simple_platformer_tests` and choose **Set as Startup Project**, then press **F5**.
Afterward, set `simple_platformer` as the startup project to run the game.

The solution is generated from `CMakeLists.txt` and `CMakePresets.json`. It belongs in
the ignored `build/` directory and should not be committed. Run `setup-windows.bat`
again after changing the CMake configuration.

### Adding your own C++ files

| What you are adding                                       | Place the `.cpp` file in                            | Place its `.hpp` file in                           |
| --------------------------------------------------------- | --------------------------------------------------- | -------------------------------------------------- |
| Simulation or gameplay rules that run without a window    | `src/`, beside related code                         | `include/simple_platformer/`, in the matching area |
| Game setup, content loading, debug tools, graphics, or UI | `app/`, beside related code                         | `app/`, beside related code                        |
| A test or test helper                                     | `tests/`, following the folder of the code it tests | `tests/`, beside the test or helper                |

CMake finds new `.cpp` and `.hpp` files in these folders automatically. You do not
need to edit a source list. If you use Visual Studio's **Add > New Item**, check
that the file is saved in the repository folder shown above, not under `build/`.
After creating a file on Windows, run
`setup-windows.bat` again to refresh the generated Visual Studio solution, then build.
Changes made only to generated project files under `build/` will be lost when the
solution is regenerated.

Tests can use core code and new files in `app/content/`, `app/game/`, or directly
in `app/debug/` automatically. Put ImGui debug drawing in `app/debug/ui/`.
If a test needs a new implementation in another `app/` folder, add it to the
headless-test selection in
[`cmake/sources/Tests.cmake`](cmake/sources/Tests.cmake).

The Windows executable is:

```text
build\windows-vs\Debug\simple_platformer.exe
```

To check performance, select **Release** in Visual Studio's **Solution Configurations**
drop-down, choose **Build > Build Solution**, then press **F5**. The game is then at
`build\windows-vs\Release\simple_platformer.exe`. Switch back to **Debug** for
everyday development.

## macOS: configure, build, and test

The shared macOS preset uses the build tools supplied with Xcode. Run the commands
below from the repository root.

```sh
cmake --preset mac-debug
cmake --build --preset mac-debug
ctest --preset mac-debug
```

Run the example game:

```sh
build/mac-debug/simple_platformer
```

To check performance,
build and run the release preset. The debug build has no optimisation, so it may not represent the performance
players will experience.

```sh
cmake --preset mac-release
cmake --build --preset mac-release
build/mac-release/simple_platformer
```

## Running focused tests

On Windows, set `simple_platformer_tests` as the startup project in Visual Studio.
Open its **Properties > Configuration Properties > Debugging** and set **Command
Arguments** to `"*Pickup*"` to select tests whose names contain `Pickup`, or
`[lifecycle]` to select a test tag. Press **F5** to debug or **Ctrl+F5** to run without
debugging. Clear the arguments to run the full suite again. See Visual Studio's
[debugging properties documentation](https://learn.microsoft.com/en-us/visualstudio/debugger/project-settings-for-a-cpp-debug-configuration?view=vs-2022).

On macOS, build before running CTest so the test executable includes your changes.
List test names or run only tests whose names contain `Pickup` with:

```sh
ctest --preset mac-debug -N
# Omit -R "Pickup" to run the complete suite.
ctest --preset mac-debug -R "Pickup" --output-on-failure
```

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
| Toggle the debug overlay                      | F1                                        |
| Close the window                              | Escape                                    |

The hearts at the top-left show the player's current and maximum health.

Find each level's key and reach its bunker door to unlock the exit. Each door consumes
one key; the third exit completes the example campaign.

## Debug overlay

F1 shows or hides all debug overlays together: actor bounds and text, paths, navigation
connections and profiles, projectiles, pickups, and camera regions.

| Action                                 | Controls |
| -------------------------------------- | -------- |
| Show or hide all debug overlays        | F1       |
| Show the next navigation profile       | N        |
| Break a labelled tile under the cursor | B        |

## Continuous integration

GitHub Actions runs the jobs below. The names are the ones shown on a pull request.
The [CI workflow](.github/workflows/ci.yml) defines the pinned tool versions and
compiler-cache setup; [Quality.cmake](cmake/Quality.cmake) defines the quality targets.

| Job                          | Runner         | What it does                                                                                                                                                        | Runs on                            |
| ---------------------------- | -------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------- |
| macOS / Apple Clang          | `macos-latest` | Configures, builds, and runs the whole test suite.                                                                                                                  | pushes to `main` and pull requests |
| Windows / Visual Studio 2022 | `windows-2022` | Generates the same solution as `setup-windows.bat`, builds it with MSBuild, and runs the tests through `run_tests`, then again from where the debugger starts them. | pushes to `main` and pull requests |
| Formatting                   | `ubuntu-24.04` | Runs the [formatting checks](#formatting), Python lint, and tests for the repository's tools.                                                                       | pull requests only                 |
| Headers stand alone          | `ubuntu-24.04` | Compiles every public header on its own using `header_self_containment`.                                                                                            | pull requests only                 |
| Static analysis (1/3 to 3/3) | `ubuntu-24.04` | Runs clang-tidy with warnings as errors on affected files (see [Static analysis](#static-analysis)), split across three shards.                                     | pull requests only                 |
| Static analysis              | `ubuntu-24.04` | Passes only if every static analysis shard passed. This is the check branch protection requires.                                                                    | pull requests only                 |

Linux is used solely for CI quality checks and is not a supported local development
workflow. Branch protection requires the quality checks before merging, so they run
only on pull requests. Compiler caching applies only to CI builds.

## Formatting

`.clang-format` defines the C and C++ style; `.prettierrc` covers JSON, YAML, and
Markdown. Ruff formats and checks first-party Python.
`.editorconfig` supplies shared whitespace rules.

|           | Config          | Tool         | VS Code                                 | Visual Studio                            |
| --------- | --------------- | ------------ | --------------------------------------- | ---------------------------------------- |
| C and C++ | `.clang-format` | clang-format | on save, through clangd                 | **Format Document** (`Ctrl+K`, `Ctrl+D`) |
| JSON      | `.prettierrc`   | Prettier     | on save, through the Prettier extension | not supported, use the command line      |
| YAML      | `.prettierrc`   | Prettier     | on save, through the Prettier extension | not supported, use the command line      |
| Markdown  | `.prettierrc`   | Prettier     | on save, through the Prettier extension | not supported, use the command line      |
| Python    | Ruff defaults   | Ruff         | on save, through the Ruff extension     | not supported, use the command line      |

Both editors read `.clang-format` and `.editorconfig` without an extension. Visual
Studio does not read the other formatter configs, so use the command below.

Install clang-format, Prettier, and Ruff on PATH, then format all first-party files
or check them without edits (including Python lint):

```sh
python3 tools/format.py
python3 tools/format.py --check
```

Use `--only cpp`, `json`, `yaml`, `markdown`, or `python` to select file kinds; see
`python3 tools/format.py --help` for executable overrides. On Windows, use `py -3`
in place of `python3`. No CMake configuration is needed.

[format.py](tools/format.py) defines the first-party file scope and excludes vendored
sources and build files. The existing CMake formatting targets also call this tool.
To match CI, use the versions pinned in the [workflow](.github/workflows/ci.yml).

Run the tests for the repository tools with:

```sh
python3 -m unittest discover -s tools -p 'test_*.py'
```

## Static analysis

The checked-in `.clang-tidy` checks naming, unused and missing includes, common bugs,
and performance mistakes. VS Code's recommended clangd extension reports unused and
missing includes while editing. Treat include-cleaner suggestions as findings to
review; do not automatically remove headers without rebuilding and running the tests.

Static analysis is optional for local builds. Developers with clang-tidy installed
can run it with:

```sh
cmake --build --preset mac-debug --target tidy
```

Local `tidy` builds check the whole tree. CI selects changed C++ files and the files
that include changed headers, with full-tree fallbacks defined in
[tidy_targets.py](tools/tidy_targets.py). Preview the selection for your branch with:

```sh
python3 tools/tidy_targets.py --since origin/main
```

To use CI's clang-tidy version locally, set `CLANG_TIDY_EXECUTABLE` in a personal
`CMakeUserPresets.json` preset, then configure and build with that preset. This selects
the analysis tool; compiler and SDK differences can still affect diagnostics.

CMake discovers `.cpp` sources in `src/`, `app/`, and `tests/` during configuration
and checks for new files when building. See [Adding your own C++ files](#adding-your-own-c-files).

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
cmake/         dependencies, quality rules, and source discovery
include/       public core headers
src/           core implementations
tests/         Catch2 tests for core systems and testable application code
  app/         application tests grouped like app/ (content, debug, game, graphics, UI)
  fixtures/    example content mirroring assets/levels, and catalogs
  support/     test-only builders and simulation helpers
tools/         repository quality and maintenance scripts
docs/          reading route, architecture, content format, glossary, and C++ style
external/      fixed third-party source releases
.github/       continuous-integration workflow
```

## License

Simple Platformer is available under the [MIT License](LICENSE). Third-party
dependencies retain their own licenses as documented in
[THIRD_PARTY.md](THIRD_PARTY.md).
