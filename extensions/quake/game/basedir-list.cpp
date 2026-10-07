#include "basedir-list.hpp"

#include <algorithm>
#include <optional>

#include "any-case-name.hpp"

namespace quake
{
  std::vector<Basedir> BasedirList::Find(
    const std::vector<std::string> &folders,
    const std::function<std::vector<std::string>(const std::string &folder)> &folders_in)
  {
    // the data straight under the folder is the one the player put there
    if (const std::optional<std::string> id1 = FindAnyCaseName(folders, "id1"))
    {
      return {Basedir{.name = std::string(default_name), .data_folder = std::string(folder) + *id1 + "/"}};
    }

    std::vector<Basedir> found;
    for (const std::string &name : folders)
    {
      const std::string path = std::string(folder) + name + "/";
      if (const std::optional<std::string> id1 = FindAnyCaseName(folders_in(path), "id1"))
      {
        found.push_back(Basedir{.name = name, .data_folder = path + *id1 + "/"});
      }
    }

    std::ranges::sort(found, {}, &Basedir::name);
    return found;
  }
} // quake
