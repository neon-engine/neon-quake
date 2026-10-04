#include "qc-fields.hpp"

#include <type_traits>

namespace quake
{
  QcFields::QcFields(const Progs &progs)
  {
    const auto find = [this, &progs](auto &place, const std::string_view name)
    {
      place = std::remove_reference_t<decltype(place)>(progs, name);
      if (!place.IsFound()) { missing.push_back(name); }
    };

    find(modelindex, "modelindex");
    find(absmin, "absmin");
    find(absmax, "absmax");
    find(ltime, "ltime");
    find(movetype, "movetype");
    find(solid, "solid");
    find(origin, "origin");
    find(oldorigin, "oldorigin");
    find(velocity, "velocity");
    find(angles, "angles");
    find(avelocity, "avelocity");
    find(punchangle, "punchangle");
    find(classname, "classname");
    find(model, "model");
    find(frame, "frame");
    find(skin, "skin");
    find(effects, "effects");
    find(mins, "mins");
    find(maxs, "maxs");
    find(size, "size");
    find(touch, "touch");
    find(use, "use");
    find(think, "think");
    find(blocked, "blocked");
    find(nextthink, "nextthink");
    find(groundentity, "groundentity");
    find(health, "health");
    find(frags, "frags");
    find(weapon, "weapon");
    find(weaponmodel, "weaponmodel");
    find(weaponframe, "weaponframe");
    find(currentammo, "currentammo");
    find(ammo_shells, "ammo_shells");
    find(ammo_nails, "ammo_nails");
    find(ammo_rockets, "ammo_rockets");
    find(ammo_cells, "ammo_cells");
    find(items, "items");
    find(takedamage, "takedamage");
    find(chain, "chain");
    find(deadflag, "deadflag");
    find(view_ofs, "view_ofs");
    find(button0, "button0");
    find(button1, "button1");
    find(button2, "button2");
    find(impulse, "impulse");
    find(fixangle, "fixangle");
    find(v_angle, "v_angle");
    find(idealpitch, "idealpitch");
    find(netname, "netname");
    find(enemy, "enemy");
    find(flags, "flags");
    find(colormap, "colormap");
    find(team, "team");
    find(max_health, "max_health");
    find(teleport_time, "teleport_time");
    find(armortype, "armortype");
    find(armorvalue, "armorvalue");
    find(waterlevel, "waterlevel");
    find(watertype, "watertype");
    find(ideal_yaw, "ideal_yaw");
    find(yaw_speed, "yaw_speed");
    find(aiment, "aiment");
    find(goalentity, "goalentity");
    find(spawnflags, "spawnflags");
    find(target, "target");
    find(targetname, "targetname");
    find(dmg_take, "dmg_take");
    find(dmg_save, "dmg_save");
    find(dmg_inflictor, "dmg_inflictor");
    find(owner, "owner");
    find(movedir, "movedir");
    find(message, "message");
    find(sounds, "sounds");
    find(noise, "noise");
    find(noise1, "noise1");
    find(noise2, "noise2");
    find(noise3, "noise3");
  }

  bool QcFields::HasEssentials() const
  {
    return classname.IsFound() && spawnflags.IsFound() && movetype.IsFound() && ltime.IsFound() &&
           origin.IsFound() && angles.IsFound() && velocity.IsFound() && avelocity.IsFound() &&
           nextthink.IsFound() && think.IsFound();
  }
} // quake
