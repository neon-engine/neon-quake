#ifndef QUAKE_LIT_FILE_HPP
#define QUAKE_LIT_FILE_HPP

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace quake
{
  /// The coloured light of a level, `maps/*.lit`, which lies next to the
  /// level it belongs to and has the same name. The level itself only says
  /// how bright every sample of its lightmaps is. This file says which
  /// colour: red, green, and blue for every byte of the lighting of the
  /// level, in the same order. So the samples of a face that start at byte
  /// `n` of the lighting start at sample `n` in here.
  ///
  /// What was read can be trusted: there are exactly three bytes for every
  /// byte of the lighting it was read for.
  class LitFile
  {
    std::vector<std::uint8_t> _colours;

  public:
    /// The four letters the file starts with, `QLIT`, read as a number.
    static constexpr std::uint32_t magic = 0x54494C51;

    /// The only version that is read.
    static constexpr std::int32_t version = 1;

    /// How many bytes come before the samples: the letters and the version.
    static constexpr std::size_t header_size = 8;

    /// Takes the light from the bytes of the file. `lighting_size` is how
    /// many bytes the lighting of its level has, `BspFile::lighting`.
    /// Returns false, says in `error` what was wrong, and stays as it was
    /// when the bytes do not start with `QLIT`, are of another version, or
    /// do not hold three bytes for every byte of the lighting, which is so
    /// when the file was made for another build of the level.
    bool Read(std::span<const std::uint8_t> bytes, std::size_t lighting_size, std::string &error);

    /// The samples, three bytes for each: red, green, and blue, from dark
    /// to twice as bright as the texture, as the bytes of the level are.
    [[nodiscard]] const std::vector<std::uint8_t> &GetColours() const;

    /// How many samples there are, which is how many bytes the lighting of
    /// the level has.
    [[nodiscard]] std::size_t GetSampleCount() const;
  };
} // quake

#endif //QUAKE_LIT_FILE_HPP
