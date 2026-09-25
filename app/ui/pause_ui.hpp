#pragma once

namespace simple_platformer
{
    struct WindowViewport;

    // The notice at the top of the view while the simulation is paused, naming the
    // keys that resume it and step it.
    void drawPauseNotice(const WindowViewport& viewport);
}
