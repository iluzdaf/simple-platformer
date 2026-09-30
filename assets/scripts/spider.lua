-- Lua chooses patrolling, pursuit, and biting; the engine follows routes, including climbs.

-- Holding still keeps the spider's grip if it stopped on a wall or ceiling.
local function holdStill()
    return { clearRoute = true, climbRequested = true }
end

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
                    return holdStill()
                end

                -- Entering the state cleared the route, so a finished route is this patrol's.
                -- A route that ends short of an unreachable end also turns it round.
                if snapshot.pathComplete then
                    self.headingToSecond = not self.headingToSecond
                    return holdStill()
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
                    return { clearRoute = true, aimAt = target, climbRequested = true }
                end

                if target == nil then
                    return { clearRoute = true, climbRequested = true }
                end

                if snapshot.facts.targetInBiteRange then
                    return {
                        clearRoute = true,
                        aimAt = target,
                        primaryAttackPressed = true,
                        climbRequested = true,
                    }
                end

                return { routeTo = target, aimAt = target }
            end,
        },
    },
}
