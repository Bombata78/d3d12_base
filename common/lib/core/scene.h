///////////////////////////////////////////////////////////////////////////////////////////////////
//  scene.h
//    Manage scene description
///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <vector>
#include <memory>
class primitive;
class light;

class scene
{
public:
    scene();

    inline void add_primitive(const std::shared_ptr<primitive>& primitive);
    inline const std::vector<std::shared_ptr<primitive>>& primitives() const;

    inline void add_light(const std::shared_ptr<light>& light);
    inline const std::vector<std::shared_ptr<light>>& lights() const;

    scene(const scene&)            = delete;
    scene(scene&&)                 = delete;
    scene& operator=(scene&&)      = delete;
    scene& operator=(const scene&) = delete;

private:
    std::vector<std::shared_ptr<primitive>> primitives_;
    std::vector<std::shared_ptr<light>> lights_;
};

//    -- inline definitions --
inline void scene::add_primitive(const std::shared_ptr<primitive>& primitive)
{
    primitives_.push_back(primitive);
}

inline const std::vector<std::shared_ptr<primitive>>& scene::primitives() const
{
    return primitives_;
}

inline void scene::add_light(const std::shared_ptr<light>& light)
{
    lights_.push_back(light);
}

inline const std::vector<std::shared_ptr<light>>& scene::lights() const
{
    return lights_;
}