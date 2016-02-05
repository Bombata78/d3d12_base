///////////////////////////////////////////////////////////////////////////////////////////////////
//  light.h
//    Manage light description
///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "glm/glm/glm.hpp"

class light
{
public:
    enum type
    {
        point
    };

    light(type type__, const glm::vec3& position__,  const glm::vec3& intensity__, const glm::vec3& direction__);

    light()                        = delete;
    light(const light&)            = delete;
    light(light&&)                 = delete;
    light& operator=(light&&)      = delete;
    light& operator=(const light&) = delete;

private:
    type      type_;
    glm::vec3 position_;
    glm::vec3 intensity_;
    glm::vec3 direction_;
};