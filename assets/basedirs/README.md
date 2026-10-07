# The data of the game

Each folder here is a copy of the game's data, named as you like, with an
`id1` in it, as Quake's `-basedir` names the folder `id1` is in:

```
assets/basedirs/
  steam/id1/         PAK0.PAK, PAK1.PAK, music/
  librequake/id1/    pak0.pak, pak1.pak
```

The game asks which to play when there is more than one, and remembers the
answer in `user://basedir.yml`; remove that file to be asked again. An `id1`
straight in this folder, `assets/basedirs/id1/`, is played alone, whatever
else is here.

Saved games and the options of the menu are kept apart for each copy, in
`user://<name>/saves/` and `user://<name>/quake.cfg`.

Nothing here but this file is committed: the data is the player's own.
