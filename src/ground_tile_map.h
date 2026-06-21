#pragma once

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

struct GroundMeshData
{
    std::vector<glm::vec3> positions;
    std::vector<glm::vec2> texCoords;
    std::vector<uint32_t>  indices;
};

class GroundTileMap
{
  public:
    GroundTileMap(uint32_t tilesPerSide, float tileSize, float uvTilingPerTile);

    [[nodiscard]] const GroundMeshData &getMeshData() const;

  private:
    GroundMeshData meshData;

    void build(uint32_t tilesPerSide, float tileSize, float uvTilingPerTile);
};