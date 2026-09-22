#include "simple_platformer/world/level_exit.hpp"

#include <cmath>
#include <optional>
#include <stdexcept>

#include "simple_platformer/actor/actor.hpp"
#include "simple_platformer/inventory/inventory.hpp"
#include "simple_platformer/math/aabb.hpp"
#include "simple_platformer/math/validation.hpp"
#include "simple_platformer/world/world.hpp"

namespace simple_platformer
{
    void validateLevelExit(const LevelExit& exit)
    {
        if (!simple_platformer::isFinite(exit.bounds.position) ||
            !simple_platformer::isFinite(exit.bounds.size) || exit.bounds.size.x <= 0.0F ||
            exit.bounds.size.y <= 0.0F)
        {
            throw std::invalid_argument("Level exits require finite positive-sized bounds");
        }
        if (exit.requirement.has_value())
        {
            if (exit.requirement->quantity <= 0)
            {
                throw std::invalid_argument("Exit requirements must be positive");
            }
        }
        if (exit.nextLevel.has_value() && *exit.nextLevel < 0)
        {
            throw std::invalid_argument("Level IDs must be non-negative");
        }
    }

    void World::setExit(LevelExit exit)
    {
        validateLevelExit(exit);
        if (exit.requirement)
        {
            itemDefinition(exit.requirement->item);
        }
        if (exit.lastLockedTouchTimeSeconds.has_value() &&
            (!std::isfinite(*exit.lastLockedTouchTimeSeconds) ||
             *exit.lastLockedTouchTimeSeconds < 0.0F ||
             *exit.lastLockedTouchTimeSeconds > elapsedSimulationTimeSeconds))
        {
            throw std::invalid_argument("Exit touch time must be within simulation time");
        }
        levelExit = exit;
        completed = false;
    }

    const std::optional<LevelExit>& World::exit() const
    {
        return levelExit;
    }

    std::optional<LevelExit>& World::exit()
    {
        return levelExit;
    }

    bool World::levelComplete() const
    {
        return completed;
    }

    void World::completeLevel()
    {
        completed = true;
    }

    bool exitUnlocked(const LevelExit& exit, const Actor& actor)
    {
        return !exit.requirement.has_value() ||
               (actor.inventory.has_value() &&
                actor.inventory->count(exit.requirement->item) >= exit.requirement->quantity);
    }

    void updateLevelExit(World& world)
    {
        Actor* player = world.findActor(world.playerId());
        std::optional<LevelExit>& levelExit = world.exit();
        if (world.levelComplete() || !levelExit.has_value() || player == nullptr ||
            player->life != LifeState::Alive)
        {
            return;
        }
        LevelExit& exit = levelExit.value();
        if (!overlaps(player->body.bounds, exit.bounds))
        {
            return;
        }
        if (!exitUnlocked(exit, *player))
        {
            exit.lastLockedTouchTimeSeconds = world.simulationTimeSeconds();
            return;
        }
        if (exit.consumeItem && exit.requirement.has_value() && player->inventory.has_value())
        {
            player->inventory->remove(exit.requirement->item, exit.requirement->quantity);
        }
        world.completeLevel();
    }
}
