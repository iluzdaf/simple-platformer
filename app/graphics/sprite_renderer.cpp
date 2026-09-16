#include "graphics/sprite_renderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <glad/glad.h>
#include <glm/vec2.hpp>
#include <stb_image.h>

#include "graphics/display_viewport.hpp"
#include "simple_platformer/math/coordinates.hpp"
#include "simple_platformer/render/render_scene.hpp"

namespace
{
    glm::vec2 rotatedCorner(glm::vec2 centre, glm::vec2 offset, float cosine, float sine)
    {
        return {
            centre.x + offset.x * cosine - offset.y * sine,
            centre.y + offset.x * sine + offset.y * cosine};
    }

    std::array<float, 12> spritePositions(const simple_platformer::SpriteDrawCommand& command)
    {
        const glm::vec2 halfSize = command.size * 0.5F;
        const glm::vec2 centre = command.position + halfSize;
        const float cosine = std::cos(command.rotationRadians);
        const float sine = std::sin(command.rotationRadians);

        const glm::vec2 topLeft = rotatedCorner(centre, -halfSize, cosine, sine);
        const glm::vec2 bottomLeft = rotatedCorner(centre, {-halfSize.x, halfSize.y}, cosine, sine);
        const glm::vec2 bottomRight = rotatedCorner(centre, halfSize, cosine, sine);
        const glm::vec2 topRight = rotatedCorner(centre, {halfSize.x, -halfSize.y}, cosine, sine);
        return {
            topLeft.x,
            topLeft.y,
            bottomLeft.x,
            bottomLeft.y,
            bottomRight.x,
            bottomRight.y,
            topLeft.x,
            topLeft.y,
            bottomRight.x,
            bottomRight.y,
            topRight.x,
            topRight.y};
    }

    unsigned int compileShader(unsigned int type, const char* source)
    {
        const unsigned int shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);

        int succeeded = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &succeeded);
        if (succeeded == GL_TRUE)
        {
            return shader;
        }

        std::array<char, 1024> message{};
        glGetShaderInfoLog(shader, static_cast<int>(message.size()), nullptr, message.data());
        glDeleteShader(shader);
        throw std::runtime_error(
            "OpenGL shader compilation failed: " + std::string(message.data()));
    }

    unsigned int createShaderProgram()
    {
        constexpr const char* VertexSource = R"(
            #version 330 core
            layout (location = 0) in vec2 vertexPosition;
            layout (location = 1) in vec2 vertexUv;
            uniform vec2 viewportSize;
            out vec2 textureUv;

            void main()
            {
                vec2 normalised = vertexPosition / viewportSize;
                gl_Position = vec4(normalised.x * 2.0 - 1.0, 1.0 - normalised.y * 2.0, 0.0, 1.0);
                textureUv = vertexUv;
            }
        )";
        constexpr const char* FragmentSource = R"(
            #version 330 core
            in vec2 textureUv;
            uniform sampler2D spriteTexture;
            out vec4 colour;

            void main()
            {
                colour = texture(spriteTexture, textureUv);
            }
        )";

        const unsigned int vertex = compileShader(GL_VERTEX_SHADER, VertexSource);
        const unsigned int fragment = compileShader(GL_FRAGMENT_SHADER, FragmentSource);
        const unsigned int program = glCreateProgram();
        glAttachShader(program, vertex);
        glAttachShader(program, fragment);
        glLinkProgram(program);
        glDeleteShader(vertex);
        glDeleteShader(fragment);

        int succeeded = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &succeeded);
        if (succeeded == GL_TRUE)
        {
            return program;
        }

        std::array<char, 1024> message{};
        glGetProgramInfoLog(program, static_cast<int>(message.size()), nullptr, message.data());
        glDeleteProgram(program);
        throw std::runtime_error("OpenGL shader linking failed: " + std::string(message.data()));
    }
}

