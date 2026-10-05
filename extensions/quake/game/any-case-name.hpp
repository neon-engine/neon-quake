#ifndef QUAKE_ANY_CASE_NAME_HPP
#define QUAKE_ANY_CASE_NAME_HPP

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace quake
{
  /// The name among `names` that is `wanted` in any letter case, as it is
  /// written there: `PAK0.PAK` for `pak0.pak`. Nothing when none is.
  ///
  /// The player brings the data of the game, and it does not always come in
  /// the case the game asks for: the original release on Steam names its
  /// archives `PAK0.PAK` and `PAK1.PAK`. The engine only opens a file by its
  /// name as it is on disk, so that what a game asks for works the same on
  /// every platform. So the folder is listed, the name is found in it here,
  /// and the file is opened by the name it has. Nothing is renamed.
  ///
  /// A name in the very case asked for wins over one in another case, where
  /// a file system that tells case apart holds both. Only the letters A to Z
  /// are compared without their case, which is all the game's names use.
  [[nodiscard]] std::optional<std::string> FindAnyCaseName(
    const std::vector<std::string> &names, std::string_view wanted);
} // quake

#endif //QUAKE_ANY_CASE_NAME_HPP
