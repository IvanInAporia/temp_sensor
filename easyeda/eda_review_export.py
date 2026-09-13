#!/usr/bin/env python3
# -*- coding: utf-8 -*-
r"""
eda_review_export.py -- turn EasyEDA Pro / Altium ASCII design files into compact
review documents that an LLM (or a human in a hurry) can read end to end.

WHY THIS EXISTS
===============
`.schdoc` and `.pcbdoc` are pipe-delimited record dumps in the "Protel for Windows
Ascii File Version 5.0" family (EasyEDA Pro exports this dialect). They are ~0.5-1 MB
of mostly drawing data: font tables, sheet borders, silkscreen polylines, 3D-model
transforms, colours, justification flags. None of that matters for a design review,
and it drowns out the parts that do.

This script extracts the *review-relevant* content and drops the rest:

  Schematic  -> WHAT IS CONNECTED TO WHAT.
               Wire routing, symbol placement, colours, fonts and drawing order are
               discarded entirely. Output is a netlist plus a per-component pin map
               plus the component attributes that carry electrical meaning
               (value, tolerance, voltage/power rating, footprint, part numbers).

  PCB        -> WHERE THINGS ARE.
               Here geometry *is* the content, so placement, coordinates, rotations,
               pad positions, board outline, keepouts, stackup and design rules are
               kept, while the 900+ silkscreen/assembly primitives are collapsed to
               counts and copper routing is summarised per net (length per layer,
               via count) instead of dumped segment by segment.

  Both       -> a cross-check report, when a .schdoc and a .pcbdoc share a stem.
               Nets are compared by their exact set of REF.PAD members, so a
               renamed net still matches and a genuinely different connection
               does not.

Both outputs are deterministic: same input -> byte-identical output, entries sorted
by natural designator/name order. That makes them diffable across board revisions.

USAGE
=====
    python eda_review_export.py                     # all .schdoc/.pcbdoc under this dir
    python eda_review_export.py easy_eda/receiver.schdoc
    python eda_review_export.py easy_eda -o review/ # explicit output directory
    python eda_review_export.py --stdout easy_eda/receiver.schdoc

    -o, --out-dir DIR   where to write (default: next to each source file)
        --stdout        print to stdout instead of writing files
        --units mm|mil  PCB units (default mm; source files are always mil)
        --origin board|file
                        PCB coordinate origin (default board = lower-left corner of
                        the board outline bbox, which keeps numbers small and
                        comparable between revisions; file = raw file coordinates)
        --tracks        also dump every copper track segment (large; off by default)
        --all-params    keep every schematic component parameter, including the
                        BOM/3D-model/UI noise that is filtered out by default
        --brief         drop the per-component pin map (netlist only)
        --no-cross-check
                        do not emit the schematic-vs-PCB consistency report

Outputs, per input stem:
    <stem>.sch.txt      schematic review document
    <stem>.pcb.txt      PCB review document
    <stem>.xcheck.txt   sch<->pcb consistency report (only when both were processed)

No third-party dependencies. Python 3.9+.

--------------------------------------------------------------------------------
FILE FORMAT NOTES  (reverse-engineered from EasyEDA Pro exports; verified against
                    schematic/easy_eda/receiver.{schdoc,pcbdoc})
--------------------------------------------------------------------------------
Both file types are lines of `|KEY=VALUE|KEY=VALUE|...`. The first line of a
.schdoc is a `|HEADER=...` line with no RECORD key. Usually one record per line,
but .pcbdoc concatenates several records onto one line, so records are split on
every `|RECORD=` occurrence rather than on newlines.

Keys may be prefixed `%UTF8%` (e.g. `%UTF8%COMPONENTDESCRIPTION`) when the value
contains non-ASCII text; the prefixed variant wins when both are present.

SCHEMATIC (.schdoc)
-------------------
Records are referenced by OWNERINDEX, which is the record's 0-based position in the
file *excluding* the header line. Record types seen / used here:

    1   Component      LIBREFERENCE, PARTCOUNT, COMPONENTDESCRIPTION, UNIQUEID
    2   Pin            OWNERINDEX, DESIGNATOR, NAME, ELECTRICAL, PINLENGTH,
                       PINCONGLOMERATE, LOCATION.X/Y
    4   Label          free text annotation (kept as sheet notes)
    6   Polyline       drawing only -> discarded
    8   Ellipse        drawing only -> discarded
    10  Round rect     drawing only -> discarded
    12  Arc            drawing only -> discarded
    15/16 Sheet symbol / sheet entry -> hierarchy, WARNED about (not resolved)
    17  Power port     TEXT is the net name (EasyEDA writes e.g. "power_GNDREF")
    18  Port           NAME + IOTYPE; ports with equal NAME are one net
    22  No-ERC marker  marks a pin as deliberately unconnected
    25  Net label      TEXT is the net name; NOTE: EasyEDA omits TEXT for
                       auto-named nets, so many net labels carry no name at all
    26  Bus / 37 bus entry -> WARNED about (bus connectivity is NOT resolved)
    27  Wire           X1/Y1..Xn/Yn, LOCATIONCOUNT vertices
    29  Junction       used if present; EasyEDA instead splits wires at junctions
    31  Sheet          sheet size/grid
    34  Designator     OWNERINDEX + TEXT -> the component reference (e.g. "U5")
    39  Template       sheet frame graphics owner -> discarded
    41  Parameter      OWNERINDEX + NAME + TEXT -> component or sheet attributes
    44/45 Implementation list / implementation -> MODELTYPE=PCBLIB gives footprint

Coordinates: `KEY` plus `KEY_FRAC` (hundred-thousandths), combined as
`int(KEY) * 100000 + int(KEY_FRAC)`; both carry the same sign. Kept as exact
integers so coincidence tests are exact -- no floating-point tolerance needed.

Pin geometry (empirically verified): LOCATION.X/Y is the pin's *body* end. The
electrical hot end is PINLENGTH away along the pin orientation, where orientation
is the low 2 bits of PINCONGLOMERATE: 0=+X, 1=+Y, 2=-X, 3=-Y.

Port geometry: a port occupies [LOCATION.X, LOCATION.X+WIDTH] when ALIGNMENT=2 and
[LOCATION.X-WIDTH, LOCATION.X] when ALIGNMENT=1; both ends are electrical
connection points, so all of {x-w, x, x+w} are tried and only those that actually
touch a wire are used.

Pin names may use Altium overbar notation, where each barred character is followed
by a backslash (`R\S\T\` = /RST). Fully-barred names are normalised to `~RST`.

NETLIST EXTRACTION ALGORITHM
    1. Every wire vertex, pin hot end, power-port location, net-label location and
       port end becomes a *point* (exact integer coordinate pair).
    2. Union-find: each wire unions its consecutive vertices; any point lying
       exactly on a wire segment (endpoint or interior -- the standard EDA
       T-junction rule) is unioned with that wire. Two wires merely crossing
       mid-span are correctly left unconnected.
    3. Explicit names are attached from power ports, net labels and ports.
       All points carrying the same name are then unioned, which is what makes
       same-named net labels, power symbols and off-page ports one net.
    4. Nets with no explicit name get the deterministic auto-name `N$<lowest pin>`
       (e.g. `N$C1.1`) rather than a serial number, so that adding a component
       elsewhere does not renumber unrelated nets in the diff.
    Caveat: EasyEDA exports every pin as ELECTRICAL=4 (Passive), so pin direction
    (input/output/power) is NOT available and driver-conflict checks are impossible
    from this file. The exporter emits the type only when it is not Passive.

PCB (.pcbdoc)
-------------
Records are referenced by explicit ID fields, not by position.

    Board       20 records: [0] board outline as KINDn/VXn/VYn vertex list with
                MAINCONTOURVERTEXCOUNT; [1] V9_STACK_LAYERn_* layer stack;
                [2..] LAYERnNAME/COPTHICK/DIEL* mechanical-layer table
    Net         ID + NAME
    Component   ID, SOURCEDESIGNATOR, PATTERN (footprint), LAYER, X, Y, ROTATION
    Pad         COMPONENT (Component.ID), NET (Net.ID), NAME (pad number),
                absolute X/Y, XSIZE/YSIZE, SHAPE, HOLESIZE, LAYER
    Via         NET, X, Y, DIAMETER, HOLESIZE, STARTLAYER/ENDLAYER
    Track       X1/Y1/X2/Y2, WIDTH, LAYER. A track on a copper layer with no
                COMPONENT key is routing; a track with a COMPONENT key is
                footprint silkscreen/assembly art and is discarded. The NET key
                is often ABSENT on real routing (130 of 404 copper primitives in
                the reference board), which is why infer_copper_nets() exists.
    Arc         same split as Track
    Region      filled polygon; almost always footprint art -> counted only
    Polygon     copper pour: NET, LAYER, vertex list
    Text        silkscreen/assembly text; designator text -> counted only
    Dimension   drawing aid -> counted only
    DXPRule     design rules (Clearance, Width, RoutingVias, PolygonConnect,
                SolderMaskExpansion, ...)

Lengths are written as bare numbers or with a `mil`/`mm` suffix; bare numbers are
mils. Everything is converted to mm internally and rendered in the requested unit.

COPPER CONNECTIVITY (infer_copper_nets / effective_nets)
    Copper is grouped into galvanically connected *islands* by geometric contact
    between tracks, arcs, vias and pads: shared endpoints, an endpoint on another
    segment, an endpoint inside a pad rectangle, or inside a via barrel. Pads and
    vias on MULTILAYER bridge layers; everything else must share a layer. A
    spatial hash keeps the pairwise scan near-linear.

    That buys three things a plain record dump cannot give:
      * copper exported without a NET key inherits the net of its island, so the
        per-net routing totals are complete;
      * a net whose pads land on several islands is reported with the number of
        airwires still to route -- the single most useful PCB review fact;
      * net objects that share copper are merged into one *effective net*.
        EasyEDA can leave a stale duplicate of a net in the PCB table (usually
        the same name in a different case) holding part of the copper; without
        merging, every one of those looks like a broken connection.

    Nets carrying a copper pour are exempt from the airwire check, because pours
    are exported as an outline only and their actual coverage is unknown here.

KNOWN LIMITATIONS (deliberate -- flagged in the output rather than guessed at)
    * Bus / bus-entry connectivity is not resolved.
    * Hierarchical sheet symbols are not flattened; each .schdoc is treated as one
      flat sheet. Multi-sheet projects must be exported sheet by sheet, and nets
      that cross sheets are only joined here if they share a port/label name.
    * Multi-part components are merged by designator; part-level detail is lost.
    * Pin direction is unavailable (EasyEDA exports every pin as Passive), so
      output-driving-output and unpowered-supply-pin checks are impossible.
    * Copper pours are reported as outline + net, not as poured geometry, so pour
      connectivity, thermals and clearance to the pour are not verified.
    * No DRC is performed and nothing here checks spacing, impedance or
      manufacturability. The FLAGS sections are cheap structural checks meant to
      point a reviewer at suspicious spots, not to replace the CAD tool's DRC/ERC.

OUTPUT SECTIONS
    <stem>.sch.txt    COMPONENTS, NETS, PIN MAP, UNCONNECTED PINS, SHEET NOTES, FLAGS
    <stem>.pcb.txt    BOARD, DESIGN RULES, PLACEMENT, PADS, NETS, COPPER POURS,
                      KEEPOUTS, [COPPER TRACKS], FLAGS
    <stem>.xcheck.txt COMPONENTS, NETS
    In every FLAGS section the entries are ordered most serious first: broken or
    missing connections, then structural warnings, then informational counts.
"""

