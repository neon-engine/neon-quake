#include "qc-globals.hpp"

#include <cstddef>
#include <string>
#include <type_traits>

namespace quake
{
  // Helpers of QcGlobals: the names of `parm1` to `parm16`, which the list
  // of the missing points into.
  namespace
  {
    constexpr std::array<std::string_view, QcGlobals::parm_count> parm_names = {
      "parm1", "parm2", "parm3", "parm4", "parm5", "parm6", "parm7", "parm8",
      "parm9", "parm10", "parm11", "parm12", "parm13", "parm14", "parm15", "parm16",
    };
  }

  QcGlobals::QcGlobals(const Progs &progs)
  {
    const auto find = [this, &progs](auto &place, const std::string_view name)
    {
      place = std::remove_reference_t<decltype(place)>(progs, name);
      if (!place.IsFound()) { missing.push_back(name); }
    };

    find(self, "self");
    find(other, "other");
    find(world, "world");
    find(time, "time");
    find(frametime, "frametime");

    find(mapname, "mapname");
    find(serverflags, "serverflags");
    find(total_secrets, "total_secrets");
    find(total_monsters, "total_monsters");
    find(found_secrets, "found_secrets");
    find(killed_monsters, "killed_monsters");
    for (std::size_t i = 0; i < parm_count; i++) { find(parms[i], parm_names[i]); }

    find(v_forward, "v_forward");
    find(v_up, "v_up");
    find(v_right, "v_right");

    find(trace_allsolid, "trace_allsolid");
    find(trace_startsolid, "trace_startsolid");
    find(trace_fraction, "trace_fraction");
    find(trace_endpos, "trace_endpos");
    find(trace_plane_normal, "trace_plane_normal");
    find(trace_plane_dist, "trace_plane_dist");
    find(trace_ent, "trace_ent");
    find(trace_inopen, "trace_inopen");
    find(trace_inwater, "trace_inwater");

    find(msg_entity, "msg_entity");
    find(force_retouch, "force_retouch");
    find(deathmatch, "deathmatch");
    find(coop, "coop");
    find(teamplay, "teamplay");

    find(main, "main");
    find(StartFrame, "StartFrame");
    find(PlayerPreThink, "PlayerPreThink");
    find(PlayerPostThink, "PlayerPostThink");
    find(ClientKill, "ClientKill");
    find(ClientConnect, "ClientConnect");
    find(PutClientInServer, "PutClientInServer");
    find(ClientDisconnect, "ClientDisconnect");
    find(SetNewParms, "SetNewParms");
    find(SetChangeParms, "SetChangeParms");
  }

  bool QcGlobals::HasEssentials() const
  {
    return self.IsFound() && other.IsFound() && world.IsFound() && time.IsFound() && frametime.IsFound();
  }
} // quake
