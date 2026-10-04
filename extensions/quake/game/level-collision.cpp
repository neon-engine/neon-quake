#include "level-collision.hpp"

#include <algorithm>
#include <charconv>
#include <string_view>
#include <utility>

#include "formats/bsp-hull.hpp"
#include "formats/bsp-trace-result.hpp"
#include "qc-flag.hpp"
#include "qc-solid.hpp"

namespace quake
{
  // Helpers of LevelCollision: the vectors of the machine as those of a
  // level and back.
  namespace
  {
    BspVector ToBsp(const LevelVector &vector)
    {
      return {vector[0], vector[1], vector[2]};
    }

    LevelVector ToLevel(const BspVector &vector)
    {
      return {vector.x, vector.y, vector.z};
    }

    /// What a hull says of a move, as a move through the level, stopped by
    /// an entity when it was stopped or started inside.
    LevelTraceResult ToLevel(const BspTraceResult &trace, const std::int32_t entity)
    {
      return {
        .all_solid = trace.all_solid,
        .start_solid = trace.start_solid,
        .in_open = trace.in_open,
        .in_water = trace.in_water,
        .fraction = trace.fraction,
        .end_position = ToLevel(trace.end_position),
        .plane_normal = ToLevel(trace.plane_normal),
        .plane_distance = trace.plane_distance,
        .entity = trace.fraction < 1.0f || trace.start_solid ? entity : LevelTraceResult::nothing,
      };
    }

    bool IsSolid(const float solid, const QcSolid kind)
    {
      return solid == static_cast<float>(kind);
    }
  }

  LevelCollision::LevelCollision(QcMachine &machine) : _machine(machine), _fields(machine.GetProgs())
  {
  }

  bool LevelCollision::Build(const BspFile &file, std::string &error)
  {
    std::vector<BspCollision> models(file.models.size());
    for (std::size_t model = 0; model < models.size(); model++)
    {
      if (!models[model].Build(file, model, error)) { return false; }
    }
    _models = std::move(models);
    return true;
  }

  std::size_t LevelCollision::GetModelCount() const
  {
    return _models.size();
  }

  QcMachine &LevelCollision::GetMachine() const
  {
    return _machine;
  }

  const QcFields &LevelCollision::GetFields() const
  {
    return _fields;
  }

  const BspCollision *LevelCollision::FindModel(const std::int32_t entity) const
  {
    if (_models.empty()) { return nullptr; }
    if (entity == 0) { return &_models[0]; }

    const std::string_view name = _fields.model.GetText(_machine, entity);
    if (!name.starts_with('*')) { return nullptr; }

    // the whole of what follows the star has to be the number
    std::size_t model = 0;
    const char *end = name.data() + name.size();
    const auto [stopped_at, problem] = std::from_chars(name.data() + 1, end, model);
    if (problem != std::errc() || stopped_at != end || model == 0 || model >= _models.size()) { return nullptr; }

    return &_models[model];
  }

  LevelTraceResult LevelCollision::ClipToEntity(
    const std::int32_t entity,
    const LevelVector &start,
    const LevelVector &mins,
    const LevelVector &maxs,
    const LevelVector &end) const
  {
    const LevelVector origin = _fields.origin.Get(_machine, entity);

    if (entity == 0 || IsSolid(_fields.solid.Get(_machine, entity), QcSolid::Bsp))
    {
      if (const BspCollision *model = FindModel(entity))
      {
        return ToLevel(model->TraceBox(ToBsp(origin), ToBsp(start), ToBsp(mins), ToBsp(maxs), ToBsp(end)), entity);
      }
      // a world that there is not is empty space
      if (entity == 0) { return {.in_open = true, .end_position = end}; }
    }

    // The box of the entity, grown by the box that moves: a point moved
    // through it is stopped where the two boxes would meet.
    BspHull box;
    box.BuildBox(
      ToBsp(Difference(_fields.mins.Get(_machine, entity), maxs)),
      ToBsp(Difference(_fields.maxs.Get(_machine, entity), mins)));
    BspTraceResult trace = box.TraceLine(ToBsp(Difference(start, origin)), ToBsp(Difference(end, origin)));

    // Back from around the entity into the world. A move that was not
    // stopped ends at `end` itself.
    LevelTraceResult result = ToLevel(trace, entity);
    if (result.fraction == 1.0f) { result.end_position = end; }
    else
    {
      result.end_position = Sum(result.end_position, origin);
      result.plane_distance += Dot(result.plane_normal, origin);
    }
    return result;
  }

