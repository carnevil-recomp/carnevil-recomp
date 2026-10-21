# Mods

The port can load replacement files from a `mods/` folder while it runs. Your extracted game
files and the disk image are never changed, so removing a mod (or running with `--no-mods`)
always gives you the original game back. Mods are a port addition: nothing in the decompiled
game code changes, and the byte-exact GAME.EXE rebuild doesn't use them.

## What can be replaced
- **Textures, as PNG pictures.** Name the picture after the texture: `NAME.png` replaces
  `NAME.WMS`. All 1148 of the game's textures can be replaced this way: enemies, levels, the
  gun, text fonts, the attract and select screens. This is the main thing the mods folder is for.
- **Sound banks (`.BNK`), as whole files.** The game loads banks without a checksum on this
  hardware, so a replacement bank is used. It has to be a DCS bank in Midway's format, with the
  sound calls the game expects at the same numbers; this project has no tool for making one.
- **Other files the game reads without a checksum**, as whole files of the same name: the
  textures in their own `.WMS` format, for example. The port reads a replacement exactly as it
  reads the original.

## What can't be replaced
- **Movies and music.** The game streams them from raw partitions of the disk image, outside the
  file system, so there is no file to replace.
- **Models (`.ZM`), animations (`.ZA`), enemy paths (`.PTH`) and level cameras (`HHOU.RAW` and
  the other levels' `.RAW`).** The game checks each of these against a size and a checksum built
  into its code. A changed file fails the check, so the game loads the backup copy next to it
  (`.ZM2`, `.ZA2`, `.PT2`, `.RW2`), which is the original, and counts a file error in the audits.
  So the mod has no effect, apart from the audit count. Replacing the backup as well doesn't help:
  it fails the same check.
- **Your saved settings and high scores** (the CMOS and the other files the game writes, in the
  state folder). A mod never overrides them.
- **The game's code.** The port is compiled from the decompiled source, so changes to how the
  game plays are made in `src/` and rebuilt, not through the mods folder.

## Where the folder goes
- **Release package:** `mods/` next to `carnevil` / `carnevil.exe` (the package ships an empty
  one with a short README). `run.sh` and `play.bat` start the game from their own folder, so the
  folder is found there.
- **From the repository:** `mods/` in the directory you run the port from.
- `--mods DIR` uses another folder; `--no-mods` turns mods off for that run.

## Layout
```
mods/
  ATTFREK.WMS           a loose file: replaces the game file of that name (any case)
  order.txt             optional: which packs win when two replace the same file
  hd-enemies/           a pack: a folder of replacements
    BABYBOD.png         a texture as a picture (in place of BABYBOD.WMS)
    textures/...        one level of subfolders inside a pack is read too
```
(The pack and file names are examples; any folder name works for a pack.)

When two mods replace the same file:
1. loose files directly in `mods/` win over all packs;
2. then come the packs named in `order.txt`, one folder name per line, first listed wins
   (blank lines and lines starting with `#` are skipped);
3. then the remaining packs, in alphabetical order.

Names are matched without regard to case. A `README.txt` or `order.txt` is never treated as a
replacement.

## Making a texture pack
1. Export the originals as a starting point:
   ```
   ./run.sh --export-textures textures          # release package, macOS / Linux
   play.bat --export-textures textures          # release package, Windows
   ./build/host/carnevil --export-textures textures   # from the repository (macOS;
                                                      # build/linux/carnevil on Linux)
   ```
   This writes every texture as `textures/NAME.png` (its largest mip level, the right way up for
   reading back) and exits without starting the game.
2. Edit the pictures you want to change. Keep each file's name.
3. **Keep the proportions.** A replacement can be any size, as long as width and height have the
   same ratio as the original: a 64x32 texture can become 256x128 or 512x256, but not 256x256.
   The game's texture coordinates are fractions of the picture, so a bigger picture is simply
   sharper (most visibly with `--hires`). A picture with the wrong proportions is skipped with a
   message, and the original is used.
4. Put the edited PNGs (only those) in a pack folder, e.g. `mods/my-pack/`.
5. Run the game as usual.

Details:
- A replacement is uploaded as is: one full-size level, with the graphics driver building the
  smaller mip levels. The PNG's alpha channel is used, so transparent pixels stay transparent.
  Pixels in the colour the game keys out on a texture stay keyed out too.
- Any PNG the bundled stb_image reader handles works (8-bit or 16-bit, with or without
  alpha, palette or greyscale). Other formats need converting to PNG first.
- Replacement data is kept outside the 32-bit memory arena the game allocates from. A large
  texture pack therefore doesn't change the free memory the game measures, or anything the game
  does with it.

## Checking what's loaded
The port prints to the terminal (or the console on Windows):
- at start-up: `[mods] 12 replacement files in mods (2 packs)`;
- the first time the game uses each replacement: `[mods] BABYBOD.png <- mods/hd-enemies/BABYBOD.png`;
- a problem with a picture: one that isn't a readable PNG, or one whose proportions don't match
  the texture's;
- an `order.txt` line naming a folder that doesn't exist.

A replacement the game never loads during a run isn't printed. For example, a level's textures
appear only once that level is reached.

## Options
| Option | Effect |
|---|---|
| `--mods DIR` | read replacements from `DIR` instead of `mods/` |
| `--no-mods` | ignore the mods folder for this run |
| `--export-textures DIR` | write every texture as `DIR/NAME.png`, then exit |

## Sharing a pack
A pack is just a folder, so zip it and others unzip it into their `mods/`. Packs should only
contain the files they change. Note that textures exported from your own disk are Midway's
artwork, so check what you're allowed to share.
