#include "light-view.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include "formats/quake-space.hpp"

namespace quake
{
  using neon::extension::Entity;
  using neon::extension::World;

  void LightView::Flash(
    const std::int32_t key,
    const std::array<float, 3> &place,
    const float radius,
    const float seconds,
    const float decay)
  {
    // the light of the key, or else a new one, or else the oldest
    Light *light = nullptr;
    if (key != 0)
    {
      for (Light &known : _lights)
      {
        if (known.key == key) { light = &known; }
      }
    }
    if (light == nullptr && _lights.size() < most)
    {
      _lights.emplace_back();
      light = &_lights.back();
    }
    if (light == nullptr) { light = &_lights.front(); }

    light->key = key;
    light->place = place;
    light->radius = radius;
    light->decay = decay;
    light->ends_at = _time + static_cast<double>(seconds);
    light->has_changed = true;
  }

  float LightView::Flicker()
  {
    _dice = _dice * 1664525u + 1013904223u;
    return static_cast<float>((_dice >> 16) & 31u);
  }

  void LightView::Update(const World &world, const Entity parent, const double time)
  {
    const auto passed = static_cast<float>(std::max(time - _time, 0.0));
    _time = time;

    for (Light &light : _lights)
    {
      if (light.decay > 0.0f && passed > 0.0f)
      {
        light.radius = std::max(light.radius - light.decay * passed, 0.0f);
        light.has_changed = true;
      }
    }

    // those that went out
    std::erase_if(_lights, [&world, time](const Light &light)
    {
      if (light.ends_at > time && light.radius > 0.0f) { return false; }

      if (light.entity != 0) { world.DestroyEntity(light.entity); }
      return true;
    });

    for (Light &light : _lights)
    {
      if (light.entity == 0)
      {
        light.entity = world.CreateEntity("light of the moment " + std::to_string(++_made), parent);
        world.AddComponent(light.entity, "Transform");
        world.AddComponent(light.entity, "Light");
        world.SetText(light.entity, world.FindField("Light", "type"), "point");
        world.SetVector3(light.entity, world.FindField("Light", "diffuse"), {1.0f, 1.0f, 1.0f});
        world.SetBoolean(light.entity, world.FindField("Light", "casts_shadows"), false);
        light.has_changed = true;
      }
      if (!light.has_changed) { continue; }

      light.has_changed = false;
      const BspVector place = QuakeSpace::ToEnginePosition({light.place[0], light.place[1], light.place[2]});
      world.SetVector3(light.entity, world.FindField("Transform", "position"), {place.x, place.y, place.z});
      // how far it reaches, in metres, which the shaders of the game read
      world.SetNumber(light.entity, world.FindField("Light", "constant"), light.radius * QuakeSpace::metres_per_unit);
    }
  }

  float LightView::FindLight(const std::array<float, 3> &place) const
  {
    float sum = 0.0f;
    for (const Light &light : _lights)
    {
      const float x = place[0] - light.place[0];
      const float y = place[1] - light.place[1];
      const float z = place[2] - light.place[2];
      sum += std::max(light.radius - std::sqrt(x * x + y * y + z * z), 0.0f);
    }
    return sum;
  }

  void LightView::Forget()
  {
    _lights.clear();
  }
} // quake
