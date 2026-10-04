#include "pak-file.hpp"

#include <algorithm>
#include <utility>

#include "byte-reader.hpp"

namespace quake
{
  // Helpers of PakFile: the checks of what the file says about itself.
  namespace
  {
    /// Whether the bytes are the four letters a pak file starts with.
    bool is_magic(const std::span<const std::uint8_t> bytes)
    {
      return std::ranges::equal(
        bytes,
        PakHeader::magic,
        [](const std::uint8_t byte, const char letter) { return byte == static_cast<std::uint8_t>(letter); });
    }

    /// Whether `size` bytes from `offset` all lie inside a file of
    /// `file_size` bytes. Written so that nothing is added that could wrap
    /// around.
    bool is_inside(const std::size_t offset, const std::size_t size, const std::size_t file_size)
    {
      return offset <= file_size && size <= file_size - offset;
    }

    /// Whether a name is that of a file under a folder. A folder may be
    /// given with or without its last slash, and no folder holds everything.
    bool is_under(const std::string_view name, std::string_view folder)
    {
      if (folder.ends_with('/')) { folder.remove_suffix(1); }
      if (folder.empty()) { return true; }

      return name.size() > folder.size() && name.starts_with(folder) && name[folder.size()] == '/';
    }
  }

  bool PakFile::Read(const std::span<const std::uint8_t> bytes, std::string &error)
  {
    ByteReader reader(bytes);

    const std::span<const std::uint8_t> magic = reader.ReadBytes(PakHeader::magic.size());
    const std::int32_t directory_offset = reader.ReadI32();
    const std::int32_t directory_size = reader.ReadI32();

    if (!reader.IsGood())
    {
      error = "A pak file starts with a header of " + std::to_string(PakHeader::size_in_bytes) +
        " bytes, and this has only " + std::to_string(bytes.size());
      return false;
    }
    if (!is_magic(magic))
    {
      error = "A pak file starts with the letters PACK, and this does not";
      return false;
    }
    if (directory_offset < 0)
    {
      error = "The directory starts at " + std::to_string(directory_offset) + ", before the start of the file";
      return false;
    }
    if (directory_size < 0)
    {
      error = "The directory has a size of " + std::to_string(directory_size) + " bytes, less than nothing";
      return false;
    }

    const PakHeader header{
      .directory_offset = static_cast<std::size_t>(directory_offset),
      .directory_size = static_cast<std::size_t>(directory_size),
    };

    if (header.directory_size % PakEntry::size_in_bytes != 0)
    {
      error = "The directory has " + std::to_string(header.directory_size) +
        " bytes, which is not a number of entries of " + std::to_string(PakEntry::size_in_bytes) + " bytes";
      return false;
    }
    if (!is_inside(header.directory_offset, header.directory_size, bytes.size()))
    {
      error = "The directory, " + std::to_string(header.directory_size) + " bytes at " +
        std::to_string(header.directory_offset) + ", lies outside the file of " + std::to_string(bytes.size()) +
        " bytes";
      return false;
    }

    const std::size_t count = header.directory_size / PakEntry::size_in_bytes;
    std::vector<PakEntry> entries;
    entries.reserve(count);

    reader.Seek(header.directory_offset);
    for (std::size_t index = 0; index < count; index++)
    {
      std::string name = reader.ReadFixedString(PakEntry::name_size);
      const std::int32_t offset = reader.ReadI32();
      const std::int32_t size = reader.ReadI32();

      const std::string which = "Entry " + std::to_string(index) + " of the directory, `" + name + "`, ";
      if (offset < 0)
      {
        error = which + "starts at " + std::to_string(offset) + ", before the start of the file";
        return false;
      }
      if (size < 0)
      {
        error = which + "has a size of " + std::to_string(size) + " bytes, less than nothing";
        return false;
      }
      if (!is_inside(static_cast<std::size_t>(offset), static_cast<std::size_t>(size), bytes.size()))
      {
        error = which + std::to_string(size) + " bytes at " + std::to_string(offset) + ", lies outside the file of " +
          std::to_string(bytes.size()) + " bytes";
        return false;
      }

      entries.push_back(PakEntry{
        .name = std::move(name),
        .offset = static_cast<std::size_t>(offset),
        .size = static_cast<std::size_t>(size),
      });
    }

    // the directory was found to lie inside the file, so this cannot happen, and is asked all the same
    if (!reader.IsGood())
    {
      error = "The directory could not be read to its end";
      return false;
    }

    _bytes = bytes;
    _header = header;
    _entries = std::move(entries);
    return true;
  }

  const PakHeader &PakFile::GetHeader() const
  {
    return _header;
  }

  const std::vector<PakEntry> &PakFile::GetEntries() const
  {
    return _entries;
  }

  const PakEntry *PakFile::Find(const std::string_view name) const
  {
    // a pak of the game has a few hundred entries, and going through them is what the game does too
    const auto found = std::ranges::find(_entries, name, &PakEntry::name);
    return found == _entries.end() ? nullptr : &*found;
  }

  bool PakFile::Has(const std::string_view name) const
  {
    return Find(name) != nullptr;
  }

  std::vector<std::string> PakFile::ListNames(const std::string_view folder) const
  {
    std::vector<std::string> names;
    for (const PakEntry &entry : _entries)
    {
      if (is_under(entry.name, folder)) { names.push_back(entry.name); }
    }
    return names;
  }

  std::span<const std::uint8_t> PakFile::GetBytes(const PakEntry &entry) const
  {
    // asked through the reader, so that an entry of another pak cannot reach outside this one
    return ByteReader(_bytes).BytesAt(entry.offset, entry.size);
  }

  std::span<const std::uint8_t> PakFile::GetBytes(const std::string_view name) const
  {
    const PakEntry *entry = Find(name);
    if (entry == nullptr) { return {}; }
    return GetBytes(*entry);
  }
} // quake