  LevelTraceResult LevelCollision::Trace(
    const LevelVector &start,
    const LevelVector &mins,
    const LevelVector &maxs,
    const LevelVector &end,
    const LevelTraceKind kind,
    const std::int32_t pass_entity) const
  {
    LevelTraceResult result = ClipToEntity(0, start, mins, maxs, end);

    // the box a monster is hit in
    LevelVector monster_mins = mins;
    LevelVector monster_maxs = maxs;
    if (kind == LevelTraceKind::Missile)
    {
      monster_mins.fill(-missile_margin);
      monster_maxs.fill(missile_margin);
    }

    // everything the move sweeps over, and a unit around it
    LevelBox swept;
    for (std::size_t i = 0; i < start.size(); i++)
    {
      swept.mins[i] = std::min(start[i], end[i]) + monster_mins[i] - box_margin;
      swept.maxs[i] = std::max(start[i], end[i]) + monster_maxs[i] + box_margin;
    }

    const bool passes = pass_entity >= 0;
    const auto width_of = [this](const std::int32_t entity)
    {
      return _fields.maxs.Get(_machine, entity)[0] - _fields.mins.Get(_machine, entity)[0];
    };
    const bool pass_has_size = passes && width_of(pass_entity) != 0.0f;
    const std::int32_t owner_of_pass = passes ? _fields.owner.Get(_machine, pass_entity) : 0;

    for (std::int32_t entity = 1; entity < _machine.GetEntityCount(); entity++)
    {
      if (_machine.IsEntityFree(entity) || entity == pass_entity) { continue; }

      const float solid = _fields.solid.Get(_machine, entity);
      if (IsSolid(solid, QcSolid::Not) || IsSolid(solid, QcSolid::Trigger)) { continue; }
      if (kind == LevelTraceKind::NoMonsters && !IsSolid(solid, QcSolid::Bsp)) { continue; }
      if (!swept.Overlaps(GetBox(entity))) { continue; }

      // a point is no obstacle to what has a size
      if (pass_has_size && width_of(entity) == 0.0f) { continue; }

      // nothing is nearer than what the whole move is inside of
      if (result.all_solid) { return result; }

      // a missile does not hit who shot it, nor what its owner shot
      if (passes && (_fields.owner.Get(_machine, entity) == pass_entity || owner_of_pass == entity)) { continue; }

      const bool is_monster = HasFlag(_fields.flags.Get(_machine, entity), QcFlag::Monster);
      LevelTraceResult trace = is_monster
                                 ? ClipToEntity(entity, start, monster_mins, monster_maxs, end)
                                 : ClipToEntity(entity, start, mins, maxs, end);

      if (trace.all_solid || trace.start_solid || trace.fraction < result.fraction)
      {
        // nearer than what was found before, which is not forgotten to
        // have held the move from the start
        trace.entity = entity;
        trace.start_solid = trace.start_solid || result.start_solid;
        result = trace;
      }
      else if (trace.start_solid) { result.start_solid = true; }
    }
    return result;
  }

  BspContents LevelCollision::GetPointContents(const LevelVector &point) const
  {
    return _models.empty() ? BspContents::Empty : _models[0].GetPointContents(ToBsp(point));
  }

  bool LevelCollision::TestPosition(const std::int32_t entity) const
  {
    const LevelVector origin = _fields.origin.Get(_machine, entity);
    return Trace(
      origin, _fields.mins.Get(_machine, entity), _fields.maxs.Get(_machine, entity), origin,
      LevelTraceKind::Normal, entity).start_solid;
  }

  LevelBox LevelCollision::GetBox(const std::int32_t entity) const
  {
    const LevelVector origin = _fields.origin.Get(_machine, entity);
    LevelBox box{
      .mins = Sum(origin, _fields.mins.Get(_machine, entity)),
      .maxs = Sum(origin, _fields.maxs.Get(_machine, entity)),
    };

    const bool is_item = HasFlag(_fields.flags.Get(_machine, entity), QcFlag::Item);
    for (std::size_t i = 0; i < origin.size(); i++)
    {
      // an item is not made taller
      if (is_item && i == 2) { continue; }

      const float margin = is_item ? item_margin : box_margin;
      box.mins[i] -= margin;
      box.maxs[i] += margin;
    }
    return box;
  }

  void LevelCollision::Link(const std::int32_t entity) const
  {
    // the world is found by nothing
    if (entity == 0 || _machine.IsEntityFree(entity)) { return; }

    const LevelBox box = GetBox(entity);
    _fields.absmin.Set(_machine, entity, box.mins);
    _fields.absmax.Set(_machine, entity, box.maxs);
  }
} // quake
