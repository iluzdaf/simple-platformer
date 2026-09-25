#include "simple_platformer/npc/npc_fact_rows.hpp"

#include <string_view>
#include <vector>

#include "simple_platformer/npc/npc_transitions.hpp"

namespace simple_platformer
{
    const std::vector<NpcFactRow>& npcFactRows()
    {
        static const std::vector<NpcFactRow> rows{
            {"targetKnown",
             "target known",
             "no target",
             [](const NpcFacts& facts) { return facts.targetKnown; }},
            {"targetVisible",
             "target in sight",
             "target out of sight",
             [](const NpcFacts& facts) { return facts.targetVisible; }},
            {"targetInBiteRange",
             "target in bite range",
             "target out of bite range",
             [](const NpcFacts& facts) { return facts.targetInBiteRange; }},
            {"biteReady",
             "bite ready",
             "bite not ready",
             [](const NpcFacts& facts) { return facts.biteReady; }},
            {"targetInSights",
             "target in sights",
             "target not in sights",
             [](const NpcFacts& facts) { return facts.targetInSights; }},
            {"targetTooClose",
             "target too close",
             "target at a distance",
             [](const NpcFacts& facts) { return facts.targetTooClose; }},
            {"hasPatrol",
             "has a patrol",
             "no patrol",
             [](const NpcFacts& facts) { return facts.hasPatrol; }},
            {"searchTimeUp",
             "search time up",
             "still searching",
             [](const NpcFacts& facts) { return facts.searchTimeUp; }},
        };
        return rows;
    }

    const NpcFactRow* npcFactRow(std::string_view name)
    {
        for (const NpcFactRow& row : npcFactRows())
        {
            if (row.name == name)
            {
                return &row;
            }
        }
        return nullptr;
    }
}
