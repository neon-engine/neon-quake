#ifndef QUAKE_BYTE_READER_HPP
#define QUAKE_BYTE_READER_HPP

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace quake
{
  /// Reads the numbers and names of a file of the game from bytes in memory.
  ///
  /// Every file of the game keeps its numbers with the lowest byte first,
  /// whatever the machine that reads them does. Nothing is read past the end
  /// of the bytes: a read that would is refused, gives zero, and is
  /// remembered, so that a file that is cut short or lies about its sizes is
  /// found by asking once, with IsGood(), after reading what was expected.
  class ByteReader
  {
    std::span<const std::uint8_t> _bytes;
    std::size_t _position = 0;
    bool _good = true;

    /// Whether `count` bytes are left to read, remembering when not.
    bool Has(std::size_t count);

  public:
    explicit ByteReader(std::span<const std::uint8_t> bytes);

    /// Whether every read so far stayed inside the bytes.
    [[nodiscard]] bool IsGood() const;

    [[nodiscard]] std::size_t GetPosition() const;

    [[nodiscard]] std::size_t GetSize() const;

    /// Moves to a place from the start. A place past the end is refused.
    void Seek(std::size_t position);

    void Skip(std::size_t count);

    std::uint8_t ReadU8();

    std::int16_t ReadI16();

    std::uint16_t ReadU16();

    std::int32_t ReadI32();

    std::uint32_t ReadU32();

    /// A number with a fraction, of four bytes.
    float ReadF32();

    /// A name kept in a fixed number of bytes and ended by a zero when it is
    /// shorter, as the game keeps the names of files and textures. What
    /// follows the zero is left out.
    std::string ReadFixedString(std::size_t size);

    /// `count` bytes from where the reader is, without copying them. Empty
    /// when they are not all there.
    std::span<const std::uint8_t> ReadBytes(std::size_t count);

    /// `count` bytes from a place, without moving the reader. Empty when
    /// they are not all there, which is remembered too.
    std::span<const std::uint8_t> BytesAt(std::size_t position, std::size_t count);
  };
} // quake

#endif //QUAKE_BYTE_READER_HPP
