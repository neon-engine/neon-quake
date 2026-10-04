#include "level-view.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <utility>
#include <vector>

#include "formats/bsp-mesh.hpp"
#include "formats/lightmap-atlas.hpp"
#include "formats/quake-space.hpp"

namespace quake
{
  // Helpers of LevelView, for this file alone.
  namespace
  {
    using neon::extension::Entity;
    using neon::extension::Vertex;
    using neon::extension::World;

    // What the light of a level is multiplied by. The game doubles its
    // light, so that a sample of 128 shows a texture as it is and one of
    // 255 twice as bright, and it does so with the numbers a screen is
    // given. The engine multiplies light itself, where twice as much on a
    // screen is two to the power of 2.2 as much: 4.59. On top of that the
    // ports of today show the game with more contrast than the original,
    // 1.4 times on the screen in vkQuake, which is 2.1 times as much light.
    constexpr double light_strength = 4.59 * 2.1;

    // the same contrast for the light of a model
    constexpr float model_contrast = 2.1f;

    // How far the middle of the body of a player is above where the level
    // puts it. A level names the place of a box that reaches 24 units down
    // and 32 up, whose middle is therefore 4 above it; the body of the
    // scene is placed by its middle.
    constexpr float middle_height = 4.0f;

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

    origin.z += middle_height;
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

  void LevelView::PlacePlayer(const World &world)
  {
    if (!_has_start || _player_placed) { return; }

    // the player of the scene, or else a camera that flies
    Entity placed = world.FindEntity("player");
    if (placed == 0) { placed = world.FindEntity("camera"); }
    if (placed == 0) { return; }

    world.SetVector3(placed, world.FindField("Transform", "position"), _start_position);
    // pitch, yaw, and roll in degrees
    world.SetVector3(placed, world.FindField("Transform", "rotation"), {0.0f, _start_yaw, 0.0f});
    _player_placed = true;
  }

  const std::string &LevelView::FindPicture(
    const World &world,
    const GameData &data,
    const BspFile &level,
    const std::string &item,
    const std::int32_t texture)
  {
    const bool is_item = !item.empty();
    if (is_item)
    {
      if (const auto known = _item_pictures.find({item, texture}); known != _item_pictures.end()) { return known->second; }
    } else if (const auto known = _pictures.find(texture); known != _pictures.end()) { return known->second; }

    std::string path;
    if (texture >= 0 && static_cast<std::size_t>(texture) < level.textures.size() && level.textures[texture].has_value())
    {
      const MipTexture &source = *level.textures[texture];
      const std::vector<std::uint8_t> pixels = data.GetPalette().ToRgba(source.pixels[0], source.HasHoles());
      // an item carries textures of its own, which may be named as those
      // of the level are
      const std::string name = is_item ? item + "/textures/" + source.name : "textures/" + source.name;
      path = world.SetImage(name, source.width, source.height, pixels);
    }
    if (is_item) { return _item_pictures.emplace(std::pair(item, texture), std::move(path)).first->second; }
    return _pictures.emplace(texture, std::move(path)).first->second;
  }

