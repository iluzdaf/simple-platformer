local function distanceSquared(from, to)
    local x = to.x - from.x
    local y = to.y - from.y
    return x * x + y * y
end

local function refugeFrom(snapshot)
    if snapshot.targetFeet == nil or snapshot.patrol == nil then
        return nil
    end

    local left = snapshot.patrol.firstFeet
    local right = snapshot.patrol.secondFeet
    if left.x > right.x then
        left, right = right, left
    end

    -- Choose the end in the direction away from the threat. Measuring which end is
    -- globally farther would sometimes send the rat through the player to the other end.
    if snapshot.feet.x < snapshot.targetFeet.x then
        return left
    end
    return right
end

return {
    activities = {
        flee = {
            update = function(self, snapshot)
                local refuge = refugeFrom(snapshot)
                if refuge == nil then
                    return { clearRoute = true }
                end

                -- At the far end, hold the corner and face the threat. Facing it lets the
                -- machine's directional bite-range fact become true
                if distanceSquared(snapshot.feet, refuge) <= 1 then
                    return { clearRoute = true, aimAt = snapshot.targetFeet }
                end

                return { routeTo = refuge }
            end,
        },
    },
}
