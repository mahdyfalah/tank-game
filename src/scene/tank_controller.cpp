#include "tank_controller.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

TankController::TankController(const glm::vec3 &initialPosition,
                               float mapHalfExtent,
                               float groundZ,
                               float initialYawRadians)
    : position(initialPosition),
      yawRadians(initialYawRadians),
      mapHalfExtent(mapHalfExtent),
      groundZ(groundZ)
{
    position.z = groundZ;
}

void TankController::update(GLFWwindow *window, float deltaTimeSeconds)
{
    if (deltaTimeSeconds <= 0.0f || window == nullptr)
    {
        return;
    }

    // Clamp delta-time to avoid big movement jumps after hitches or startup stalls.
    const float dt = std::min(deltaTimeSeconds, 0.05f);

    float moveInput = 0.0f;
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
    {
        moveInput += 1.0f;
    }
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
    {
        moveInput -= 1.0f;
    }

    float turnInput = 0.0f;
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
    {
        turnInput -= 1.0f;
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
    {
        turnInput += 1.0f;
    }

    yawRadians += turnInput * turnSpeedRadiansPerSecond * dt;

    constexpr float twoPi = 6.28318530718f;
    if (yawRadians > twoPi)
    {
        yawRadians -= twoPi;
    }
    else if (yawRadians < -twoPi)
    {
        yawRadians += twoPi;
    }

    // Forward must match the model's facing produced by getModelMatrix().
    // getModelMatrix() applies rotate(yaw, +Z) to the tank whose model-space
    // forward is +Y, which yields the world forward below. Using the same
    // vector here guarantees the tank drives exactly where it points.
    const glm::vec3 forward = {-std::sin(yawRadians), std::cos(yawRadians), 0.0f};
    const float     speed   = (moveInput >= 0.0f) ? moveSpeedUnitsPerSecond : reverseSpeedUnitsPerSecond;
    position += forward * (moveInput * speed * dt);

    const float minCoord = -mapHalfExtent + colliderRadius;
    const float maxCoord = mapHalfExtent - colliderRadius;
    position.x = std::clamp(position.x, minCoord, maxCoord);
    position.y = std::clamp(position.y, minCoord, maxCoord);
    position.z = groundZ;
}

glm::mat4 TankController::getModelMatrix() const
{
    // The tank model's natural facing is opposite of +Y, so add a half turn
    // to make its visible front line up with the movement direction.
    constexpr float pi = 3.14159265359f;
    glm::mat4       model(1.0f);
    model = glm::translate(model, position);
    model = glm::rotate(model, yawRadians + pi, glm::vec3(0.0f, 0.0f, 1.0f));
    return model;
}