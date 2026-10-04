#ifndef QUAKE_PALETTE_HPP
#define QUAKE_PALETTE_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace quake
{
  /// The 256 colours every picture of the game is painted with, as
  /// `gfx/palette.lmp` holds them: red, green, and blue of each, a byte
  /// apiece, 768 bytes in all.
  ///
  /// A picture of the game is one byte for each pixel, the number of a
  /// colour here. The engine draws pictures of four bytes for each pixel,
  /// red, green, blue, and how solid it is, so that is what this makes of
  /// them.
  class Palette
  {
    std::array<std::uint8_t, 768> _colours{};

  public:
    /// How many bytes the file of a palette has.
    static constexpr std::size_t size_in_bytes = 768;

    /// The colour that stands for nothing being there, in pictures that have
    /// holes: sprites, the pictures of the menus, textures whose name starts
    /// with `{`.
    static constexpr std::uint8_t see_through = 255;

    /// The first of the colours that glow: from here to the end of the
    /// palette a colour is shown as it is, however dark its place, in a
    /// texture of a wall and in the skin of a model.
    static constexpr std::uint8_t first_glowing = 224;

    /// Takes the colours from the bytes of the file. Returns false, and
    /// stays as it was, when there are not exactly 768.
    bool Read(std::span<const std::uint8_t> bytes);

    /// Red, green, and blue of a colour.
    [[nodiscard]] std::array<std::uint8_t, 3> GetColour(std::uint8_t index) const;

    /// The pixels of a picture as red, green, blue, and solidness, four
    /// bytes for each. With `holes`, pixels of the colour `see_through` are
    /// not solid at all, and black; without, every pixel is solid.
    [[nodiscard]] std::vector<std::uint8_t> ToRgba(std::span<const std::uint8_t> pixels, bool holes = false) const;

    /// The pixels of a picture that glow, as ToRgba() gives them, and every
    /// other pixel black and not solid at all: what is laid over the picture
    /// once it has its light. With `holes`, pixels of the colour
    /// `see_through` do not glow. Empty for a picture without a pixel that
    /// glows.
    [[nodiscard]] std::vector<std::uint8_t> ToGlowRgba(std::span<const std::uint8_t> pixels, bool holes = false) const;
  };
} // quake

#endif //QUAKE_PALETTE_HPP
