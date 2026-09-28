#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "simple_platformer/render/sprite.hpp"
#include "simple_platformer/world/tile_map.hpp"
#include "support/tile_size.hpp"

namespace tests
{
    // The properties of one tile, for tests where they are the subject. A tile starts out
    // blocking nothing, and a test asks for exactly what it depends on.
    class Tile
    {
    public:
        Tile blocksMovement() &&
        {
            definition.blocksMovement = true;
            return *this;
        }

        Tile blocksSight() &&
        {
            definition.blocksSight = true;
            return *this;
        }

        Tile climbable() &&
        {
            definition.climbable = true;
            return *this;
        }

        Tile withSprite(simple_platformer::SpriteRegion region) &&
        {
            definition.sprite = region;
            return *this;
        }

        // What breaking the tile leaves, named by its map symbol.
        Tile breaksInto(char symbol) &&
        {
            breaksIntoSymbol = symbol;
            return *this;
        }

    private:
        friend class TileMapBuilder;

        simple_platformer::TileDefinition definition;
        std::optional<char> breaksIntoSymbol;
    };

    // '.' is empty (ID 0); '#' blocks movement and sight. Tests that depend on
    // specific tile properties should declare another symbol with where(). Neither
    // built-in symbol can be redefined, and undeclared symbols are rejected.
    // Conversion builds a TileMap using tests::TileSize.
    class TileMapBuilder
    {
    public:
        explicit TileMapBuilder(std::vector<std::string> rows)
            : mapRows(std::move(rows))
        {
        }

        TileMapBuilder where(char symbol, Tile tile) &&
        {
            if (symbol == '.' || symbol == '#')
            {
                throw std::logic_error("'.' and '#' are always empty and solid");
            }
            for (const auto& declared : tiles)
            {
                if (declared.first == symbol)
                {
                    throw std::logic_error("A map symbol was declared twice");
                }
            }
            tiles.emplace_back(symbol, tile);
            return std::move(*this);
        }

        operator simple_platformer::TileMap() &&
        {
            std::vector<simple_platformer::TileDefinition> definitions{{}};
            std::map<char, int> legend{{'.', 0}};
            for (const auto& declared : tiles)
            {
                legend.emplace(declared.first, static_cast<int>(definitions.size()));
                definitions.push_back(declared.second.definition);
            }

            simple_platformer::TileDefinition solid;
            solid.blocksMovement = true;
            solid.blocksSight = true;
            const auto side = static_cast<float>(TileSize);
            solid.sprite = {{0.0F, 0.0F}, {side, side}};
            legend.emplace('#', static_cast<int>(definitions.size()));
            definitions.push_back(solid);

            for (std::size_t index = 0; index < tiles.size(); ++index)
            {
                const std::optional<char>& breaksInto = tiles[index].second.breaksIntoSymbol;
                if (!breaksInto.has_value())
                {
                    continue;
                }
                const auto target = legend.find(*breaksInto);
                if (target == legend.end())
                {
                    throw std::logic_error("A tile breaks into a symbol the map does not declare");
                }
                definitions[index + 1].breaksIntoTileId = target->second;
            }

            return simple_platformer::TileMap::fromAscii(
                TileSize, mapRows, std::move(definitions), legend);
        }

    private:
        std::vector<std::string> mapRows;
        std::vector<std::pair<char, Tile>> tiles;
    };
}
