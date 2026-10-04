#include "saved-game-text.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <utility>

#include "formats/entity-text.hpp"

namespace quake
{
  // Helpers of SavedGameText: the words of the head, read and written.
  namespace
  {
    /// What ends a word of the head, as for `%s` of C.
    bool IsSpace(const char letter)
    {
      return letter == ' ' || (letter >= '\t' && letter <= '\r');
    }

    /// A number as `%f` of C prints it.
    std::string PrintNumber(const double value)
    {
      // the largest double has 309 digits before the point
      std::array<char, 400> text{};
      const int length = std::snprintf(text.data(), text.size(), "%f", value);
      return length > 0 ? std::string(text.data(), static_cast<std::size_t>(length)) : std::string();
    }

    /// A word of the head that can be read back: no white space, and never
    /// empty.
    std::string AsWord(const std::string_view text)
    {
      std::string word(text.substr(0, SavedGameText::max_token_length));
      std::ranges::replace_if(word, IsSpace, '_');
      return word.empty() ? "_" : word;
    }

    /// A name or a value that can stand between two quotes.
    std::string AsQuoted(const std::string_view text)
    {
      std::string quoted(text.substr(0, SavedGameText::max_token_length));
      std::ranges::replace(quoted, '"', '\'');
      return quoted;
    }

    void WriteBlock(std::string &text, const std::vector<std::pair<std::string, std::string>> &pairs)
    {
      text += "{\n";
      for (const auto &[name, value] : pairs)
      {
        text += std::format("\"{}\" \"{}\"\n", AsQuoted(name), AsQuoted(value));
      }
      text += "}\n";
    }

    /// The head of a saved game while it is read, a word at a time.
    class Head final
    {
      std::string_view _text;
      std::size_t _position = 0;

    public:
      explicit Head(const std::string_view text) : _text(text)
      {
      }

      /// Where the head has been read up to.
      [[nodiscard]] std::size_t GetPosition() const
      {
        return _position;
      }

      /// The next word. `what` names it for the error.
      bool ReadWord(const std::string_view what, std::string &word, std::string &error)
      {
        while (_position < _text.size() && IsSpace(_text[_position])) { _position++; }
        if (_position >= _text.size())
        {
          error = std::format("The saved game ends where {} is due", what);
          return false;
        }

        const std::size_t start = _position;
        while (_position < _text.size() && !IsSpace(_text[_position])) { _position++; }
        if (_position - start > SavedGameText::max_token_length)
        {
          error = std::format(
            "The saved game has {} bytes for {}, and at most {} are taken", _position - start, what,
            SavedGameText::max_token_length);
          return false;
        }
        word = _text.substr(start, _position - start);
        return true;
      }

      /// The next word as a number.
      bool ReadNumber(const std::string_view what, double &number, std::string &error)
      {
        std::string word;
        if (!ReadWord(what, word, error)) { return false; }

        char *end = nullptr;
        number = std::strtod(word.c_str(), &end);
        if (end == word.c_str() || *end != '\0' || !std::isfinite(number))
        {
          error = std::format("The saved game has `{}` for {}, which is no number", word, what);
          return false;
        }
        return true;
      }
    };
  }

  std::string SavedGameText::Write(const SavedGame &game)
  {
    std::string text = std::format("{}\n{}\n", game.version, AsWord(game.comment));
    for (const float parm : game.parms) { text += PrintNumber(parm) + "\n"; }
    text += std::format("{}\n{}\n{}\n", game.skill, AsWord(game.map_name), PrintNumber(game.time));

    for (const std::string &style : game.light_styles)
    {
      std::string letters(style.substr(0, max_token_length));
      std::erase_if(letters, IsSpace);
      text += (letters.empty() ? "m" : letters) + "\n";
    }

    WriteBlock(text, game.globals);
    for (const SavedGameEntity &entity : game.entities)
    {
      // a free one is a block with nothing in it
      if (entity.free) { text += "{\n}\n"; }
      else { WriteBlock(text, entity.pairs); }
    }
    return text;
  }

