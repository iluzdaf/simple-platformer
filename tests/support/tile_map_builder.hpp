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

        // The region of the tile texture it is drawn with.
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

    // Builds a TileMap from rows of symbols. Two symbols are always defined:
    //
    //   '.' is empty, passable and see-through. TileMap requires tile ID zero to be empty.
    //   '#' is solid, blocking movement and sight together. It stands in for an obstacle
    //       rather than for any tile in the game, for tests whose tiles are scenery: they
    //       need somewhere to stand and something to bump into, and don't care which
    //       property provides it. Its sprite region is one tile at the atlas origin, as a
    //       catalogue would give it, and no test asserts on it.
    //
    // Neither can be redefined. When a tile's properties are what a test is about, give it
    // another symbol and declare exactly what it blocks with where(), so the test states its
    // own premise; the builder rejects any symbol that is used but not declared. Declared
    // tiles take IDs from 1 in order, and '#' takes the next one.
    //
    // The builder converts to a TileMap wherever one is expected. Its tiles are tests::TileSize
    // unless the test says otherwise with withTileSize().
    class TileMapBuilder
    {
    public:
        explicit TileMapBuilder(std::vector<std::string> rows)
            : mapRows(std::move(rows))
        {
        }

        TileMapBuilder withTileSize(int tileSize) &&
        {
            cellSize = tileSize;
            return std::move(*this);
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
            const auto side = static_cast<float>(cellSize);
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
                cellSize, mapRows, std::move(definitions), legend);
        }

    private:
        int cellSize = TileSize;
        std::vector<std::string> mapRows;
        std::vector<std::pair<char, Tile>> tiles;
    };
}
