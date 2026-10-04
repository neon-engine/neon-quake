#ifndef QUAKE_SPRITE_VIEW_HPP
#define QUAKE_SPRITE_VIEW_HPP

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include <neon/extension/neon-extension.hpp>

#include "formats/sprite-file.hpp"
#include "game-data.hpp"

namespace quake
{
  /// Shows the sprites of the game, `progs/*.spr`, on entities of the
  /// engine: the ball of fire of an explosion, a bubble of air, the glow of
  /// a light.
  ///
  /// A sprite is a flat picture that turns to whoever looks at it. Here it
  /// is a square of two triangles with the picture on it, and Face() turns
  /// every one of them towards the camera in every frame that is drawn. A
  /// sprite the file says stands upright only turns around, and one the
  /// file says is laid a way of its own is left as its entity is turned.
  ///
  /// As a model, a sprite has frames the game code names by a number, and a
  /// frame may be a group of pictures that plays by itself, see Update().
  class SpriteView
  {
    /// A sprite as it was read, and the pictures of it that were handed to
    /// the renderer.
    struct Sprite
    {
      SpriteFile file;
      std::string name;

      /// The path a material reads each picture by, by its frame and its
      /// place in the frame.
      std::map<std::pair<std::size_t, std::size_t>, std::string> paths;
    };

    /// What an entity shows.
    struct Shown
    {
      Sprite *sprite = nullptr;
      std::size_t frame = 0;
      std::size_t picture = 0;
    };

    // The sprites by their names. One that cannot be read is kept as
    // nothing, so that it is read, and said, once.
    std::map<std::string, std::unique_ptr<Sprite>> _sprites;

    std::map<neon::extension::Entity, Shown> _shown;

    // the time Update() was last told, by which a group picks its picture
    double _time = 0.0;

    Sprite *Find(const neon::extension::World &world, const GameData &data, const std::string &name);

    /// Which picture of a frame is shown at the time that is.
    [[nodiscard]] std::size_t ChoosePicture(const Sprite &sprite, std::size_t frame) const;

    /// Hands the engine the square and the picture of what an entity shows.
    static bool ShowPicture(
      const neon::extension::World &world,
      const GameData &data,
      neon::extension::Entity entity,
      const Shown &shown);

  public:
    /// Has an entity show a frame of a sprite by its name, such as
    /// `progs/s_explod.spr`. A frame the sprite does not have shows the
    /// first. Returns false when there is no such sprite.
    bool Show(
      const neon::extension::World &world,
      const GameData &data,
      neon::extension::Entity entity,
      const std::string &name,
      std::int32_t frame);

    /// Turns every sprite towards a camera that looks up by `pitch` and
    /// around by `yaw`, in degrees as the engine counts them.
    void Face(const neon::extension::World &world, float pitch, float yaw) const;

    /// No longer keeps track of an entity.
    void Forget(neon::extension::Entity entity);

    /// Forgets every entity, when a level ends.
    void Clear();

    /// Lets the groups play, at `time` in seconds.
    void Update(const neon::extension::World &world, const GameData &data, double time);
  };
} // quake

#endif //QUAKE_SPRITE_VIEW_HPP
