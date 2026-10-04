#include "lit-file.hpp"

#include "byte-reader.hpp"

namespace quake
{
  bool LitFile::Read(const std::span<const std::uint8_t> bytes, const std::size_t lighting_size, std::string &error)
  {
    if (bytes.size() < header_size)
    {
      error = "The file ends before its header";
      return false;
    }

    ByteReader reader(bytes);
    if (reader.ReadU32() != magic)
    {
      error = "The file does not start with QLIT";
      return false;
    }
    if (const std::int32_t found = reader.ReadI32(); found != version)
    {
      error = "The version is " + std::to_string(found) + ", not " + std::to_string(version);
      return false;
    }

    // compared without multiplying, so that no size is too large to ask for
    const std::size_t sample_bytes = bytes.size() - header_size;
    if (sample_bytes % 3 != 0 || sample_bytes / 3 != lighting_size)
    {
      error = "The file has " + std::to_string(sample_bytes) + " bytes of colours, which are not three for each of the " +
        std::to_string(lighting_size) + " bytes of the lighting of the level";
      return false;
    }

    _colours.assign(bytes.begin() + header_size, bytes.end());
    return true;
  }

  const std::vector<std::uint8_t> &LitFile::GetColours() const
  {
    return _colours;
  }

  std::size_t LitFile::GetSampleCount() const
  {
    return _colours.size() / 3;
  }
} // quake
