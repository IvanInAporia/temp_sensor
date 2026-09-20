# Review notes — settled items

Things that have already been checked and found **correct, intentional, or harmless**.
Recorded here so they don't get re-raised on every review pass.

Open defects are deliberately **not** listed here — this file is only the "stop looking at
this" list.

| | |
|---|---|
| Design | temp_sensor v2 (EasyEDA Pro) |
| Reviewed against | `P1.schdoc` sha256[:12] `fa324c6c4f47`, `PCB1.pcbdoc` sha256[:12] `8a2e19924876` |
| Last updated | 2026-09-20 (pre-production pass, 10-off PCBA) |

Datasheet summaries referenced below live in [`../docs/`](../docs/).

---

## 1. Tooling caveats

### 1.1 `eda_review_export.py` reports false airwires — the board IS fully routed

**The script cannot see copper fill regions.** Its own record table says so:

> `Region      filled polygon; almost always footprint art -> counted only`

It builds copper islands from **tracks, vias and pads only**. Any net routed with a fill
region is reported as unrouted. On this board that produces four bogus flags:

```
- net $1N30: pads sit on 3 separate copper islands -- 2 connection(s) still unrouted
- net $1N61: 3 pads but no copper -- unrouted
- net +3V3:  pads sit on 10 separate copper islands -- 9 connection(s) still unrouted
- net adc_batt: pads sit on 5 separate copper islands -- 4 connection(s) still unrouted
```

All four are routed with net-assigned `Region` fills:

| Layer | Net | Pads covered |
|---|---|---|
| TOP | `$1N61` (SW node) | L1.2, U1.6, U1.7 |
| TOP | `ADC_BATT` | L1.1, C1.2, C11.2, C12.2 |
| TOP | `+3V3` | U1.3, U1.4, C3.2, C13.2, C14.2, C10.2, R2.2, U5.1 |
| TOP | `$1N30` | U2.8, C6.2, C7.2 |
| TOP | `$1N30` | U5.6 |

Re-running connectivity with fills folded into the island graph (tracks + vias + pads +
fills, width-aware contact) gives **one island per net, 0 airwires**. There are no copper
arcs, so nothing else is being missed. Re-verified on revision `8a2e19924876`: all 18 nets,
0 airwires, and the same four nets still produce the same four bogus flags.

Fold the two solid **pours** in as copper as well, not just the `Region` fills. Pours are
likewise exported as outlines only, so without them `power_GNDREF` fragments into 23
islands — all false. In particular `C9.1` reaches GND through a top stub to a via at
(37.790, 33.048) and then the bottom layer; it is *not* pour-fed, because the sensor tab is
outside both pour outlines (§3.3).

The failure mode is asymmetric: the script exempts pour-carrying nets from the island
check, so it can only ever produce *false airwires*, never a false "routed". Treat its
`## FLAGS` airwire lines as advisory and confirm against EasyEDA's own DRC.

### 1.2 The cross-check report needs matching file stems

`P1.schdoc` / `PCB1.pcbdoc` don't share a stem, so the script skips
`<stem>.xcheck.txt`. Copy both to a common stem in a scratch directory to get it:

```sh
cp P1.schdoc /tmp/x/temp_sensor.schdoc && cp PCB1.pcbdoc /tmp/x/temp_sensor.pcbdoc
python eda_review_export.py /tmp/x
```

Result for this revision: **18 of 18 schematic nets have an identical pad set on the PCB.**
The only component delta is `U3: no pad for pin 9`, which is intentional — see §3.2.

### 1.3 The `## DESIGN RULES` section is NOT EasyEDA's live rule set — don't check against it

The `.pcbdoc` is an **Altium-dialect export**, and its `DXPRule` records do not carry the
rules EasyEDA actually enforces. The export writes:

```
RULEKIND=Clearance  NAME=copperThickness1oz  GAP=7mil  GENERICCLEARANCE=7mil
```

identically on all three rule packs (1 oz, 2 oz, other) — which looks like a hardcoded
default in EasyEDA's Altium exporter, not anything derived from the design. **The real
clearance is 6 mil.** Checking copper against the exported 7 mil manufactures false
violations; this has already happened once.

The authoritative source is the `.epro2` backup. It is a zip; `temp_sensor.epru` inside it
is plain JSON records separated by `||`:

```json
{"type":"RULE","id":"[\"RULE\",\"SAFE\",\"safeClearance\"]"}
{"ruleState":"DEFAULT","ruleContext":{"safeSpacing":[{"content":[
  [6],[6,6],[6,6,6],[6,6,6,6],[6,6,6,6,6],[6,6,6,6,6,6],[6,6,6,6,6,6,6],
  [10,10,10,10,10,10,10,10],            <- board-outline row
  [6,6,6,6,6,6,6,10,6], ...
  [11.8,...,10,11.8,...],               <- hole row
]}]}}
```

Every copper-object pair — track-to-track, track-to-via, pad-to-anything — is **6 mil**.

**The values are mils even though the record says `"unit":"mm"`** (that field is the display
preference). Two independent confirmations:

- `viaSize`: `defRadius 12.0079`, `defInner 6.0039` — the board's vias are exactly
  `DIAMETER=24mil` (r=12) and `HOLESIZE=12mil` (r=6).
- `trackWidth`: `stroDef 10` → 0.254 mm, the dominant track width on this board.

The matrix also cross-validates against the export's *other* rules, which did survive
translation: the 10 mil row matches `BoardOutlineClearance GENERICCLEARANCE=10mil`, and the
11.8 mil row matches `otherClearance holeClearance:11.8`. Only the generic clearance is
wrong.

`ruleState: "DEFAULT"` means clearance was never customised — it is EasyEDA's stock 6 mil.

**Settled consequence:** a full pairwise clearance sweep of revision `8a2e19924876`
(tracks, vias, pads and net-assigned fills; pours excluded because EasyEDA re-pours them with
clearance) finds **0 violations of the 6 mil rule**. The three tightest pairs are:

| Gap | Pair | Layer |
|---|---|---|
| **6.21 mil** | `U4.11` pad (`$1N80`, PA5) ↔ `adc_batt` track | TOP |
| 7.50 mil | `U1.2` pad (`$1N51`, FB) ↔ `+3V3` fill | TOP |
| 7.88 mil | `U1.5` pad (`adc_batt`, EN) ↔ `$1N61` fill | TOP |

The 6.25 mil `+3V3`/via spot named in earlier revisions of this note is no longer the
minimum — R3/R5 moved and the spacing opened up. 6.21 mil **passes** and is comfortable for
any fab (JLCPCB standard is 5 mil at 1 oz). Not a defect — just the spot with the least
margin.

---

## 2. Cosmetic artifacts that are not defects

### 2.1 Duplicate case-variant net objects in the PCB — not an issue, DRC is clean

The PCB net table holds 24 nets where the schematic has 18. Six exist twice, differing
only in case:

`+3v3`/`+3V3` · `adc_batt`/`ADC_BATT` · `power_GNDREF`/`POWER_GNDREF` ·
`uC_reset`/`UC_RESET` · `Wifi_Tx`/`WIFI_TX` · `Wifi_Rx`/`WIFI_RX`

**Every one of the 123 pads sits on one net object of the pair**; the other carries routing
primitives only. Counts for revision `1b172970e7c4` (**bold** = the pad-owning object):

| Net pair | pads | tracks | vias | fills / pours |
|---|---|---|---|---|
| **`adc_batt`** / `ADC_BATT` | **10** / 0 | 8 / 11 | 1 / 0 | – / 1 fill |
| `+3v3` / **`+3V3`** | 0 / **20** | 15 / 28 | – | – / 1 fill |
| `power_GNDREF` / **`POWER_GNDREF`** | 0 / **30** | 21 / 11 | 12 / 79 | – / 2 pours |
| **`uC_reset`** / `UC_RESET` | **4** / 0 | 7 / 3 | – | – |
| **`Wifi_Tx`** / `WIFI_TX` | **3** / 0 | 2 / 8 | – | – |
| **`Wifi_Rx`** / `WIFI_RX` | **3** / 0 | 3 / 15 | – | – |

**Which object owns the pads is not stable across revisions.** For `+3v3` and
`power_GNDREF` it flipped to the uppercase object in this revision (it was lowercase
before); the other four pairs are unchanged. This is cosmetic churn, not a regression — the
cross-check compares nets by their *pad set*, not by name, so it still reports 18/18 and
merely notes `same pads, different name`. Don't chase it.