from __future__ import annotations

import argparse
import hashlib
import math
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path

# 1.1  Copper-island contact became width-aware. Until 1.0 a track was a
#      zero-width centre-line and two items touched only if an endpoint of one
#      landed on the other within 0.1 um, so tracks that overlap by most of
#      their width but whose endpoints miss by ~50 um -- and tracks that cross a
#      pad without terminating in it -- were reported as separate islands. That
#      produced ten false "unrouted" airwires on a fully routed receiver board,
#      across three reviews. Items now carry their real width and touch when the
#      copper genuinely overlaps.
VERSION = "1.1"

# EasyEDA's auto-generated PCB net names, e.g. "$2N150".
AUTONAME = re.compile(r"^\$\d*N\d+$")

MM_PER_MIL = 0.0254

# ---------------------------------------------------------------------------
# Generic record reader
# ---------------------------------------------------------------------------

_RECORD_SPLIT = re.compile(r"(?=\|RECORD=)")


def _parse_chunk(chunk: str) -> dict:
    """Parse one `|K=V|K=V|` chunk. `%UTF8%K` overrides plain `K`."""
    out = {}
    for field in chunk.split("|"):
        if "=" not in field:
            continue
        key, value = field.split("=", 1)
        if key.startswith("%UTF8%"):
            out[key[6:]] = value          # UTF-8 variant always wins
        else:
            out.setdefault(key, value)
    return out


def read_records(path: Path):
    """Return (header_dict, [record_dict, ...]).

    Record order is preserved because .schdoc OWNERINDEX values are positions in
    that list (0-based, header excluded).
    """
    text = path.read_text(encoding="utf-8", errors="replace").lstrip("﻿")
    header: dict = {}
    records: list[dict] = []
    for line in text.split("\n"):
        if not line.strip():
            continue
        for chunk in _RECORD_SPLIT.split(line):
            if not chunk.strip():
                continue
            rec = _parse_chunk(chunk)
            if not rec:
                continue
            if "RECORD" not in rec and not records and not header:
                header = rec           # the leading |HEADER=... line
                continue
            records.append(rec)
    return header, records


# ---------------------------------------------------------------------------
# Small helpers
# ---------------------------------------------------------------------------

_NAT = re.compile(r"(\d+)")


def natkey(s: str):
    """Natural sort key: R2 < R10, C1 < C10; numeric chunks sort first."""
    return tuple(
        (0, int(p), "") if p.isdigit() else (1, 0, p.upper())
        for p in _NAT.split(s or "")
        if p != ""
    )


def norm_overbar(name: str) -> str:
    r"""Altium overbar notation: each barred char is followed by '\'."""
    if "\\" not in name:
        return name
    stripped = name.replace("\\", "")
    if stripped and name == "".join(c + "\\" for c in stripped):
        return "~" + stripped
    return stripped


def parse_len_mm(value, default=None):
    """Parse a PCB length. Bare numbers are mils; `mm`/`mil` suffixes honoured."""
    if value is None:
        return default
    s = str(value).strip()
    if not s:
        return default
    mult = MM_PER_MIL
    low = s.lower()
    if low.endswith("mm"):
        s, mult = s[:-2], 1.0
    elif low.endswith("mil"):
        s, mult = s[:-3], MM_PER_MIL
    try:
        return float(s) * mult
    except ValueError:
        return default


class DisjointSet:
    def __init__(self):
        self.parent = {}

    def find(self, x):
        p = self.parent.setdefault(x, x)
        while p != x:
            x, p = p, self.parent.setdefault(p, p)
            self.parent[x] = self.parent.get(p, p)
        return x

    def union(self, a, b):
        ra, rb = self.find(a), self.find(b)
        if ra != rb:
            self.parent[rb] = ra

    def groups(self):
        out = defaultdict(list)
        for x in list(self.parent):
            out[self.find(x)].append(x)
        return out


def wrap_tokens(tokens, indent="    ", width=98):
    """Pack tokens onto wrapped lines; deterministic and diff-friendly."""
    lines, cur = [], indent
    for tok in tokens:
        if cur != indent and len(cur) + 1 + len(tok) > width:
            lines.append(cur)
            cur = indent
        cur += ("" if cur == indent else " ") + tok
    if cur != indent:
        lines.append(cur)
    return lines


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()[:12]


# ===========================================================================
# SCHEMATIC
# ===========================================================================

# RECORD ids used below
R_COMPONENT, R_PIN, R_LABEL, R_WIRE = "1", "2", "4", "27"
R_POWER, R_PORT, R_NETLABEL, R_JUNCTION = "17", "18", "25", "29"
R_SHEET, R_DESIGNATOR, R_PARAM, R_IMPL = "31", "34", "41", "45"
R_BUS, R_BUSENTRY, R_SHEETSYM, R_SHEETENTRY, R_NOERC = "26", "37", "15", "16", "22"

ELECTRICAL_TYPE = {
    "0": "IN", "1": "IO", "2": "OUT", "3": "OC",
    "4": "PAS", "5": "HIZ", "6": "EMIT", "7": "PWR",
}
# Parameters that are pure tooling/BOM/UI noise, or duplicated elsewhere in the
# output. Everything not listed here survives, so new vendor attributes are kept
# by default rather than silently dropped.
PARAM_NOISE = {
    "symbol", "device", "designator", "comment", "value", "footprintname",
    "description", "reuse block", "group id", "channel id", "3d model",
    "3d model transform", "modelid", "modelname", "modeltransform",
    "add into bom", "addinbom", "convert to pcb", "converttopcb",
    "jlcpcb part class", "supplier footprint", "supplier", "origin footprint",
    "ki_keywords", "ki_fp_filters", "id", "name", "comp", "sheet", "file",
    "attribute1", "attribute2", "attribute3",
    "comment1", "comment2", "comment3", "comment4",
}
# Kept but rendered on the identity line instead of the attribute line.
PARAM_IDENTITY = {
    "manufacturer part": "mpn", "manufacturerpart": "mpn",
    "supplier part": "lcsc", "supplierpart": "lcsc",
    "manufacturer": "mfr", "datasheet": "datasheet", "userdoclink": "doc",
}


def sch_coord(rec: dict, base: str) -> int:
    """Exact integer coordinate: units of 1e-5 of the schematic base unit."""
    try:
        whole = int(rec.get(base, 0) or 0)
    except ValueError:
        whole = 0
    try:
        frac = int(rec.get(base + "_FRAC", 0) or 0)
    except ValueError:
        frac = 0
    return whole * 100000 + frac


class SchComponent:
    __slots__ = ("ref", "lib", "desc", "footprint", "params", "pins", "indices")

    def __init__(self, ref):
        self.ref = ref
        self.lib = ""
        self.desc = ""
        self.footprint = ""
        self.params = {}
        self.pins = []          # list of dicts: number, name, etype, point
        self.indices = []


class SchModel:
    def __init__(self):
        self.sheet_params = {}
        self.components = {}    # ref -> SchComponent
        self.nets = []          # list of dicts: name, pins[(ref, pin, pinname)], named
        self.unconnected = []   # (ref, pin, pinname, noerc)
        self.notes = []         # free text labels on the sheet
        self.warnings = []
        self.counts = Counter()


