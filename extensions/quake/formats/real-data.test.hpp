#ifndef QUAKE_REAL_DATA_TEST_HPP
#define QUAKE_REAL_DATA_TEST_HPP

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "pak-file.hpp"
#include "pak-layers.hpp"

namespace quake
{
  /// The data of a real game, for the tests that check a format against
  /// what a real compiler or editor wrote: `pak0.pak` of the folder that
  /// `QUAKE_TEST_DATA_DIRECTORY` names, with `pak1.pak` over it when the
  /// folder has one, read once for a test program.
  ///
  /// The data is not always there: a checkout without Git LFS has a few
  /// lines of text in the place of a pak. A test asks IsThere() first and
  /// skips itself when it is not.
  class RealData
  {
    /// The bytes of each pak, which the layers hand parts of out.
    std::array<std::vector<std::uint8_t>, 2> _bytes;
    PakLayers _paks;
    bool _is_there = false;
    std::string _problem;

    /// Reads a file of the folder whole. False, with the reason in
    /// `_problem`, when it is not there or is not the file itself.
    bool ReadFile(const std::string_view name, std::vector<std::uint8_t> &bytes)
    {
      const std::filesystem::path path = std::filesystem::path(QUAKE_TEST_DATA_DIRECTORY) / name;
      std::ifstream stream(path, std::ios::binary);
      if (!stream)
      {
        _problem = "There is no " + path.string();
        return false;
      }

      // in one read: a pak is many megabytes, and every test reads it anew
      std::error_code ignored;
      const std::uintmax_t size = std::filesystem::file_size(path, ignored);
      bytes.resize(ignored ? 0 : static_cast<std::size_t>(size));
      stream.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
      bytes.resize(static_cast<std::size_t>(stream.gcount()));

      // what Git LFS leaves where the file is not downloaded
      constexpr std::string_view pointer_start = "version https://git-lfs";
      const std::string_view start(
        reinterpret_cast<const char *>(bytes.data()), std::min(bytes.size(), pointer_start.size()));
      if (start == pointer_start)
      {
        _problem = path.string() + " is a pointer of Git LFS, not the pak itself";
        return false;
      }
      return true;
    }

    RealData()
    {
      if (!ReadFile("pak0.pak", _bytes[0])) { return; }

      // a pak that is there and is refused is not skipped over: the tests
      // then fail for the file they do not find
      _is_there = true;
      PakFile pak;
      if (!pak.Read(_bytes[0], _problem))
      {
        _problem = "pak0.pak: " + _problem;
        return;
      }
      _paks.Add(std::move(pak));

      // the second pak is the rest of the game, which a folder need not have
      std::string ignored;
      if (ReadFile("pak1.pak", _bytes[1]) && pak.Read(_bytes[1], ignored)) { _paks.Add(std::move(pak)); }
      _problem.clear();
    }

  public:
    /// The data, read when it is asked for the first time.
    static const RealData &Get()
    {
      static const RealData data;
      return data;
    }

    /// Whether there is a pak to test with.
    [[nodiscard]] bool IsThere() const
    {
      return _is_there;
    }

    /// Why there is no pak, or why it was refused.
    [[nodiscard]] const std::string &GetProblem() const
    {
      return _problem;
    }

    /// The bytes of a file of the paks. Empty when they have no such file.
    [[nodiscard]] std::span<const std::uint8_t> GetBytes(const std::string_view name) const
    {
      return _paks.GetBytes(name);
    }

    /// The names of the files of the paks under a folder, sorted.
    [[nodiscard]] std::vector<std::string> ListNames(const std::string_view folder) const
    {
      return _paks.ListNames(folder);
    }
  };
} // quake

#endif //QUAKE_REAL_DATA_TEST_HPP
