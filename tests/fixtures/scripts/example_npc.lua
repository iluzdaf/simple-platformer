return {
    activities = {
        idle = {
            update = function(self, snapshot, dt)
                return {
                    direction = { x = snapshot.tuning.direction * dt, y = 0 },
                    primaryAttackPressed = snapshot.facts.targetKnown,
                }
            end,
        },
    },
}
