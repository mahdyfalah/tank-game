#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

struct BulletMeshData
{
    std::vector<glm::vec3> positions;
    std::vector<glm::vec2> texCoords;
    std::vector<uint32_t>  indices;
    glm::vec3              color = {1.0f, 1.0f, 1.0f};
};

struct BulletMeshConfig
{
    float desiredLength = 0.6f;
};

// Loads the shared bullet mesh once, centered at the origin and laid out so its
// longest axis points along +Y (the same forward convention the tank uses).
[[nodiscard]] BulletMeshData loadBulletMesh(const std::string &gltfPath, const BulletMeshConfig &config = {});

// A single in-flight bullet instance.
class Bullet
{
  public:
    Bullet(const glm::vec3 &spawnPosition, const glm::vec3 &direction, float speed);

    void update(float deltaTimeSeconds);

    [[nodiscard]] glm::mat4        getModelMatrix() const;
    [[nodiscard]] const glm::vec3 &getPosition() const { return position; }
    [[nodiscard]] float            getDistanceTravelled() const { return distanceTravelled; }

  private:
    glm::vec3 position;
    glm::vec3 direction;        // unit vector in the XY plane
    float     speed;
    float     yawRadians;       // orientation of the mesh around +Z
    float     distanceTravelled = 0.0f;
};

// Owns the active bullets and handles spawning and out-of-bounds culling.
class BulletSystem
{
  public:
    BulletSystem(std::size_t maxBullets, float mapHalfExtent, float maxRange);

    // Spawns a bullet if capacity allows. Returns true when a bullet was fired.
    bool fire(const glm::vec3 &spawnPosition, const glm::vec3 &direction, float speed);

    // Advances every bullet and removes those that left the map or exceeded range.
    void update(float deltaTimeSeconds);

    // Removes the bullets at the given indices (used when they hit a crate).
    void removeAt(std::vector<std::size_t> indices);

    [[nodiscard]] const std::vector<Bullet> &getBullets() const { return bullets; }

  private:
    std::vector<Bullet> bullets;
    std::size_t         maxBullets;
    float               mapHalfExtent;
    float               maxRange;
};