  bool LevelView::ShowModel(
    const World &world,
    const GameData &data,
    const BspFile &level,
    const std::string &item,
    const std::size_t model,
    const Entity parent,
    std::size_t &triangles,
    std::string &error)
  {
    BspMesh mesh;
    if (!mesh.Build(level, model, error)) { return false; }

    const bool is_item = !item.empty();
    const std::string &file_name = is_item ? item : _map;

    const NeonField shader_field = world.FindField("Renderable", "shader");
    const NeonField textures_field = world.FindField("Renderable", "textures");
    const NeonField body_kind_field = world.FindField("RigidBody", "kind");
    const NeonField collider_shape_field = world.FindField("Collider", "shape");
    const NeonField lightmap_field = world.FindField("Renderable", "material.lightmap");
    const NeonField strength_field = world.FindField("Renderable", "material.lightmap_strength");

    // The light the level carries for this model, packed into pictures. A
    // level without any is shown as its textures are.
    LightmapAtlas atlas;
    std::vector<std::string> lightmaps;
    if (!level.lighting.empty())
    {
      // an item is lit without colours: the data has none for the small levels
      const LitFile *lit = !is_item && _has_lit ? &_lit : nullptr;
      if (std::string problem; !atlas.Build(mesh, problem, LightmapAtlas::default_largest_side, lit))
      {
        world.Warn("The light of model " + std::to_string(model) + " of " + file_name + " cannot be packed: " + problem);
      } else
      {
        // A face the level gave no light is dark in a level that has light,
        // and not as bright as can be, which is what the atlas makes of it
        // for the sake of what is drawn without light. Liquids are drawn
        // without the lightmap below, so its bright spot is put out here.
        for (std::size_t face = 0; face < atlas.blocks.size(); face++)
        {
          const LightmapAtlasBlock &block = atlas.blocks[face];
          if (block.is_lit || block.page >= atlas.pages.size()) { continue; }

          LightmapAtlasPage &page = atlas.pages[block.page];
          for (std::uint32_t y = block.y > 0 ? block.y - 1 : 0; y <= block.y + 1 && y < page.height; y++)
          {
            for (std::uint32_t x = block.x > 0 ? block.x - 1 : 0; x <= block.x + 1 && x < page.width; x++)
            {
              std::uint8_t *pixel = &page.pixels[(static_cast<std::size_t>(y) * page.width + x) * 4];
              pixel[0] = pixel[1] = pixel[2] = 0;
            }
          }
        }

        // an item is shown many times and its light is one picture for all
        const auto known_light = is_item ? _item_lightmaps.find(item) : _item_lightmaps.end();
        if (known_light != _item_lightmaps.end()) { lightmaps = known_light->second; }

        for (std::size_t page = 0; lightmaps.size() < atlas.pages.size(); page++)
        {
          lightmaps.push_back(world.SetImage(
            "lightmaps/" + file_name + "/" + std::to_string(model) + "/" + std::to_string(page),
            atlas.pages[page].width,
            atlas.pages[page].height,
            atlas.pages[page].pixels));
        }
        if (is_item) { _item_lightmaps[item] = lightmaps; }
      }
    }
    const bool is_lit = !lightmaps.empty();

    // one entity for each texture, with the faces that show it; and one for
    // each page of the light those faces lie on, which is one but for a
    // level larger than any here
    for (const BspMeshGroup &group : mesh.groups)
    {
      // the sky is no wall with a picture on it
      if (group.is_sky || group.indices.empty()) { continue; }

      const bool has_texture = group.texture >= 0 &&
                               static_cast<std::size_t>(group.texture) < level.textures.size() &&
                               level.textures[group.texture].has_value();
      const std::string name = has_texture ? level.textures[group.texture]->name : "missing-" + std::to_string(group.texture);
      if (is_unseen(name)) { continue; }

      // a liquid is shown as it is, and glows in the dark as it does in the
      // game
      const bool group_is_lit = is_lit && !group.is_liquid;
      const std::size_t page_count = group_is_lit ? lightmaps.size() : 1;

      for (std::size_t page = 0; page < page_count; page++)
      {
        // The corners the group uses, handed over once each. A corner belongs
        // to one face and a face to one texture, so the groups share none.
        std::vector<Vertex> corners;
        std::vector<neon::extension::Vector2> light_places;
        std::vector<std::uint32_t> indices;
        std::vector<std::uint32_t> place_of(mesh.vertices.size(), UINT32_MAX);

        for (const std::uint32_t index : group.indices)
        {
          // a face lies on one page of the light, with all its corners
          if (group_is_lit && atlas.vertices[index].page != page) { continue; }

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
            if (group_is_lit) { light_places.push_back({atlas.vertices[index].u, atlas.vertices[index].v}); }
          }
          indices.push_back(place_of[index]);
        }
        if (indices.empty()) { continue; }

        // the game winds clockwise seen from outside, the engine the other way
        for (std::size_t i = 0; i + 2 < indices.size(); i += 3) { std::swap(indices[i + 1], indices[i + 2]); }

        // an entity is known by its name under its parent, so a second page
        // of the same texture says which it is
        const Entity entity = world.CreateEntity(page == 0 ? name : name + " " + std::to_string(page), parent);
        world.AddComponent(entity, "Transform");
        world.AddComponent(entity, "Renderable");
        world.SetText(entity, shader_field, "assets://shaders/unlit");

        if (const std::string &picture = FindPicture(world, data, level, item, group.texture); !picture.empty())
        {
          world.SetTexts(entity, textures_field, {picture});
        }

        // a texture with holes, a fence or the letters of a sign, is seen
        // through where it has them
        if (has_texture && level.textures[group.texture]->HasHoles())
        {
          world.SetText(entity, world.FindField("Renderable", "material.alpha_mode"), "blend");
        }

        if (!world.SetMesh(entity, corners, indices)) { continue; }
        triangles += indices.size() / 3;

        // What is seen is what is walked on and into: the faces stand still
        // and collide as the mesh they are. A liquid is waded through, and
        // an item walked through.
        if (!group.is_liquid && !is_item)
        {
          world.AddComponent(entity, "RigidBody");
          // the level itself never moves; a door or a lift is moved by the
          // game, and pushes what is in its way
          world.SetText(entity, body_kind_field, model == 0 ? "static" : "kinematic");
          world.AddComponent(entity, "Collider");
          world.SetText(entity, collider_shape_field, "mesh");
        }

        if (group_is_lit && !lightmaps[page].empty())
        {
          world.SetMeshLightmap(entity, light_places);
          world.SetText(entity, lightmap_field, lightmaps[page]);
          world.SetNumber(entity, strength_field, light_strength);
        }
      }
    }

