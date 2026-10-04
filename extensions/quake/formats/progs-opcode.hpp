#ifndef QUAKE_PROGS_OPCODE_HPP
#define QUAKE_PROGS_OPCODE_HPP

#include <cstdint>

namespace quake
{
  /// What a statement of the game code does, by the number the compiler
  /// writes for it. These are the 66 of the original; the letters after the
  /// name say what it works on: F a float, V a vector, S a string, E or Ent
  /// an entity, Fld a field, Fnc a function.
  enum class ProgsOpcode : std::uint16_t
  {
    Done = 0,
    MulF = 1,
    MulV = 2,
    MulFV = 3,
    MulVF = 4,
    DivF = 5,
    AddF = 6,
    AddV = 7,
    SubF = 8,
    SubV = 9,
    EqF = 10,
    EqV = 11,
    EqS = 12,
    EqE = 13,
    EqFnc = 14,
    NeF = 15,
    NeV = 16,
    NeS = 17,
    NeE = 18,
    NeFnc = 19,
    Le = 20,
    Ge = 21,
    Lt = 22,
    Gt = 23,
    LoadF = 24,
    LoadV = 25,
    LoadS = 26,
    LoadEnt = 27,
    LoadFld = 28,
    LoadFnc = 29,
    Address = 30,
    StoreF = 31,
    StoreV = 32,
    StoreS = 33,
    StoreEnt = 34,
    StoreFld = 35,
    StoreFnc = 36,
    StorepF = 37,
    StorepV = 38,
    StorepS = 39,
    StorepEnt = 40,
    StorepFld = 41,
    StorepFnc = 42,
    Return = 43,
    NotF = 44,
    NotV = 45,
    NotS = 46,
    NotEnt = 47,
    NotFnc = 48,
    If = 49,
    IfNot = 50,
    Call0 = 51,
    Call1 = 52,
    Call2 = 53,
    Call3 = 54,
    Call4 = 55,
    Call5 = 56,
    Call6 = 57,
    Call7 = 58,
    Call8 = 59,
    State = 60,
    Goto = 61,
    And = 62,
    Or = 63,
    BitAnd = 64,
    BitOr = 65,
  };
} // quake

#endif //QUAKE_PROGS_OPCODE_HPP
