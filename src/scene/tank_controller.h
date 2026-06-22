#pragma once

#include <glm/glm.hpp>

struct GLFWwindow;

class TankController
{
  public:
    TankController(const glm::vec3 &initialPosition,
                   float mapHalfExtent,
                   float groundZ = 0.0f,
                   float initialYawRadians = 0.0f);

    void update(GLFWwindow *window, float deltaTimeSeconds);

    [[nodiscard]] glm::mat4 getModelMatrix() const;
    [[nodiscard]] glm::vec3 getPosition() const;

  private:
    glm::vec3 position;
    float     yawRadians;
    float     mapHalfExtent;
    float     groundZ;
    float     moveSpeedUnitsPerSecond = 6.0f;
    float     reverseSpeedUnitsPerSecond = 3.0f;
    float     turnSpeedRadiansPerSecond = 2.4f;
    float     colliderRadius = 1.2f;
};