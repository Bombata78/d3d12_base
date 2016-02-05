///////////////////////////////////////////////////////////////////////////////////////////////////
//  camera.h
//    Manage camera description
///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "glm/glm/glm.hpp"

class camera
{
public:
    enum type
    {
        projective
    };

    camera(const glm::vec3& position__, const glm::vec3& direction__);
    camera(glm::vec3&& position__, glm::vec3&& direction__);

    camera()                         = delete;
    camera(const camera&)            = delete;
    camera(camera&&)                 = delete;
    camera& operator=(camera&&)      = delete;
    camera& operator=(const camera&) = delete;

private:
    glm::vec3 position_;
    glm::vec3 direction_;
};