#ifndef QUAKE_LEVEL_VIEW_HPP
#define QUAKE_LEVEL_VIEW_HPP

#include <string>

#include <neon/extension/neon-extension.hpp>

#include "game-data.hpp"

namespace quake
{
  /// Shows a level of the game in the world of the engine: its walls,
  /// floors, and ceilings as meshes, each with its texture as a picture,
  /// and the camera where a player starts.
  ///
  /// Everything is turned from the space of the game into that of the
  /// engine on the way, see QuakeSpace. The textures are shown as they are,
  /// without the light of the level, which comes with lightmaps; the sky is
  /// left out until it is drawn as one.
  class LevelView
  {
    // where a player starts, in the space of the engine, once a level that
    // says so is shown
    bool _has_start = false;
    neon::extension::Vector3 _start_position{0.0f, 0.0f, 0.0f};
    float _start_yaw = 0.0f;
    bool _camera_placed = false;

    /// Finds where a player starts among the entities of a level.
    void FindStart(const neon::extension::World &world, const std::string &entities, const std::string &map);

  public:
    /// Shows the level of a name such as `maps/start.bsp`. Returns false
    /// when it is not in the data or cannot be read, and says why in
    /// `error`.
    bool Show(
      const neon::extension::World &world,
      const GameData &data,
      const std::string &map,
      std::string &error);

    /// Puts the camera of the scene, the entity `camera`, where a player
    /// starts, looking the way the level says. The scene is read after the
    /// level is shown, so this is asked in every frame until the camera is
    /// there, and does nothing after.
    void PlaceCamera(const neon::extension::World &world);
  };
} // quake

#endif //QUAKE_LEVEL_VIEW_HPP