def build_sch(records: list[dict]) -> SchModel:
    model = SchModel()
    by_owner = defaultdict(list)
    for idx, rec in enumerate(records):
        model.counts[rec.get("RECORD", "?")] += 1
        if "OWNERINDEX" in rec:
            try:
                by_owner[int(rec["OWNERINDEX"])].append(rec)
            except ValueError:
                pass

    # --- structural warnings: things this exporter deliberately does not resolve
    if model.counts[R_BUS] or model.counts[R_BUSENTRY]:
        model.warnings.append(
            "sheet contains buses/bus entries: bus connectivity is NOT resolved, "
            "the netlist below is incomplete for bussed signals"
        )
    if model.counts[R_SHEETSYM] or model.counts[R_SHEETENTRY]:
        model.warnings.append(
            "sheet contains hierarchical sheet symbols: sub-sheets are NOT "
            "flattened, export and review each sheet separately"
        )
    if model.counts[R_SHEET] > 1:
        model.warnings.append(
            f"file contains {model.counts[R_SHEET]} sheets; they are merged flat"
        )

    # ---------------------------------------------------------------- components
    for idx, rec in enumerate(records):
        if rec.get("RECORD") != R_COMPONENT:
            continue
        designators = [d.get("TEXT", "") for d in by_owner[idx]
                       if d.get("RECORD") == R_DESIGNATOR]
        ref = designators[0] if designators else f"?{idx}"
        comp = model.components.get(ref)
        if comp is None:
            comp = model.components[ref] = SchComponent(ref)
        comp.indices.append(idx)
        comp.lib = comp.lib or rec.get("LIBREFERENCE", "")
        comp.desc = comp.desc or rec.get("COMPONENTDESCRIPTION", "")

        for sub in by_owner[idx]:
            kind = sub.get("RECORD")
            if kind == R_PARAM:
                name, text = sub.get("NAME", ""), sub.get("TEXT", "")
                if name and text:
                    comp.params.setdefault(name, text)
            elif kind == R_IMPL and sub.get("MODELTYPE") == "PCBLIB":
                if sub.get("ISCURRENT", "T") == "T" or not comp.footprint:
                    comp.footprint = sub.get("MODELNAME", "")
            elif kind == R_PIN:
                length = 0
                try:
                    length = int(float(sub.get("PINLENGTH", 0) or 0)) * 100000
                except ValueError:
                    pass
                try:
                    orient = int(sub.get("PINCONGLOMERATE", 0) or 0) & 3
                except ValueError:
                    orient = 0
                dx, dy = ((1, 0), (0, 1), (-1, 0), (0, -1))[orient]
                x = sch_coord(sub, "LOCATION.X") + dx * length
                y = sch_coord(sub, "LOCATION.Y") + dy * length
                comp.pins.append({
                    "num": sub.get("DESIGNATOR", "?"),
                    "name": norm_overbar(sub.get("NAME", "")),
                    "etype": ELECTRICAL_TYPE.get(sub.get("ELECTRICAL", "4"), "?"),
                    "pt": (x, y),
                })
        if not comp.footprint:
            comp.footprint = comp.params.get("FootprintName", "")

    for comp in model.components.values():
        if len(comp.indices) > 1:
            model.warnings.append(
                f"{comp.ref}: {len(comp.indices)} symbol instances merged "
                f"(multi-part component; part-level detail not preserved)"
            )

    # ---------------------------------------------------------------- sheet info
    for rec in records:
        if rec.get("RECORD") == R_PARAM and "OWNERINDEX" not in rec:
            name, text = rec.get("NAME", ""), rec.get("TEXT", "")
            if name and text and name.lower() not in PARAM_NOISE:
                model.sheet_params.setdefault(name, text)
        elif rec.get("RECORD") == R_LABEL and rec.get("TEXT"):
            model.notes.append(rec["TEXT"])

    # Sheet-frame zone letters and title-block text (which is just the sheet
    # parameters rendered as "Key:Value") carry no review value; real free-text
    # annotations do, so keep only what is neither.
    boilerplate = {f"{k}:{v}" for k, v in model.sheet_params.items()}
    boilerplate |= set(model.sheet_params.values())
    model.notes = [n for n in model.notes
                   if len(n) > 3 and n not in boilerplate]

    # ------------------------------------------------------------- connectivity
    dsu = DisjointSet()
    segments = []           # (p1, p2)
    for rec in records:
        if rec.get("RECORD") != R_WIRE:
            continue
        try:
            n = int(rec.get("LOCATIONCOUNT", 2))
        except ValueError:
            n = 2
        verts = [(sch_coord(rec, f"X{i}"), sch_coord(rec, f"Y{i}"))
                 for i in range(1, max(n, 2) + 1)]
        for a, b in zip(verts, verts[1:]):
            segments.append((a, b))
            dsu.union(a, b)

    attachments = []        # (point, kind, payload)
    for comp in model.components.values():
        for pin in comp.pins:
            attachments.append((pin["pt"], "pin", (comp.ref, pin)))
    for rec in records:
        kind = rec.get("RECORD")
        pt = (sch_coord(rec, "LOCATION.X"), sch_coord(rec, "LOCATION.Y"))
        if kind == R_POWER:
            attachments.append((pt, "name", rec.get("TEXT", "")))
        elif kind == R_NETLABEL:
            if rec.get("TEXT"):
                attachments.append((pt, "name", rec["TEXT"]))
        elif kind == R_JUNCTION:
            attachments.append((pt, "junction", None))
        elif kind == R_PORT:
            try:
                w = int(float(rec.get("WIDTH", 0) or 0)) * 100000
            except ValueError:
                w = 0
            for off in (-w, 0, w):
                attachments.append(((pt[0] + off, pt[1]), "port",
                                    rec.get("NAME", "")))

    # Attach every point that lies exactly on a wire segment (endpoint or
    # interior -- the standard T-junction rule; pure crossings stay separate).
    def on_segment(p, seg):
        (x1, y1), (x2, y2) = seg
        x, y = p
        if (x2 - x1) * (y - y1) - (y2 - y1) * (x - x1) != 0:
            return False
        return (min(x1, x2) <= x <= max(x1, x2)
                and min(y1, y2) <= y <= max(y1, y2))

    touched = set()
    for pt, kind, _payload in attachments:
        if pt in touched:
            continue
        for seg in segments:
            if on_segment(pt, seg):
                dsu.union(pt, seg[0])
                touched.add(pt)
                break

    # Ports and named objects only join a net once they are physically on a wire;
    # then equal names merge their nets (net labels, power symbols, off-page ports).
    name_anchor = {}
    for pt, kind, payload in attachments:
        if kind in ("name", "port") and payload and pt in touched:
            if payload in name_anchor:
                dsu.union(name_anchor[payload], pt)
            else:
                name_anchor[payload] = pt

    # ---------------------------------------------------------------- net build
    net_names = defaultdict(set)
    for pt, kind, payload in attachments:
        if kind in ("name", "port") and payload and pt in touched:
            net_names[dsu.find(pt)].add(payload)

    net_pins = defaultdict(list)
    noerc_points = {(sch_coord(r, "LOCATION.X"), sch_coord(r, "LOCATION.Y"))
                    for r in records if r.get("RECORD") == R_NOERC}
    for comp in sorted(model.components.values(), key=lambda c: natkey(c.ref)):
        for pin in sorted(comp.pins, key=lambda p: natkey(p["num"])):
            if pin["pt"] in touched:
                net_pins[dsu.find(pin["pt"])].append((comp.ref, pin))
            else:
                model.unconnected.append(
                    (comp.ref, pin, pin["pt"] in noerc_points))

    for root, pins in net_pins.items():
        names = sorted(net_names.get(root, ()))
        if names:
            name, named = names[0], True
        else:
            ref, pin = pins[0]
            name, named = f"N${ref}.{pin['num']}", False
        model.nets.append({
            "name": name,
            "aliases": names[1:],
            "named": named,
            "pins": pins,
        })
    # Named nets with no pins at all (dangling label on a wire stub) still matter.
    for root, names in net_names.items():
        if root not in net_pins:
            model.nets.append({"name": sorted(names)[0], "aliases": sorted(names)[1:],
                               "named": True, "pins": []})
    model.nets.sort(key=lambda n: natkey(n["name"]))
    return model


def sch_pin_token(ref: str, pin: dict) -> str:
    tok = f"{ref}.{pin['num']}"
    if pin["name"] and pin["name"] != pin["num"]:
        tok += "=" + pin["name"]
    if pin["etype"] not in ("PAS", "?"):
        tok += ":" + pin["etype"]
    return tok


def sch_flags(model: SchModel) -> list[str]:
    flags = []
    lowered = defaultdict(list)
    for net in model.nets:
        lowered[net["name"].lower()].append(net["name"])
    for low, names in sorted(lowered.items()):
        if len(set(names)) > 1:
            flags.append("net names differ only by case: " + ", ".join(sorted(set(names))))

    for net in model.nets:
        if net["aliases"]:
            flags.append(
                f"net {net['name']}: carries {1 + len(net['aliases'])} names ("
                + ", ".join([net["name"]] + net["aliases"])
                + ") -- normal for a power symbol plus a net label, a mistake if "
                "two signals were meant to stay separate"
            )
        if len(net["pins"]) == 1:
            ref, pin = net["pins"][0]
            flags.append(f"net {net['name']}: only one pin ({sch_pin_token(ref, pin)})")
        elif not net["pins"]:
            flags.append(f"net {net['name']}: named but has no pins")

    for comp in sorted(model.components.values(), key=lambda c: natkey(c.ref)):
        if not comp.footprint:
            flags.append(f"{comp.ref}: no footprint assigned")
        if not comp.pins:
            flags.append(f"{comp.ref}: symbol has no pins")

    dup = [r for r, c in model.components.items() if len(c.indices) > 1]
    for ref in sorted(dup, key=natkey):
        flags.append(f"{ref}: designator used by {len(model.components[ref].indices)} symbols")
    return flags