  bool SavedGameText::Read(std::string_view text, SavedGame &game, std::string &error)
  {
    if (text.size() > max_text_size)
    {
      error = std::format("The saved game has {} bytes, and at most {} are taken", text.size(), max_text_size);
      return false;
    }

    // a zero ends the text, and a line of Windows ends as any other
    text = text.substr(0, text.find('\0'));
    std::string plain;
    plain.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); i++)
    {
      if (text[i] == '\r' && i + 1 < text.size() && text[i + 1] == '\n') { continue; }
      plain += text[i];
    }

    SavedGame read;
    Head head(plain);

    double number = 0.0;
    if (!head.ReadNumber("the version", number, error)) { return false; }
    if (number != static_cast<double>(SavedGame::current_version))
    {
      error = std::format(
        "The saved game is of version {}, and only version {} is read", number, SavedGame::current_version);
      return false;
    }

    if (!head.ReadWord("the comment", read.comment, error)) { return false; }
    for (std::size_t i = 0; i < read.parms.size(); i++)
    {
      if (!head.ReadNumber(std::format("parm {}", i + 1), number, error)) { return false; }
      read.parms[i] = static_cast<float>(number);
    }

    // the first games that were sold wrote the skill as a float
    if (!head.ReadNumber("the skill", number, error)) { return false; }
    read.skill = static_cast<int>(std::clamp(number, 0.0, 3.0) + 0.1);

    if (!head.ReadWord("the name of the level", read.map_name, error)) { return false; }
    if (!head.ReadNumber("the time", read.time, error)) { return false; }
    for (std::size_t i = 0; i < read.light_styles.size(); i++)
    {
      if (!head.ReadWord(std::format("light style {}", i), read.light_styles[i], error)) { return false; }
    }

    // The rest is blocks as the text of a level has them. The head is
    // blanked and not cut off, so that an error names the line of the file.
    std::string body = plain;
    for (std::size_t i = 0; i < head.GetPosition(); i++)
    {
      if (body[i] != '\n') { body[i] = ' '; }
    }

    // what an engine of today adds after the last entity
    const std::size_t last_block_end = body.rfind('}');
    if (last_block_end != std::string::npos)
    {
      std::size_t after = last_block_end + 1;
      while (after < body.size() && IsSpace(body[after])) { after++; }
      if (body.compare(after, 2, "/*") == 0) { body.resize(last_block_end + 1); }
    }

    EntityText blocks;
    if (!blocks.Read(body, error))
    {
      error = "The saved game is not read: " + error;
      return false;
    }
    if (blocks.entities.empty())
    {
      error = "The saved game has no block of globals";
      return false;
    }
    if (blocks.entities.size() - 1 > max_entities)
    {
      error = std::format(
        "The saved game has {} entities, and at most {} are taken", blocks.entities.size() - 1, max_entities);
      return false;
    }

    for (std::size_t i = 0; i < blocks.entities.size(); i++)
    {
      auto &pairs = blocks.entities[i].pairs;
      const std::string which = i == 0 ? "The globals" : std::format("Entity {}", i - 1);
      if (pairs.size() > max_pairs)
      {
        error = std::format(
          "{} of the saved game has {} pairs, and at most {} are taken", which, pairs.size(), max_pairs);
        return false;
      }
      for (const auto &[name, value] : pairs)
      {
        if (name.size() > max_token_length || value.size() > max_token_length)
        {
          error = std::format(
            "{} of the saved game has a name or a value of more than {} bytes", which, max_token_length);
          return false;
        }
      }

      if (i == 0)
      {
        read.globals = std::move(pairs);
        continue;
      }
      const bool free = pairs.empty();
      read.entities.push_back({.free = free, .pairs = std::move(pairs)});
    }

    game = std::move(read);
    return true;
  }

  std::string SavedGameText::MakeComment(
    const std::string_view level_name, const int killed_monsters, const int total_monsters)
  {
    std::string comment(comment_length, ' ');
    comment.replace(0, std::min(level_name.size(), comment_level_length), level_name.substr(0, comment_level_length));

    const std::string kills = std::format("kills:{:3}/{:3}", killed_monsters, total_monsters);
    const std::size_t kills_length = std::min(kills.size(), comment_length - comment_level_length);
    comment.replace(comment_level_length, kills_length, kills, 0, kills_length);

    // a space would end the word the comment is read as
    for (char &letter : comment)
    {
      if (static_cast<unsigned char>(letter) <= ' ') { letter = '_'; }
    }
    return comment;
  }
} // quake
