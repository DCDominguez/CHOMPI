<p align="center"><img src="assets/choowi.svg" width="160" alt="Choowi"></p>

# the forge family

Forge is a workshop, not a single firmware. Things get forged here for CHOMPI, and each
one that leaves the anvil is its own little world with its own name and face. Here's how
the family works, so the next one fits right in.

## the tree

```
forge                         the workshop: shared tools, docs, tests, the bridge
├── Choowi                    firmware line #1 (this one), mascot: Choowi
│   ├── … 0.15                the foundation: synth, TAPE sampler + looper, presets,
│   │                         harmony, arp + bass, event recorder, AI webapp
│   ├── 0.16  Colour Lantern  (green)
│   ├── 0.17  Wake-up Bell    (yellow)
│   │   └── 0.17.1 Sweep Brush  (small update, small tool)
│   ├── 0.18  Dream Bellows   (orange)
│   ├── 0.19  Tinker Wrench   (red)
│   └── 0.20  Choowivision    (pink: CHOMPI's screen on your phone, and the FX section)
└── (the next firmware line)  a new name, a new mascot, same workshop
```

## the rules

1. **Forge is the family name.** It's the workshop, the tree, the place. A firmware is
   never just "Forge"; it's "Forge · *something*".
2. **Every firmware line gets its own name and mascot.** Choowi is the first. A new line
   means a genuinely different firmware for CHOMPI, not a new version of an existing one.
3. **Releases of a line are tools its mascot forges**, not new characters. Each tool is
   named for what that release adds (a Colour Lantern for the colours, a Dream Bellows
   for dream mode) and drawn in Choowi's jank-but-cute style.
4. **Tools take CHOMPI's panel colours in order**, one per release: white, green, yellow,
   orange, red, pink, purple, blue, teal, then round again. White belongs to the mascot.
5. **Small updates (x.y.1) get small tools** in their parent release's colour.
6. **Name the tool in the changelog**: every release heading in
   [CHANGELOG.md](CHANGELOG.md) says which tool it is.
7. **Our characters are our own.** No CHOMPI logo, artwork or character, no CHOMPI salmon
   pink for our mascots, no headphones. Forge is "for CHOMPI", never "by" it
   ([TRADEMARKS.md](../../TRADEMARKS.md)).

## how it talks

Kinda magic, kinda lazy, kinda fun. Short sentences. Say what a thing *does for you* before
how it works. A little wonder is good ("it starts dreaming"); hype isn't. Never claim
something works on CHOMPI until it has been checked on one, and say so plainly when it
hasn't.

## the look

**Spark**: CHOMPI's nine panel colours are the palette, white first. Ink `#1B1B22` on warm
paper `#FBFAF7`, spark pink `#FF5EA0` for accents. Fredoka for words, Space Mono for
settings and numbers (both free and open; bundle the font files and their licences when
the webapp or Tab5 uses them offline). The wordmark is a lowercase **forge** with the
nine-dot light row under it.

## Choowi, the first of the family

Choowi started as the owner's sketch: a square with spiral, slightly crazy eyes and a big
chewy grin. He got a crooked wizard hat full of stars and a hammer with a glowing rune
that throws sparks in the nine colours. His line turns CHOMPI into a playable instrument
that does half the work for you; see the [manual](MANUAL.md) for everything he does.

## whose it is

The code is MIT, free for anyone. The **names and mascots** (Forge, Choowi, and every future
mascot) and their artwork are kept as Forge's own, the way CHOMPI Club keeps its character:
see [TRADEMARKS.md](../../TRADEMARKS.md). A fork gets its own name and mascot.
