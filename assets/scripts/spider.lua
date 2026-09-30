-- Lua chooses patrolling, pursuit, and biting; the engine follows routes, including climbs.
-- Commands leave out climbGrip, so a spider that stops on a wall or ceiling stays on it.

return {
    activities = {
        patrol = {
            enter = function(self, snapshot)
                local patrol = snapshot.patrol
                if patrol == nil then
                    return
                end
                -- Resume from the nearer end; arriving there turns the spider round.
                self.headingToSecond = snapshot.feet:distanceSquared(patrol.secondFeet)
                    < snapshot.feet:distanceSquared(patrol.firstFeet)
            end,
            update = function(self, snapshot)
                local patrol = snapshot.patrol
                if patrol == nil then
                    return { clearRoute = true }
                end

                -- Entering the state cleared the route, so a finished route is this patrol's.
                -- A route that ends short of an unreachable end also turns it round.
                if snapshot.routeComplete then
                    self.headingToSecond = not self.headingToSecond
                    return { clearRoute = true }
                end

                local destination = self.headingToSecond and patrol.secondFeet or patrol.firstFeet
                return { routeTo = destination }
            end,
        },
        pursue = {
            update = function(self, snapshot)
                local target = snapshot.targetFeet

                -- Finish the current bite before moving, even if the target leaves.
                if not snapshot.facts.biteReady then
                    return { clearRoute = true, aimAt = target }
                end

                if target == nil then
                    return { clearRoute = true }
                end

                if snapshot.facts.targetInBiteRange then
                    return {
                        clearRoute = true,
                        aimAt = target,
                        primaryAttackPressed = true,
                    }
                end

                return { routeTo = target, aimAt = target }
            end,
        },
    },
}
