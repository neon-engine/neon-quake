#include "byte-reader.hpp"

#include <cstring>

namespace quake
{
  ByteReader::ByteReader(const std::span<const std::uint8_t> bytes)
  {
    _bytes = bytes;
  }

  bool ByteReader::Has(const std::size_t count)
  {
    if (count > _bytes.size() - _position)
    {
      _good = false;
      return false;
    }
    return true;
  }

  bool ByteReader::IsGood() const
  {
    return _good;
  }

  std::size_t ByteReader::GetPosition() const
  {
    return _position;
  }

  std::size_t ByteReader::GetSize() const
  {
    return _bytes.size();
  }

  void ByteReader::Seek(const std::size_t position)
  {
    if (position > _bytes.size())
    {
      _good = false;
      return;
    }
    _position = position;
  }

  void ByteReader::Skip(const std::size_t count)
  {
    if (Has(count)) { _position += count; }
  }

  std::uint8_t ByteReader::ReadU8()
  {
    if (!Has(1)) { return 0; }
    return _bytes[_position++];
  }

  std::uint16_t ByteReader::ReadU16()
  {
    if (!Has(2)) { return 0; }

    const auto value = static_cast<std::uint16_t>(_bytes[_position] | _bytes[_position + 1] << 8);
    _position += 2;
    return value;
  }

  std::int16_t ByteReader::ReadI16()
  {
    return static_cast<std::int16_t>(ReadU16());
  }

  std::uint32_t ByteReader::ReadU32()
  {
    if (!Has(4)) { return 0; }

    const std::uint32_t value =
      static_cast<std::uint32_t>(_bytes[_position]) |
      static_cast<std::uint32_t>(_bytes[_position + 1]) << 8 |
      static_cast<std::uint32_t>(_bytes[_position + 2]) << 16 |
      static_cast<std::uint32_t>(_bytes[_position + 3]) << 24;
    _position += 4;
    return value;
  }

  std::int32_t ByteReader::ReadI32()
  {
    return static_cast<std::int32_t>(ReadU32());
  }

  float ByteReader::ReadF32()
  {
    // the same four bytes, taken as a number with a fraction
    const std::uint32_t bits = ReadU32();
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
  }

  std::string ByteReader::ReadFixedString(const std::size_t size)
  {
    const std::span<const std::uint8_t> bytes = ReadBytes(size);

    std::string text;
    for (const std::uint8_t byte : bytes)
    {
      if (byte == 0) { break; }
      text.push_back(static_cast<char>(byte));
    }
    return text;
  }

  std::span<const std::uint8_t> ByteReader::ReadBytes(const std::size_t count)
  {
    if (!Has(count)) { return {}; }

    const auto bytes = _bytes.subspan(_position, count);
    _position += count;
    return bytes;
  }

  std::span<const std::uint8_t> ByteReader::BytesAt(const std::size_t position, const std::size_t count)
  {
    if (position > _bytes.size() || count > _bytes.size() - position)
    {
      _good = false;
      return {};
    }
    return _bytes.subspan(position, count);
  }
} // quake
