#pragma once

#include <glm/glm.hpp>

class Camera
{
  public:
    Camera(const glm::vec3 &position,
           const glm::vec3 &target,
           const glm::vec3 &up,
           float fovDegrees,
           float nearPlane,
           float farPlane);

    [[nodiscard]] glm::mat4 getViewMatrix() const;
    [[nodiscard]] glm::mat4 getProjectionMatrix(float aspectRatio) const;

    // Smoothly moves the camera toward (focusPoint + offset) while keeping it
    // aimed at focusPoint. interpolationFactor in [0,1] controls how much of
    // the remaining distance is covered this frame.
    void follow(const glm::vec3 &focusPoint, const glm::vec3 &offset, float interpolationFactor);

  private:
    glm::vec3 position;
    glm::vec3 target;
    glm::vec3 up;
    float     fovDegrees;
    float     nearPlane;
    float     farPlane;
};