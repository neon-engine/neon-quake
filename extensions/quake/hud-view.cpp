#include "hud-view.hpp"

#include <cmath>
#include <numbers>
#include <span>

#include "formats/picture-reader.hpp"
#include "formats/picture.hpp"

namespace quake
{
  using neon::extension::Entity;
  using neon::extension::Vertex;
  using neon::extension::World;

  const HudView::Known &HudView::Find(const World &world, const GameData &data, const std::string_view name)
  {
    if (const auto known = _pictures.find(name); known != _pictures.end()) { return known->second; }

    Known &made = _pictures[std::string(name)];

    if (!_was_read)
    {
      _was_read = true;
      std::string problem;
      _has_wad = _wad.Read(data.Find("gfx.wad"), problem);
      if (!_has_wad) { world.Warn("The pictures of the status bar cannot be read: " + problem); }
    }

    // a picture of the `gfx/` folder is a file of its own; any other is in
    // the archive of the status bar
    Picture picture;
    std::string problem;
    const bool is_file = name.starts_with("gfx/");
    const bool was_read = is_file
                            ? read_picture_file(name, data.Find(name), picture, problem)
                            : _has_wad && _wad.ReadPicture(name, picture, problem);
    if (!was_read)
    {
      world.Warn("The picture " + std::string(name) + " is not shown: " + problem);
      return made;
    }

    // The letters are see-through where they have the first colour of the
    // palette, every other picture where it has the last.
    std::vector<std::uint8_t> pixels;
    if (name == HudPicture::characters_name)
    {
      std::vector<std::uint8_t> holes = picture.pixels;
      for (std::uint8_t &pixel : holes) { if (pixel == 0) { pixel = 255; } }
      pixels = data.GetPalette().ToRgba(holes, true);
    } else
    {
      pixels = data.GetPalette().ToRgba(picture.pixels, true);
    }

    made.path = world.SetImage(
      "hud/" + std::string(name),
      static_cast<std::uint32_t>(picture.width),
      static_cast<std::uint32_t>(picture.height),
      pixels);
    made.width = picture.width;
    made.height = picture.height;
    return made;
  }

  std::int32_t HudView::GetWidth(const World &world, const GameData &data, const std::string_view name)
  {
    return Find(world, data, name).width;
  }

  void HudView::Show(
    const World &world,
    const GameData &data,
    const Entity camera,
    const std::vector<HudPicture> &pictures)
  {
    if (camera == 0) { return; }
    if (camera == _camera && pictures == _shown) { return; }

    // another camera: what stood in front of the one before went with it
    if (camera != _camera)
    {
      _entities.clear();
      _camera = camera;
    }
    _shown = pictures;

    // how large a pixel of the original is on the plane, in metres
    const float half_height = distance * std::tan(field_of_view * 0.5f * std::numbers::pi_v<float> / 180.0f);
    const float pixel = 2.0f * half_height / screen_height;

    // the squares, by the picture they show
    struct Mesh
    {
      std::vector<Vertex> corners;
      std::vector<std::uint32_t> indices;
    };
    std::map<std::string, Mesh, std::less<>> meshes;
    for (auto &[name, entity] : _entities) { meshes[name]; }

    // a little nearer for each that is drawn later, so that what is drawn
    // last is in front
    float nearer = 0.0f;
    for (const HudPicture &command : pictures)
    {
      const bool is_letter = command.kind == HudPictureKind::Character;
      const std::string_view name = is_letter ? HudPicture::characters_name : command.name;
      const Known &known = Find(world, data, name);
      if (known.path.empty()) { continue; }

      const float width = static_cast<float>(is_letter ? HudPicture::character_size : known.width);
      const float height = static_cast<float>(is_letter ? HudPicture::character_size : known.height);

      // from the screen of the original, with its origin at the top left, to
      // the plane, with its origin in the middle and up as more
      const float left = (static_cast<float>(command.x) - 160.0f) * pixel;
      const float top = command.anchor == HudAnchor::Bottom
                          ? -half_height + (200.0f - static_cast<float>(command.y)) * pixel
                          : (100.0f - static_cast<float>(command.y)) * pixel;
      const float right = left + width * pixel;
      const float bottom = top - height * pixel;

      // a letter is one of 16 by 16 in its picture
      float u0 = 0.0f;
      float v0 = 0.0f;
      float u1 = 1.0f;
      float v1 = 1.0f;
      if (is_letter)
      {
        const std::int32_t letter = command.character & 255;
        u0 = static_cast<float>(letter % 16) / 16.0f;
        v0 = static_cast<float>(letter / 16) / 16.0f;
        u1 = u0 + 1.0f / 16.0f;
        v1 = v0 + 1.0f / 16.0f;
      }

      Mesh &mesh = meshes[std::string(name)];
      const auto first = static_cast<std::uint32_t>(mesh.corners.size());
      const float depth = -distance + nearer;
      nearer += 0.000002f;
      mesh.corners.push_back({{left, top, depth}, {0.0f, 0.0f, 1.0f}, {u0, v0}, {1.0f, 1.0f, 1.0f, 1.0f}});
      mesh.corners.push_back({{left, bottom, depth}, {0.0f, 0.0f, 1.0f}, {u0, v1}, {1.0f, 1.0f, 1.0f, 1.0f}});
      mesh.corners.push_back({{right, bottom, depth}, {0.0f, 0.0f, 1.0f}, {u1, v1}, {1.0f, 1.0f, 1.0f, 1.0f}});
      mesh.corners.push_back({{right, top, depth}, {0.0f, 0.0f, 1.0f}, {u1, v0}, {1.0f, 1.0f, 1.0f, 1.0f}});
      for (const std::uint32_t corner : {0u, 1u, 2u, 0u, 2u, 3u}) { mesh.indices.push_back(first + corner); }
    }

    for (auto &[name, mesh] : meshes)
    {
      Entity &entity = _entities[name];
      if (entity == 0)
      {
        if (mesh.corners.empty()) { continue; }

        entity = world.CreateEntity("hud " + name, camera);
        world.AddComponent(entity, "Transform");
        world.AddComponent(entity, "Renderable");
        world.SetText(entity, world.FindField("Renderable", "shader"), "assets://shaders/unlit");
        world.SetText(entity, world.FindField("Renderable", "material.alpha_mode"), "blend");
        world.SetTexts(entity, world.FindField("Renderable", "textures"), {Find(world, data, name).path});

        // What is seen through is drawn from the farthest to the nearest, so
        // the bars stand behind what is drawn on them, and the letters in
        // front of everything.
        const bool is_bar = name == "sbar" || name == "ibar" || name == "scorebar";
        const float forward = is_bar ? 0.0f : name == HudPicture::characters_name ? 2.0f * layer_step : layer_step;
        world.SetVector3(entity, world.FindField("Transform", "position"), {0.0f, 0.0f, forward});
      }

      if (mesh.corners.empty())
      {
        // a picture that is not shown for now: a speck behind the camera
        mesh.corners = {
          {{0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}},
          {{0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}},
          {{0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}},
        };
        mesh.indices = {0, 1, 2};
      }
      world.SetMesh(entity, mesh.corners, mesh.indices);
    }
  }
} // quake
