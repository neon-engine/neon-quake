#include "basedir-file.hpp"

namespace quake
{
  // what the anonymous namespace holds: the cutting of a line of the file
  namespace
  {
    std::string_view Trimmed(std::string_view text)
    {
      const std::size_t first = text.find_first_not_of(" \t\r");
      if (first == std::string_view::npos) { return {}; }
      return text.substr(first, text.find_last_not_of(" \t\r") - first + 1);
    }
  }

  std::string BasedirFile::Read(std::string_view text)
  {
    static constexpr std::string_view key = "basedir:";

    while (!text.empty())
    {
      const std::size_t end = text.find('\n');
      std::string_view line = text.substr(0, end);
      text = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);

      // a comment that follows the value goes, and the line has to start
      // with the name, so that one indented below another is not it
      if (const std::size_t comment = line.find(" #"); comment != std::string_view::npos) { line = line.substr(0, comment); }
      if (!line.starts_with(key)) { continue; }

      std::string_view value = Trimmed(line.substr(key.size()));
      if (value.size() >= 2 && (value.front() == '"' || value.front() == '\'') && value.back() == value.front())
      {
        value = value.substr(1, value.size() - 2);
      }
      return std::string(value);
    }
    return "";
  }

  std::string BasedirFile::Write(const std::string_view name)
  {
    return "# The copy of the game's data the game starts with, under assets://basedirs/.\n"
           "# The game writes it when one is picked; remove it to be asked again.\n"
           "version: 1\n"
           "basedir: " + std::string(name) + "\n";
  }
} // quake
