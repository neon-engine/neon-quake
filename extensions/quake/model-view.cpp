#include "model-view.hpp"

#include <cmath>
#include <span>
#include <utility>

#include "formats/quake-space.hpp"
#include "game-shaders.hpp"

namespace quake
{
  using neon::extension::Entity;
  using neon::extension::Vertex;
  using neon::extension::World;

  ModelView::Model *ModelView::Find(const World &world, const GameData &data, const std::string &name)
  {
    if (const auto known = _models.find(name); known != _models.end()) { return known->second.get(); }

    std::unique_ptr<Model> &kept = _models[name];

    const std::span<const std::uint8_t> bytes = data.Find(name);
    if (bytes.empty())
    {
      world.Warn("The data of the game holds no model " + name);
      return nullptr;
    }

    auto model = std::make_unique<Model>();
    if (std::string problem; !model->file.Read(bytes, problem))
    {
      world.Warn("The model " + name + " cannot be read: " + problem);
      return nullptr;
    }

    model->mesh = MdlMesh(model->file);

    // the game winds clockwise seen from outside, the engine the other way
    model->indices = model->mesh.GetIndices();
    for (std::size_t i = 0; i + 2 < model->indices.size(); i += 3) { std::swap(model->indices[i + 1], model->indices[i + 2]); }

    // A skin that is a group changes its picture with time. The first
    // picture is shown until that is done.
    const MdlHeader &header = model->file.GetHeader();
    for (std::size_t skin = 0; skin < model->file.GetSkins().size(); skin++)
    {
      const MdlSkin &source = model->file.GetSkins()[skin];
      std::string path;
      if (!source.pictures.empty())
      {
        path = world.SetImage(
          name + "/skin-" + std::to_string(skin),
          static_cast<std::uint32_t>(header.skin_width),
          static_cast<std::uint32_t>(header.skin_height),
          data.GetPalette().ToRgba(source.pictures[0]));
      }
      model->skins.push_back(std::move(path));

      // the last colours of the palette glow: the eyes of a monster, a flame
      std::string glow;
      if (!source.pictures.empty())
      {
        const std::vector<std::uint8_t> pixels = data.GetPalette().ToGlowRgba(source.pictures[0]);
        if (!pixels.empty())
        {
          glow = world.SetImage(
            name + "/glow-" + std::to_string(skin),
            static_cast<std::uint32_t>(header.skin_width),
            static_cast<std::uint32_t>(header.skin_height),
            pixels);
        }
      }
      if (glow.empty())
      {
        // An entity may show one model after another, so a skin without a
        // glow takes away the glow of the one before.
        if (_no_glow.empty()) { _no_glow = world.SetImage("models/no-glow", 1, 1, {0, 0, 0, 0}); }
        glow = _no_glow;
      }
      model->glows.push_back(std::move(glow));
    }

    kept = std::move(model);
    return kept.get();
  }

  std::size_t ModelView::ChoosePose(const Model &model, const std::size_t frame) const
  {
    const MdlFrame &source = model.file.GetFrames()[frame];
    if (!source.is_group || source.times.empty() || source.times.back() <= 0.0f) { return 0; }

    // the times say when each pose ends, counted from the start of the group
    const auto at = static_cast<float>(std::fmod(_time, static_cast<double>(source.times.back())));
    for (std::size_t pose = 0; pose < source.times.size(); pose++)
    {
      if (at < source.times[pose]) { return pose; }
    }
    return 0;
  }

  const std::vector<Vertex> *ModelView::FindCorners(Model &model, const std::size_t frame, const std::size_t pose)
  {
    if (const auto known = model.poses.find({frame, pose}); known != model.poses.end()) { return &known->second; }

    const MdlPose *source = model.file.FindPose(frame, pose);
    if (source == nullptr) { return nullptr; }

    std::vector<Vertex> corners;
    for (const MdlMeshVertex &from : model.mesh.MakeVertices(*source))
    {
      const BspVector place = QuakeSpace::ToEnginePosition({from.position.x, from.position.y, from.position.z});
      const BspVector normal = QuakeSpace::ToEngineDirection({from.normal.x, from.normal.y, from.normal.z});
      corners.push_back({
        {place.x, place.y, place.z},
        {normal.x, normal.y, normal.z},
        {from.u, from.v},
        {1.0f, 1.0f, 1.0f, 1.0f},
      });
    }
    if (corners.empty()) { return nullptr; }

    return &model.poses.emplace(std::pair(frame, pose), std::move(corners)).first->second;
  }

