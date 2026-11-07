# BlockCraft PE (client)
Original voxel sandbox in C++17 + raylib 5.0. Inspired by Minecraft PE; uses no Mojang code, art, audio, or name.

## Build (GitHub)
Push this folder to a new repo -> Actions tab -> `build` makes Linux / Windows / macOS binaries (artifacts).
Local: `cmake -B build && cmake --build build -j` (needs a C++17 compiler; Linux also needs X11/GL/ALSA dev packages, see `.github/workflows/build.yml`).

## Controls
WASD move, Space jump (swim up), Ctrl sprint, mouse look, LMB mine/attack (hold), RMB place/use/eat, 1-9 or wheel hotbar,
E inventory (2x2 crafting; right-click a Crafting Table for 3x3), Esc pause, F fly (creative; Shift = down), F3 debug, T chat (servers).

## What's in
- Survival + Creative, hearts/hunger, fall damage, death/respawn, day/night cycle
- Infinite chunked terrain: hills, beaches, oceans, trees, caves + caverns, coal/iron ore
- Blocks: stone, grass, dirt, cobblestone, planks, log, leaves, sand, water, glass, bedrock, ores, crafting table
- Tools: wood/stone/iron pickaxe, axe, shovel, sword, hoe with real tiers, mining-speed formula and durability
- Inventory (36 slots), 2x2/3x3 shaped crafting (mirrored too), creative palette
- Mobs: pigs (passive, drop porkchop), zombies (night, chase, melee, drop flesh)
- Procedural ambient music + synthesized sfx (original, generated in code)
- Main menu, world list/create (name, seed, mode), loading screen, settings, saves in `saves/`
- Servers tab: saved list (`servers.txt`), Add / Direct Connect with `ip:port`, chat, remote players, shared block edits

## Physics
Fixed 20 TPS tick with the documented Java/Bedrock constants: gravity 0.08, drag 0.98, jump 0.42, ground friction 0.546, air 0.91,
air accel 0.02, sprint-jump +0.2, water drag 0.8. Jump apex measures 1.2522 blocks (same as vanilla).

## Known gaps / next steps
No furnace/smelting (iron ore drops an ingot directly), no dropped-item entities (drops go straight to inventory), no redstone/lava/biomes,
mobs are box models, no touch controls yet (Android port = raylib Android template, planned).
