#include "camera.h"

#include <glm/gtc/matrix_transform.hpp>

Camera::Camera(const glm::vec3 &position,
               const glm::vec3 &target,
               const glm::vec3 &up,
               float fovDegrees,
               float nearPlane,
               float farPlane)
    : position(position),
      target(target),
      up(up),
      fovDegrees(fovDegrees),
      nearPlane(nearPlane),
      farPlane(farPlane)
{
}

glm::mat4 Camera::getViewMatrix() const
{
    return glm::lookAt(position, target, up);
}

glm::mat4 Camera::getProjectionMatrix(float aspectRatio) const
{
    return glm::perspective(glm::radians(fovDegrees), aspectRatio, nearPlane, farPlane);
}
