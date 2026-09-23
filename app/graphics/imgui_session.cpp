#include "imgui_session.hpp"

#include <stdexcept>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <implot.h>

namespace simple_platformer
{
    namespace
    {
        void destroyContexts()
        {
            ImPlot::DestroyContext();
            ImGui::DestroyContext();
        }
    }

    ImGuiSession::ImGuiSession(GLFWwindow* window)
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImPlot::CreateContext();
        ImGui::StyleColorsDark();

        if (!ImGui_ImplGlfw_InitForOpenGL(window, true))
        {
            destroyContexts();
            throw std::runtime_error("ImGui could not start its GLFW backend");
        }
        if (!ImGui_ImplOpenGL3_Init("#version 330 core"))
        {
            ImGui_ImplGlfw_Shutdown();
            destroyContexts();
            throw std::runtime_error("ImGui could not start its OpenGL backend");
        }
    }

    ImGuiSession::~ImGuiSession()
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        destroyContexts();
    }

    void ImGuiSession::beginFrame() const
    {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void ImGuiSession::render() const
    {
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
}
