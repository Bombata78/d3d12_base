///////////////////////////////////////////////////////////////////////////////////////////////////
//  trackball.cpp
//    Trackball implementation
///////////////////////////////////////////////////////////////////////////////////////////////////
#include <algorithm>
#include "utils/trackball.h"

namespace utils
{

glm::quat trackball(float radius, glm::vec2 p1, glm::vec2 p2)
{
    assert(radius);
    // If p1 and p2 are the same, return the identity
    if (p1 == p2)
        return glm::quat();

    // Project the 2d coord of a sphere of a specified radius.
    // Return the normalize vector to that position
    auto project_to_sphere = [&radius](const glm::vec2& point)
    {
        float d = glm::length(point);

        // Check if we're inside the sphere
        float z;
        if (d < radius * sqrt(0.5f))
        {
            z = sqrt(radius*radius - d*d);
        }
        else
        {
            float t = radius * sqrt(0.5f);
            z = t * t / d;
        }

        return glm::normalize(glm::vec3(point, z));
    };

    glm::vec3 spherePos1 = project_to_sphere(p1);
    glm::vec3 spherePos2 = project_to_sphere(p2);

    // Get the rotation axis/angle
    glm::vec3 axis = glm::normalize(glm::cross(spherePos1, spherePos2));
    float angle    = glm::acos(std::min(1.0f, std::max(-1.0f, glm::dot(glm::normalize(spherePos1), glm::normalize(spherePos2)))));

    return glm::angleAxis(angle, axis);
}

}