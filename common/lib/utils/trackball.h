///////////////////////////////////////////////////////////////////////////////////////////////////
//  trackball.h
//    Trackball implementation
///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "glm/glm/glm.hpp"
#include "glm/glm/gtc/quaternion.hpp"

namespace utils
{
    // Return a quaternion describing the orientation given by two 2D positions with a domain of [0.0..1.0].
    // Origin being at the lower left corner.
    glm::quat trackball(float radius, glm::vec2 p1, glm::vec2 p2);
};