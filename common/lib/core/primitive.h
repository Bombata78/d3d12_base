///////////////////////////////////////////////////////////////////////////////////////////////////
//  primitive.h
//    Manage primitive description
///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "core/mesh.h"
#include "core/material.h"

class primitive
{
public:
    primitive(const std::shared_ptr<mesh>& mesh__, const std::shared_ptr<material>& material__);

    primitive(const primitive&)            = delete;
    primitive(primitive&&)                 = delete;
    primitive& operator=(primitive&&)      = delete;
    primitive& operator=(const primitive&) = delete;

private:
    glm::mat4                 object_to_world;
    std::shared_ptr<mesh>     mesh_;
    std::shared_ptr<material> material_;
};