def render_sch(path: Path, model: SchModel, opts) -> str:
    out = []
    add = out.append

    add("# SCHEMATIC REVIEW EXPORT")
    add(f"source: {opts.relpath(path)}")
    add(f"sha256-12: {digest(path)}")
    add(f"exporter: eda_review_export.py v{VERSION}")
    add("content: connectivity + component attributes only; symbol placement, "
        "wire routing, colours and fonts are intentionally omitted")
    for key in sorted(model.sheet_params):
        add(f"sheet.{key}: {model.sheet_params[key]}")
    pin_total = sum(len(c.pins) for c in model.components.values())
    add(f"summary: {len(model.components)} components, {pin_total} pins, "
        f"{len(model.nets)} nets, {len(model.unconnected)} unconnected pins")
    add("pin token: REF.PAD[=PINNAME][:TYPE]; TYPE omitted when Passive "
        "(EasyEDA exports all pins as Passive, so pin direction is unavailable)")

    if model.warnings:
        add("")
        add("## PARSER WARNINGS")
        for w in model.warnings:
            add(f"- {w}")

    add("")
    add("## COMPONENTS")
    add("# REF  val=  fp=  dev=  [mpn/lcsc/mfr]   then attributes, then description")
    for comp in sorted(model.components.values(), key=lambda c: natkey(c.ref)):
        params = dict(comp.params)
        value = params.get("Value") or params.get("Comment") or ""
        ident = [comp.ref]
        if value:
            ident.append(f"val={value}")
        if comp.footprint:
            ident.append(f"fp={comp.footprint}")
        if comp.lib and comp.lib != value:
            ident.append(f"dev={comp.lib}")
        ident.append(f"pins={len(comp.pins)}")
        for key in sorted(params, key=str.lower):
            short = PARAM_IDENTITY.get(key.lower())
            if short and params[key]:
                if short in ("datasheet", "doc") and not opts.all_params:
                    continue
                ident.append(f"{short}={params[key]}")
        add("  ".join(ident))

        attrs = []
        for key in sorted(params, key=str.lower):
            low = key.lower()
            if not opts.all_params and (low in PARAM_NOISE or low in PARAM_IDENTITY):
                continue
            if opts.all_params and low in PARAM_IDENTITY:
                continue
            val = params[key].strip()
            if not val:
                continue
            if len(val) > opts.max_param:
                val = val[: opts.max_param - 3] + "..."
            attrs.append(f"{key}={val}")
        if attrs:
            for line in wrap_tokens([a + ";" for a in attrs], indent="    "):
                add(line)
        desc = comp.desc or params.get("Description", "")
        if desc and not opts.all_params:
            # EasyEDA fills Description with the attribute list concatenated as
            # "Key:Value Key:Value ..."; if nothing survives removing the pairs
            # already printed above, the line is pure duplication.
            rest = desc
            for key, val in params.items():
                rest = rest.replace(f"{key}:{val}", " ")
            if not re.sub(r"[\s;,.]+", "", rest):
                desc = ""
        if desc:
            desc = re.sub(r"\s+", " ", desc).strip()
            if len(desc) > opts.max_param:
                desc = desc[: opts.max_param - 3] + "..."
            add(f"    desc: {desc}")

    add("")
    add("## NETS")
    add("# name (pin count) then the pins on it")
    for net in model.nets:
        marker = "" if net["named"] else "   [auto-named, no net label in source]"
        if net["aliases"]:
            marker += "   aka " + ", ".join(net["aliases"])
        add(f"{net['name']} ({len(net['pins'])}){marker}")
        toks = [sch_pin_token(ref, pin) for ref, pin in net["pins"]]
        for line in wrap_tokens(toks):
            add(line)

    if not opts.brief:
        add("")
        add("## PIN MAP  (same data as ## NETS, indexed by component)")
        for comp in sorted(model.components.values(), key=lambda c: natkey(c.ref)):
            pin_net = {}
            for net in model.nets:
                for ref, pin in net["pins"]:
                    if ref == comp.ref:
                        pin_net[pin["num"]] = net["name"]
            add(f"{comp.ref}  {comp.lib}")
            for pin in sorted(comp.pins, key=lambda p: natkey(p["num"])):
                label = pin["name"] if pin["name"] and pin["name"] != pin["num"] else ""
                add(f"    {pin['num']:>4} {label:<16} -> "
                    f"{pin_net.get(pin['num'], '(unconnected)')}")

    add("")
    add("## UNCONNECTED PINS")
    if model.unconnected:
        toks = []
        for ref, pin, noerc in model.unconnected:
            tok = sch_pin_token(ref, pin)
            if noerc:
                tok += "[no-ERC]"
            toks.append(tok)
        for line in wrap_tokens(toks, indent="    "):
            add(line)
    else:
        add("    none")

    if model.notes:
        add("")
        add("## SHEET NOTES  (free text placed on the schematic)")
        for note in sorted(set(model.notes)):
            add(f"    {re.sub(chr(10), ' ', note)}")

    flags = sch_flags(model)
    add("")
    add("## FLAGS  (structural hints only -- not an ERC)")
    if flags:
        for f in flags:
            add(f"- {f}")
    else:
        add("    none")
    add("")
    return "\n".join(out)


# ===========================================================================
# PCB
# ===========================================================================

COPPER_LAYER = re.compile(r"^(TOP|BOTTOM|MID(LAYER)?\d*|INTERNALPLANE\d*)$")


class PcbModel:
    def __init__(self):
        self.outline = []       # [(x, y)] in mm, file coordinates
        self.stack = []         # [(name, extra)]
        self.rules = []         # [(kind, name, "k=v k=v")]
        self.nets = {}          # id -> name
        self.components = {}    # id -> dict
        self.pads = []          # dicts
        self.vias = []
        self.tracks = []        # copper only
        self.arcs = []          # copper only
        self.polygons = []
        self.keepouts = []      # (layer, bbox)
        self.islands = []       # copper islands: {"nets": set, "pads": [(ref, pad)]}
        self.joined = []        # islands carrying more than one net id
        self.net_dsu = None     # net ids that share copper -> one electrical net
        self.inferred = 0       # copper primitives that got their net geometrically
        self.counts = Counter()
        self.warnings = []


def _vertices(rec: dict, count_key="MAINCONTOURVERTEXCOUNT"):
    try:
        n = int(rec.get(count_key, 0))
    except ValueError:
        n = 0
    pts = []
    i = 0
    while True:
        if f"VX{i}" not in rec:
            break
        pts.append((parse_len_mm(rec[f"VX{i}"], 0.0), parse_len_mm(rec[f"VY{i}"], 0.0)))
        i += 1
        if n and i >= n:
            break
    return pts


RULE_NOISE = {
    "RECORD", "SELECTION", "LOCKED", "POLYGONOUTLINE", "USERROUTED", "UNIONINDEX",
    "INDEXFORSAVE", "DEFINEDBYLOGICALDOCUMENT", "OBJECTCLEARANCES", "COMMENT",
    "IGNOREPADTOPADCLEARANCEINFOOTPRINT", "RULEKIND", "NAME",
}
# Fields whose value is the same on every rule EasyEDA writes; printing them on
# each line triples the section length without telling a reviewer anything.
RULE_DEFAULTS = {
    "LAYER": ("TOP", "UNKNOWN"), "NETSCOPE": ("AnyNet",), "LAYERKIND": ("SameLayer",),
    "ENABLED": ("TRUE",), "PRIORITY": ("1",), "SCOPE2EXPRESSION": ("All",),
}
# The diff-pair rule repeats identical width limits for 30 unused mid-layers.
RULE_PER_LAYER = re.compile(r"^MIDLAYER\d+_")
# Rule values that are lengths: bare numbers in these fields are mils.
RULE_LENGTH = re.compile(
    r"(width|clearance|gap|limit|expansion|expansionbottom|size|length)$")


