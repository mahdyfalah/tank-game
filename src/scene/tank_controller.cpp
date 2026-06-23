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
    : initialPosition(initialPosition),
      initialYawRadians(initialYawRadians),
      position(initialPosition),
      yawRadians(initialYawRadians),
      mapHalfExtent(mapHalfExtent),
      groundZ(groundZ)
{
    position.z = groundZ;
}

void TankController::reset()
{
    position   = initialPosition;
    position.z = groundZ;
    yawRadians = initialYawRadians;
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

    // Gamepad input (overrides/augments keyboard): R2 forward, L2 reverse,
    // left stick (L3) steers. Triggers report -1 when released, 1 when fully
    // pressed, so remap them into a 0..1 range first.
    GLFWgamepadstate gamepad;
    if (glfwJoystickIsGamepad(GLFW_JOYSTICK_1) &&
        glfwGetGamepadState(GLFW_JOYSTICK_1, &gamepad))
    {
        const float rightTrigger = (gamepad.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] + 1.0f) * 0.5f;
        const float leftTrigger  = (gamepad.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] + 1.0f) * 0.5f;

        constexpr float triggerDeadzone = 0.1f;
        if (rightTrigger > triggerDeadzone)
        {
            moveInput += rightTrigger;
        }
        if (leftTrigger > triggerDeadzone)
        {
            moveInput -= leftTrigger;
        }

        const float stickX = gamepad.axes[GLFW_GAMEPAD_AXIS_LEFT_X];
        constexpr float stickDeadzone = 0.2f;
        if (std::fabs(stickX) > stickDeadzone)
        {
            // Pushing the stick right turns the tank right (negative yaw).
            turnInput -= stickX;
        }
    }

    moveInput = std::clamp(moveInput, -1.0f, 1.0f);
    turnInput = std::clamp(turnInput, -1.0f, 1.0f);

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

    // Direction the tank is facing (same formula as getModelMatrix uses), so
    // it drives where it points.
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
    // The model faces backwards by default, so add half a turn (pi) to make its
    // front line up with the driving direction.
    constexpr float pi = 3.14159265359f;
    glm::mat4       model(1.0f);
    model = glm::translate(model, position);
    model = glm::rotate(model, yawRadians + pi, glm::vec3(0.0f, 0.0f, 1.0f));
    return model;
}

glm::vec3 TankController::getPosition() const
{
    return position;
}

glm::vec3 TankController::getForward() const
{
    return {-std::sin(yawRadians), std::cos(yawRadians), 0.0f};
}