    return true;
  }

  void LevelView::Clear(const World &world)
  {
    if (_root != 0) { world.DestroyEntity(_root); }
    _root = 0;

    // the pictures of a level that was shown before are not those of the next
    _pictures.clear();
    _parts.clear();
    _has_start = false;
    _player_placed = false;
    _has_lit = false;
    _has_light = false;
  }

  std::array<float, 3> LevelView::FindLight(const BspVector &place, const float least) const
  {
    if (!_has_light) { return {1.0f, 1.0f, 1.0f}; }

    const BspLightSample sample = _light.Sample(place, {}, BspLightFilter::Bilinear);

    // The original shows a model with its colours times its light over 200,
    // counted as a screen is given them, and the ports of today twice as
    // bright, as vkQuake does unless told otherwise: over 100. The engine
    // multiplies light itself, so the factor is taken to the power of 2.2,
    // and the level is shown with more contrast, as its walls are.
    std::array<float, 3> light{};
    const float samples[3] = {sample.red, sample.green, sample.blue};
    for (std::size_t i = 0; i < 3; i++)
    {
      const float on_screen = std::clamp(std::max(samples[i], least), 0.0f, 255.0f) / 100.0f;
      light[i] = std::pow(on_screen, 2.2f) * model_contrast;
    }
    return light;
  }

  Entity LevelView::FindPart(const std::size_t model) const
  {
    const auto found = _parts.find(model);
    return found != _parts.end() ? found->second : 0;
  }

  bool LevelView::ShowItem(const World &world, const GameData &data, const std::string &name, const Entity parent)
  {
    auto known = _items.find(name);
    if (known == _items.end())
    {
      known = _items.emplace(name, nullptr).first;

      const std::span<const std::uint8_t> bytes = data.Find(name);
      auto file = std::make_unique<BspFile>();
      if (bytes.empty())
      {
        world.Warn("The data of the game holds no " + name);
      } else if (std::string problem; !file->Read(bytes, problem))
      {
        world.Warn(name + " cannot be read: " + problem);
      } else
      {
        known->second = std::move(file);
      }
    }
    if (known->second == nullptr) { return false; }

    std::size_t triangles = 0;
    if (std::string problem; !ShowModel(world, data, *known->second, name, 0, parent, triangles, problem))
    {
      world.Warn("No mesh can be made of " + name + ": " + problem);
      return false;
    }
    return triangles > 0;
  }

  bool LevelView::Show(const World &world, const GameData &data, const std::string &map, std::string &error)
  {
    _map = map;

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

    Clear(world);

    // The colours of the light, when the level comes with them. Without, or
    // with colours that are not of this level, the light is white.
    std::string lit_name = map;
    if (lit_name.ends_with(".bsp")) { lit_name.replace(lit_name.size() - 4, 4, ".lit"); }
    if (const std::span<const std::uint8_t> colours = data.Find(lit_name); !colours.empty())
    {
      std::string problem;
      _has_lit = _lit.Read(colours, level.lighting.size(), problem);
      if (!_has_lit) { world.Warn("The colours of the light of " + map + " are left out: " + problem); }
    }

    if (std::string problem; !_light.Build(level, problem, _has_lit ? std::span<const std::uint8_t>(_lit.GetColours()) : std::span<const std::uint8_t>()))
    {
      world.Warn("Models in " + map + " are shown as bright as their skins: " + problem);
    } else
    {
      _has_light = true;
    }

    const Entity root = world.CreateEntity("level");
    world.AddComponent(root, "Transform");
    _root = root;

    std::size_t triangles = 0;

    // the first model of a level is the level itself
    if (std::string problem; !ShowModel(world, data, level, "", 0, root, triangles, problem))
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
      _parts[model] = part;
      if (BspVector origin; read_vector(entity, "origin", origin))
      {
        const BspVector place = QuakeSpace::ToEnginePosition(origin);
        world.SetVector3(part, position_field, {place.x, place.y, place.z});
      }

      const std::size_t triangles_before = triangles;
      if (std::string problem; !ShowModel(world, data, level, "", model, part, triangles, problem))
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
