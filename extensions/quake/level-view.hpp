#ifndef QUAKE_LEVEL_VIEW_HPP
#define QUAKE_LEVEL_VIEW_HPP

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

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
  /// engine on the way, see QuakeSpace. A level is lit by the light it
  /// carries, its lightmaps, packed into a picture for each model, and is
  /// stood on and walked into as the mesh it is; the sky
  /// is left out until it is drawn as one, and liquids are shown as they
  /// are.
  class LevelView
  {
    // where a player starts, in the space of the engine, once a level that
    // says so is shown
    bool _has_start = false;
    neon::extension::Vector3 _start_position{0.0f, 0.0f, 0.0f};
    float _start_yaw = 0.0f;
    bool _player_placed = false;

    // The path a material reads the picture of a texture by, by the number
    // of the texture in the level that is shown, so that a picture is made
    // known to the renderer once, however many models show it. It is empty
    // for a texture that has no picture.
    std::map<std::int32_t, std::string> _pictures;

    // The same for the pictures of the small levels that are items, by the
    // name of the file and the number of the texture in it.
    std::map<std::pair<std::string, std::int32_t>, std::string> _item_pictures;

    // The paths of the pictures of the light of such an item, by the name
    // of its file.
    std::map<std::string, std::vector<std::string>> _item_lightmaps;

    // The small levels that are items, such as `maps/b_bh25.bsp`, as they
    // were read. One that cannot be read is kept as nothing, so that it is
    // read, and said, once.
    std::map<std::string, std::unique_ptr<BspFile>> _items;

    // The entity each model of the level but the first is shown under, by
    // the number of the model.
    std::map<std::size_t, neon::extension::Entity> _parts;

    // the name of the level that is shown, which the pictures of its light
    // are named after
    std::string _map;

    /// Finds where a player starts among the entities of a level.
    void FindStart(const neon::extension::World &world, const EntityText &text, const std::string &map);

    /// The path of the picture of a texture of a level, which is made
    /// known to the renderer the first time it is asked for. Empty when the
    /// level does not carry the texture or the renderer does not take it.
    /// `item` is empty for the level that is shown, and the name of the
    /// file for a small level that is an item.
    const std::string &FindPicture(
      const neon::extension::World &world,
      const GameData &data,
      const BspFile &level,
      const std::string &item,
      std::int32_t texture);

    /// Shows one model of a level: an entity under `parent` for each of its
    /// textures, with the faces that show it. The sky is left out, and so is
    /// what is there to collide with and not to be seen. Adds what it showed
    /// to `triangles`. Returns false when no mesh can be made of the model,
    /// and says why in `error`.
    ///
    /// `item` is empty for the level that is shown. For a small level that
    /// is an item it is the name of its file, and what is shown stops
    /// nothing: an item is walked through and picked up.
    bool ShowModel(
      const neon::extension::World &world,
      const GameData &data,
      const BspFile &level,
      const std::string &item,
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

    /// The entity a model of the level that is shown stands under, by its
    /// number, which a door, a lift, or a button is moved by. Nothing for
    /// the level itself, for a trigger, which is not shown, and for a number
    /// the level has no model of.
    [[nodiscard]] neon::extension::Entity FindPart(std::size_t model) const;

    /// Shows a small level that is an item, such as the box of health
    /// `maps/b_bh25.bsp`, under an entity: with its textures and its light,
    /// and without stopping anything. Returns false when the data has no
    /// such file or no mesh can be made of it.
    bool ShowItem(
      const neon::extension::World &world,
      const GameData &data,
      const std::string &name,
      neon::extension::Entity parent);

    /// Puts the player of the scene, the entity `player`, or else its
    /// `camera`, where a player starts, looking the way the level says. The
    /// scene is read after the level is shown, so this is asked in every
    /// frame until the entity is there, and does nothing after.
    void PlacePlayer(const neon::extension::World &world);
  };
} // quake

#endif //QUAKE_LEVEL_VIEW_HPP
