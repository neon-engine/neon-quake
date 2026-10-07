#include "particle-view.hpp"

#include <cmath>
#include <cstdint>
#include <vector>

#include "formats/quake-space.hpp"

namespace quake
{
  using neon::extension::Vertex;
  using neon::extension::World;

  void ParticleView::Show(
    const World &world,
    const Palette &palette,
    const neon::extension::Entity parent,
    const std::span<const Particle> particles,
    const LevelVector &eyes,
    const LevelVector &forward,
    const LevelVector &right,
    const LevelVector &up)
  {
    if (particles.empty() && !_has_mesh) { return; }

    if (_entity == 0)
    {
      _entity = world.CreateEntity("particles", parent);
      world.AddComponent(_entity, "Transform");
      world.AddComponent(_entity, "Renderable");
      world.SetText(_entity, world.FindField("Renderable", "shader"), "engine://shaders/unlit");
      // a particle is its colour and nothing else
      world.SetBoolean(_entity, world.FindField("Renderable", "material.use_textures"), false);
    }

    if (!_has_colours)
    {
      // the palette holds what a screen is given; the engine wants light
      for (std::size_t i = 0; i < _colours.size(); i++)
      {
        const std::array<std::uint8_t, 3> colour = palette.GetColour(static_cast<std::uint8_t>(i));
        _colours[i] = {
          std::pow(static_cast<float>(colour[0]) / 255.0f, 2.2f),
          std::pow(static_cast<float>(colour[1]) / 255.0f, 2.2f),
          std::pow(static_cast<float>(colour[2]) / 255.0f, 2.2f),
          1.0f,
        };
      }
      _has_colours = true;
    }

    std::vector<Vertex> corners;
    std::vector<std::uint32_t> indices;
    corners.reserve(particles.size() * 4);
    indices.reserve(particles.size() * 6);

    // the square looks back at the camera
    const BspVector normal = QuakeSpace::ToEngineDirection({-forward[0], -forward[1], -forward[2]});

    for (const Particle &particle : particles)
    {
      // a little larger the further away, as the original has it
      const float depth = (particle.position[0] - eyes[0]) * forward[0] + (particle.position[1] - eyes[1]) * forward[1] +
                          (particle.position[2] - eyes[2]) * forward[2];
      const float side = size * (depth < 20.0f ? 1.08f : 1.0f + depth * 0.004f);

      const auto first = static_cast<std::uint32_t>(corners.size());
      // the corner of a square is where the particle is, and it reaches up
      // and to the right from there
      for (const auto &[along_right, along_up] : {std::pair(0.0f, 0.0f), std::pair(1.0f, 0.0f), std::pair(1.0f, 1.0f), std::pair(0.0f, 1.0f)})
      {
        const BspVector place = QuakeSpace::ToEnginePosition({
          particle.position[0] + (right[0] * along_right + up[0] * along_up) * side,
          particle.position[1] + (right[1] * along_right + up[1] * along_up) * side,
          particle.position[2] + (right[2] * along_right + up[2] * along_up) * side,
        });
        corners.push_back({{place.x, place.y, place.z}, {normal.x, normal.y, normal.z}, {along_right, along_up}, _colours[particle.colour]});
      }
      // both ways round, so that it shows whichever way the engine takes
      // as the front
      for (const std::uint32_t corner : {0u, 1u, 2u, 0u, 2u, 3u, 0u, 2u, 1u, 0u, 3u, 2u}) { indices.push_back(first + corner); }
    }

    if (corners.empty())
    {
      // nothing lives: a speck too small to see takes the place of the last
      corners = {
        {{0.0f, -10000.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}},
        {{0.0f, -10000.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}},
        {{0.0f, -10000.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}},
      };
      indices = {0, 1, 2};
      _has_mesh = false;
    } else
    {
      _has_mesh = true;
    }
    world.SetMesh(_entity, corners, indices);
  }

  void ParticleView::Clear()
  {
    _entity = 0;
    _has_mesh = false;
  }
} // quake
