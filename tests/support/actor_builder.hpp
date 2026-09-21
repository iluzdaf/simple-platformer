#pragma once

#include <utility>

#include <glm/vec2.hpp>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/combat/combat.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/movement/flying_movement.hpp"
#include "simple_platformer/movement/platformer_movement.hpp"
#include "simple_platformer/navigation/path_follower.hpp"
#include "simple_platformer/npc/npc.hpp"
#include "simple_platformer/render/sprite.hpp"

namespace tests
{
    // Builds an actor that World accepts, from the components and settings a test asks for.
    // The builder knows how components fit together, such as movement being required and
    // an NPC needing a brain, senses, and path follower at once. It holds no gameplay values:
    // a test supplies every size, speed, and range it depends on, so a passing test says
    // which ones mattered. A real player or NPC comes from the game's composition recipe.
    //
    // Start from walking or flying, since World requires exactly one movement component,
    // then chain what the test needs. The chain works on a fresh builder and converts to an
    // Actor wherever one is expected, such as World::addActor.
    class ActorBuilder
    {
    public:
        static ActorBuilder walking(const simple_platformer::Aabb& bounds)
        {
            simple_platformer::Actor actor;
            actor.body.bounds = bounds;
            actor.platformerMovement = simple_platformer::PlatformerMovement{};
            return ActorBuilder(std::move(actor));
        }

        static ActorBuilder flying(const simple_platformer::Aabb& bounds, float speed)
        {
            simple_platformer::Actor actor;
            actor.body.bounds = bounds;
            actor.flyingMovement = simple_platformer::FlyingMovement{speed};
            return ActorBuilder(std::move(actor));
        }

        ActorBuilder onTeam(simple_platformer::Team team) &&
        {
            built.team = team;
            return std::move(*this);
        }

        ActorBuilder thinking(simple_platformer::NpcSenses senses) &&
        {
            built.brain = simple_platformer::NpcBrain{};
            built.senses = senses;
            built.pathFollower = simple_platformer::PathFollower{};
            return std::move(*this);
        }

        ActorBuilder patrolling(glm::vec2 firstFeet, glm::vec2 secondFeet) &&
        {
            built.patrol = simple_platformer::Patrol{firstFeet, secondFeet, true};
            return std::move(*this);
        }

        ActorBuilder withSprite(simple_platformer::Sprite sprite) &&
        {
            built.sprite = sprite;
            return std::move(*this);
        }

        ActorBuilder thatBites() &&
        {
            built.bite = simple_platformer::BiteAttack{};
            return std::move(*this);
        }

        ActorBuilder thatShoots() &&
        {
            built.rangedWeapon = simple_platformer::RangedWeapon{};
            return std::move(*this);
        }

        operator simple_platformer::Actor() &&
        {
            return std::move(built);
        }

    private:
        explicit ActorBuilder(simple_platformer::Actor actor)
            : built(std::move(actor))
        {
        }

        simple_platformer::Actor built;
    };
}
