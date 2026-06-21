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

  private:
    glm::vec3 position;
    glm::vec3 target;
    glm::vec3 up;
    float     fovDegrees;
    float     nearPlane;
    float     farPlane;
};