#include "ground_tile_map.h"

GroundTileMap::GroundTileMap(uint32_t tilesPerSide, float tileSize, float uvTilingPerTile)
{
    build(tilesPerSide, tileSize, uvTilingPerTile);
}

const GroundMeshData &GroundTileMap::getMeshData() const
{
    return meshData;
}

void GroundTileMap::build(uint32_t tilesPerSide, float tileSize, float uvTilingPerTile)
{
    meshData.positions.clear();
    meshData.texCoords.clear();
    meshData.indices.clear();

    const uint32_t verticesPerSide = tilesPerSide + 1;
    meshData.positions.reserve(static_cast<size_t>(verticesPerSide) * verticesPerSide);
    meshData.texCoords.reserve(static_cast<size_t>(verticesPerSide) * verticesPerSide);
    meshData.indices.reserve(static_cast<size_t>(tilesPerSide) * tilesPerSide * 6);

    const float halfExtent = static_cast<float>(tilesPerSide) * tileSize * 0.5f;

    for (uint32_t row = 0; row < verticesPerSide; ++row)
    {
        for (uint32_t col = 0; col < verticesPerSide; ++col)
        {
            const float x = static_cast<float>(col) * tileSize - halfExtent;
            const float y = static_cast<float>(row) * tileSize - halfExtent;
            const float u = static_cast<float>(col) * uvTilingPerTile;
            const float v = static_cast<float>(row) * uvTilingPerTile;

            meshData.positions.push_back({x, y, 0.0f});
            meshData.texCoords.push_back({u, v});
        }
    }

    for (uint32_t row = 0; row < tilesPerSide; ++row)
    {
        for (uint32_t col = 0; col < tilesPerSide; ++col)
        {
            const uint32_t topLeft = row * verticesPerSide + col;
            const uint32_t topRight = topLeft + 1;
            const uint32_t bottomLeft = (row + 1) * verticesPerSide + col;
            const uint32_t bottomRight = bottomLeft + 1;

            meshData.indices.insert(meshData.indices.end(), {
                topLeft, bottomLeft, bottomRight,
                bottomRight, topRight, topLeft
            });
        }
    }
}
