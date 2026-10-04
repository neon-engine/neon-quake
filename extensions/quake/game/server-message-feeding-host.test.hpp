#ifndef QUAKE_SERVER_MESSAGE_FEEDING_HOST_TEST_HPP
#define QUAKE_SERVER_MESSAGE_FEEDING_HOST_TEST_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "qc-host.hpp"
#include "qc-message-destination.hpp"
#include "qc-message-value.hpp"
#include "server-message-reader.hpp"

namespace quake
{
  /// A host for the tests, which hands every part of a message the game
  /// code writes to a `ServerMessageReader`, as a host of the game is to,
  /// and writes down which levels the game code asked for.
  class ServerMessageFeedingHost final : public QcHost
  {
    ServerMessageReader &_reader;

  public:
    /// How many parts the game code wrote.
    std::size_t written = 0;

    /// The levels `changelevel` was called with.
    std::vector<std::string> levels;

    /// The reader has to outlive this.
    explicit ServerMessageFeedingHost(ServerMessageReader &reader)
      : _reader(reader)
    {
    }

    void ChangeLevel(const std::string_view level) override
    {
      levels.emplace_back(level);
    }

    void WriteMessage(
      const QcMessageDestination destination, const std::int32_t client, const QcMessageValue &value) override
    {
      written++;
      _reader.Read(destination, client, value);
    }
  };
} // quake

#endif //QUAKE_SERVER_MESSAGE_FEEDING_HOST_TEST_HPP
