#ifndef QUAKE_LIGHT_STYLES_HPP
#define QUAKE_LIGHT_STYLES_HPP

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

namespace quake
{
  /// How bright each of the lights of a level is at a time: steady, or
  /// flickering, or pulsing, or switched on and off by the game.
  ///
  /// Every lightmap of a face names a style, a number below `count`, and is
  /// shown multiplied by what the style is worth at the moment. A style is
  /// a text of letters, each of which is the brightness for a tenth of a
  /// second: `a` is dark, `m` is the light as the level has it, and `z` is
  /// about twice that. When the text is over it starts again.
  ///
  /// Style 0 is the light that stays as it is. Styles 1 to 11 are the
  /// flickers and pulses a mapper chooses from. Styles 32 to 62 belong to
  /// lights that are switched: the game code gives them `"a"` when they go
  /// out and `"m"` when they go on. The game code sets every style it
  /// uses with its builtin `lightstyle`, which is `Set` here, also 0 to 11
  /// when a level starts. Until then 0 to 11 have the texts of the original
  /// game, so that a level shown without game code flickers as it should.
  ///
  /// A style without a text is the light as the level has it, as in the
  /// original, whose renderer takes an empty style for a normal one.
  class LightStyles
  {
  public:
    /// How many styles there are.
    static constexpr std::size_t count = 64;

    /// How many letters of a style go by in a second.
    static constexpr double letters_per_second = 10.0;

    /// What each style is worth: 0 is dark, 1 is the light as it is.
    using Values = std::array<float, count>;

  private:
    std::array<std::string, count> _texts;

  public:
    /// Styles 0 to 11 as the original game has them, the others empty.
    LightStyles();

    /// Gives a style its text. Returns false, and changes nothing, when
    /// there is no such style.
    bool Set(std::size_t style, std::string_view text);

    /// The text of a style. Empty when there is no such style.
    [[nodiscard]] const std::string &GetText(std::size_t style) const;

    /// What a letter of a style is worth: `a` is 0, `m` is 1, `z` is a bit
    /// more than 2. What is not a letter from `a` to `z` counts as the
    /// nearest of them.
    [[nodiscard]] static float GetValueOfLetter(char letter);

    /// Which letter of a text of `length` letters is shown at a time in
    /// seconds: ten a second, from the start again when the text is over.
    /// A time before 0 is 0.
    [[nodiscard]] static std::size_t GetLetterAt(std::size_t length, double time);

    /// What a style is worth at a time in seconds. 1 for a style without a
    /// text, and for a style there is not.
    [[nodiscard]] float GetValue(std::size_t style, double time) const;

    /// What every style is worth at a time in seconds, which is what
    /// `LightmapAtlas::Compose` takes.
    [[nodiscard]] Values GetValues(double time) const;
  };
} // quake

#endif //QUAKE_LIGHT_STYLES_HPP
