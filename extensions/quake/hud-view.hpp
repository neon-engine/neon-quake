#ifndef QUAKE_HUD_VIEW_HPP
#define QUAKE_HUD_VIEW_HPP

#include <map>
#include <string>
#include <vector>

#include <neon/extension/neon-extension.hpp>

#include "formats/wad.hpp"
#include "game-data.hpp"
#include "game/hud-picture.hpp"

namespace quake
{
  /// Draws what the game shows on top of the world: the status bar, the
  /// counts of a level that is over, the words in the middle of the screen.
  ///
  /// It is handed a list of pictures with their places on the screen of the
  /// original, 320 by 200, and shows them on a plane that stands right in
  /// front of the camera and moves with it: a square for every picture,
  /// those of the same picture in one mesh. The pictures are those of
  /// `gfx.wad` and of the `gfx/` folder of the data, handed to the renderer
  /// the first time each is shown.
  ///
  /// The bar is drawn as the ports of today draw it: three times as large
  /// on a screen 1080 pixels high, standing in the middle of the lower
  /// edge. What is anchored to the middle is in the middle of the screen.
  /// Nothing is made anew while the list stays the same.
  class HudView
  {
    /// A picture that was handed to the renderer: where a material reads it
    /// from, and its size in its own pixels.
    struct Known
    {
      std::string path;
      std::int32_t width = 0;
      std::int32_t height = 0;
    };

    // the archive of pictures of the status bar, once it was read
    Wad _wad;
    bool _has_wad = false;
    bool _was_read = false;

    // the pictures by their names; one that is not there has no path
    std::map<std::string, Known, std::less<>> _pictures;

    // the entity that shows the squares of each picture, by its name
    std::map<std::string, neon::extension::Entity, std::less<>> _entities;

    // the camera the plane stands in front of
    neon::extension::Entity _camera = 0;

    // the entity that lays a colour over the view, the camera it stands in
    // front of, and the colour it has
    neon::extension::Entity _tint = 0;
    neon::extension::Entity _tint_camera = 0;
    NeonColor _tint_colour{0.0f, 0.0f, 0.0f, 0.0f};

    // what is shown, to tell when it has to be made anew
    std::vector<HudPicture> _shown;

    /// How far in front of the camera the plane stands, in metres: behind
    /// what the camera leaves out as too near, and before everything else.
    static constexpr float distance = 0.06f;

    /// How much nearer to the camera a layer of pictures stands than the one
    /// behind it, in metres.
    static constexpr float layer_step = 0.0005f;

    /// How many pixels of the original the screen is high. The original is
    /// 200; more makes everything smaller, as a larger screen does.
    static constexpr float screen_height = 360.0f;

    /// From top to bottom, how much the camera of the game sees, in degrees.
    static constexpr float field_of_view = 73.74f;

    const Known &Find(const neon::extension::World &world, const GameData &data, std::string_view name);

  public:
    /// Shows a list of pictures in front of a camera. An empty list shows
    /// nothing.
    void Show(
      const neon::extension::World &world,
      const GameData &data,
      neon::extension::Entity camera,
      const std::vector<HudPicture> &pictures);

    /// Lays a colour over everything the camera sees, behind the pictures:
    /// red, green, and blue as the engine multiplies light, and how much of
    /// it, from 0 for none to 1 for nothing else.
    void ShowTint(const neon::extension::World &world, neon::extension::Entity camera, const NeonColor &colour);

    /// The size of a picture of a name in its own pixels, for laying out
    /// what depends on it. Zero for one that is not there.
    std::int32_t GetWidth(const neon::extension::World &world, const GameData &data, std::string_view name);
  };
} // quake

#endif //QUAKE_HUD_VIEW_HPP
