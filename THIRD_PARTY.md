# Third-party source

The project vendors dependency source so a classroom checkout builds without a
package manager or a network connection.

| Directory | Version | Upstream revision/source |
| --- | --- | --- |
| `external/catch2` | Catch2 3.8.1 | tag `v3.8.1`, commit `2b60af89e23d28eefc081bc930831ee9d45ea58b` |
| `external/glfw` | GLFW 3.4 | tag `3.4`, commit `7b6aead9fb88b3623e3b3725ebb42670cbe4c579` |
| `external/glm` | GLM 1.0.1 | tag `1.0.1`, commit `0af55ccecd98d4e5a8d1fad7de25ba429d60e863` |
| `external/imgui` | Dear ImGui 1.91.8 | tag `v1.91.8`, commit `dbb5eeaadffb6a3ba6a60de1290312e5802dba5a` |
| `external/nlohmann` | JSON for Modern C++ 3.12.0 | tag `v3.12.0`, release `json.hpp` SHA-256 `aaf127c04cb31c406e5b04a63f1ae89369fccde6d8fa7cdda1ed4f32dfc5de63` |
| `external/glad` | GLAD 0.1.36 generated loader | OpenGL 4.6 core loader generated 2025-05-20 |
| `external/stb` | stb_image 2.30 | `stb_image.h` plus its implementation translation unit |

Each dependency retains its upstream licence or licensing notice in its source tree.
The example application uses GLFW and GLAD for its window and OpenGL rendering,
stb_image for texture loading, ImGui for UI, and JSON for Modern C++ for level loading.
