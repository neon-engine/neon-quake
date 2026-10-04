#ifndef QUAKE_LEVEL_VIEW_HPP
#define QUAKE_LEVEL_VIEW_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <neon/extension/neon-extension.hpp>

#include "formats/bsp-file.hpp"
#include "formats/bsp-light-point.hpp"
#include "formats/entity-text.hpp"
#include "formats/light-styles.hpp"
#include "formats/lightmap-atlas.hpp"
#include "formats/lit-file.hpp"
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
    // The colours of the light of the level that is shown, when the data
    // has them, `maps/<level>.lit` next to the level; and the light of the
    // level at a place, for what stands in it.
    LitFile _lit;
    bool _has_lit = false;
    BspLightPoint _light;
    bool _has_light = false;

    // What the level says of itself that is for drawing it: how thick its
    // fog is and its colour, as a screen is given it, and how much of each
    // kind of liquid is seen through, from 0 for all to 1 for none.
    float _fog_density = 0.0f;
    std::array<float, 3> _fog_colour{0.0f, 0.0f, 0.0f};
    float _water_alpha = 1.0f;
    float _lava_alpha = 1.0f;
    float _slime_alpha = 1.0f;
    float _teleporter_alpha = 1.0f;

    /// Reads what the level says of itself for drawing it, from the keys of
    /// its first entity: `fog`, `wateralpha`, and the like.
    void ReadSettings(const BspFile &level);

    /// The pictures of a texture, as paths a material reads them by: the
    /// texture, and its pixels that glow, which are laid over it once it has
    /// its light. Either is empty when there is none.
    struct Pictures
    {
      std::string path;
      std::string glow;
    };

    /// An entity that shows a texture that changes: the pictures it shows
    /// in turn, those it shows while the frame of its model is not 0, and
    /// which it shows now.
    struct ChangingTexture
    {
      neon::extension::Entity entity = 0;

      /// The number of the model of the level it is a part of.
      std::size_t model = 0;

      std::vector<Pictures> frames;
      std::vector<Pictures> alternate;

      /// Whether any of them glows, so that each is told its glow.
      bool glows = false;

      std::size_t shown = SIZE_MAX;
      bool shows_alternate = false;
    };

    /// The light of a model of the level that changes with time: the atlas,
    /// which composes its pictures anew for other values of the styles, and
    /// the names the renderer knows those pictures by.
    struct ChangingLight
    {
      LightmapAtlas atlas;
      std::vector<std::string> names;
    };

    // The styles of the lights of the level, as the game code sets them: a
    // flicker, a pulse, a light that a switch turns on. And the models that
    // have a face lit by any style but the steady one.
    LightStyles _styles;
    std::vector<ChangingLight> _changing_lights;
    LightStyles::Values _style_values{};
    bool _has_style_values = false;

    // the entity everything of the level that is shown stands under
    neon::extension::Entity _root = 0;

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
    std::map<std::int32_t, Pictures> _pictures;

    // The same for the pictures of the small levels that are items, by the
    // name of the file and the number of the texture in it.
    std::map<std::pair<std::string, std::int32_t>, Pictures> _item_pictures;

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

    // what shows a texture that changes, and the frame the game code set
    // for each model of the level, by its number
    std::vector<ChangingTexture> _changing_textures;
    std::map<std::size_t, std::int32_t> _part_frames;

    // the path of a picture in which nothing glows, once it was made
    std::string _no_glow;

    /// Finds where a player starts among the entities of a level.
    void FindStart(const neon::extension::World &world, const EntityText &text, const std::string &map);

    /// The path of the picture of a texture of a level, which is made
    /// known to the renderer the first time it is asked for. Empty when the
    /// level does not carry the texture or the renderer does not take it.
    /// With it comes the picture of its pixels that glow, when it has any.
    /// `item` is empty for the level that is shown, and the name of the
    /// file for a small level that is an item.
    const Pictures &FindPicture(
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

    /// How bright a model is that stands at a place of the level, in the
    /// space of the game: what its colours are multiplied by, red, green,
    /// and blue. A model is lit by the floor under it, as in the original,
    /// and takes the colour of its light. `least` is the least light it
    /// has, of 255: what the player holds is never all dark.
    /// `more` is light on top of that of the level, of 255 as well: that of
    /// an explosion or a shot nearby.
    [[nodiscard]] std::array<float, 3> FindLight(const BspVector &place, float least = 0.0f, float more = 0.0f) const;

    /// Sets the style of a light, as the game code does with `lightstyle`:
    /// a text of letters from `a` for dark to `z` for twice as bright, ten
    /// of them shown in a second.
    void SetLightStyle(std::int32_t style, std::string_view text);

    /// Makes the light of the level what its styles make it at a time of
    /// the game, in seconds: the pictures of the light that hold a face
    /// whose style changed are composed anew and handed to the renderer
    /// again. A level whose lights are all steady costs nothing.
    void UpdateLight(const neon::extension::World &world, double time);

    /// Says which frame the game code set for a model of the level, by its
    /// number: with a frame that is not 0 the model shows the second run of
    /// its textures that change, as a button that was pressed does.
    void SetPartFrame(std::size_t model, std::int32_t frame);

    /// Shows the textures that change as they are at a time of the game, in
    /// seconds. Only what shows another picture than before is told.
    void UpdateTextures(const neon::extension::World &world, double time);

    /// Takes a model of the level that is shown out of the world for good,
    /// by its number.
    void RemovePart(const neon::extension::World &world, std::size_t model);

    /// How thick the fog of the level that is shown is, as the level says
    /// it, 0 for none, and its colour as the engine multiplies light.
    [[nodiscard]] float GetFogDensity() const;

    [[nodiscard]] std::array<float, 3> GetFogColour() const;

    /// Takes the level that is shown out of the world, with everything
    /// under it. Show() does so itself before it shows another.
    void Clear(const neon::extension::World &world);

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
