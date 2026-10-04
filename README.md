# neon-quake

The original Quake, run by [Neon Engine](../neon-engine): its `id1` data
loaded, and its game code interpreted, by a native extension. It is the
proof that the engine can be extended from outside, and that a game can be
made on it without touching the engine. The engine knows nothing of Quake.

**Where this stands.** The extension reads the archives of the game, shows a
level, chosen by a line in a text file, and runs the game code of the game
for it. The level has its walls, floors, and ceilings with their textures,
lit by the light it carries. The game code makes its entities: monsters,
items, and torches stand where the level puts them, with their models and
the frames the game code gives them. A player stands where the game code
puts one and is moved as the original moves one, at its pace: running,
jumping, climbing stairs, and swimming. Doors open when they are walked up to, lifts
and buttons move, teleporters take the player away, and items are picked up.
What the game code plays is heard where it sounds: doors, items, monsters,
the hum of a level. Monsters see the player, walk, and attack, and what
falls comes to lie, all colliding with the level as the original does. The
player holds a weapon, shoots with the left button of the mouse, and chooses
a weapon with the number keys; what is shot dies. A player who is killed
lies where the player fell and starts the level again with the fire button,
and the end of a level leads to the next, with what the player carries. What
moves is shown on its way in every frame that is drawn, a monster from one
pose and one stride to the next, so the game is as fluid as the display.
The status bar of the original is there, with what the game code tells the
player above it and the counts of a level at its end. Still, the sky,
liquids, and lights that flicker are not drawn as such yet, and there is no
menu.

## What it is made of

```
assets/project.yml          the project: its name, and the scene it starts with
assets/input/               the input map of the game: what the player does, and the keys for it
extensions/quake/
  extension.yml             the recipe of the extension
  quake.cpp                 its code: what it brings to the engine
  game-data.hpp/.cpp        the archives of the game, and its palette
  game-code.hpp/.cpp        the game code run for a level, and the world made to agree with it
  hud-view.hpp/.cpp         the status bar and what else is shown on top of the world
  level-view.hpp/.cpp       a level shown in the world of the engine
  model-view.hpp/.cpp       the models of the game shown on entities of the engine
  particle-view.hpp/.cpp    the particles of the game, sparks and smoke, drawn as one mesh
  sound-view.hpp/.cpp       the sounds of the game played in the world of the engine
  sprite-view.hpp/.cpp      the sprites of the game, flat pictures that turn to the camera
  formats/                  the formats of the game's data, read from bytes
  game/                     what the game does with its data, without the engine
  assets/                   its scenes, and the data of the game under id1/
tools/tour.py               pictures of everything a level shows, for checking by eye
```

`formats/` knows nothing of the engine: an archive, a picture, a level, a
model, the game code, each read from bytes in memory into plain structs. It is
a library of its own, `quake-formats`, which the extension links, and which
is tested without the engine. What things collide with in a level is there
too: its hulls, and a move traced through them as the original game traces
it. `quake.cpp` and what joins it turn what the
formats read into entities, meshes, and textures of the engine. The game
counts in units of its own with z up, and winds its triangles the other way
than the engine; `formats/quake-space.hpp` is the one place that turns it
over, a unit being a thirty-second of a metre.

`game/` is what the game code asks of its engine, answered without the engine,
in a library of its own, `quake-game`, tested like the formats. It has the
builtins of the original that need no world, `QcCoreBuiltins`: the arithmetic,
the text, the entities of the machine, the console variables, the models and
sounds the game code names, the styles of the lights. What they leave behind
is kept for the host to read, and what the host has to act on, a line of
text, an entity that went, a change of level, reaches it through the
interface `QcHost`.

What the entities of the game code collide with is `LevelCollision`: a box
moved through the world, through the doors and lifts where they stand, and
through the boxes of the other entities, with the rules of the original for
what a move passes. It answers the builtins that ask the level,
`QcWorldBuiltins`: `traceline`, `pointcontents`, `droptofloor`, `findradius`,
`checkclient`, and `aim`, and those a monster walks with, `walkmove`,
`movetogoal`, `checkbottom`, and `ChangeYaw`, whose steps are `LevelStepping`.
Two of them are simpler than the original:
`checkclient` does not ask whether the player can be seen from where the
monster stands, and `aim` does not bend a shot towards a target. The builtins
that place, size, and show an entity, and those that make a sound, are a
host's own.

It also has what the engine of the original did around the game code. A
level starts with `LevelSpawning`: each entity of the level's text gets an
entity of the machine, its keys written into the fields of the same names,
and is handed to the function its `classname` names, unless the skill leaves
it out. Time passes with `LevelRunning`, a step at a time: the game code
starts the frame, every entity thinks when its time has come, and what
pushes, a door or a lift, moves by a clock of its own. What takes the walls
of a level, falling and walking and what a door runs into, is asked of a
`LevelMover`. `LevelPhysics` is the one for a level with its collision: what
is tossed falls and lands, a missile flies until it hits, a monster without a
floor drops, and a lift carries what stands on it and stops for what has
nowhere to go, each as the original moves it. Entities that meet are told
through `LevelTouching`. `QcGlobals` and `QcFields`
are the globals and fields the two work with, found by name once, for a host
to read and write as well: `fields.origin.Get(machine, entity)`.

