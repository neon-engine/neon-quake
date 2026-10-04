#ifndef QUAKE_QC_RECORDING_HOST_TEST_HPP
#define QUAKE_QC_RECORDING_HOST_TEST_HPP

#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <vector>

#include "qc-host.hpp"
#include "qc-message-destination.hpp"
#include "qc-message-kind.hpp"
#include "qc-message-value.hpp"

namespace quake
{
  /// A host for the tests, which writes down everything the builtins hand
  /// on to it: a line for each call, with the name of the builtin it came
  /// from and what it carried.
  class QcRecordingHost final : public QcHost
  {
  public:
    /// Every call in the order it came, such as `sprint 1: hello`.
    std::vector<std::string> calls;

    /// Everything that was printed for a player or the console, as one text.
    std::string printed;

    void PrintToAll(const std::string_view text) override
    {
      calls.push_back(std::format("bprint: {}", text));
      printed += text;
    }

    void PrintToClient(const std::int32_t client, const std::string_view text) override
    {
      calls.push_back(std::format("sprint {}: {}", client, text));
      printed += text;
    }

    void PrintToConsole(const std::string_view text) override
    {
      calls.push_back(std::format("dprint: {}", text));
      printed += text;
    }

    void PrintToCenter(const std::int32_t client, const std::string_view text) override
    {
      calls.push_back(std::format("centerprint {}: {}", client, text));
      printed += text;
    }

    void PrintEntity(const std::int32_t entity) override
    {
      calls.push_back(std::format("eprint {}", entity));
    }

    void Error(const std::int32_t self, const std::string_view text) override
    {
      calls.push_back(std::format("error {}: {}", self, text));
    }

    void ObjectError(const std::int32_t entity, const std::string_view text) override
    {
      calls.push_back(std::format("objerror {}: {}", entity, text));
    }

    void EntityMade(const std::int32_t entity) override
    {
      calls.push_back(std::format("made {}", entity));
    }

    void EntityRemoved(const std::int32_t entity) override
    {
      calls.push_back(std::format("removed {}", entity));
    }

    void LightStyleSet(const std::int32_t style, const std::string_view text) override
    {
      calls.push_back(std::format("lightstyle {}: {}", style, text));
    }

    void ClientCommand(const std::int32_t client, const std::string_view text) override
    {
      calls.push_back(std::format("stuffcmd {}: {}", client, text));
    }

    void ServerCommand(const std::string_view text) override
    {
      calls.push_back(std::format("localcmd: {}", text));
    }

    void ChangeLevel(const std::string_view level) override
    {
      calls.push_back(std::format("changelevel: {}", level));
    }

    void SetSpawnParameters(const std::int32_t client) override
    {
      calls.push_back(std::format("setspawnparms {}", client));
    }

    void WriteMessage(
      const QcMessageDestination destination, const std::int32_t client, const QcMessageValue &value) override
    {
      constexpr std::string_view kinds[] = {"byte", "char", "short", "long", "coord", "angle", "string", "entity"};
      const std::string_view kind = kinds[static_cast<std::size_t>(value.kind)];
      const std::string to =
        std::format("write to {} for {}: {}", static_cast<std::int32_t>(destination), client, kind);
      switch (value.kind)
      {
        case QcMessageKind::String:
        {
          calls.push_back(std::format("{} {}", to, value.text));
          break;
        }
        case QcMessageKind::Entity:
        {
          calls.push_back(std::format("{} {}", to, value.entity));
          break;
        }
        default:
        {
          calls.push_back(std::format("{} {}", to, value.number));
          break;
        }
      }
    }
  };
} // quake

#endif //QUAKE_QC_RECORDING_HOST_TEST_HPP
