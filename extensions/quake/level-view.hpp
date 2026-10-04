#ifndef QUAKE_LEVEL_VIEW_HPP
#define QUAKE_LEVEL_VIEW_HPP

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>

#include <neon/extension/neon-extension.hpp>

#include "formats/bsp-file.hpp"
#include "formats/entity-text.hpp"
#include "game-data.hpp"

namespace quake
{
  /// Shows a level of the game in the world of the engine: its walls,
  /// floors, and ceilings as meshes, each with its texture as a picture,
  /// its doors, lifts, buttons, and other parts that move, at rest where
  /// the level puts them, and the camera where a player starts.
  ///
  /// The first model of a level is the level itself. Every other model
  /// belongs to an entity of the level that names it, as `*3`, and is shown
  /// under an entity of its own, so that it can be moved later. A trigger
  /// is a volume and no thing to see, and is left out.
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

    // The path a material reads the picture of a texture by, by the number
    // of the texture in the level that is shown, so that a picture is made
    // known to the renderer once, however many models show it. It is empty
    // for a texture that has no picture.
    std::map<std::int32_t, std::string> _pictures;

    /// Finds where a player starts among the entities of a level.
    void FindStart(const neon::extension::World &world, const EntityText &text, const std::string &map);

    /// The path of the picture of a texture of the level, which is made
    /// known to the renderer the first time it is asked for. Empty when the
    /// level does not carry the texture or the renderer does not take it.
    const std::string &FindPicture(
      const neon::extension::World &world,
      const GameData &data,
      const BspFile &level,
      std::int32_t texture);

    /// Shows one model of a level: an entity under `parent` for each of its
    /// textures, with the faces that show it. The sky is left out, and so is
    /// what is there to collide with and not to be seen. Adds what it showed
    /// to `triangles`. Returns false when no mesh can be made of the model,
    /// and says why in `error`.
    bool ShowModel(
      const neon::extension::World &world,
      const GameData &data,
      const BspFile &level,
      std::size_t model,
      neon::extension::Entity parent,
      std::size_t &triangles,
      std::string &error);

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