A player is moved as the original moves one, by the same numbers in the same
order, when a host asks for it. `PlayerMovement` steers: what the player asks
for in a step, a `PlayerCommand`, becomes the velocity of the player's entity,
with friction and acceleration on the ground, no more than thirty gained along
a direction in the air, and swimming in water. `LevelPhysics` then walks the
player by that velocity, told to with `SetWalksClients`: with gravity, sliding
along walls, up steps no higher than eighteen, noting whether the player
stands and how deep in water, and touching the triggers the player comes
into. `LevelRunning`, told how many players there are with `SetClientCount`,
gives each its frame as the original does: `PlayerPreThink`, the move,
`PlayerPostThink`. Jumping is the game code's. Both are off at the start, and
the player is then not moved there: a host moves the player with the
character of its engine.

It is a project with an extension, see
[projects.md](../neon-engine/docs/projects.md#a-project-with-code-in-c) and
[extensions.md](../neon-engine/docs/extensions.md) of the engine.

## Checking a level by eye

```sh
tools/tour.py build maps/lq_e1m2.bsp /tmp/tour
```

takes a picture of one of every kind of thing a level shows, a monster, an
item, a weapon, each from where there is room to look at it, and puts them
together into sheets of twelve numbered pictures, with a list of what each
number is. It runs the game without a window with the environment variable
`QUAKE_TOUR` set, which has the game fly the player from one thing to the
next. It is how a model, a skin, or a place that is wrong is found without
playing every level.

## Building it

Neon Engine is checked out next to this repository, as `../neon-engine`, and
built once, so that there is a NeonRuntime. Nothing of the engine is compiled
here: only the files of the extension are.

```sh
cmake -S . -B build -G Ninja
cmake --build build
build/quake/NeonRuntime
```

| Option | Says |
|---|---|
| `-DNEON_ENGINE_DIRECTORY=<folder>` | Where the engine is, when it is not `../neon-engine` |
| `-DNEON_RUNTIME_DIRECTORY=<folder>` | Which NeonRuntime the game is put together with, when it is not one of the engine's builds. A folder with `NeonRuntime` and its `assets` |

The game is put together in `build/quake/`: the runtime, its assets with this
project's `project.yml` on top, and `extensions/quake/`.

## Running it

```sh
build/quake/NeonRuntime
```

The level that is shown is `maps/start.bsp`, where the game itself starts,
or the first level the data has when it has no such one. Another is chosen
without changing code, by a text file next to the data whose first line is
the name of a level:

```sh
echo maps/lq_e1m1.bsp > build/quake/extensions/quake/assets/level.txt
```

The extension reads it as `extensions://quake/assets/level.txt`. It is a
choice of one's own machine and is never committed: Git leaves
`extensions/quake/assets/level.txt` alone. When the data has no level of that
name the game says so and shows the level it starts with; without the file
it shows that level and says nothing.

A level is shown with its other models, each under an entity named after
what it is and its number, such as `func_door *3`. Triggers are volumes and
are left out, as are faces painted `trigger`, `clip`, or `skip`.

## The data of the game

The repository carries a game to run: the data of
[LibreQuake](https://github.com/lavenderdotpet/LibreQuake) 0.9, a free game
made for this engine's kind, under `extensions/quake/assets/id1/`, kept by
Git LFS. Its maps, models, textures, and sounds are under the BSD 3-clause
licence, and its game code, `progs.dat` inside `pak0.pak`, and `pop.lmp`
under the GPL 2; the notices are next to the data, in `id1/docs/`, and stay
with it.

```sh
git lfs install
git lfs pull
```

The extension reads it as `extensions://quake/assets/id1/pak0.pak`,
`pak1.pak`, and so on, each archive as the file it is, and takes it apart
itself; the engine knows nothing of archives. Whoever owns the original game
puts its `id1` in place of this one, on their own machine. The original's
data is not ours and is never committed.

Without any data the game starts and says that it is missing.

## Tests

A test sits next to the file it tests and is named after it, with `.test` in
front of the extension, as in the engine. The files of `formats/` are found
by the build, so a format is added by adding its files.

```sh
cmake --build build --target quake-tests
ctest --test-dir build
```

The tests make the bytes they read themselves. One that wants real data
looks for it in `QUAKE_TEST_DATA_DIRECTORY`, by default the data this
repository carries, and skips itself when the file it wants is not there as
a file of its own: the levels under `id1/maps` are, what is inside the
archives is not. A checkout of LibreQuake has the rest loose, under `lq1`.

## How it is written

The code is written for this repository, in the conventions of Neon Engine,
see its [style guide](../neon-engine/docs/style-guide.md). Other source ports
are read to understand the formats, and nothing is copied from them.

## Order of work

From the engine's issue #287: the pak files as a scheme of the file system
and the palette; a level drawn with its lightmaps; the collision of a level
and the player walking it; the models with their frames; the interpreter of
the game code with its entities as components; sounds and music.
