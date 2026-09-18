#pragma once

#include <string>
#include <vector>

namespace simple_platformer
{
    struct RenderScene;

    struct TextureView
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
        TextureView textureView(int textureId) const;
        void render(const RenderScene& scene, int framebufferWidth, int framebufferHeight);

    private:
        struct Texture
        {
            unsigned int handle = 0;
            int width = 0;
            int height = 0;
        };

        unsigned int shader = 0;
        unsigned int vertexArray = 0;
        unsigned int positionBuffer = 0;
        unsigned int textureCoordinateBuffer = 0;
        unsigned int framebuffer = 0;
        unsigned int framebufferTexture = 0;
        int viewportLocation = -1;
        int opacityLocation = -1;
        std::vector<Texture> textures;
    };
}
