#pragma once

struct GLFWwindow;

namespace simple_platformer
{
    // ImGui and ImPlot with their GLFW and OpenGL 3 backends, for as long as it lives.
    class ImGuiSession
    {
    public:
        explicit ImGuiSession(GLFWwindow* window);
        ~ImGuiSession();

        ImGuiSession(const ImGuiSession&) = delete;
        ImGuiSession& operator=(const ImGuiSession&) = delete;

        // Starts a frame; everything drawn until render is part of it.
        void beginFrame() const;
        // Submits the frame over whatever the scene has already drawn.
        void render() const;
    };
}