The copper is galvanically one island per net, DRC passes, and the board is correct.

**Re-importing the schematic will not clear this, and that is expected.** "Update PCB from
schematic" only re-nets *pads*, because EasyEDA owns those through the footprints. It never
touches user-drawn tracks, vias, fills or pours, and it won't drop a net object that still
has copper attached — so the duplicates are self-sustaining. Don't keep re-importing to try
to fix it.

If it ever needs cleaning up (it doesn't for function): select all copper on the wrong-cased
net, reassign the net **from the dropdown** in Properties, then re-run the update so the now-
empty net object is dropped. Typing a name that differs only in case is what creates a second
net object in the first place.

### 2.2 `N copper primitives had no net in the file` (52 in `8a2e19924876`, was 36, was 40)

Exporter note, not a design problem — it recovers the net from what the copper physically
touches. Related to the fill-region handling in §1.1.

---

## 3. Sensor — HDC3020 (U3)

### 3.1 Pin configuration matches the datasheet

| Pin | Wiring | Why it's right |
|---|---|---|
| ADDR (2), ADDR1 (7) | both → GND | 7-bit address **0x44**. Datasheet: must be tied directly to GND or VDD, never floating. |
| RESET (6) | → VDD | Datasheet §7.1: tying nRESET to VDD when unused is preferred, avoids EMI-induced glitches. |
| ALERT (3) | **floating** | Datasheet §7.1 explicitly requires ALERT to be left floating when unused. This is correct, not a missed connection. |
| VDD (5) | +3v3, C9 100 nF adjacent | 1.62–5.5 V range; C9 is the recommended local X7R bypass. |

### 3.2 Thermal pad deliberately absent from the footprint

`DFN-8_L2.5-W2.5-P0.50-BL-EP` has 8 pads, no EP land — hence the `U3: no pad for pin 9`
cross-check delta. Datasheet §7.2 note 5 and §2: the thermal pad *may* be left unsoldered,
and doing so is preferred "to minimize thermal mass ... or to measure ambient temperature",
which is this design's job. Intentional.

### 3.3 Thermal isolation is correctly implemented

Verified geometrically against the pour outlines:

- Board outline has a routing notch at x 29.0–34.0, y **30.196**–40.0, putting U3 on a
  peninsula fed by a ~6 mm neck. **The notch was deepened from y 30.858 to y 30.196 in
  revision `8a2e19924876`; both pour outlines were moved with it**, so the relationship below
  still holds — but any note quoting 30.858 is stale.
- **Both the top and bottom GND pours are excluded from the sensor tab.** This one *is*
  provable from the file: both pour outlines step in to `y ≤ 30.196` for `x > 28.999`, so
  the whole tab is outside them. Re-verified point-in-polygon on `8a2e19924876`: U3.1–U3.8
  and C9.1/C9.2 are all outside both pour outlines. Confirmed again by the fact that U3.2/7/8 and C9.1 are the
  only GND pads not enclosed by a pour outline (§1.1). Matches datasheet §7.2 rule 2
  ("eliminate copper layers below the device") and rule 3 (slots/cutout).
- The tab is fed by discrete traces, not by plane copper.
- C9 is the only component adjacent to U3, per §7.2 rule 1 ("ideally the only onboard
  component close to the device is the supply bypass capacitor").

**Correction (2026-09-05):** an earlier revision of this note claimed the tab is fed by
"four discrete traces only (SDA, SCL, +3V3, GND)". That undercounts.

**Re-measured on `8a2e19924876` (2026-09-20).** Copper crossing y = 30.196 mm, x 34–40:

| Layer | Net | Width | Crosses at x |
|---|---|---|---|
| TOP | SDA | 0.201 mm | 34.783 |
| TOP | SCL | 0.201 mm | 35.750 |
| TOP | +3V3 | 0.399 mm | 38.504 |
| BOTTOM | GND | 0.320 mm | 34.783 |
| BOTTOM | GND | 0.450 mm | 37.790 |
| BOTTOM | GND | 0.500 mm | 38.910 |

Total **2.070 mm** of copper through the neck (TOP 0.800 + BOTTOM 1.270), down from ~2.3 mm.
Copper still runs under the package body: **0.944 mm on BOTTOM** (two GND segments, 0.320 mm
wide, down from a single 0.45 mm run of ~1.3 mm) plus 2.500 mm on TOP, which is unavoidable
since the pads are there.

Only the pour exclusion is settled. The neck copper inventory is **not** settled and is
tracked as an open nit, not here — but it moved in the right direction.

### 3.4 I2C pull-ups R3 / R5 = 10 kΩ — settled, do not change again

Changed from 1 kΩ to **10 kΩ** in revision `c60022f58cd2`. Both bounds are satisfied with
margin, so this is closed:

- **Lower bound:** R_min = (3.3 − 0.4) / 3 mA = **967 Ω**, set by the HDC3020's V_OL spec
  (0.4 V at I_OL = 3 mA). The old 1 kΩ met this with essentially no margin; 10 kΩ draws only
  0.29 mA, a 10× margin on the sink current.
- **Rise time:** bus capacitance is ~15–20 pF (HDC3020 4.5 pF max + STM32 ~5 pF + ~1.5 pF
  of trace — 36 mm of 0.2 mm line over a 1.5 mm dielectric is only ~0.4 pF/cm + pads).
  t_r = 0.847·R·C ≈ **169 ns** at 10 kΩ / 20 pF — inside Fast mode's 300 ns and far inside
  Standard mode's 1000 ns. 10 kΩ is the practical ceiling here; don't go higher.
- **Power is a non-argument.** At one sample every few seconds the pull-up energy is ~165 nA
  average at 1 kΩ vs ~17 nA at 10 kΩ — both invisible next to the TPS61021A's 17 µA
  quiescent draw on VOUT.
- **Leakage offset:** (0.5 µA + 0.05 µA) × 10 kΩ = 5.5 mV against a 2.31 V V_IH. Irrelevant.

Anything from ~1 kΩ to 10 kΩ works electrically; 10 kΩ was chosen partly to consolidate the
BOM (§6.4). No reason to revisit.

---

## 4. Boost converter — TPS61021A (U1)

### 4.1 Feedback network is deliberate, don't "correct" it

- R2 = 634 kΩ (top) / R1 = 200 kΩ (bottom) → V_OUT = 0.795 × (1 + 634/200) = **3.31 V**.
- R_bottom 200 kΩ is under the datasheet's 400 kΩ ceiling (§8.1).
- **C10 = 50 pF is not an arbitrary value.** C = 1/(2π·f·R_top) = 1/(2π·5000·634k) =
  **50.2 pF**, i.e. the 5 kHz feedforward zero the datasheet specifies for
  C_OUT(eff) > 40 µF (§8.2). C_OUT here is 114 µF nominal, so the 5 kHz case is the right
  one — not the 50 kHz / 10 pF case from the 3.3 V reference design.

### 4.2 Capacitor selection is in range

- **C_OUT:** C13 + C14 (47 µF) + C3 + C5 (10 µF) = 114 µF nominal. Recommended window is
  10–200 µF effective for I_OUT > 0.3 A; comfortably inside after DC-bias derating.
- **C_IN:** C11 + C12 (47 µF) + C1 (10 µF) + C15 (100 nF) on `adc_batt`. Recommendation is
  10 µF minimum.
- **L1** 470 nH is the datasheet reference value (0.2–1.3 µH allowed); 12 A Isat / 9 mΩ is
  heavily over-specified for this load, which is fine.

### 4.3 Power-path copper is adequate — the thin traces are not the current path

- SC1.1 → L1.1 is **0.899 mm** (~2.3 A at 1 oz, 10 °C rise) feeding the `ADC_BATT` fill.
  Worst-case inductor current is ~1.2 A DC at V_IN 1.2 V with a Wi-Fi TX burst.
- SW and VOUT are **fill regions**, not traces.
- Wi-Fi rail (`$1N30`) is 0.6–0.8 mm.
- The 0.254 mm tracks on `adc_batt` are the PA6 battery-sense path and the feed to **U1.8**.
  **VIN (pin 8) is the IC supply pin, not the power path** — the inductor carries the load
  current. 0.254 mm there is fine.
- Output loop: C3 (10 µF 0402) sits **1.27 mm** from U1's VOUT pads, so the high-di/dt loop
  is short. The 1206 bulk caps sitting further out is not a problem.

Only open nit: U1.8 reaches the bulk caps through ~5 mm of 0.254 mm trace with no local
ceramic. A 0.1 µF at VIN/PGND would match datasheet §9.4, but nothing depends on it.

---

## 5. Load switch — TPS22919 (U5)

### 5.1 The ON pull-down (R6) is optional — it is now fitted, and that is fine either way

**Superseded 2026-09-20.** Earlier revisions of this note said "do not add a pull-down
resistor". **R6 (10 kΩ, 0603, C98220) was added on `Wifi_On` in `fa324c6c4f47`** and is
present on the board at (9.180, 20.094). Leave it.

The original reasoning still stands — the TPS22919 has an internal **smart pull-down** on the
ON pin that holds it low while the driver is high-impedance, and STM32 I/Os are analog inputs
during and after reset, so the internal pull-down already covers the float-through-reset case.
R6 is therefore redundant, not wrong.

It is also free in the only place that matters. R6 draws **0 A while the Wi-Fi is off**
(PA4 low) and 3.3 V / 10 kΩ = **330 µA only while PA4 is high** — i.e. only while the T3-3S
is already drawing 50–350 mA. Against the boost's 17 µA quiescent draw and the 3.97 µA burnt
continuously by the R1/R2 feedback divider, it changes nothing in the sleep budget.

So: **don't add a second one, and don't remove this one.** Both states are defensible; the
board is built with it.

### 5.2 QOD (5) and NC (4) unconnected

Both intentional. QOD is optional output discharge.

---

## 6. MCU — STM32L010F4P6 (U4)

### 6.1 Every alternate function is legal for TSSOP20

| Signal | Pins | AF |
|---|---|---|
| I2C1 SCL / SDA | PA9 / PA10 | AF1 / AF1 |
| USART2 TX / RX | PA2 / PA3 | AF4 / AF4 |
| SWDIO / SWCLK | PA13 / PA14 | AF0 |

Checked against the TSSOP20 signal-availability table. PA8 is not bonded on this package,
so PA9 is the only MCO pin — not used here.

### 6.2 Reset and boot circuitry is per datasheet

- **NRST:** C8 100 nF to ground plus the Reset button. Datasheet §13.3 asks for exactly the
  100 nF cap and nothing else. **No external pull-up or reset IC is needed** — the pin has a
  permanent internal pull-up and the device has POR/PDR/BOR.
- **BOOT0 (PB9, pin 1):** R4 10 kΩ pull-down for normal Flash boot. Input-only pin, correct.
  R4 moved from 0402 to **0603** in revision `c60022f58cd2` (see §6.4); clearances re-checked
  — 0.215 mm to C8, 0.503 mm to the U4 pin-1 pad, both well over the 6 mil rule.
- **Wifi button on PA7:** switch to GND relying on the internal pull-up (25–65 kΩ). Adequate;
  debounce is a firmware concern.

### 6.3 VDDA and VDD share the +3v3 rail

Required — the datasheet mandates they come from the same source within 300 mV, and there is
no separate VSSA pin on TSSOP20 (VSSA is internally bonded to VSS, pin 15).

### 6.4 R3 / R4 / R5 / R6 are deliberately the same part

All **four** are now **YAGEO RC0603FR-0710KL, 10 kΩ ±1% 0603, LCSC C98220**. R3/R5 are the
I2C pull-ups (§3.4), R4 is the BOOT0 pull-down (§6.2) and **R6 is the `Wifi_On` pull-down
(§5.1, added 2026-09-20)**; they share a value and a footprint purely to cut a line from the
BOM and a reel from assembly. The 0402→0603 change on R4 and the 1 kΩ→10 kΩ change on R3/R5
are the same decision, not two unrelated edits. Don't "optimise" one of them back to a
different part — one reel now covers four placements.

---

## 7. Wi-Fi module — T3-3S (U2)

### 7.1 Antenna keep-out — no tracks, vias or pads; the pour must be confirmed in EasyEDA

The keep-out `Region` is declared at x 10.351–18.351, y 28.397–40.396 (8.00 × 12.00 mm) and
belongs to the U2 footprint, so it moves with the module. Re-verified on `8a2e19924876`:
**no track, via, pad or net-assigned fill intersects it — 0 hits.** That much is settled.

**Correction (2026-09-05):** an earlier revision of this note claimed the tests also showed
"no copper on either the top or bottom pour". That claim was unsound — both pour *outlines*
span the keep-out (the keep-out centre is inside both), and pours are exported as outlines
only, so the file records nothing about whether the pour was actually subtracted there.
EasyEDA does honour a keep-out `Region` when it pours, but **that can only be confirmed in
EasyEDA's own DRC / plot preview**, never from the `.pcbdoc`. Same blind spot as §1.1.

### 7.2 CEN is header-driven, not MCU-driven

`J4.3 → U2.3 (CEN)` with no MCU connection. The module has an internal pull-up on CEN, so it
runs by default; the module is power-gated by U5 instead of being held in reset. Intentional
topology.

### 7.3 Unused module pins

NC (1), ADC (2), P48 (4), P24 (5), P32 (6), P34 (7), P19 (10), TX1 (11), RX1 (12), P36 (13),
P18 (14) are all left open by design.

### 7.4 UART crossover is correct as of `c60022f58cd2` — do not "fix" it back

The MCU↔module UART was **wired straight through (TX→TX, RX→RX) in the previous revision**
and could never have linked. It was corrected in `c60022f58cd2`. Current, correct wiring:

| Net | Members | Direction |
|---|---|---|
| `Wifi_Rx` | J5.1, **U2.16 = TX0**, U4.9 = PA3 (USART2_RX) | module → MCU |
| `Wifi_Tx` | J5.2, **U2.15 = RX0**, U4.8 = PA2 (USART2_TX) | MCU → module |

Net names are **MCU-centric**: `Wifi_Rx` is the MCU's receive line and therefore lands on the
module's *transmit* pin. That looks like a crossed net name at a glance and has already been
"corrected" once in the wrong direction. The rule to check against is the pin function, not
the net name — `U4.9`/`PA3` must sit with `U2.16`/`TX0`.

J5 pins 1 and 2 swapped with this change, so **any bench notes or jumper harness made before
`c60022f58cd2` are now wrong.** J5 carries no pin labels on silkscreen (only the designator);
pin 1 is identifiable solely by its square pad — confirmed in copper on `8a2e19924876`
(J5.1 is `RECTANGLE`, J5.2/J5.3 are `ROUND`).

**Re-confirmed correct on `fa324c6c4f47` (2026-09-20)** against the TSSOP20 pinout in
[`../docs/microcontroller/stm32l010f4-tssop20.md`](../docs/microcontroller/stm32l010f4-tssop20.md):
pin 8 = PA2 = USART2_TX (AF4) sits on `Wifi_Tx` with U2.15/RX0, and pin 9 = PA3 = USART2_RX
(AF4) sits on `Wifi_Rx` with U2.16/TX0. Note that the *exported* netlist snapshot in
`P1.sch.txt` dated 2026-09-05 (sha `81829856c3c5`) still shows the **old, broken** straight-
through wiring; that file is a stale artifact, not the design. Regenerate it before reading it.

---

## 8. Fabrication / DFM — swept clean on `8a2e19924876` (2026-09-20)

A full geometric sweep was run for the 10-off PCBA order. All of this **passed** and does not
need re-running unless the board changes:

| Check | Rule | Result |
|---|---|---|
| Copper-to-copper clearance | 6 mil (§1.3) | **0 violations**; tightest 6.21 mil |
| Copper-to-board-edge | 10 mil | **0 violations** |
| Hole-to-copper (other nets) | 11.8 mil | **0 violations** |
| Connectivity, fills+pours folded in | — | **0 airwires**, 18/18 nets one island |
| Schematic ↔ PCB pad sets | — | **18/18 identical**; only delta `U3` pin 9 (§3.2) |
| Silkscreen on solderable copper | — | **none** (791 silk segments vs 130 pads) |
| Designators on silkscreen | — | **all 42 present** |
| Pin-1 marker on THT connectors | — | present in copper (square pad) on J4, J5, SC1, Reset, Wifi, program |
| Via annular ring | 0.13 mm | 97 vias, 0.305 mm drill / 0.610 mm pad → **0.152 mm** |
| THT annular ring | 0.13 mm | **0.300–0.450 mm** |
| Narrowest track | fab min | **0.201 mm** (SDA/SCL) |
| Via tenting | — | all 97 tented (mask expansion −1000 mil) |
| Stackup | — | 1 oz outer, 1.51 mm core → 1.6 mm finished |

### 8.1 Footprints were checked against the datasheets — they are right

| Part | Board land | Datasheet | Verdict |
|---|---|---|---|
| U1 TPS61021A DSG0008A | 0.249 × 0.521 mm, 0.5 pitch, rows 1.9 mm; EP 1.600 × 0.899 | 0.25 × 0.55, 0.5 pitch, rows 1.9; thermal land 0.9 × 1.6 | **exact match**; EP tied to GND as the datasheet requires |
| U3 HDC3020 DEF0008A | 0.701 × 0.249 mm, 0.5 pitch, rows 2.4 mm, no EP land | 0.6 × 0.25, span 2.3 mm | IPC-style land with outward fillets rather than TI's minimal span — **fine**, and the missing EP land is deliberate (§3.2) |
| U4 STM32L010F4P6 TSSOP-20 | 0.363 × 1.742 mm, 0.65 pitch, rows 5.74 mm | body 6.5 × 4.4, lead span 6.4 | standard IPC land, **fine** |
| U5 TPS22919 SC-70-6 | 0.599 × 0.419 mm, 0.65 pitch, rows 1.9 mm | DCK | **fine** |
| U2 T3-3S | 2.2 × 1.1 mm pads, 2.0 mm pitch, rows 15.002 mm | body 16 ± 0.35 × 24 ± 0.35 mm | castellated land, 0.6 mm of pad outside the module edge for the fillet — **fine** (but see §8.2) |
| L1 MWSA0518S | 2.499 × 1.900 mm, 4.1 mm apart | 5.2 × 5.4 body | **fine** |

Component body outlines live on **MECHANICAL7** and are trustworthy — U4 measures exactly
6.50 × 4.40 mm, U1 2.00 × 2.00, U3 2.50 × 2.50, C11 1.60 × 3.20. Use that layer, not the
silkscreen, when checking mechanical fit.

### 8.2 U2 overhangs the board edge by 7.00 mm — verified, flagged, not settled

The T3-3S body (MECHANICAL7: x 6.350–22.352, y **22.997–46.995**) runs past the board's top
edge at y = 40.000 by **exactly 7.00 mm**. All 16 pads are on the board (y 24.496–38.496), so
this is electrically fine and looks deliberate — hanging the antenna end off the carrier board
is the standard way to keep it away from ground copper, and Tuya asks for 15 mm of clearance
to metal.

**This is recorded as verified-and-intentional, not as a defect** — but it is *not* a
"stop looking" item, because it constrains every board house:

- the panel needs ≥ 7 mm of clearance or a rail cutout on that edge;
- the board cannot sit flat on a conveyor during assembly;
- the module is cantilevered on its solder joints — handle the assembled boards by the PCB.

Tell the assembler about it explicitly when placing an order.

### 8.3 Decoupling placement is good — with one known exception

Distance from each supply pin to its nearest bypass cap on the same net:

| Pin | Nearest cap | Distance |
|---|---|---|
| U4.16 VDD | C2 100 nF | 1.83 mm |
| U4.5 VDDA | C4 100 nF | 1.70 mm |
| U1.3/U1.4 VOUT | C3 10 µF | 1.27 mm |
| U3.5 VDD | C9 100 nF | 1.33 mm |
| U2.8 VBAT | C7 100 nF | 2.24 mm |
| **U1.8 VIN** | **C1 10 µF** | **5.39 mm** |

The last row is the open nit already named in §4.3. C15 (100 nF, `adc_batt`) is **13.8 mm**
away, sitting by the battery connector instead of at the boost. TPS61021A §9.4 wants a 100 nF
at VIN/PGND. Nothing depends on it at this load, but moving C15 next to U1.8 is the single
cheapest layout improvement left on this board.

---

## 9. Summary of intentionally unconnected pins

Do not flag these as missed connections:

| Part | Pins | Reason |
|---|---|---|
| U3 HDC3020 | ALERT (3) | Datasheet **requires** floating when unused |
| U3 HDC3020 | EP (9) | No land in footprint — minimises thermal mass, §3.2 |
| U5 TPS22919 | NC (4), QOD (5) | Optional |
| U2 T3-3S | 1, 2, 4, 5, 6, 7, 10, 11, 12, 13, 14 | Unused peripherals |
| U4 STM32 | PC14, PC15, PA0, PA1, PB1 | No LSE crystal, no external HSE clock, spare I/O |
