#pragma once

#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

#include <glm/glm.hpp>

struct CrateMeshData
{
    std::vector<glm::vec3> positions;
    std::vector<glm::vec2> texCoords;
    std::vector<uint32_t>  indices;
    glm::vec3              color = {1.0f, 1.0f, 1.0f};
};

struct CrateMeshConfig
{
    float desiredSize = 1.4f;
};

// Loads the shared crate mesh once, centered at the origin so it can hover and
// spin in place via its per-instance model matrix.
[[nodiscard]] CrateMeshData loadCrateMesh(const std::string &gltfPath, const CrateMeshConfig &config = {});

// A single crate sitting on the map. It bobs up and down and slowly rotates so
// it stays easy to spot.
class Crate
{
  public:
    Crate(const glm::vec3 &groundPosition, float initialPhase);

    void update(float deltaTimeSeconds);

    [[nodiscard]] glm::mat4        getModelMatrix() const;
    [[nodiscard]] const glm::vec3 &getPosition() const { return position; }

  private:
    glm::vec3 position;             // base position on the ground (z = 0)
    float     animationTime = 0.0f; // drives the hover and spin
};

// Spawns crates at random spots, keeps their count capped, animates them, and
// removes ones that get hit.
class CrateSystem
{
  public:
    CrateSystem(std::size_t maxCrates, float spawnIntervalSeconds, float mapHalfExtent, float spawnMargin);

    void update(float deltaTimeSeconds);

    // Removes the crates at the given indices (used when bullets hit them).
    void removeAt(std::vector<std::size_t> indices);

    // Clears all crates and resets the spawn timer (used when a round starts).
    void reset();

    [[nodiscard]] const std::vector<Crate> &getCrates() const { return crates; }

  private:
    void spawnCrate();

    std::vector<Crate> crates;
    std::size_t        maxCrates;
    float              spawnInterval;
    float              spawnTimer = 0.0f;
    float              mapHalfExtent;
    float              spawnMargin;

    std::mt19937                          rng;
    std::uniform_real_distribution<float> coordDist;
};
