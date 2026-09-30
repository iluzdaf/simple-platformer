#pragma once

#include "simple_platformer/math/aabb.hpp"

namespace simple_platformer
{
    struct Actor;

    // Where an actor's sprite is drawn. The art is drawn standing on a floor and facing
    // right; a climber's is turned so its feet rest on the surface it holds.
    struct ActorSpritePlacement
    {
        // The rectangle before turning. The renderer turns it about its centre.
        Aabb drawn;
        // Clockwise.
        float rotationRadians = 0.0F;
        // Mirrors the art before it is turned, so the head leads the way the actor goes.
        bool flipHorizontal = false;
        // What the turned sprite covers; a quarter turn swaps its width and height.
        Aabb visible;
    };

    // On the floor the feet anchor stands the sprite on the body's feet. On a wall or
    // ceiling the sprite's feet edge lies along the body's edge against that surface,
    // centred on the body; a centre-anchored sprite turns about the body's centre. On a
    // ceiling the head points the way the actor faces; on a wall it points the climb's
    // wall heading. The actor must have a sprite.
    ActorSpritePlacement placeActorSprite(const Actor& actor);
}
