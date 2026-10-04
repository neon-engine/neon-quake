#include "bsp-collision.hpp"

#include <utility>

namespace quake
{
  bool BspCollision::Build(const BspFile &file, const std::size_t model, std::string &error)
  {
    std::array<BspHull, BspHull::hull_count> hulls;
    for (std::size_t hull = 0; hull < hulls.size(); hull++)
    {
      if (!hulls[hull].Build(file, model, hull, error)) { return false; }
    }
    _hulls = std::move(hulls);
    return true;
  }

  const BspHull &BspCollision::GetHull(const std::size_t hull) const
  {
    return _hulls.at(hull);
  }

  std::size_t BspCollision::ChooseHull(const BspVector &mins, const BspVector &maxs)
  {
    const float width = maxs.x - mins.x;
    if (width < 3.0f) { return 0; }
    if (width <= 32.0f) { return 1; }
    return 2;
  }

  BspContents BspCollision::GetPointContents(const BspVector &point) const
  {
    return _hulls[0].GetPointContents(point);
  }

  BspTraceResult BspCollision::TraceBox(
    const BspVector &origin,
    const BspVector &start,
    const BspVector &mins,
    const BspVector &maxs,
    const BspVector &end) const
  {
    const BspHull &hull = _hulls[ChooseHull(mins, maxs)];

    // What is added to a place of the hull to get the place of the box in
    // the world: the hull was grown for a box whose lowest corner is the
    // one of the hull, and the model stands at its origin.
    const BspVector offset{
      hull.GetMins().x - mins.x + origin.x,
      hull.GetMins().y - mins.y + origin.y,
      hull.GetMins().z - mins.z + origin.z,
    };

    BspTraceResult result = hull.TraceLine(
      {start.x - offset.x, start.y - offset.y, start.z - offset.z},
      {end.x - offset.x, end.y - offset.y, end.z - offset.z});

    // back into the world. A move that was not stopped ends at `end` itself
    // and not at what taking the offset away and adding it again made of it
    if (result.fraction == 1.0f)
    {
      result.end_position = end;
    }
    else
    {
      result.end_position.x += offset.x;
      result.end_position.y += offset.y;
      result.end_position.z += offset.z;
      result.plane_distance +=
        result.plane_normal.x * offset.x + result.plane_normal.y * offset.y + result.plane_normal.z * offset.z;
    }
    return result;
  }
} // quake