  std::vector<Vertex> ModelView::MakeCorners(Shown &shown) const
  {
    const std::vector<Vertex> *to = FindCorners(*shown.model, shown.frame, shown.pose);
    if (to == nullptr) { return {}; }

    std::vector<Vertex> corners = *to;

    // how far the pose before has become this one
    const double done = shown.is_blending ? (_time - shown.changed_at) / blend_time : 1.0;
    if (done >= 1.0 || shown.from.size() != corners.size())
    {
      shown.is_blending = false;
    } else
    {
      const auto part = static_cast<float>(done < 0.0 ? 0.0 : done);
      for (std::size_t i = 0; i < corners.size(); i++)
      {
        Vertex &corner = corners[i];
        const Vertex &from = shown.from[i];
        corner.position.x = from.position.x + (corner.position.x - from.position.x) * part;
        corner.position.y = from.position.y + (corner.position.y - from.position.y) * part;
        corner.position.z = from.position.z + (corner.position.z - from.position.z) * part;
        // not of length one in between, and near enough for what is unlit
        corner.normal.x = from.normal.x + (corner.normal.x - from.normal.x) * part;
        corner.normal.y = from.normal.y + (corner.normal.y - from.normal.y) * part;
        corner.normal.z = from.normal.z + (corner.normal.z - from.normal.z) * part;
      }
    }

    for (Vertex &corner : corners) { corner.color = {shown.light[0], shown.light[1], shown.light[2], 1.0f}; }
    return corners;
  }

  bool ModelView::ShowPose(const World &world, const Entity entity, Shown &shown) const
  {
    const std::vector<Vertex> corners = MakeCorners(shown);
    if (corners.empty()) { return false; }

    return world.SetMesh(entity, corners, shown.model->indices);
  }

  bool ModelView::Show(
    const World &world,
    const GameData &data,
    const Entity entity,
    const std::string &name,
    const std::int32_t frame,
    const std::int32_t skin,
    const std::array<float, 3> &light,
    const bool blend)
  {
    Model *model = Find(world, data, name);
    if (model == nullptr || model->file.GetFrames().empty()) { return false; }

    Shown wanted;
    wanted.model = model;
    wanted.frame = frame >= 0 && static_cast<std::size_t>(frame) < model->file.GetFrames().size() ? frame : 0;
    wanted.skin = skin >= 0 && static_cast<std::size_t>(skin) < model->skins.size() ? skin : 0;
    wanted.pose = ChoosePose(*model, wanted.frame);
    wanted.light = light;

    const auto known = _shown.find(entity);
    const bool is_new = known == _shown.end();
    if (is_new)
    {
      world.AddComponent(entity, "Transform");
      world.AddComponent(entity, "Renderable");
      world.SetText(entity, world.FindField("Renderable", "shader"), GameShaders::surface);
    }

    if (is_new || known->second.model != model || known->second.skin != wanted.skin)
    {
      if (!model->skins.empty() && !model->skins[wanted.skin].empty())
      {
        world.SetTexts(entity, world.FindField("Renderable", "textures"), {model->skins[wanted.skin]});
        world.SetText(entity, world.FindField("Renderable", "material.emissive_texture"), model->glows[wanted.skin]);
      }
    }

    const bool same_model = !is_new && known->second.model == model;
    const bool same_pose = same_model && known->second.frame == wanted.frame && known->second.pose == wanted.pose;
    if (same_pose && known->second.light == wanted.light)
    {
      known->second.skin = wanted.skin;
      return true;
    }

    if (same_pose)
    {
      // only the light changed: the blend that is under way goes on
      wanted.from = std::move(known->second.from);
      wanted.changed_at = known->second.changed_at;
      wanted.is_blending = known->second.is_blending;
    } else if (same_model && blend)
    {
      // from what the entity shows now, which may lie between two poses
      wanted.from = MakeCorners(known->second);
      wanted.changed_at = _time;
      wanted.is_blending = !wanted.from.empty();
    }
    if (!ShowPose(world, entity, wanted)) { return false; }

    _shown[entity] = std::move(wanted);
    return true;
  }

  std::uint32_t ModelView::GetFlags(const Entity entity) const
  {
    const auto known = _shown.find(entity);
    return known != _shown.end() ? known->second.model->file.GetHeader().flags : 0;
  }

  void ModelView::Forget(const Entity entity)
  {
    _shown.erase(entity);
  }

  void ModelView::Clear()
  {
    _shown.clear();
  }

  void ModelView::Update(const World &world, const double time)
  {
    _time = time;

    for (auto &[entity, shown] : _shown)
    {
      if (shown.model->file.GetFrames()[shown.frame].is_group)
      {
        // The poses of a group follow each other closely as they are: a
        // flame flickers, and does not glide.
        if (const std::size_t pose = ChoosePose(*shown.model, shown.frame); pose != shown.pose)
        {
          shown.pose = pose;
          shown.is_blending = false;
          ShowPose(world, entity, shown);
        }
        continue;
      }

      if (shown.is_blending) { ShowPose(world, entity, shown); }
    }
  }
} // quake
