#ifndef QUAKE_SAVED_GAME_TEXT_HPP
#define QUAKE_SAVED_GAME_TEXT_HPP

#include <cstddef>
#include <string>
#include <string_view>

#include "saved-game.hpp"

namespace quake
{
  /// The file of a saved game, which is a text, as the original writes and
  /// reads it:
  ///
  /// ```
  /// 5                                          the version
  /// the_Slipgate_Complex__kills:__3/_42____    the comment
  /// 100.000000                                 16 lines: the parms
  /// 1                                          the skill
  /// e1m1                                       the name of the level
  /// 12.345678                                  the time
  /// m                                          64 lines: the light styles
  /// {                                          the globals
  /// "killed_monsters" "3.000000"
  /// }
  /// {                                          the world, then every entity
  /// "classname" "worldspawn"
  /// }
  /// {                                          a free entity
  /// }
  /// ```
  ///
  /// Lines end with a new line alone. The original reads the lines of the
  /// head as words, so none of them can be empty or hold a space, and it
  /// has no way to write a quote inside a value. Write() therefore changes
  /// what could not be read back:
  ///
  /// - White space in the comment and in the name of the level is `_`, and
  ///   an empty one is `_`.
  /// - A light style without a text is `m`, the light as it is, which is
  ///   what the original writes for one that was never set. White space in
  ///   one is left out.
  /// - A quote in a name or a value is `'`.
  /// - A name or a value longer than `max_token_length` is cut there, as
  ///   by the buffer of the original.
  ///
  /// A new line in a value is written as it is, and read as it is. Numbers
  /// have six places after the point, `%f` of C, so a float comes back as
  /// near as that and not bit for bit.
  class SavedGameText final
  {
  public:
    /// How many bytes a name or a value of a pair, and a word of the head,
    /// may have. The buffer of the original holds as many.
    static constexpr std::size_t max_token_length = 1023;

    /// How many bytes Read() takes a text of.
    static constexpr std::size_t max_text_size = 64u * 1024u * 1024u;

    /// How many entities Read() takes. The engines of today stop at as
    /// many.
    static constexpr std::size_t max_entities = 32000;

    /// How many pairs the globals and each entity may have.
    static constexpr std::size_t max_pairs = 8192;

    /// How long the comment is, and where in it the kills start.
    static constexpr std::size_t comment_length = 39;
    static constexpr std::size_t comment_level_length = 22;

    /// The text of a saved game.
    [[nodiscard]] static std::string Write(const SavedGame &game);

    /// Takes a saved game from its text. Returns false, says in `error`
    /// what was wrong, and leaves `game` as it was, when the text is of
    /// another version than 5, a line of the head is missing or is no
    /// number where one is due, a block does not close, a name has no
    /// value, there are no globals, or one of the limits above is passed.
    ///
    /// An entity without pairs is free. Line ends of Windows are taken as
    /// plain ones. What an engine of today adds after the last entity, a
    /// comment between `/*` and `*/`, is left out.
    static bool Read(std::string_view text, SavedGame &game, std::string &error);

    /// The comment as the original makes it, always `comment_length` long:
    /// the name of the level as it is shown, the `message` of the world, or
    /// the name of its file when it has none, cut to
    /// `comment_level_length`, then `kills:` with how many monsters were
    /// killed and how many there are, three places each, with `_` for
    /// every space and for what would end a line.
    [[nodiscard]] static std::string MakeComment(std::string_view level_name, int killed_monsters, int total_monsters);
  };
} // quake

#endif //QUAKE_SAVED_GAME_TEXT_HPP
