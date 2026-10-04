#include "entity-text.hpp"

#include <cstddef>
#include <utility>

namespace quake
{
  // Helpers of EntityText: the place in the text that is being read, which
  // knows its line and column for the errors.
  namespace
  {
    class Cursor
    {
      std::string_view _text;
      std::size_t _position = 0;
      std::size_t _line = 1;
      std::size_t _column = 1;

    public:
      explicit Cursor(const std::string_view text)
      {
        _text = text;
      }

      /// Whether nothing is left. A zero ends the text, as it ends the lump
      /// of a level.
      [[nodiscard]] bool IsAtEnd() const
      {
        return _position >= _text.size() || _text[_position] == '\0';
      }

      [[nodiscard]] char Peek() const
      {
        return IsAtEnd() ? '\0' : _text[_position];
      }

      /// "line 3, column 5", counted from one.
      [[nodiscard]] std::string GetPlace() const
      {
        return "line " + std::to_string(_line) + ", column " + std::to_string(_column);
      }

      void Advance()
      {
        if (IsAtEnd()) { return; }

        if (_text[_position] == '\n')
        {
          _line++;
          _column = 1;
        } else
        {
          _column++;
        }
        _position++;
      }

      /// Goes past spaces, ends of lines, and comments, which start with
      /// `//` and end with their line.
      void SkipBlanks()
      {
        while (!IsAtEnd())
        {
          const char letter = Peek();
          if (letter == ' ' || letter == '\t' || letter == '\r' || letter == '\n')
          {
            Advance();
          } else if (letter == '/' && _position + 1 < _text.size() && _text[_position + 1] == '/')
          {
            while (!IsAtEnd() && Peek() != '\n') { Advance(); }
          } else
          {
            return;
          }
        }
      }

      /// Reads what stands between two quotes, the first of which is where
      /// the cursor is. A backslash is a letter like any other here: the
      /// game code makes an end of line of `\n` in the texts it shows.
      bool ReadQuoted(std::string &quoted, std::string &error)
      {
        const std::string start = GetPlace();
        Advance();

        quoted.clear();
        while (!IsAtEnd() && Peek() != '"')
        {
          quoted.push_back(Peek());
          Advance();
        }

        if (IsAtEnd())
        {
          error = GetPlace() + ": the text ends inside the quotes opened at " + start;
          return false;
        }
        Advance();
        return true;
      }
    };

    /// How a letter is shown in an error.
    std::string show(const char letter)
    {
      return std::string("`") + letter + "`";
    }
  }

  bool EntityText::Read(const std::string_view text, std::string &error)
  {
    std::vector<BspEntity> read;
    Cursor cursor(text);

    while (true)
    {
      cursor.SkipBlanks();
      if (cursor.IsAtEnd()) { break; }

      if (cursor.Peek() != '{')
      {
        error = cursor.GetPlace() + ": an entity starts with `{`, and " + show(cursor.Peek()) + " was found";
        return false;
      }
      const std::string start = cursor.GetPlace();
      cursor.Advance();

      BspEntity entity;
      while (true)
      {
        cursor.SkipBlanks();
        if (cursor.IsAtEnd())
        {
          error = cursor.GetPlace() + ": the text ends inside the entity opened at " + start;
          return false;
        }

        if (cursor.Peek() == '}')
        {
          cursor.Advance();
          break;
        }

        if (cursor.Peek() != '"')
        {
          error = cursor.GetPlace() + ": a key in quotes or `}` was expected, and " + show(cursor.Peek()) +
            " was found";
          return false;
        }

        std::string key;
        if (!cursor.ReadQuoted(key, error)) { return false; }

        cursor.SkipBlanks();
        if (cursor.IsAtEnd() || cursor.Peek() != '"')
        {
          error = cursor.GetPlace() + ": the key \"" + key + "\" has no value in quotes";
          return false;
        }

        std::string value;
        if (!cursor.ReadQuoted(value, error)) { return false; }

        entity.pairs.emplace_back(std::move(key), std::move(value));
      }
      read.push_back(std::move(entity));
    }

    entities = std::move(read);
    return true;
  }
} // quake
