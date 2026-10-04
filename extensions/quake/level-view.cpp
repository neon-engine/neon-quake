#include "level-view.hpp"

#include <cstdint>
#include <cstdlib>
#include <vector>

#include "formats/bsp-file.hpp"
#include "formats/bsp-mesh.hpp"
#include "formats/entity-text.hpp"
#include "formats/quake-space.hpp"

namespace quake
{
  // Helpers of LevelView, for this file alone.
  namespace
  {
    using neon::extension::Entity;
    using neon::extension::Vertex;
    using neon::extension::World;

    // how high the eyes of a player are above where the level puts it
    constexpr float eye_height = 22.0f;

    /// The three numbers of a key such as `origin`, or false when the
    /// entity has no such key or it holds something else.
    bool read_vector(const BspEntity &entity, const std::string &key, BspVector &vector)
    {
      const std::string *text = entity.Find(key);
      if (text == nullptr) { return false; }

      char *end = nullptr;
      const char *at = text->c_str();
      float numbers[3] = {};
      for (float &number : numbers)
      {
        number = std::strtof(at, &end);
        if (end == at) { return false; }
        at = end;
      }

      vector = {numbers[0], numbers[1], numbers[2]};
      return true;
    }

    /// Makes the picture of a texture of the level known to the renderer,
    /// and returns the path a material reads it by, or nothing.
    std::string show_texture(const World &world, const GameData &data, const MipTexture &texture)
    {
      const std::vector<std::uint8_t> pixels = data.GetPalette().ToRgba(texture.pixels[0], texture.HasHoles());
      return world.SetImage("textures/" + texture.name, texture.width, texture.height, pixels);
    }
  }

  void LevelView::FindStart(const World &world, const std::string &entities, const std::string &map)
  {
    EntityText text;
    if (std::string problem; !text.Read(entities, problem))
    {
      world.Warn("The entities of " + map + " cannot be read: " + problem);
      return;
    }

    // the start of a single player, or else where a player of a deathmatch
    // comes in
    const BspEntity *start = nullptr;
    for (const char *wanted : {"info_player_start", "info_player_deathmatch"})
    {
      for (const BspEntity &entity : text.entities)
      {
        const std::string *kind = entity.Find("classname");
        if (kind != nullptr && *kind == wanted)
        {
          start = &entity;
          break;
        }
      }
      if (start != nullptr) { break; }
    }

    BspVector origin;
    if (start == nullptr || !read_vector(*start, "origin", origin))
    {
      world.Warn("The level " + map + " says nowhere a player starts, so the camera stays where the scene has it");
      return;
    }

    origin.z += eye_height;
    const BspVector place = QuakeSpace::ToEnginePosition(origin);
    _start_position = {place.x, place.y, place.z};

    float angle = 0.0f;
    if (const std::string *written = start->Find("angle"); written != nullptr)
    {
      angle = std::strtof(written->c_str(), nullptr);
    }
    _start_yaw = QuakeSpace::ToEngineYaw(angle);
    _has_start = true;
  }

  void LevelView::PlaceCamera(const World &world)
  {
    if (!_has_start || _camera_placed) { return; }

    const Entity camera = world.FindEntity("camera");
    if (camera == 0) { return; }

    world.SetVector3(camera, world.FindField("Transform", "position"), _start_position);
    // pitch, yaw, and roll in degrees
    world.SetVector3(camera, world.FindField("Transform", "rotation"), {0.0f, _start_yaw, 0.0f});
    _camera_placed = true;
  }

  bool LevelView::Show(const World &world, const GameData &data, const std::string &map, std::string &error)
  {
    const std::span<const std::uint8_t> bytes = data.Find(map);
    if (bytes.empty())
    {
      error = "the data of the game holds no " + map;
      return false;
    }

    BspFile level;
    if (std::string problem; !level.Read(bytes, problem))
    {
      error = map + " cannot be read: " + problem;
      return false;
    }

    // the first model of a level is the level itself; the others are its
    // doors and lifts, which come with what moves them
    BspMesh mesh;
    if (std::string problem; !mesh.Build(level, 0, problem))
    {
      error = "no mesh can be made of " + map + ": " + problem;
      return false;
    }

    const NeonField shader_field = world.FindField("Renderable", "shader");
    const NeonField textures_field = world.FindField("Renderable", "textures");

    const Entity root = world.CreateEntity("level");
    world.AddComponent(root, "Transform");

    std::size_t shown = 0;
    std::size_t triangles = 0;

    // one entity for each texture, with the faces that show it
    for (const BspMeshGroup &group : mesh.groups)
    {
      // the sky is no wall with a picture on it
      if (group.is_sky || group.indices.empty()) { continue; }

      // The corners the group uses, handed over once each. A corner belongs
      // to one face and a face to one texture, so the groups share none.
      std::vector<Vertex> corners;
      std::vector<std::uint32_t> indices;
      std::vector<std::uint32_t> place_of(mesh.vertices.size(), UINT32_MAX);

      for (const std::uint32_t index : group.indices)
      {
        if (place_of[index] == UINT32_MAX)
        {
          const BspMeshVertex &from = mesh.vertices[index];
          const BspVector place = QuakeSpace::ToEnginePosition(from.position);
          const BspVector normal = QuakeSpace::ToEngineDirection(from.normal);

          place_of[index] = static_cast<std::uint32_t>(corners.size());
          corners.push_back({
            {place.x, place.y, place.z},
            {normal.x, normal.y, normal.z},
            {from.texture_u, from.texture_v},
            {1.0f, 1.0f, 1.0f, 1.0f},
          });
        }
        indices.push_back(place_of[index]);
      }

      // the game winds clockwise seen from outside, the engine the other way
      for (std::size_t i = 0; i + 2 < indices.size(); i += 3) { std::swap(indices[i + 1], indices[i + 2]); }

      const bool has_texture = group.texture >= 0 &&
                               static_cast<std::size_t>(group.texture) < level.textures.size() &&
                               level.textures[group.texture].has_value();
      const std::string name = has_texture ? level.textures[group.texture]->name : "missing-" + std::to_string(group.texture);

      const Entity entity = world.CreateEntity(name, root);
      world.AddComponent(entity, "Transform");
      world.AddComponent(entity, "Renderable");
      world.SetText(entity, shader_field, "assets://shaders/unlit");

      if (has_texture)
      {
        if (const std::string picture = show_texture(world, data, *level.textures[group.texture]); !picture.empty())
        {
          world.SetTexts(entity, textures_field, {picture});
        }
      }

      if (world.SetMesh(entity, corners, indices))
      {
        shown++;
        triangles += indices.size() / 3;
      }
    }

    FindStart(world, level.entities, map);

    world.Info("Showing " + map + ": " + std::to_string(shown) + " textures, " + std::to_string(triangles) + " triangles");
    return true;
  }
} // quake
