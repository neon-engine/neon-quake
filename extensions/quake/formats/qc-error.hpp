#ifndef QUAKE_QC_ERROR_HPP
#define QUAKE_QC_ERROR_HPP

#include <cstdint>
#include <string>

namespace quake
{
  /// Why `QcMachine` stopped a run, and where the program was.
  struct QcError
  {
    /// What went wrong, as a sentence.
    std::string message;

    /// The function that was running, by number and by name. Function 0,
    /// without a name, when the run never got into one.
    std::int32_t function = 0;
    std::string function_name;

    /// The statement that was running, or -1 when none was.
    std::int32_t statement = -1;
  };
} // quake

#endif //QUAKE_QC_ERROR_HPP