def build_pcb(records: list[dict]) -> PcbModel:
    model = PcbModel()
    boards = []
    for rec in records:
        kind = rec.get("RECORD")
        model.counts[kind] += 1
        if kind == "Board":
            boards.append(rec)
        elif kind == "Net":
            model.nets[rec.get("ID")] = rec.get("NAME", "")
        elif kind == "Component":
            model.components[rec.get("ID")] = {
                "ref": rec.get("SOURCEDESIGNATOR", "?"),
                "fp": rec.get("PATTERN", ""),
                "layer": rec.get("LAYER", "?"),
                "x": parse_len_mm(rec.get("X"), 0.0),
                "y": parse_len_mm(rec.get("Y"), 0.0),
                "rot": rec.get("ROTATION", "0"),
                "locked": rec.get("LOCKED", "FALSE") == "TRUE",
                "pads": [],
            }
        elif kind == "Pad":
            model.pads.append({
                "comp": rec.get("COMPONENT"),
                "net": rec.get("NET"),
                "name": rec.get("NAME", "?"),
                "x": parse_len_mm(rec.get("X"), 0.0),
                "y": parse_len_mm(rec.get("Y"), 0.0),
                "xs": parse_len_mm(rec.get("XSIZE"), 0.0),
                "ys": parse_len_mm(rec.get("YSIZE"), 0.0),
                "rot": float(rec.get("ROTATION", 0) or 0),
                "shape": rec.get("SHAPE", "?"),
                "layer": rec.get("LAYER", "?"),
                "hole": parse_len_mm(rec.get("HOLESIZE"), 0.0),
                "plated": rec.get("PLATED", "TRUE") == "TRUE",
            })
        elif kind == "Via":
            model.vias.append({
                "net": rec.get("NET"),
                "x": parse_len_mm(rec.get("X"), 0.0),
                "y": parse_len_mm(rec.get("Y"), 0.0),
                "dia": parse_len_mm(rec.get("DIAMETER"), 0.0),
                "hole": parse_len_mm(rec.get("HOLESIZE"), 0.0),
                "span": f"{rec.get('STARTLAYER','?')}-{rec.get('ENDLAYER','?')}",
            })
        elif kind in ("Track", "Arc"):
            layer = rec.get("LAYER", "")
            if layer == "KEEPOUT" or rec.get("KEEPOUT") == "TRUE":
                model.keepouts.append((layer, rec))
            if "COMPONENT" in rec or not COPPER_LAYER.match(layer):
                continue        # footprint art / mechanical / documentation
            # NET may be absent: EasyEDA exports some routed copper without net
            # attribution, so it is recovered geometrically in infer_copper_nets().
            if kind == "Track":
                x1 = parse_len_mm(rec.get("X1"), 0.0)
                y1 = parse_len_mm(rec.get("Y1"), 0.0)
                x2 = parse_len_mm(rec.get("X2"), 0.0)
                y2 = parse_len_mm(rec.get("Y2"), 0.0)
                model.tracks.append({
                    "net": rec.get("NET"), "layer": layer,
                    "x1": x1, "y1": y1, "x2": x2, "y2": y2,
                    "w": parse_len_mm(rec.get("WIDTH"), 0.0),
                    "len": math.hypot(x2 - x1, y2 - y1),
                })
            else:
                radius = parse_len_mm(rec.get("RADIUS"), 0.0)
                start = float(rec.get("STARTANGLE", 0) or 0)
                end = float(rec.get("ENDANGLE", 0) or 0)
                cx = parse_len_mm(rec.get("LOCATION.X"), 0.0)
                cy = parse_len_mm(rec.get("LOCATION.Y"), 0.0)
                ends = [(cx + radius * math.cos(math.radians(a)),
                         cy + radius * math.sin(math.radians(a)))
                        for a in (start, end)]
                model.arcs.append({
                    "net": rec.get("NET"), "layer": layer,
                    "w": parse_len_mm(rec.get("WIDTH"), 0.0),
                    "len": 2 * math.pi * radius * (abs(end - start) / 360.0),
                    "x1": ends[0][0], "y1": ends[0][1],
                    "x2": ends[1][0], "y2": ends[1][1],
                })
        elif kind == "Region" and (rec.get("KEEPOUT") == "TRUE"
                                   or rec.get("LAYER") == "KEEPOUT"):
            model.keepouts.append((rec.get("LAYER", "?"), rec))
        elif kind == "Polygon":
            pts = _vertices(rec)
            model.polygons.append({
                "net": rec.get("NET"), "layer": rec.get("LAYER", "?"),
                "style": rec.get("HATCHSTYLE", "?"), "pts": pts,
                "locked": rec.get("LOCKED", "FALSE") == "TRUE",
            })
        elif kind == "DXPRule":
            fields = []
            for key in rec:
                if key in RULE_NOISE or RULE_PER_LAYER.match(key):
                    continue
                val = rec[key]
                if val in ("", "FALSE") or val in RULE_DEFAULTS.get(key, ()):
                    continue
                fields.append((key.lower(), val))
            model.rules.append((rec.get("RULEKIND", "?"), rec.get("NAME", ""),
                                fields))

    free = {"ref": "(free)", "fp": "", "layer": "-", "x": 0.0, "y": 0.0,
            "rot": "0", "locked": False, "pads": []}
    for pad in model.pads:
        comp = model.components.get(pad["comp"], free)
        comp["pads"].append(pad)
        pad["owner"] = comp["ref"]
    if free["pads"]:
        model.components["(free)"] = free
        model.warnings.append(
            f"{len(free['pads'])} pads belong to no component (loose pads placed "
            "directly on the board); listed under the designator \"(free)\""
        )

    # Board outline lives on the first Board record; layer stack on the second.
    for rec in boards:
        if not model.outline and rec.get("ISSHAPEBASED") == "TRUE" and "VX0" in rec:
            model.outline = _vertices(rec)
        for key, val in rec.items():
            m = re.match(r"V9_STACK_LAYER(\d+)_NAME$", key)
            if m:
                idx = m.group(1)
                extra = []
                for suffix, label in (("COPTHICK", "cu"), ("DIELCONST", "er"),
                                      ("DIELHEIGHT", "h")):
                    v = rec.get(f"V9_STACK_LAYER{idx}_{suffix}")
                    if v:
                        extra.append(f"{label}={v}")
                model.stack.append((int(idx), val, " ".join(extra)))
    model.stack.sort()
    model.rules.sort(key=lambda r: (r[0], r[1]))
    if not model.outline:
        model.warnings.append("no board outline found in the Board record")
    infer_copper_nets(model)
    return model


