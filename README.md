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
a weapon with the number keys or goes through them with the wheel; what is
shot dies. A player who is killed
lies where the player fell and starts the level again with the fire button,
and the end of a level leads to the next, with what the player carries. What
moves is shown on its way in every frame that is drawn, a monster from one
pose and one stride to the next, so the game is as fluid as the display.
The status bar of the original is there, with what the game code tells the
player above it and the counts of a level at its end. Textures are shown pixel by pixel, liquids swim and are seen through as far
as a level says, the sky drifts, and a level has its fog. The game greets a player with the menu of the original, which escape opens
again: a new game, the options, the manual, leaving. A game is saved and gone back to from the menu, as the files the original
writes, `s0.sav` to `s11.sav` under `user://saves/`, and what the menu sets
is kept in `user://quake.cfg`. Lights flicker, pulse, and are switched as the game code sets them.

## What it is made of

```
assets/                     the game, assets://
  project.yml               the project: its name, and the scene it starts with
  input/                    the input map of the game: what the player does, and the keys for it
  settings.yml              the settings of the game: its window, and no menu of the engine
  scenes/                   the scene it starts with
  ui/                       the status bar
  prompts/                  the pictures of the buttons of gamepads
  basedirs/<name>/id1/      the data of the game, which a player brings, one copy or several
extensions/quake/           the extension, extensions://quake/: its code, and what it needs to run
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
  shaders/                  the shaders of the game: pixels as they are, liquids, the sky
  formats/                  the formats of the game's data, read from bytes
  game/                     what the game does with its data, without the engine
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
built once, so that there is a NeonRuntime. `glslang` compiles the shaders of
the game (`brew install glslang`, `apt install glslang-tools`), as it does
those of the engine. Nothing of the engine is compiled
here: only the files of the extension are.

```sh
cmake -S . -B build -G Ninja
cmake --build build
build/quake/neon-quake
```

| Option | Says |
|---|---|
| `-DNEON_ENGINE_DIRECTORY=<folder>` | Where the engine is, when it is not `../neon-engine` |
| `-DCMAKE_BUILD_TYPE=<kind>` | `Release` when nothing is said, which is the build for playing. `Debug` is not optimised, and is several times slower in what the game does every frame: use it with a debugger, not to judge how the game runs |
| `-DNEON_RUNTIME_DIRECTORY=<folder>` | Which NeonRuntime the game is put together with, when it is not one of the engine's builds. A folder with `NeonRuntime` and its `engine` folder |

The game is put together in `build/quake/`: the runtime, named `neon-quake`
there, with its folder `engine`, this project's `assets/`, and
`extensions/quake/` with the library and the compiled shaders.

## Running it

```sh
build/quake/neon-quake
```

The level that is shown is `maps/start.bsp`, where the game itself starts,
or the first level the data has when it has no such one. Another is chosen
without changing code, by a text file next to the data whose first line is
the name of a level:

```sh
echo maps/lq_e1m1.bsp > build/quake/assets/level.txt
```

The extension reads it as `assets://level.txt`. It is a
choice of one's own machine and is never committed: Git leaves
`assets/level.txt` alone. When the data has no level of that
name the game says so and shows the level it starts with; without the file
it shows that level and says nothing.

A level is shown with its other models, each under an entity named after
what it is and its number, such as `func_door *3`. Triggers are volumes and
are left out, as are faces painted `trigger`, `clip`, or `skip`.

## The data of the game

The repository carries no data of any game. Whoever builds it puts an `id1`
folder of their own in a folder named as they like under `assets/basedirs/`,
as Quake's `-basedir` names the folder `id1` is in: the original game's in
`assets/basedirs/steam/id1/`, or
[LibreQuake](https://github.com/lavenderdotpet/LibreQuake)'s, a free game
made for this engine's kind, in `assets/basedirs/librequake/id1/`. Git leaves
them alone, and the build copies them, with the rest of `assets/`, to where
the game is put together; one put there after the build is found as well.

| There is | The game |
|---|---|
| One copy | Plays it |
| Several | Asks which, with a menu that works with a controller, and remembers the answer in `user://basedir.yml`. Remove the file, or write another name in it, to change |
| An `id1` straight in `assets/basedirs/` | Plays it alone, whatever else is there, as `default` |
| None, or a copy whose archives cannot be read | Says why on the screen, and closes with a button |

Each copy keeps its saved games and the options of its menu apart, in
`user://<name>/saves/` and `user://<name>/quake.cfg`. Choosing a copy on the
command line, and asking for the menu even when one was chosen, waits for
the engine to hand options to an extension.

The extension reads the data as `assets://basedirs/<name>/id1/pak0.pak`,
`pak1.pak`, and so on, each archive as the file it is, and takes it apart
itself; the engine knows nothing of archives. The data is not ours and is
never committed.

The archives and the music, `music/track02.ogg` and on, are found in any
letter case: the original release on Steam has `PAK0.PAK` and `PAK1.PAK`, and
they are used as they are. The engine opens a file only by its name as it is
on disk, so the extension lists the folder and takes the name it finds there.

Without any data the game says so on the screen, and closes when the button is chosen.

## Tests

A test sits next to the file it tests and is named after it, with `.test` in
front of the extension, as in the engine. The files of `formats/` are found
by the build, so a format is added by adding its files.

```sh
cmake --build build --target quake-tests
ctest --test-dir build
```

The tests make the bytes they read themselves. One that wants real data
looks for it in `QUAKE_TEST_DATA_DIRECTORY`, by default the `id1` of the first copy put
under the assets, and skips itself when the file it wants is not there as
a file of its own: the levels under `id1/maps` are, what is inside the
archives is not. A checkout of LibreQuake has the rest loose, under `lq1`.

## How it is written

The code is written for this repository, in the conventions of Neon Engine,
see its [style guide](../neon-engine/docs/style-guide.md). Other source ports
are read to understand the formats, and nothing is copied from them.

## Licence

neon-quake is free software: you can redistribute it and modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 2 of the License, or (at your option) any later
version. It comes without any warranty. The text of the licence is in
[LICENSE.txt](LICENSE.txt), which is the file vkQuake carries, as it is.
These are the terms id Software released the source of Quake under, and the
terms of vkQuake and the other source ports that are read here.

What that covers and what it does not:

- **The game, this repository**: the extension, its shaders, its tools, and
  the files of its project.
- **Not the engine.** The game is a library of its own, `quake-<system>`,
  which NeonRuntime loads when it starts and talks to through the functions
  of `neon-extension.h` alone. Nothing of the engine is compiled into the
  game, and nothing of the game into the engine. Neon Engine has its own
  licence.
- **Not the data of a game.** `id1` is the player's own copy, of Quake or of
  LibreQuake, under the licence it came with, and is never part of this
  repository.
- **The pictures of buttons** in `assets/prompts/`, when
  they are there, are by Kenney and in the public domain (CC0).

## Order of work

From the engine's issue #287: the pak files as a scheme of the file system
and the palette; a level drawn with its lightmaps; the collision of a level
and the player walking it; the models with their frames; the interpreter of
the game code with its entities as components; sounds and music.
