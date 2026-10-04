#include <type_traits>

#include "simple_platformer/actor/actor.hpp"
#include "support/actor_builder.hpp"

// Only a completed builder can become an Actor.
static_assert(!std::is_convertible_v<tests::ActorBuilder::Sized, simple_platformer::Actor>);
static_assert(!std::is_convertible_v<tests::ActorBuilder::Placed, simple_platformer::Actor>);
static_assert(std::is_convertible_v<tests::ActorBuilder, simple_platformer::Actor>);