def infer_copper_nets(model: PcbModel) -> None:
    """Recover net attribution for copper that was exported without a NET field.

    EasyEDA writes a NET key on most routed copper but not all of it (in the
    reference board, 130 of 404 copper segments had none). Reporting those nets
    as "unrouted" would be actively misleading, so copper is grouped into
    galvanically connected *islands* and each island lends its net to whatever
    inside it lacks one.

    Islands are built by geometric contact between tracks, arcs, vias and pads.
    Contact means the copper *overlaps*: every conductor carries its real width,
    so a track is a capsule of half-width w/2 around its centre-line, a pad is
    its rectangle, and a via is its barrel. Two items touch when those regions
    intersect. Pads and vias on MULTILAYER bridge layers; everything else must
    share a layer, so top and bottom copper only merge through a via or a
    through-hole pad. Copper pours are NOT part of the graph (only their outline
    is exported), so pour-connected nets -- typically ground -- legitimately
    show several islands.

    Width matters, and testing centre-lines alone does not work. Two 0.8 mm
    tracks meeting end to end with their endpoints 0.05 mm apart share 0.75 mm
    of copper and are one conductor; a track crossing a pad connects to it even
    though neither endpoint lands inside. An endpoint-coincidence test calls both
    of those unrouted, which is how three consecutive reviews of the receiver
    board came to report ten airwires on a board that was fully routed.

    Requiring true overlap rather than proximity is what keeps the test from
    erring the other way: distinct nets are held apart by the clearance rule
    (0.254 mm on the reference board), which is an order of magnitude more than
    the tolerance below, so no realistic layout merges two nets here. A merge
    that does happen is reported under "nets joined" rather than silently.

    Side effects: fills model.islands / model.joined / model.inferred and sets
    the "net" key on copper items that had none.
    """
    items = []
    for kind, seq in (("T", model.tracks), ("A", model.arcs)):
        for it in seq:
            items.append({"o": it, "kind": kind, "layer": it["layer"],
                          "pts": [(it["x1"], it["y1"]), (it["x2"], it["y2"])],
                          "seg": True, "r": max(it.get("w") or 0.0, 0.0) / 2})
    for via in model.vias:
        # DIAMETER vs HOLESIZE is unreliable in this dialect (see ## VIAS), so
        # take the larger as the copper contact radius.
        items.append({"o": via, "kind": "V", "layer": "*",
                      "pts": [(via["x"], via["y"])], "seg": False,
                      "r": max(via["dia"], via["hole"]) / 2})
    pad_index = {}
    for comp in model.components.values():
        for pad in comp["pads"]:
            x0, y0, x1, y1 = pad_extent(pad)
            item = {"o": pad, "kind": "P",
                    "layer": "*" if pad["layer"] == "MULTILAYER" else pad["layer"],
                    "pts": [(pad["x"], pad["y"])], "seg": False, "r": 0.0,
                    "box": (x0, y0, x1, y1), "ref": comp["ref"]}
            pad_index[id(pad)] = item
            items.append(item)

    tol = 1e-4          # 0.1 um: immune to mil->mm rounding, far under any clearance

    def _pt_seg(p, a, b):
        """Distance from point p to segment ab."""
        dx, dy = b[0] - a[0], b[1] - a[1]
        span = dx * dx + dy * dy
        if span == 0.0:
            return math.hypot(p[0] - a[0], p[1] - a[1])
        t = ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / span
        t = 0.0 if t < 0.0 else (1.0 if t > 1.0 else t)
        return math.hypot(p[0] - (a[0] + t * dx), p[1] - (a[1] + t * dy))

    def _seg_seg(a0, a1, b0, b1):
        """Distance between segments a and b; 0 if they cross."""
        d1x, d1y = a1[0] - a0[0], a1[1] - a0[1]
        d2x, d2y = b1[0] - b0[0], b1[1] - b0[1]
        denom = d1x * d2y - d1y * d2x
        if denom != 0.0:                       # not parallel: test for a crossing
            ex, ey = b0[0] - a0[0], b0[1] - a0[1]
            t = (ex * d2y - ey * d2x) / denom
            u = (ex * d1y - ey * d1x) / denom
            if 0.0 <= t <= 1.0 and 0.0 <= u <= 1.0:
                return 0.0
        return min(_pt_seg(a0, b0, b1), _pt_seg(a1, b0, b1),
                   _pt_seg(b0, a0, a1), _pt_seg(b1, a0, a1))

    def _pt_box(p, box):
        """Distance from point p to an axis-aligned box; 0 if inside."""
        x0, y0, x1, y1 = box
        dx = max(x0 - p[0], 0.0, p[0] - x1)
        dy = max(y0 - p[1], 0.0, p[1] - y1)
        return math.hypot(dx, dy)

    def _seg_box(a, b, box):
        """Distance from segment ab to an axis-aligned box; 0 if it enters."""
        if _pt_box(a, box) == 0.0 or _pt_box(b, box) == 0.0:
            return 0.0
        x0, y0, x1, y1 = box
        edges = (((x0, y0), (x1, y0)), ((x1, y0), (x1, y1)),
                 ((x1, y1), (x0, y1)), ((x0, y1), (x0, y0)))
        return min(_seg_seg(a, b, e0, e1) for e0, e1 in edges)

    def gap(a, b):
        """Edge-to-edge distance between two copper items; <= 0 means overlap."""
        ka, kb = a["kind"], b["kind"]
        if ka == "P" and kb == "P":             # rect vs rect
            ax0, ay0, ax1, ay1 = a["box"]
            bx0, by0, bx1, by1 = b["box"]
            dx = max(bx0 - ax1, 0.0, ax0 - bx1)
            dy = max(by0 - ay1, 0.0, ay0 - by1)
            return math.hypot(dx, dy)
        if ka == "P":                           # keep the pad on the right
            a, b, ka, kb = b, a, kb, ka
        if ka == "V" and kb == "V":             # barrel vs barrel
            return math.hypot(a["pts"][0][0] - b["pts"][0][0],
                              a["pts"][0][1] - b["pts"][0][1]) - a["r"] - b["r"]
        if ka == "V":                           # barrel vs pad
            return _pt_box(a["pts"][0], b["box"]) - a["r"]
        # a is a segment (track or arc chord)
        if kb == "V":
            return _pt_seg(b["pts"][0], *a["pts"]) - a["r"] - b["r"]
        if kb == "P":
            return _seg_box(a["pts"][0], a["pts"][1], b["box"]) - a["r"]
        return _seg_seg(*a["pts"], *b["pts"]) - a["r"] - b["r"]

    def touch(a, b):
        if a["layer"] != "*" and b["layer"] != "*" and a["layer"] != b["layer"]:
            return False
        return gap(a, b) <= tol

    # Spatial hash so the pairwise scan stays near-linear on large boards.
    CELL = 2.0
    buckets = defaultdict(list)
    for idx, item in enumerate(items):
        xs = [p[0] for p in item["pts"]]
        ys = [p[1] for p in item["pts"]]
        pad = max(item["r"], 0.0)
        if item["kind"] == "P":
            xs, ys = item["box"][0::2], item["box"][1::2]
        for cx in range(int((min(xs) - pad) // CELL), int((max(xs) + pad) // CELL) + 1):
            for cy in range(int((min(ys) - pad) // CELL), int((max(ys) + pad) // CELL) + 1):
                buckets[(cx, cy)].append(idx)

    model.net_dsu = DisjointSet()
    for net_id in model.nets:
        model.net_dsu.find(net_id)

    dsu = DisjointSet()
    for idx in range(len(items)):
        dsu.find(idx)
    for cell in buckets.values():
        for i, a in enumerate(cell):
            for b in cell[i + 1:]:
                if dsu.find(a) != dsu.find(b) and touch(items[a], items[b]):
                    dsu.union(a, b)

    for root, members in sorted(dsu.groups().items()):
        nets = {items[m]["o"]["net"] for m in members
                if items[m]["o"].get("net") is not None}
        pads = sorted(((items[m]["ref"], items[m]["o"]) for m in members
                       if items[m]["kind"] == "P"),
                      key=lambda rp: (natkey(rp[0]), natkey(rp[1]["name"])))
        island = {"nets": nets, "pads": pads,
                  "copper": [items[m]["o"] for m in members
                             if items[m]["kind"] in ("T", "A")]}
        model.islands.append(island)
        if len(nets) > 1:
            model.joined.append(island)
            first = min(nets, key=lambda n: (model.nets.get(n, ""), str(n)))
            for other in nets:
                model.net_dsu.union(first, other)
        if len(nets) == 1:
            (only,) = tuple(nets)
            for item in island["copper"]:
                if item["net"] is None:
                    item["net"] = only
                    model.inferred += 1
        elif len(nets) > 1:
            # Ambiguous: attribute to the net that owns the most pads here so the
            # length totals stay complete, and report the join in ## FLAGS.
            counts = Counter(p["net"] for _ref, p in pads if p["net"] is not None)
            # Deterministic tie-break: most pads, then net name, then id.
            pick = min(nets, key=lambda n: (-counts.get(n, 0),
                                            model.nets.get(n, ""), str(n)))
            for item in island["copper"]:
                if item["net"] is None:
                    item["net"] = pick
                    model.inferred += 1
    orphan = sum(1 for it in model.tracks + model.arcs if it["net"] is None)
    if orphan:
        model.warnings.append(
            f"{orphan} copper primitives touch no pad or via and carry no net "
            "attribution; they are excluded from the per-net routing totals"
        )


def effective_nets(model: PcbModel):
    """Collapse net objects that are physically the same copper into one entry.

    EasyEDA can leave a stale duplicate of a net in the PCB net table (typically
    the same name in a different case) with part of the copper on one and the
    pads on the other. Electrically there is one net, so reporting them
    separately would invent routing errors that do not exist. Entries are keyed
    by the union-find root built in infer_copper_nets(); the name owning the most
    pads becomes the primary and the rest are reported as aliases.
    """
    groups = defaultdict(lambda: {"ids": set(), "pads": [], "islands": set(),
                                  "lengths": defaultdict(float), "widths": set(),
                                  "vias": 0, "pours": []})
    root = (model.net_dsu.find if model.net_dsu else (lambda n: n))
    for net_id in model.nets:
        groups[root(net_id)]["ids"].add(net_id)
    for comp in sorted(model.components.values(), key=lambda c: natkey(c["ref"])):
        for pad in sorted(comp["pads"], key=lambda p: natkey(p["name"])):
            if pad["net"] is not None:
                groups[root(pad["net"])]["pads"].append((comp["ref"], pad))
    for idx, island in enumerate(model.islands):
        for _ref, pad in island["pads"]:
            if pad["net"] is not None:
                groups[root(pad["net"])]["islands"].add(idx)
    for item in model.tracks + model.arcs:
        if item["net"] is None:
            continue
        g = groups[root(item["net"])]
        g["lengths"][item["layer"]] += item["len"]
        g["widths"].add(round(item["w"], 4))
    for via in model.vias:
        if via["net"] is not None:
            groups[root(via["net"])]["vias"] += 1
    for poly in model.polygons:
        if poly["net"] is not None:
            groups[root(poly["net"])]["pours"].append(poly["layer"])

    out = []
    for g in groups.values():
        pads_per_id = Counter(p["net"] for _r, p in g["pads"])
        names = {}
        for net_id in g["ids"]:
            names[model.nets.get(net_id, str(net_id))] = pads_per_id.get(net_id, 0)
        # Deterministic even when two names tie on pad count and compare equal
        # under the case-insensitive natural sort (e.g. "spare_tx"/"SPARE_TX").
        primary = min(names, key=lambda n: (-names[n], n))
        g["name"] = primary
        g["aliases"] = [n for n in sorted(names, key=lambda n: (natkey(n), n))
                        if n != primary]
        out.append(g)
    out.sort(key=lambda g: (natkey(g["name"]), g["name"]))
    return out


def bbox(points):
    xs = [p[0] for p in points]
    ys = [p[1] for p in points]
    return min(xs), min(ys), max(xs), max(ys)


def pad_extent(pad):
    """Axis-aligned extent of a (possibly rotated) rectangular/round pad."""
    r = math.radians(pad["rot"])
    c, s = abs(math.cos(r)), abs(math.sin(r))
    w = pad["xs"] * c + pad["ys"] * s
    h = pad["xs"] * s + pad["ys"] * c
    return (pad["x"] - w / 2, pad["y"] - h / 2, pad["x"] + w / 2, pad["y"] + h / 2)


def render_pcb(path: Path, model: PcbModel, opts) -> str:
    unit, scale = ("mm", 1.0) if opts.units == "mm" else ("mil", 1 / MM_PER_MIL)
    ox = oy = 0.0
    if opts.origin == "board" and model.outline:
        ox, oy, _, _ = bbox(model.outline)

    def q(v):
        return f"{(v - 0) * scale:.3f}".rstrip("0").rstrip(".")

    def X(v):
        return f"{(v - ox) * scale:.3f}".rstrip("0").rstrip(".")

    def Y(v):
        return f"{(v - oy) * scale:.3f}".rstrip("0").rstrip(".")

    out = []
    add = out.append
    add("# PCB REVIEW EXPORT")
    add(f"source: {opts.relpath(path)}")
    add(f"sha256-12: {digest(path)}")
    add(f"exporter: eda_review_export.py v{VERSION}")
    add("content: placement, coordinates, stackup, rules and per-net routing "
        "summary; silkscreen/assembly art is reduced to counts")
    add(f"units: {unit}   (source file is mil)")
    add("origin: " + ("board outline lower-left corner" if opts.origin == "board"
                      and model.outline else "raw file coordinates")
        + f"   X right, Y up   offset applied = ({q(ox)}, {q(oy)}) {unit}")

    real = [c for c in model.components.values() if c["ref"] != "(free)"]
    top = sum(1 for c in real if c["layer"] == "TOP")
    bot = len(real) - top
    routed = sum(t["len"] for t in model.tracks) + sum(a["len"] for a in model.arcs)
    add(f"summary: {len(real)} components ({top} top / {bot} bottom), "
        f"{len(model.pads)} pads, {len(model.nets)} nets, {len(model.vias)} vias, "
        f"{len(model.tracks)} copper track segments ({q(routed)} {unit} total), "
        f"{len(model.polygons)} copper pours")
    if model.inferred:
        add(f"note: {model.inferred} copper primitives had no net in the file; "
            "their net was recovered from what the copper physically touches")
    art = model.counts['Region'] + model.counts['Text'] + model.counts['Dimension']
    add(f"omitted: {art} silkscreen/assembly/dimension primitives, "
        f"{model.counts['Track'] - len(model.tracks)} non-copper line primitives")

    if model.warnings:
        add("")
        add("## PARSER WARNINGS")
        for w in model.warnings:
            add(f"- {w}")

    add("")
    add("## BOARD")
    if model.outline:
        x0, y0, x1, y1 = bbox(model.outline)
        add(f"outline bbox: {q(x1 - x0)} x {q(y1 - y0)} {unit}   "
            f"({len(model.outline)} vertices)")
        add("outline: " + " ".join(f"({X(px)},{Y(py)})" for px, py in model.outline))
    else:
        add("outline: not found")
    add("stackup:")
    for _idx, name, extra in model.stack:
        add(f"    {name}" + (f"   {extra}" if extra else ""))

    if model.rules:
        add("")
        add("## DESIGN RULES")
        add(f"# lengths shown in {unit}")
        for kind, name, fields in model.rules:
            parts = []
            for key, val in fields:
                if RULE_LENGTH.search(key):
                    mm = parse_len_mm(val)
                    if mm is not None and math.isfinite(mm):
                        val = q(mm)
                parts.append(f"{key}={val}")
            add(f"{kind} \"{name}\": " + " ".join(parts))

    add("")
    add("## PLACEMENT")
    add(f"# REF  layer  origin(x,y)  rot  fp  padcount  copper extent [{unit}]")
    comps = sorted(model.components.values(), key=lambda c: natkey(c["ref"]))
    outline_box = bbox(model.outline) if model.outline else None
    for comp in comps:
        if comp["ref"] == "(free)":
            continue        # loose pads have no footprint origin; see ## PADS
        line = (f"{comp['ref']}  {comp['layer']}  ({X(comp['x'])},{Y(comp['y'])})  "
                f"rot={comp['rot']}  fp={comp['fp']}  pads={len(comp['pads'])}")
        if comp["locked"]:
            line += "  LOCKED"
        add(line)
        if comp["pads"]:
            ex = [pad_extent(p) for p in comp["pads"]]
            bx0, by0 = min(e[0] for e in ex), min(e[1] for e in ex)
            bx1, by1 = max(e[2] for e in ex), max(e[3] for e in ex)
            add(f"    extent {q(bx1 - bx0)} x {q(by1 - by0)}  "
                f"x {X(bx0)}..{X(bx1)}  y {Y(by0)}..{Y(by1)}")

    add("")
    add("## PADS")
    add(f"# REF.PAD  net  (x,y)  size  shape  layer  [hole]   [{unit}]")
    for comp in comps:
        for pad in sorted(comp["pads"], key=lambda p: natkey(p["name"])):
            net = model.nets.get(pad["net"], "(no net)")
            line = (f"{comp['ref']}.{pad['name']}  {net}  "
                    f"({X(pad['x'])},{Y(pad['y'])})  {q(pad['xs'])}x{q(pad['ys'])}  "
                    f"{pad['shape']}  {pad['layer']}")
            if pad["rot"]:
                line += f"  rot={pad['rot']:g}"
            if pad["hole"]:
                line += f"  hole={q(pad['hole'])}"
                if not pad["plated"]:
                    line += "(NPTH)"
            add(line)

    # --------------------------------------------------------------- net summary
    enets = effective_nets(model)

    add("")
    add("## NETS")
    add("# name (pad count) [aka ...]: pads, then a routing summary")
    add("# \"aka\" means the file holds several net objects that copper proves are "
        "one net")
    for g in enets:
        header = f"{g['name']} ({len(g['pads'])})"
        if g["aliases"]:
            header += "   aka " + ", ".join(g["aliases"])
        add(header)
        if g["pads"]:
            for line in wrap_tokens([f"{r}.{p['name']}" for r, p in g["pads"]]):
                add(line)
        summary = []
        if g["lengths"]:
            summary.append("copper " + ", ".join(
                f"{layer} {q(g['lengths'][layer])}" for layer in sorted(g["lengths"]))
                + f" {unit}")
        if g["widths"]:
            summary.append("width " + "/".join(q(w) for w in sorted(g["widths"])))
        if g["vias"]:
            summary.append(f"vias {g['vias']}")
        if g["pours"]:
            summary.append("pour " + "+".join(sorted(set(g["pours"]))))
        pieces = len(g["islands"])
        if pieces > 1 and not g["pours"]:
            summary.append(f"*** {pieces} disconnected copper islands "
                           f"-> {pieces - 1} airwire(s) ***")
        if not summary and len(g["pads"]) > 1:
            summary.append("*** NO COPPER (unrouted) ***")
        if summary:
            add("    routing: " + "; ".join(summary))

    if model.polygons:
        add("")
        add("## COPPER POURS")
        for poly in model.polygons:
            box = bbox(poly["pts"]) if poly["pts"] else (0, 0, 0, 0)
            add(f"{model.nets.get(poly['net'], '(no net)')}  {poly['layer']}  "
                f"{poly['style']}  bbox x {X(box[0])}..{X(box[2])} "
                f"y {Y(box[1])}..{Y(box[3])}"
                + ("  LOCKED" if poly["locked"] else ""))

    if model.vias:
        add("")
        add("## VIAS")
        add(f"# count  DIAMETER  HOLESIZE  layer span   [{unit}]")
        add("# reported verbatim: EasyEDA writes DIAMETER smaller than HOLESIZE, "
            "so which field is the pad and which is the drill is not decidable "
            "from the file -- read them as the pair {0.3, 0.6} mm, not as labelled")
        styles = Counter((round(v["dia"], 4), round(v["hole"], 4), v["span"])
                         for v in model.vias)
        for (dia, hole, span), count in sorted(styles.items()):
            add(f"{count:>5}  {q(dia)}  {q(hole)}  {span}")

    if model.keepouts:
        add("")
        add("## KEEPOUTS")
        for layer, rec in model.keepouts:
            pts = _vertices(rec)
            if not pts and "X1" in rec:
                pts = [(parse_len_mm(rec["X1"]), parse_len_mm(rec["Y1"])),
                       (parse_len_mm(rec["X2"]), parse_len_mm(rec["Y2"]))]
            if not pts:
                continue
            box = bbox(pts)
            add(f"{rec.get('RECORD')}  {layer}  x {X(box[0])}..{X(box[2])}  "
                f"y {Y(box[1])}..{Y(box[3])}")

    if opts.tracks:
        add("")
        add("## COPPER TRACKS")
        add(f"# net  layer  (x1,y1)->(x2,y2)  width   [{unit}]")
        for t in sorted(model.tracks, key=lambda t: (natkey(model.nets.get(t["net"], "")),
                                                     t["layer"], t["x1"], t["y1"])):
            add(f"{model.nets.get(t['net'], '(no net)')}  {t['layer']}  "
                f"({X(t['x1'])},{Y(t['y1'])})->({X(t['x2'])},{Y(t['y2'])})  {q(t['w'])}")

    add("")
    add("## FLAGS  (structural hints only -- not a DRC)")
    critical, warn, info = [], [], []
    stale = []
    for g in enets:
        pads, pieces = g["pads"], len(g["islands"])
        label = g["name"]
        if len(pads) > 1 and not g["lengths"] and not g["pours"]:
            critical.append(f"net {label}: {len(pads)} pads but no copper -- unrouted")
        elif pieces > 1 and not g["pours"]:
            critical.append(f"net {label}: pads sit on {pieces} separate copper "
                            f"islands -- {pieces - 1} connection(s) still unrouted")
        if len(pads) == 1:
            warn.append(f"net {label}: single pad ({pads[0][0]}.{pads[0][1]['name']})")
        if not pads and not g["lengths"] and not g["pours"]:
            warn.append(f"net {label}: no pads and no copper -- stale net entry")
        if g["aliases"]:
            stale.append(" = ".join([g["name"]] + g["aliases"]))
    if stale:
        same_case = all(len({n.lower() for n in e.split(" = ")}) == 1 for e in stale)
        warn.append(
            f"{len(stale)} nets exist as several net objects that copper proves are "
            "one net" + (", differing only in case" if same_case else "")
            + " -- the PCB net table is out of sync with the schematic: "
            + "; ".join(sorted(stale)))
    if any(g["pours"] for g in enets):
        info.append("copper pours are exported as outlines only, so pads a pour "
                    "connects are not verified here; nets with a pour are exempt "
                    "from the island check above")
    merged = {n for g in enets for n in [g["name"]] + g["aliases"] if g["aliases"]}
    lowered = defaultdict(set)
    for name in model.nets.values():
        if name not in merged:
            lowered[name.lower()].add(name)
    for _low, names in sorted(lowered.items()):
        if len(names) > 1:
            warn.append("net names differ only by case and are NOT joined by "
                        "copper: " + ", ".join(sorted(names)))
    nonet = []
    for pad in model.pads:
        if pad["net"] is not None:
            continue
        ref = pad.get("owner", "?")
        tok = f"{ref}.{pad['name']}"
        if ref == "(free)":
            tok += f"@({X(pad['x'])},{Y(pad['y'])})"
        nonet.append((ref, pad["name"], tok))
    if nonet:
        info.append(f"{len(nonet)} pads with no net: "
                    + " ".join(t for _r, _n, t in
                               sorted(nonet, key=lambda t: (natkey(t[0]), natkey(t[1])))))
    dup = [r for r, c in Counter(c["ref"] for c in comps).items() if c > 1]
    for ref in sorted(dup, key=natkey):
        warn.append(f"{ref}: designator used by more than one footprint")
    if outline_box:
        for comp in comps:
            if not comp["pads"] or comp["ref"] == "(free)":
                continue
            ex = [pad_extent(pd) for pd in comp["pads"]]
            if (min(e[0] for e in ex) < outline_box[0]
                    or min(e[1] for e in ex) < outline_box[1]
                    or max(e[2] for e in ex) > outline_box[2]
                    or max(e[3] for e in ex) > outline_box[3]):
                critical.append(f"{comp['ref']}: copper extends past the board outline")
    min_w = min((t["w"] for t in model.tracks), default=None)
    if min_w:
        info.append(f"narrowest copper track: {q(min_w)} {unit} "
                    f"(compare against the Width rule above)")
    flags = critical + warn + info
    if flags:
        for f in flags:
            add(f"- {f}")
    else:
        add("    none")
    add("")
    return "\n".join(out)


# ===========================================================================
# Schematic <-> PCB cross-check
# ===========================================================================

def render_xcheck(stem: str, sch: SchModel, pcb: PcbModel, opts) -> str:
    out = []
    add = out.append
    add("# SCHEMATIC <-> PCB CROSS-CHECK")
    add(f"design: {stem}")
    add(f"exporter: eda_review_export.py v{VERSION}")
    add("method: components compared by designator, nets compared by their exact "
        "set of REF.PAD members (names ignored, so a renamed net still matches)")

    sch_refs = {c.ref: c for c in sch.components.values()}
    pcb_refs = {c["ref"]: c for c in pcb.components.values() if c["ref"] != "(free)"}

    add("")
    add("## COMPONENTS")
    only_sch = sorted(set(sch_refs) - set(pcb_refs), key=natkey)
    only_pcb = sorted(set(pcb_refs) - set(sch_refs), key=natkey)
    if only_sch:
        add("in schematic but not on PCB: " + " ".join(only_sch))
    if only_pcb:
        add("on PCB but not in schematic: " + " ".join(only_pcb))
    fp_mismatch = []
    pin_mismatch = []
    for ref in sorted(set(sch_refs) & set(pcb_refs), key=natkey):
        s_fp = sch_refs[ref].footprint.split(":")[-1]
        p_fp = pcb_refs[ref]["fp"].split(":")[-1]
        if s_fp and p_fp and s_fp != p_fp:
            fp_mismatch.append(f"{ref}: sch={sch_refs[ref].footprint} "
                               f"pcb={pcb_refs[ref]['fp']}")
        s_pins = {p["num"] for p in sch_refs[ref].pins}
        p_pins = {p["name"] for p in pcb_refs[ref]["pads"]}
        if s_pins != p_pins:
            missing = sorted(s_pins - p_pins, key=natkey)
            extra = sorted(p_pins - s_pins, key=natkey)
            parts = []
            if missing:
                parts.append("no pad for pin " + ",".join(missing))
            if extra:
                parts.append("pad with no pin " + ",".join(extra))
            pin_mismatch.append(f"{ref}: " + "; ".join(parts))
    if fp_mismatch:
        add("footprint differs:")
        for m in fp_mismatch:
            add(f"    {m}")
    if pin_mismatch:
        add("pin/pad set differs:")
        for m in pin_mismatch:
            add(f"    {m}")
    if not (only_sch or only_pcb or fp_mismatch or pin_mismatch):
        add("all components match")

    # ------------------------------------------------------------------- nets
    sch_sets = {}
    for net in sch.nets:
        key = frozenset(f"{ref}.{pin['num']}" for ref, pin in net["pins"])
        if key:
            sch_sets.setdefault(key, []).append(net["name"])
    # Compare against the *effective* PCB nets: net objects that copper proves
    # are one net are merged first, otherwise a stale duplicate in the PCB net
    # table shows up here as a fake missing connection.
    enets = effective_nets(pcb)
    pcb_sets = {}
    empty = []
    merged = []
    for g in enets:
        members = frozenset(f"{r}.{p['name']}" for r, p in g["pads"])
        if g["aliases"]:
            merged.append([g["name"]] + g["aliases"])
        if members:
            pcb_sets.setdefault(members, []).append(g["name"])
        elif not g["lengths"] and not g["pours"]:
            empty.append(g["name"])

    add("")
    add("## NETS")
    matched, auto, renamed, sch_only, pcb_only = 0, 0, [], [], []
    for key, names in sch_sets.items():
        if key in pcb_sets:
            matched += 1
            if set(names) != set(pcb_sets[key]):
                # Both sides auto-generated (N$... here, $2N... in EasyEDA) is
                # not a discrepancy, just two naming schemes for unnamed nets.
                if all(n.startswith("N$") for n in names) and                         all(AUTONAME.match(n) for n in pcb_sets[key]):
                    auto += 1
                else:
                    renamed.append(f"{'/'.join(sorted(names))}  (sch)  <->  "
                                   f"{'/'.join(sorted(pcb_sets[key]))}  (pcb)")
        else:
            sch_only.append((sorted(names)[0], key))
    for key, names in pcb_sets.items():
        if key not in sch_sets:
            pcb_only.append((sorted(names)[0], key))

    add(f"{matched} of {len(sch_sets)} schematic nets have an identical pad set on the PCB")
    if merged:
        add(f"{len(merged)} PCB net objects were merged before comparing because "
            "copper physically joins them:")
        for names in sorted(merged):
            add("    " + " = ".join(names))
    if auto:
        add(f"{auto} further nets match but are auto-named on both sides "
            "(no net label in the schematic) -- names not compared")
    if renamed:
        add("same pads, different name:")
        for r in sorted(renamed):
            add(f"    {r}")
    if sch_only:
        add("schematic nets with no matching PCB net:")
        for name, key in sorted(sch_only, key=lambda t: natkey(t[0])):
            best, score = None, 0
            for pkey, pnames in pcb_sets.items():
                overlap = len(key & pkey)
                if overlap > score:
                    best, score = pnames, overlap
            hint = (f"   closest pcb net: {'/'.join(sorted(best))} "
                    f"({score}/{len(key)} pads shared)") if best else ""
            add(f"    {name}: " + " ".join(sorted(key, key=natkey)) + hint)
    if pcb_only:
        add("PCB nets with no matching schematic net:")
        for name, key in sorted(pcb_only, key=lambda t: natkey(t[0])):
            add(f"    {name}: " + " ".join(sorted(key, key=natkey)))
    if empty:
        add("PCB nets with no pads at all (stale entries): "
            + " ".join(sorted(empty, key=natkey)))
    add("")
    return "\n".join(out)


# ===========================================================================
# CLI
# ===========================================================================

def collect_inputs(paths):
    files = []
    for raw in paths:
        p = Path(raw)
        if p.is_dir():
            files += sorted(p.rglob("*.schdoc")) + sorted(p.rglob("*.pcbdoc"))
        elif p.is_file():
            files.append(p)
        else:
            print(f"warning: no such file or directory: {p}", file=sys.stderr)
    # Deterministic order, schematics first so xcheck has the sch model ready.
    return sorted(set(files), key=lambda f: (f.suffix.lower() != ".schdoc", str(f).lower()))


def main(argv=None):
    root = Path(__file__).resolve().parent
    ap = argparse.ArgumentParser(
        description=__doc__.split("USAGE")[0].strip(),
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    ap.add_argument("paths", nargs="*", default=[str(root)],
                    help="files or directories (default: this script's directory)")
    ap.add_argument("-o", "--out-dir", default=None,
                    help="output directory (default: alongside each source file)")
    ap.add_argument("--stdout", action="store_true", help="print instead of writing")
    ap.add_argument("--units", choices=("mm", "mil"), default="mm")
    ap.add_argument("--origin", choices=("board", "file"), default="board")
    ap.add_argument("--tracks", action="store_true", help="dump every copper track")
    ap.add_argument("--all-params", action="store_true",
                    help="keep BOM/3D-model/UI parameters too")
    ap.add_argument("--brief", action="store_true",
                    help="omit the per-component pin map")
    ap.add_argument("--max-param", type=int, default=160,
                    help="truncate parameter/description values (default 160)")
    ap.add_argument("--no-cross-check", dest="cross_check", action="store_false",
                    help="skip the schematic-vs-PCB report")
    opts = ap.parse_args(argv)
    opts.relpath = lambda p: _relpath(p, root)

    files = collect_inputs(opts.paths or [str(root)])
    if not files:
        print("nothing to do: no .schdoc/.pcbdoc files found", file=sys.stderr)
        return 1

    sch_models, pcb_models, stems = {}, {}, {}
    written = []
    for path in files:
        suffix = path.suffix.lower()
        if suffix not in (".schdoc", ".pcbdoc"):
            print(f"skipping unsupported file: {path}", file=sys.stderr)
            continue
        _header, records = read_records(path)
        if suffix == ".schdoc":
            model = build_sch(records)
            sch_models[path.stem] = model
            text, out_name = render_sch(path, model, opts), path.stem + ".sch.txt"
        else:
            model = build_pcb(records)
            pcb_models[path.stem] = model
            text, out_name = render_pcb(path, model, opts), path.stem + ".pcb.txt"
        stems.setdefault(path.stem, path)
        written.append(_emit(text, path, out_name, opts, root))

    if opts.cross_check:
        for stem in sorted(set(sch_models) & set(pcb_models)):
            text = render_xcheck(stem, sch_models[stem], pcb_models[stem], opts)
            written.append(_emit(text, stems[stem], stem + ".xcheck.txt", opts, root))

    if not opts.stdout:
        for w in written:
            print(w)
    return 0


def _relpath(path: Path, root: Path) -> str:
    try:
        return path.resolve().relative_to(root).as_posix()
    except ValueError:
        return path.as_posix()


def _emit(text: str, source: Path, out_name: str, opts, root: Path) -> str:
    if opts.stdout:
        # Part numbers and tolerances carry non-ASCII; a Windows ANSI console
        # would otherwise raise UnicodeEncodeError on perfectly good output.
        try:
            sys.stdout.reconfigure(encoding="utf-8", errors="replace")
        except (AttributeError, ValueError):
            pass
        sys.stdout.write(text)
        return ""
    out_dir = Path(opts.out_dir) if opts.out_dir else source.parent
    out_dir.mkdir(parents=True, exist_ok=True)
    out_path = out_dir / out_name
    out_path.write_text(text, encoding="utf-8", newline="\n")
    return f"wrote {_relpath(out_path, root)}  ({len(text.splitlines())} lines)"


if __name__ == "__main__":
    sys.exit(main())
