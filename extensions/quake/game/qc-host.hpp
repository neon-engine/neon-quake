#ifndef QUAKE_QC_HOST_HPP
#define QUAKE_QC_HOST_HPP

#include <cstdint>
#include <string_view>

#include "qc-message-destination.hpp"
#include "qc-message-value.hpp"

namespace quake
{
  /// What the builtins of `QcCoreBuiltins` hand on to whoever runs the game:
  /// the text the game code prints, the entities it makes and removes, the
  /// commands it gives, the messages it writes for the players.
  ///
  /// The builtins know nothing of a screen, a console, or a network, so each
  /// of these is a method here, and the host does with it what it has the
  /// means for. Every method does nothing unless the host says otherwise:
  /// a host that shows no text and sends no messages still runs the game.
  ///
  /// A method is called while the game code runs. It may read and write the
  /// machine, and must not make the builtins or the machine go away. A text
  /// it is given holds only until it returns.
  class QcHost
  {
  public:
    virtual ~QcHost() = default;

    // What the game code prints.

    /// `bprint`: a line for every player, such as who died of what.
    virtual void PrintToAll(std::string_view text) {}

    /// `sprint`: a line for one player, the entity `client`.
    virtual void PrintToClient(std::int32_t client, std::string_view text) {}

    /// `dprint`: a line for whoever develops the game, not for a player.
    virtual void PrintToConsole(std::string_view text) {}

    /// `centerprint`: a text in the middle of the screen of one player.
    virtual void PrintToCenter(std::int32_t client, std::string_view text) {}

    /// `eprint`, and `coredump` for every entity: the game code asks for
    /// the fields of an entity to be printed, to find a fault with.
    virtual void PrintEntity(std::int32_t entity) {}

    /// `error`: the game code cannot go on, and says why. `self` is the
    /// entity it ran for. The run is stopped right after this.
    virtual void Error(std::int32_t self, std::string_view text) {}

    /// `objerror`: the game code cannot go on with one entity, and says
    /// why. The entity is removed right after this, and the run goes on.
    virtual void ObjectError(std::int32_t entity, std::string_view text) {}

    // The entities.

    /// `spawn` made an entity, with every field zero.
    virtual void EntityMade(std::int32_t entity) {}

    /// An entity was removed, by `remove`, by `objerror`, or by the host
    /// itself through QcCoreBuiltins::RemoveEntity(). What the host shows
    /// for it goes now. The machine may give its number out again.
    virtual void EntityRemoved(std::int32_t entity) {}

    // What the game code sets up and asks for.

    /// `lightstyle`: how the lights of a style flicker was set. The text
    /// is a letter for each tenth of a second, from `a`, dark, to `z`, twice
    /// as bright as `m`.
    virtual void LightStyleSet(std::int32_t style, std::string_view text) {}

    /// `stuffcmd`: a command for the console of one player.
    virtual void ClientCommand(std::int32_t client, std::string_view text) {}

    /// `localcmd`: a command for the console of the game itself.
    virtual void ServerCommand(std::string_view text) {}

    /// `changelevel`: the level is over and another is to start.
    virtual void ChangeLevel(std::string_view level) {}

    /// `setspawnparms`: the game code wants what the host keeps of a player
    /// from one level to the next in the globals `parm1` to `parm16`.
    virtual void SetSpawnParameters(std::int32_t client) {}

    /// `WriteByte` to `WriteEntity`: one part of a message of the network.
    /// `client` is the player it is for when the destination is One, and
    /// the world otherwise.
    virtual void WriteMessage(QcMessageDestination destination, std::int32_t client, const QcMessageValue &value) {}
  };
} // quake

#endif //QUAKE_QC_HOST_HPP
