#ifndef QUAKE_QC_CELL_HPP
#define QUAKE_QC_CELL_HPP

#include <bit>
#include <cstdint>

namespace quake
{
  /// One cell of the memory of the game code: 32 bits that are a float or an
  /// integer, as whoever reads them takes them. Nothing in a cell says which.
  ///
  /// As an integer it is the offset of a string, the number of an entity or
  /// of a function, the offset of a field in an entity, or a pointer to a
  /// field of an entity. A vector is three cells in a row, each a float.
  struct QcCell
  {
    std::uint32_t bits = 0;

    [[nodiscard]] static QcCell OfFloat(const float value)
    {
      return {std::bit_cast<std::uint32_t>(value)};
    }

    [[nodiscard]] static QcCell OfInteger(const std::int32_t value)
    {
      return {static_cast<std::uint32_t>(value)};
    }

    [[nodiscard]] float AsFloat() const
    {
      return std::bit_cast<float>(bits);
    }

    [[nodiscard]] std::int32_t AsInteger() const
    {
      return static_cast<std::int32_t>(bits);
    }
  };
} // quake

#endif //QUAKE_QC_CELL_HPP
