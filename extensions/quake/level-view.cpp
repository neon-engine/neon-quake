#include "level-view.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <utility>
#include <vector>

#include "formats/bsp-mesh.hpp"
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

    /// Whether a texture is one the tools of levels paint on what is not
    /// to be seen: the faces of a trigger, of what only stops a player, and
    /// of what is to be skipped.
    bool is_unseen(const std::string &texture_name)
    {
      std::string name = texture_name;
      std::transform(name.begin(), name.end(), name.begin(), [](const unsigned char letter)
      {
        return static_cast<char>(std::tolower(letter));
      });
      return name == "trigger" || name == "clip" || name == "skip";
    }

    /// The number of the model an entity names with a key such as
    /// `"model" "*3"`, or 0 when it names none of the level: the world
    /// itself is never named so.
    std::size_t read_model_number(const BspEntity &entity)
    {
      const std::string *text = entity.Find("model");
      if (text == nullptr || text->size() < 2 || (*text)[0] != '*') { return 0; }

      std::size_t number = 0;
      for (std::size_t i = 1; i < text->size(); i++)
      {
        const char digit = (*text)[i];
        if (digit < '0' || digit > '9' || number > 1000000) { return 0; }
        number = number * 10 + static_cast<std::size_t>(digit - '0');
      }
      return number;
    }
  }

  void LevelView::FindStart(const World &world, const EntityText &text, const std::string &map)
  {
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

  const std::string &LevelView::FindPicture(
    const World &world,
    const GameData &data,
    const BspFile &level,
    const std::int32_t texture)
  {
    if (const auto known = _pictures.find(texture); known != _pictures.end()) { return known->second; }

    std::string path;
    if (texture >= 0 && static_cast<std::size_t>(texture) < level.textures.size() && level.textures[texture].has_value())
    {
      const MipTexture &source = *level.textures[texture];
      const std::vector<std::uint8_t> pixels = data.GetPalette().ToRgba(source.pixels[0], source.HasHoles());
      path = world.SetImage("textures/" + source.name, source.width, source.height, pixels);
    }
    return _pictures.emplace(texture, std::move(path)).first->second;
  }

  bool LevelView::ShowModel(
    const World &world,
    const GameData &data,
    const BspFile &level,
    const std::size_t model,
    const Entity parent,
    std::size_t &triangles,
    std::string &error)
  {
    BspMesh mesh;
    if (!mesh.Build(level, model, error)) { return false; }

    const NeonField shader_field = world.FindField("Renderable", "shader");
    const NeonField textures_field = world.FindField("Renderable", "textures");

    // one entity for each texture, with the faces that show it
    for (const BspMeshGroup &group : mesh.groups)
    {
      // the sky is no wall with a picture on it
      if (group.is_sky || group.indices.empty()) { continue; }

      const bool has_texture = group.texture >= 0 &&
                               static_cast<std::size_t>(group.texture) < level.textures.size() &&
                               level.textures[group.texture].has_value();
      const std::string name = has_texture ? level.textures[group.texture]->name : "missing-" + std::to_string(group.texture);
      if (is_unseen(name)) { continue; }

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

      const Entity entity = world.CreateEntity(name, parent);
      world.AddComponent(entity, "Transform");
      world.AddComponent(entity, "Renderable");
      world.SetText(entity, shader_field, "assets://shaders/unlit");

      if (const std::string &picture = FindPicture(world, data, level, group.texture); !picture.empty())
      {
        world.SetTexts(entity, textures_field, {picture});
      }

      if (world.SetMesh(entity, corners, indices)) { triangles += indices.size() / 3; }
    }

    return true;
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

    // the pictures of a level that was shown before are not those of this one
    _pictures.clear();

    const Entity root = world.CreateEntity("level");
    world.AddComponent(root, "Transform");

    std::size_t triangles = 0;

    // the first model of a level is the level itself
    if (std::string problem; !ShowModel(world, data, level, 0, root, triangles, problem))
    {
      error = "no mesh can be made of " + map + ": " + problem;
      return false;
    }

    EntityText text;
    if (std::string problem; !text.Read(level.entities, problem))
    {
      world.Warn("The entities of " + map + " cannot be read: " + problem);
      world.Info("Showing " + map + ": " + std::to_string(_pictures.size()) + " textures, " +
        std::to_string(triangles) + " triangles, no models");
      return true;
    }

    // The other models are its doors, lifts, and buttons, each named by the
    // entity it belongs to. They stand where the level has them at rest.
    const NeonField position_field = world.FindField("Transform", "position");
    std::size_t models = 0;
    std::size_t triggers = 0;

    for (const BspEntity &entity : text.entities)
    {
      const std::size_t model = read_model_number(entity);
      if (model == 0) { continue; }

      const std::string *written_kind = entity.Find("classname");
      const std::string kind = written_kind != nullptr ? *written_kind : "model";

      // a trigger is a volume that something happens in, and is not drawn
      if (kind.starts_with("trigger_"))
      {
        triggers++;
        continue;
      }

      const std::string name = kind + " *" + std::to_string(model);
      if (model >= level.models.size())
      {
        world.Warn("The level " + map + " has no model for " + name + ", it has " + std::to_string(level.models.size()));
        continue;
      }

      const Entity part = world.CreateEntity(name, root);
      world.AddComponent(part, "Transform");
      if (BspVector origin; read_vector(entity, "origin", origin))
      {
        const BspVector place = QuakeSpace::ToEnginePosition(origin);
        world.SetVector3(part, position_field, {place.x, place.y, place.z});
      }

      const std::size_t triangles_before = triangles;
      if (std::string problem; !ShowModel(world, data, level, model, part, triangles, problem))
      {
        world.Warn("No mesh can be made of " + name + " of " + map + ": " + problem);
        continue;
      }
      // one with nothing to see, as a door that only stops the player, is
      // there to be moved and is not counted
      if (triangles > triangles_before) { models++; }
    }

    FindStart(world, text, map);

    world.Info("Showing " + map + ": " + std::to_string(_pictures.size()) + " textures, " +
      std::to_string(triangles) + " triangles, " + std::to_string(models) + " models, " +
      std::to_string(triggers) + " left out as triggers");
    return true;
  }
} // quake
