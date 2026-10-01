# C++ style

How the code is written, so it is easier to read and so new code fits in. The tools in
[README.md](../README.md#formatting) check the formatting and naming; this page covers
the rest.

## Language and libraries

The project uses C++17. Before reading the engine, it helps to be comfortable with:

- [`std::optional`](https://en.cppreference.com/w/cpp/utility/optional), for a value
  that may be absent, such as an actor's optional components.
- [`std::variant`](https://en.cppreference.com/w/cpp/utility/variant), for a value that
  is one of a few types, such as an NPC activity that is either built in or scripted.
- [`std::function`](https://en.cppreference.com/w/cpp/utility/functional/function) and
  [lambdas](https://en.cppreference.com/w/cpp/language/lambda), for callbacks such as
  the connections a search asks for.
- [Structured bindings](https://en.cppreference.com/w/cpp/language/structured_binding),
  such as `for (const auto& [name, value] : map)`.

Positions, sizes and velocities are [GLM](https://github.com/g-truc/glm) `glm::vec2`.
Tests use [Catch2](https://github.com/catchorg/Catch2). Lua scripting goes through
[sol2](https://github.com/ThePhD/sol2), only in `scripting/`.

## Names

| Kind                                     | Style        | Example                                        |
| ---------------------------------------- | ------------ | ---------------------------------------------- |
| Types, enums and enum values             | `CamelCase`  | `PlatformerMovement`, `ClimbSurface::LeftWall` |
| Functions, variables, parameters, fields | `camelBack`  | `updateNpcSenses`, `goalFeet`                  |
| Constants                                | `CamelCase`  | `ExitOpenSeconds`, `JumpStartPenaltyTicks`     |
| Namespaces                               | `lower_case` | `simple_platformer`, `tests`                   |
| Files                                    | `snake_case` | `platformer_movement.cpp`                      |

Names say what a thing is in the words of the [glossary](GLOSSARY.md). A number that
has a unit says it, in the name (`openedTimeSeconds`, `JumpStartPenaltyTicks`) or in the
comment above it ("In pixels: …"). Time is in seconds and distance in pixels unless a
name says otherwise.

## Data and functions

Most code is plain data and free functions, not classes with behaviour:

- **Components are plain structs** with public fields, such as `Health`, `Body` and
  `PlatformerMovement`. An actor holds them, most as `std::optional`.
- **Systems are free functions** that take what they work on as parameters, such as
  `updatePlatformerMovement(map, body, movement, intentions, deltaTime)`. They have no
  hidden state, so a test calls one with exactly the data it wants.
- **Classes are kept for things with rules to protect**, such as `World`, `TileMap` and
  `PlatformerConnectionCache`. Their data is private, and their functions keep it valid.
- **Inheritance is rare.** An actor's role comes from the components it has, not from a
  subclass. An interface with virtual functions appears only at a real boundary, such as
  `NpcActivityScripts`, which tests replace with a fake.

## Optional components

An actor's components are mostly `std::optional`, so code checks before it uses one:

```cpp
if (!actor.platformerMovement.has_value())
{
    return;
}
```

A system skips actors without the components it needs. Static analysis flags any use of
an optional value that was not checked first.

## Checking inputs

Many functions start by checking their inputs, and throw if they are wrong:

```cpp
requireSeconds(deltaTime, "Platformer movement time step");
validatePlatformerMovementConfig(config);
```

When reading, skip past these to find the rule the function is about. They are there
so a mistake fails loudly at its source, with a message that says what was wrong.

- `std::invalid_argument` means the caller passed something wrong: a negative time
  step, a missing component, bad content.
- `std::logic_error` means the code broke one of its own rules, which is a bug.
- Shared checks live in [`validation.hpp`](../include/simple_platformer/math/validation.hpp),
  such as `requireSeconds` and `requireFinite`.

[Error handling and validation](ARCHITECTURE.md#error-handling-and-validation) explains
where each kind of check belongs.

## Files and headers

- **Engine code** is in `include/simple_platformer/<area>/` for public headers and
  `src/<area>/` for their sources. **The game** is in `app/`, and **Lua scripting** in
  `scripting/`.
- **A header declares only what other files use.** Helpers used by one source file go in
  an anonymous namespace in that file.
- **Every header compiles on its own,** and every file includes the headers for what it
  uses, rather than relying on another header to bring them in. CI checks both.
- **Forward declarations** (`class World;`) replace includes where a header only names a
  type.

## Comments

Comments explain what is not obvious from the code: what a function promises, why a
rule exists, what a number means. They use plain words and short sentences, and the
words of the [glossary](GLOSSARY.md).

- A comment above a function says what it gives back and when it gives nothing, not
  how it works inside.
- A long function is split into numbered steps, one comment per step, such as the main
  loop in [`route_search.cpp`](../src/navigation/route_search.cpp).
- Code that is clear on its own has no comment.

## Tests

- **A test file for each source file,** under `tests/` in the same folder structure, such
  as `tests/movement/test_platformer_movement.cpp`. A large subject may have more than
  one.
- **Test names are sentences** that say what should happen, such as "A search waits for
  a cell the cache does not hold yet, even when a costlier path exists".
- **Builders in [`tests/support/`](../tests/support)** make test data readable:
  `ActorBuilder`, `TileMapBuilder` and `NpcMachineBuilder`. Each explains its own rules
  at the top of its header.
- **Helpers stay in the test file that uses them.** Only helpers several files share go
  in `tests/support/`.