namespace simple_platformer
{
    SpriteRenderer::SpriteRenderer() : shader(createShaderProgram())
    {
        glGenVertexArrays(1, &vertexArray);
        glBindVertexArray(vertexArray);

        constexpr std::size_t ValuesPerBuffer = 12;
        glGenBuffers(1, &positionBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, positionBuffer);
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(ValuesPerBuffer * sizeof(float)),
            nullptr,
            GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

        glGenBuffers(1, &textureCoordinateBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, textureCoordinateBuffer);
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(ValuesPerBuffer * sizeof(float)),
            nullptr,
            GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

        viewportLocation = glGetUniformLocation(shader, "viewportSize");
        glUseProgram(shader);
        glUniform1i(glGetUniformLocation(shader, "spriteTexture"), 0);

        glGenFramebuffers(1, &framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glGenTextures(1, &framebufferTexture);
        glBindTexture(GL_TEXTURE_2D, framebufferTexture);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA8,
            InternalWidth,
            InternalHeight,
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(
            GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, framebufferTexture, 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            throw std::runtime_error("OpenGL could not create the internal framebuffer");
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    SpriteRenderer::~SpriteRenderer()
    {
        for (const Texture& texture : textures)
        {
            glDeleteTextures(1, &texture.handle);
        }
        glDeleteTextures(1, &framebufferTexture);
        glDeleteFramebuffers(1, &framebuffer);
        glDeleteBuffers(1, &textureCoordinateBuffer);
        glDeleteBuffers(1, &positionBuffer);
        glDeleteVertexArrays(1, &vertexArray);
        glDeleteProgram(shader);
    }

    int SpriteRenderer::loadTexture(const std::string& path)
    {
        int width = 0;
        int height = 0;
        int channels = 0;
        unsigned char* pixels = stbi_load(path.c_str(), &width, &height, &channels, 4);
        if (pixels == nullptr)
        {
            throw std::runtime_error(
                "Could not load texture '" + path + "': " + stbi_failure_reason());
        }

        Texture texture;
        texture.width = width;
        texture.height = height;
        glGenTextures(1, &texture.handle);
        glBindTexture(GL_TEXTURE_2D, texture.handle);
        glTexImage2D(
            GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        stbi_image_free(pixels);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        textures.push_back(texture);
        return static_cast<int>(textures.size() - 1);
    }

    void SpriteRenderer::render(
        const RenderScene& scene,
        int framebufferWidth,
        int framebufferHeight)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glViewport(0, 0, InternalWidth, InternalHeight);
        glClearColor(0.08F, 0.12F, 0.18F, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(shader);
        glUniform2f(
            viewportLocation,
            static_cast<float>(InternalWidth),
            static_cast<float>(InternalHeight));
        glBindVertexArray(vertexArray);

        for (const SpriteDrawCommand& command : scene.sprites)
        {
            if (command.textureId < 0 ||
                static_cast<std::size_t>(command.textureId) >= textures.size())
            {
                throw std::out_of_range("A sprite refers to an unknown texture");
            }

            const Texture& texture = textures[static_cast<std::size_t>(command.textureId)];
            float leftUv = command.source.position.x / static_cast<float>(texture.width);
            float rightUv = (command.source.position.x + command.source.size.x) /
                            static_cast<float>(texture.width);
            if (command.flipHorizontal)
            {
                std::swap(leftUv, rightUv);
            }
            const float topUv = command.source.position.y / static_cast<float>(texture.height);
            const float bottomUv = (command.source.position.y + command.source.size.y) /
                                   static_cast<float>(texture.height);
            const std::array<float, 12> positions = spritePositions(command);
            const std::array<float, 12> textureCoordinates = {
                leftUv,
                topUv,
                leftUv,
                bottomUv,
                rightUv,
                bottomUv,
                leftUv,
                topUv,
                rightUv,
                bottomUv,
                rightUv,
                topUv};

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texture.handle);
            glBindBuffer(GL_ARRAY_BUFFER, positionBuffer);
            glBufferSubData(
                GL_ARRAY_BUFFER,
                0,
                static_cast<GLsizeiptr>(positions.size() * sizeof(float)),
                positions.data());
            glBindBuffer(GL_ARRAY_BUFFER, textureCoordinateBuffer);
            glBufferSubData(
                GL_ARRAY_BUFFER,
                0,
                static_cast<GLsizeiptr>(textureCoordinates.size() * sizeof(float)),
                textureCoordinates.data());
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glViewport(0, 0, framebufferWidth, framebufferHeight);
        glClearColor(0.0F, 0.0F, 0.0F, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT);
        const std::optional<DisplayViewport> output =
            makeDisplayViewport({framebufferWidth, framebufferHeight});
        if (!output.has_value())
        {
            return;
        }

        glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
        glBlitFramebuffer(
            0,
            0,
            InternalWidth,
            InternalHeight,
            output->topLeftMargin.x,
            output->bottomMargin,
            output->topLeftMargin.x + output->size.x,
            output->bottomMargin + output->size.y,
            GL_COLOR_BUFFER_BIT,
            GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
}
