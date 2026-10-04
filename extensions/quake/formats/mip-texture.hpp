#ifndef QUAKE_MIP_TEXTURE_HPP
#define QUAKE_MIP_TEXTURE_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace quake
{
  /// A texture of a wall, as a level keeps it and as an archive of textures
  /// keeps its entries of type 'D': a name, a size, and the picture four
  /// times, each half as wide and high as the one before.
  ///
  /// A pixel is one byte, the number of a colour of the palette. The name
  /// says what kind of texture it is, see the questions below.
  struct MipTexture
  {
    /// How many bytes come before the pictures: the name, the size, and
    /// where the four pictures start.
    static constexpr std::size_t header_size = 40;

    /// How many sizes of the picture there are.
    static constexpr std::size_t level_count = 4;

    std::string name;
    std::uint32_t width = 0;
    std::uint32_t height = 0;

    /// The pixels of each size, row after row from the top. The first is the
    /// full size, the next ones are `width >> level` by `height >> level`.
    /// A smaller size the file does not have is empty.
    std::array<std::vector<std::uint8_t>, level_count> pixels;

    /// Takes the texture from bytes that start with its name. They may go on
    /// past its end, as they do in a level. Returns false, says why in
    /// `error`, and stays as it was when the bytes are not a texture.
    bool Read(std::span<const std::uint8_t> bytes, std::string &error);

    /// Whether it is water, slime, lava, or a teleporter, which the game
    /// draws waving and without a lightmap.
    [[nodiscard]] bool IsLiquid() const;

    /// Whether it is the sky, which the game draws as two layers of clouds
    /// that drift, and never as a wall.
    [[nodiscard]] bool IsSky() const;

    /// Whether the last colour of the palette is a hole in it, as in a fence
    /// or a grate.
    [[nodiscard]] bool HasHoles() const;

    /// Which picture of an animation it is, from 0, or -1 when it is not
    /// part of one. The frames of an animation share what follows the first
    /// two letters of their names.
    [[nodiscard]] int GetAnimationFrame() const;

    /// Whether it belongs to the second animation of its name, the one shown
    /// while the thing it is on is switched: a button that was pressed.
    [[nodiscard]] bool IsAlternateAnimation() const;
  };
} // quake

#endif //QUAKE_MIP_TEXTURE_HPP
