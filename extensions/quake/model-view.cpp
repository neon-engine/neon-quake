#include "model-view.hpp"

#include <cmath>
#include <span>
#include <utility>

#include "formats/quake-space.hpp"

namespace quake
{
  using neon::extension::Entity;
  using neon::extension::Vertex;
  using neon::extension::World;

  const ModelView::Model *ModelView::Find(const World &world, const GameData &data, const std::string &name)
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

  bool ModelView::ShowPose(const World &world, const Entity entity, const Shown &shown)
  {
    const MdlPose *pose = shown.model->file.FindPose(shown.frame, shown.pose);
    if (pose == nullptr) { return false; }

    std::vector<Vertex> corners;
    for (const MdlMeshVertex &from : shown.model->mesh.MakeVertices(*pose))
    {
      const BspVector place = QuakeSpace::ToEnginePosition({from.position.x, from.position.y, from.position.z});
      const BspVector normal = QuakeSpace::ToEngineDirection({from.normal.x, from.normal.y, from.normal.z});
      corners.push_back({
        {place.x, place.y, place.z},
        {normal.x, normal.y, normal.z},
        {from.u, from.v},
        {shown.light, shown.light, shown.light, 1.0f},
      });
    }
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
    const float light)
  {
    const Model *model = Find(world, data, name);
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
      world.SetText(entity, world.FindField("Renderable", "shader"), "assets://shaders/unlit");
    }

    if (is_new || known->second.model != model || known->second.skin != wanted.skin)
    {
      if (!model->skins.empty() && !model->skins[wanted.skin].empty())
      {
        world.SetTexts(entity, world.FindField("Renderable", "textures"), {model->skins[wanted.skin]});
      }
    }

    const bool same_mesh = !is_new && known->second.model == model && known->second.frame == wanted.frame &&
                           known->second.pose == wanted.pose && known->second.light == wanted.light;
    if (!same_mesh && !ShowPose(world, entity, wanted)) { return false; }

    _shown[entity] = wanted;
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
      if (!shown.model->file.GetFrames()[shown.frame].is_group) { continue; }

      const std::size_t pose = ChoosePose(*shown.model, shown.frame);
      if (pose == shown.pose) { continue; }

      shown.pose = pose;
      ShowPose(world, entity, shown);
    }
  }
} // quake
