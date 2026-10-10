# Component documentation

Two kinds of file live here, one folder per component:

**The digests** (`*.md`) are the datasheets restated in English — a page instead of
thirty, greppable and diffable, and usually the faster answer. They are tracked.

**The datasheets** (`*.pdf`) are not tracked. `manifest.json` is the tracked stand-in:
for every PDF it records where it belongs, which part it is, where to download it
again and the SHA-256 of the copy these digests were written from. 12 MB of binary
that changes rarely and diffs never stays out of the history; the 4 KB record of
*what we read* stays in.

## Opening them

```
docs\browse.bat
```

The PDFs are in `%LOCALAPPDATA%\temp_sensor\datasheets`, outside the repo. Click a
row and the datasheet opens — instantly if it is cached, after a download if it is
not. The right-hand pane has a tab for the PDF and a tab for the digest, and a link
to a PDF from inside a digest resolves through the manifest, so it keeps working even
though the file is no longer beside it.

## The app is not in this repo

The Datasheet Browser lives in the water_tank checkout, at
`docs/datasheet_browser/`, and serves any number of projects: it takes a manifest and
derives everything else from it — the tree the digests sit in, the cache, the window
title. `browse.bat` assumes that checkout is beside this one and otherwise reads
`DATASHEET_BROWSER`. Its README is the manual; what follows is only what is specific
to this project.

The digests stay here, in the repo that uses them, which is the point of the
arrangement. water_tank documents a TPS22919 too, and the two write-ups being
separate files is a feature: either project can be read on its own, and neither can
be broken by an edit made for the other.

## On a fresh clone

```
python <water_tank>\docs\datasheet_browser\tools\sync_cache.py -m docs\manifest.json fetch
```

Five of the six download again from TI or ST. The sixth,
`diode/GK-SOD-323_BAS70WS-2025.pdf`, has no source URL that could be resolved, so the
cached copy is the only copy — both the GUI and `sync_cache.py` refuse to re-fetch it
rather than deleting it first. Back up the cache folder, or add a `url` to its entry
if you find one.

`sync_cache.py -m docs\manifest.json status --verify` re-hashes everything against the
manifest.

## Adding a datasheet

Drop the PDF into the right folder and record it:

```
python <water_tank>\docs\datasheet_browser\tools\build_manifest.py -m docs\manifest.json --no-network
python <water_tank>\docs\datasheet_browser\tools\sync_cache.py  -m docs\manifest.json adopt
```

The first hashes it and links any digest that cites it; the second moves it out of the
tree into the cache. Then add its `url`, `brand` and `variant` by hand — or in the
app's Edit dialog — because the file names here are vendor ones and there is no LCSC
code to enrich from. Commit the manifest.

## Writing a digest

Select a datasheet with no digest and the Digest tab offers **Copy prompt**: the whole
job, with the cache path of the PDF, the part identity, the file to write and the
house style filled in. Paste it into Claude Code in this repo.

The house style is `sensor_component/hdc3020.md` and
`boost_supply/tps61021adsgr.md` — the manifest header names them, which is how the
prompt knows what to point at. A digest states what the part is and what it does, as
its own documentation has it, with every value carrying the page it came from, and
whatever could not be reproduced (curves, drawings) named rather than dropped. It says
nothing about this board: why a part was chosen and what it is wired to belong in
[../design/](../design/), and a digest carrying that goes stale the moment the design
moves on.

**The citation is load-bearing.** A digest is tied to its manifest entry by the PDF it
quotes, not by its name — `microcontroller/stm32l010f4-tssop20.md` and
`stm32l010f4.pdf` are not named alike. Mentioning the file name in the opening lines
is what records the link; drop it in a later rewrite and the next refresh forgets which
part the file is about.
