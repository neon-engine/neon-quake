#ifndef QUAKE_DEMO_FILE_HPP
#define QUAKE_DEMO_FILE_HPP

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "demo-block.hpp"

namespace quake
{
  /// A recorded game, a `.dem`: what a server sent one player, packet
  /// after packet, as it came.
  ///
  /// The file starts with a line of text, the number of the music track
  /// to play, or -1 for the one the server names. Blocks follow to the
  /// end: the length of a packet as a long, three floats for where the
  /// player looked, and the bytes of the packet. The file does not say
  /// when a block came: the messages in it do, see `DemoPlayer`.
  ///
  /// ```
  /// DemoFile file;
  /// std::string problem;
  /// if (!file.Read(bytes, problem)) { return; }
  /// for (const DemoBlock &block : file.GetBlocks()) { ... }
  /// ```
  class DemoFile final
  {
  public:
    /// The most bytes a block may have. No packet of any port is larger.
    static constexpr std::size_t max_message_size = 65536;

    /// What the first line has when the server chooses the music.
    static constexpr std::int32_t no_forced_track = -1;

  private:
    std::int32_t _forced_track = no_forced_track;
    std::vector<DemoBlock> _blocks;
    bool _is_cut_short = false;

  public:
    /// Reads a file. The blocks point into `bytes`, which have to stay
    /// for as long as the blocks are used.
    ///
    /// False, with the reason in `problem`, for bytes that do not start
    /// with the line of the track, or have a block with a length that
    /// cannot be. A file that ends in the middle of a block is not
    /// refused, as a recording that was cut off plays up to there in the
    /// original: the blocks that are whole are kept, see IsCutShort().
    bool Read(std::span<const std::uint8_t> bytes, std::string &problem);

    /// The music track the one who recorded forced, or `no_forced_track`.
    [[nodiscard]] std::int32_t GetForcedTrack() const;

    [[nodiscard]] std::span<const DemoBlock> GetBlocks() const;

    /// Whether the file ended in the middle of a block, which was left out.
    [[nodiscard]] bool IsCutShort() const;
  };
} // quake

#endif //QUAKE_DEMO_FILE_HPP
