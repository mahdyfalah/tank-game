#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

struct TankMeshData
{
    std::vector<glm::vec3> positions;
    std::vector<glm::vec2> texCoords;
    std::vector<uint32_t> indices;
    glm::vec3 color = {0.6f, 0.6f, 0.6f};
};

struct TankSpawnConfig
{
    glm::vec3 position = {0.0f, 0.0f, 0.0f};
    float desiredLength = 1.8f;
    float groundOffset = 0.02f;
    glm::vec3 color = {0.6f, 0.6f, 0.6f};
};

class Tank
{
  public:
    Tank(const std::string &gltfPath, const TankSpawnConfig &spawnConfig = {});

    [[nodiscard]] const TankMeshData &getMeshData() const;

  private:
    TankMeshData meshData;

    void loadFromGltf(const std::string &gltfPath, const TankSpawnConfig &spawnConfig);
};