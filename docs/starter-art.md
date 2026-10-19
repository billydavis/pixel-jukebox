# Starter art (backlog #28): shortlist

Art built into the firmware's spare flash, played on a new panel before it is
linked to Makapix, offline without a card, or as a channel. About 30 pieces:
around 20 from free, public-domain (CC0) pixel art packs, and around 10 drawn
by code to show what the panel can do. Every piece will be listed in
`CREDITS.md` with its source and licence, CC0 ones included.

Not the owner's own art, and no Makapix art unless an artist gives written
permission (a possible later addition).

## Sources

| Pack | Author | Licence | What it offers |
|---|---|---|---|
| [Pixel Adventure 1](https://pixelfrog-assets.itch.io/pixel-adventure-1) | Pixel Frog | CC0 1.0 | Four 32x32 characters with run, jump and idle animations at 20 fps; animated fruit; tiles. Zip, 204 KB |
| [DungeonTileset II](https://0x72.itch.io/dungeontileset-ii) v1.7 | 0x72 (Robert) | CC0 1.0 (assets), MIT (code, none used) | 16x16 animated heroes, monsters, fountains, chests. Zip, 406 KB |
| [Sunny Land](https://ansimuz.itch.io/sunny-land-pixel-game-art) | ansimuz (Luis Zuno) | CC0 1.0 | An animated fox, enemies (eagle, frog, opossum), items and parallax backgrounds. Art zip only, 3.3 MB |
| [Pixel Platformer](https://kenney.nl/assets/pixel-platformer) | Kenney | CC0 1.0 | 18x18 tiles and characters, for small scenes. Zip, 260 KB |
| [Tiny Dungeon](https://kenney.nl/assets/tiny-dungeon) | Kenney | CC0 1.0 | 16x16 tiles and characters. Zip, 98 KB |

Left out: Pixel Adventure 2 (now a paid download, so not used even though it
is marked CC0); Sunny Land's music (other authors) and its paid "Plus" files;
bevouliin's CC0 monsters (large cartoon drawings, not pixel art: they would
blur when shrunk to 64x64); anything CC-BY-SA, NC or GPL; the LPC sprites
(CC-BY-SA/GPL); Kenney's logo (Kenney reserves it).

## Licence evidence (checked 2026-10-19)

Each pack was checked three ways: the licence field on its download page, the
licence file in the download (where there is one), and the author's own words.

- **Pixel Adventure 1.** Page licence field: "Creative Commons Zero v1.0
  Universal". Description: released under CC0, usable "even for commercial
  purposes", and "Attribution is not required". The zip has no licence file
  (PNG files only). Downloaded free (pay-what-you-want, $0).
- **DungeonTileset II.** Page licence fields: asset licence "Creative Commons
  Zero v1.0 Universal", code licence MIT. The author: "You can use this
  tileset for whatever you like (CC-0)" and "Credit is not necessary". The
  zip's README is technical only. Note: v2 was "recolored (GrafxKid)", that
  is, given GrafxKid's palette; a palette is not a separate work.
- **Sunny Land.** Page licence field: "Creative Commons Zero v1.0 Universal".
  The zip's `public-license.pdf`: all assets in the package are CC0, usable
  in personal or commercial projects without attribution, with no
  restrictions on use, modification or redistribution. Taken from the
  author's itch.io page (OpenGameArt was down); only `Sunny-land-files.zip`
  was downloaded.
- **Kenney (both packs).** Each zip's `License.txt`: "Creative Commons Zero,
  CC0". Kenney's support page: all assets on the asset pages are public
  domain (CC0), free to use even commercially; attribution is not required;
  do not use the Kenney logo.

The three itch.io pages also state "No generative AI was used". Every piece
will still be credited in `CREDITS.md`.

## The pieces

All 28 candidates were kept (owner, 2026-10-19). They are in
`assets/starter/` with `manifest.json`, made by `tools/make_starter_art.py`
from the packs above (its header says where to put them), and listed in
`CREDITS.md`:

- Pixel Adventure 1 (8): each hero running over a scrolling sky, the four
  together, a fruit bowl, a pineapple scaled x2, a saw on a chain
- DungeonTileset II (5): knight and demon by a lava fountain, a party of four,
  demon and ogre, the mimic chest, a crowd of eight monsters
- Sunny Land (4): the fox with three-layer parallax, the eagle over the sea,
  frog and opossum, gem and cherry scaled x2
- Kenney (2): a meadow platformer scene, a tiny dungeon room
- Drawn by code (9): plasma, sunset, colour wheel, spinning cube, ripples,
  Game of Life, a Pixel Jukebox logo, a starfield, glowing colour bands

## In the firmware

`tools/stage_starter_art.py` packs them into one file at build time
(`lib/jukebox_core/starter_pack.h` has the format), which is embedded in the
firmware: about 1.1 MB of the 4.5 MB firmware slot. Built in rather than kept
in the unused 23 MB storage area, so a new panel has them from its first
start with no second flashing step, and an update over Wi-Fi brings any
change with it. The storage area stays free for later.

`src/storage/starter_art.cpp` reads the pack. The "starter" channel plays the
pieces shuffled, and the artwork slot falls back to them whenever it has
nothing else to play: before the panel is linked (turns go between the
registration code and starter art), offline with an empty card, a channel
with a problem, or a download that has not arrived. `GET /api/v1/starter`
lists them with credits, and the Built-in view of the Art tab shows them all;
tap one to show it now.
