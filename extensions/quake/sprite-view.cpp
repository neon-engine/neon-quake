#include "sprite-view.hpp"

#include <cmath>
#include <span>
#include <vector>

#include "formats/quake-space.hpp"

namespace quake
{
  using neon::extension::Entity;
  using neon::extension::Vertex;
  using neon::extension::World;

  SpriteView::Sprite *SpriteView::Find(const World &world, const GameData &data, const std::string &name)
  {
    if (const auto known = _sprites.find(name); known != _sprites.end()) { return known->second.get(); }

    std::unique_ptr<Sprite> &kept = _sprites[name];

    const std::span<const std::uint8_t> bytes = data.Find(name);
    if (bytes.empty())
    {
      world.Warn("The data of the game holds no sprite " + name);
      return nullptr;
    }

    auto sprite = std::make_unique<Sprite>();
    sprite->name = name;
    if (std::string problem; !sprite->file.Read(bytes, problem))
    {
      world.Warn("The sprite " + name + " cannot be read: " + problem);
      return nullptr;
    }

    kept = std::move(sprite);
    return kept.get();
  }

  std::size_t SpriteView::ChoosePicture(const Sprite &sprite, const std::size_t frame) const
  {
    const SpriteFrame &source = sprite.file.GetFrames()[frame];
    if (!source.is_group || source.times.empty() || source.times.back() <= 0.0f) { return 0; }

    // the times say when each picture ends, counted from the start of the group
    const auto at = static_cast<float>(std::fmod(_time, static_cast<double>(source.times.back())));
    for (std::size_t picture = 0; picture < source.times.size(); picture++)
    {
      if (at < source.times[picture]) { return picture; }
    }
    return 0;
  }

  bool SpriteView::ShowPicture(const World &world, const GameData &data, const Entity entity, const Shown &shown)
  {
    const SpritePicture *picture = shown.sprite->file.FindPicture(shown.frame, shown.picture);
    if (picture == nullptr || picture->width <= 0 || picture->height <= 0) { return false; }

    // the picture, with the colour that stands for nothing seen through
    std::string &path = shown.sprite->paths[{shown.frame, shown.picture}];
    if (path.empty())
    {
      path = world.SetImage(
        shown.sprite->name + "/" + std::to_string(shown.frame) + "-" + std::to_string(shown.picture),
        static_cast<std::uint32_t>(picture->width),
        static_cast<std::uint32_t>(picture->height),
        data.GetPalette().ToRgba(picture->pixels, true));
      if (path.empty()) { return false; }
    }
    world.SetTexts(entity, world.FindField("Renderable", "textures"), {path});

    // A square that stands in the plane of x and y and looks along z, which
    // is where a camera that was not turned looks from. A picture says how
    // far left of the origin it starts and how far above it.
    const float left = static_cast<float>(picture->left) * QuakeSpace::metres_per_unit;
    const float right = static_cast<float>(picture->left + picture->width) * QuakeSpace::metres_per_unit;
    const float top = static_cast<float>(picture->up) * QuakeSpace::metres_per_unit;
    const float bottom = static_cast<float>(picture->up - picture->height) * QuakeSpace::metres_per_unit;

    const std::vector<Vertex> corners = {
      {{left, top, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
      {{left, bottom, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
      {{right, bottom, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
      {{right, top, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
    };
    // seen from both sides, since one laid a way of its own may be looked at
    // from behind
    return world.SetMesh(entity, corners, {0, 1, 2, 0, 2, 3, 0, 2, 1, 0, 3, 2});
  }

  bool SpriteView::Show(
    const World &world,
    const GameData &data,
    const Entity entity,
    const std::string &name,
    const std::int32_t frame)
  {
    Sprite *sprite = Find(world, data, name);
    if (sprite == nullptr || sprite->file.GetFrames().empty()) { return false; }

    Shown wanted;
    wanted.sprite = sprite;
    wanted.frame = frame >= 0 && static_cast<std::size_t>(frame) < sprite->file.GetFrames().size() ? frame : 0;
    wanted.picture = ChoosePicture(*sprite, wanted.frame);

    const auto known = _shown.find(entity);
    if (known == _shown.end())
    {
      world.AddComponent(entity, "Transform");
      world.AddComponent(entity, "Renderable");
      world.SetText(entity, world.FindField("Renderable", "shader"), "assets://shaders/unlit");
      // what the picture leaves out is seen through
      world.SetText(entity, world.FindField("Renderable", "material.alpha_mode"), "blend");
    } else if (known->second.sprite == sprite && known->second.frame == wanted.frame &&
               known->second.picture == wanted.picture)
    {
      return true;
    }

    if (!ShowPicture(world, data, entity, wanted)) { return false; }
    _shown[entity] = wanted;
    return true;
  }

  void SpriteView::Face(const World &world, const float pitch, const float yaw) const
  {
    const NeonField rotation_field = world.FindField("Transform", "rotation");
    for (const auto &[entity, shown] : _shown)
    {
      switch (shown.sprite->file.GetHeader().orientation)
      {
        case SpriteOrientation::Oriented:
          // laid as its entity is turned
          break;
        case SpriteOrientation::ParallelUpright:
        case SpriteOrientation::FacingUpright:
          world.SetVector3(entity, rotation_field, {0.0f, yaw, 0.0f});
          break;
        case SpriteOrientation::Parallel:
        case SpriteOrientation::ParallelOriented:
        default:
          world.SetVector3(entity, rotation_field, {pitch, yaw, 0.0f});
          break;
      }
    }
  }

  void SpriteView::Forget(const Entity entity)
  {
    _shown.erase(entity);
  }

  void SpriteView::Clear()
  {
    _shown.clear();
  }

  void SpriteView::Update(const World &world, const GameData &data, const double time)
  {
    _time = time;

    for (auto &[entity, shown] : _shown)
    {
      if (!shown.sprite->file.GetFrames()[shown.frame].is_group) { continue; }

      if (const std::size_t picture = ChoosePicture(*shown.sprite, shown.frame); picture != shown.picture)
      {
        shown.picture = picture;
        ShowPicture(world, data, entity, shown);
      }
    }
  }
} // quake
