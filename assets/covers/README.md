# Version covers

Production resources are `webp/minecraft-<series>.webp`, embedded through
`resources/covers.qrc`. Each is approximately 100 KiB and retains its original
1672 × 941 resolution. No network download is required.

The images are AI-generated version-themed illustrations, not actual gameplay
screenshots. Only the WebP production images are embedded in the application.

## Themes

| Series | Main subject |
| --- | --- |
| 1.0 | The End, Ender Dragon and obsidian pillars |
| 1.1 | Classic grasslands and coloured sheep wool (not a new mob claim) |
| 1.2 | Jungle, vines and ocelot |
| 1.3 | Desert temple and village trading |
| 1.4 | Swamp witch hut |
| 1.5 | Redstone, hoppers, comparators and daylight sensors |
| 1.6 | Horses and grassland stable |
| 1.7 | Mesa / badlands |
| 1.8 | Ocean monument and guardian; no later aquatic content |
| 1.9 | End city, End ship and chorus plants |
| 1.10 | Snow landscape and polar bears |
| 1.11 | Woodland mansion |
| 1.12 | Colourful concrete / glazed terracotta and parrots |
| 1.13 | Coral, kelp, dolphins and shipwreck |
| 1.14 | Renewed village and pillager outpost |
| 1.15 | Bees, bee nest and flowers |
| 1.16 | Crimson and warped Nether forests |
| 1.17 | Amethyst, copper and axolotl |
| 1.18 | Large mountains and caves |
| 1.19 | Ancient city, sculk and Warden |
| 1.20 | Cherry grove and archaeology |
| 1.21 | Trial chamber and Breeze |
| 26.1 | Baby animals and golden dandelion |
| 26.2 | Sulfur cave, cinnabar, bubbling pools and sulfur cube |
| 26.3 | Autumn poplar forest and campsite |

1.x patch releases share their series cover. 26.x is grouped by each game drop
(e.g. 26.1.2 uses 26.1), not by the entire year. Named pre-release / release
candidate suffixes use their series cover. Legacy weekly snapshots and unknown
future series keep the existing generic background rather than guessing a theme.
Custom user covers continue to take priority.

26.x themes checked against official release notes:
- https://www.minecraft.net/en-us/article/minecraft-java-edition-26-1
- https://www.minecraft.net/en-us/article/minecraft-java-edition-26-2
- https://www.minecraft.net/en-us/article/minecraft-java-edition-26-3

## Compression and packaging

Use the original source, never recompress a compressed copy:

```sh
cwebp -quiet -size 102400 -pass 8 -m 6 source.png -o minecraft-VERSION.webp
```

`-size` is a target, so check the output bytes. Preserve all high-resolution
sources during asset preparation. Distributions must ship the WebP image format plugin
(`qwebp` / `libqwebp`) and its deployment dependencies. The UI cover test checks
plugin availability and decodes every embedded version cover from a different
working directory. Do not distribute only the executable without Qt plugins.
