#pragma once

#include <string>
#include <vector>

namespace simple_platformer
{
    struct RenderScene;

    // A view of a loaded texture. SpriteRenderer owns the GPU resource; copying this
    // value does not transfer ownership or extend its lifetime.
    struct Texture
    {
        unsigned int handle = 0;
        int width = 0;
        int height = 0;
    };

    class SpriteRenderer
    {
    public:
        SpriteRenderer();
        ~SpriteRenderer();

        SpriteRenderer(const SpriteRenderer&) = delete;
        SpriteRenderer& operator=(const SpriteRenderer&) = delete;

        int loadTexture(const std::string& path);
        Texture texture(int textureId) const;
        void render(const RenderScene& scene, int framebufferWidth, int framebufferHeight);

    private:
        unsigned int shader = 0;
        unsigned int vertexArray = 0;
        unsigned int positionBuffer = 0;
        unsigned int textureCoordinateBuffer = 0;
        unsigned int framebuffer = 0;
        unsigned int framebufferTexture = 0;
        int viewportLocation = -1;
        int opacityLocation = -1;
        int whiteFlashLocation = -1;
        int shadeLocation = -1;
        std::vector<Texture> textures;
    };
}
