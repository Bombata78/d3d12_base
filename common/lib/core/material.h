///////////////////////////////////////////////////////////////////////////////////////////////////
//  scene.h
//    Manage scene description
///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <memory>
#include <vector>
#include "core/texture.h"
#include "glm/glm/glm.hpp"

enum class bxdf
{
    lambertian_reflection = 0,
    blinn_phong_reflection = 1
};

class material
{
public:
    material(const std::vector<bxdf> bsdf__, const std::shared_ptr<texture>& base_color__, float roughness__);

    material()                           = delete;
    material(const material&)            = delete;
    material(material&&)                 = delete;
    material& operator=(material&&)      = delete;
    material& operator=(const material&) = delete;

private:
    std::vector<bxdf>        bsdf_;
    std::shared_ptr<texture> base_color_;
    float                    roughness_;
};