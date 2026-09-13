# TPS61021ADSGR — 3 A Synchronous Boost Converter (0.5 V Ultra-Low VIN)

PCB-design summary of TI datasheet **SLVSDM0 (June 2016)**, restricted to the
**TPS61021ADSGR** orderable part. Graphics, curves and block diagrams are in the
source PDF: [tps61021a.pdf](tps61021a.pdf).

## 1. Part Identification

| Item | Value |
|---|---|
| Orderable part number | TPS61021A**DSGR** |
| Package | WSON-8 (DSG), 2.00 × 2.00 mm, 0.5 mm pitch, 0.8 mm max height, exposed thermal pad |
| Status | Active / Production |
| Carrier | Large tape & reel, 3000 pcs |
| RoHS | Yes — Green (no Sb/Br) |
| Lead finish | NIPDAU (or NIPDAU \| NIPDAUAG) |
| MSL / peak reflow | **Level 2 — 260 °C, 1 year floor life** |
| Operating temp | −40 °C to 125 °C (TJ) |
| Top marking | `11G` |

Sibling part (not covered here): `TPS61021ADSGT` — same die and package, 250 pc small reel.

## 2. Pinout (Top View)

```
        +-----------+
 AGND 1 |           | 8 VIN
   FB 2 |     9     | 7 SW
 VOUT 3 |   PGND    | 6 SW
 VOUT 4 |  (th.pad) | 5 EN
        +-----------+
```

| Pin | Name | Type | Description |
|---|---|---|---|
| 1 | AGND | I | Signal ground of the IC |
| 2 | FB | I | Feedback for adjustable output voltage |
| 3, 4 | VOUT | PWR | Boost converter output |
| 5 | EN | I | Enable. High = enabled, low = shutdown (true input/output disconnect) |
| 6, 7 | SW | PWR | Switch node — drains of both internal power FETs |
| 8 | VIN | I | IC power supply input |
| 9 | PGND | PWR | Power ground / exposed thermal pad — **must be soldered to the board** |

Connect **both** VOUT pins and **both** SW pins with wide copper.

## 3. Absolute Maximum Ratings

| Parameter | Min | Max | Unit |
|---|---|---|---|
| EN, FB (DC) | −0.3 | 3.6 | V |
| VIN, SW, VOUT (DC) | −0.3 | 4.6 | V |
| VIN, SW, VOUT (10 % duty cycle) | −0.3 | 4.8 | V |
| Operating junction temperature TJ | −40 | 150 | °C |
| Storage temperature | −65 | 150 | °C |

ESD: HBM ±2000 V, CDM ±500 V.

**Design flag:** the EN absolute max is 3.6 V — lower than the VOUT max. Do not tie EN to a 4.0 V rail.

## 4. Recommended Operating Conditions

| Symbol | Parameter | Min | Nom | Max | Unit |
|---|---|---|---|---|---|
| VIN | Input voltage | 0.5 | | 4.4 | V |
| VOUT | Output voltage setting | 1.8 | | 4.0 | V |
| L | Effective inductance | 0.2 | 0.47 | 1.3 | µH |
| CIN | Effective input capacitance | 1.0 | 4.7 | | µF |
| COUT | Effective output cap, IOUT ≤ 0.3 A | 3.0 | 10 | 200 | µF |
| COUT | Effective output cap, IOUT > 0.3 A | 10 | 20 | 200 | µF |
| TJ | Junction temperature | −40 | | 125 | °C |

"Effective" = capacitance **after** DC-bias, aging and tolerance derating.

## 5. Thermal Information (DSG / WSON-8)

| Metric | Value | Unit |
|---|---|---|
| RθJA (junction-to-ambient) | 71.1 | °C/W |
| RθJC(top) | 95.2 | °C/W |
| RθJB | 41.6 | °C/W |
| ψJT | 3.1 | °C/W |
| ψJB | 42.0 | °C/W |
| RθJC(bot) | 13.0 | °C/W |

Max allowable dissipation: `PD(max) = (125 − TA) / RθJA`. Keep TJ ≤ 125 °C. Real RθJA
depends heavily on copper thickness, thermal-pad soldering and via count.

## 6. Key Electrical Characteristics

TJ = −40 °C to 125 °C, VIN = 2.4 V, VOUT = 3.3 V; typicals at 25 °C.

| Symbol | Parameter | Min | Typ | Max | Unit |
|---|---|---|---|---|---|
| VIN_UVLO | UVLO, VIN rising | | 0.8 | 0.9 | V |
| VIN_UVLO | UVLO, VIN falling | 0.28 | 0.4 | 0.5 | V |
| IQ | Quiescent into VIN (no switching) | | | 3.0 | µA |
| IQ | Quiescent into VOUT (no switching) | | 17 | 30 | µA |
| ISD | Shutdown current into VIN + SW | | 0.5 | 3.0 | µA |
| VREF | FB reference, PWM mode | 775 | **795** | 815 | mV |
| VREF | FB reference, PFM mode | | 801 | | mV |
| VOVP | Output OVP threshold (rising) | 4.15 | 4.35 | 4.60 | V |
| VOVP_HYS | OVP hysteresis | | 0.1 | | V |
| IFB_LKG | FB pin leakage | | | 20 | nA |
| ISW_LKG | SW leakage (disabled, VOUT = 4.0 V) | | | 3.0 | µA |
| IVOUT_LKG | VOUT leakage | | 1 | 2 | µA |
| RDS(on) | High-side PMOS @ VOUT = 3.3 V | | 51 | | mΩ |
| RDS(on) | Low-side NMOS @ VOUT = 3.3 V | | 58 | | mΩ |
| fSW | Switching frequency (PWM) | | 2.0 | | MHz |
| tOFF_min | Minimum off time | | 80 | 120 | ns |
| ILIM_SW | Valley current limit | 3.0 | 4.3 | | A |
| VEN_H | EN logic high, VIN > 1.2 V | | | 0.84 | V |
| VEN_H | EN logic high, VIN ≤ 1.2 V | | | 0.7 × VIN | V |
| VEN_L | EN logic low, VIN > 1.2 V | | | 0.36 | V |
| VEN_L | EN logic low, VIN ≤ 1.2 V | | | 0.3 × VIN | V |
| TSD | Thermal shutdown (TJ rising) | | 150 | | °C |
| TSD_HYS | Thermal shutdown hysteresis | | 20 | | °C |

## 7. Behaviour Relevant to the Schematic

- **Startup:** needs VIN ≥ 0.9 V (UVLO rising) to start. Once running and VOUT > 1.6 V it
  keeps operating down to VIN = 0.5 V.
- **Soft start:** internal; ~200 µs typical for a 44 µF output cap with no load. Load
  capability is limited until VOUT > 1.6 V.
- **Switching frequency:** quasi-constant 2 MHz for VIN > 1.5 V; ramps down toward 1 MHz
  as VIN falls 1.5 V → 1 V; fixed at ~1 MHz below 1 V.
- **PFM / power-save:** entered at light load (valley current floor ≈ 100 mA). Output sits
  about 0.8 % high (1.008 × VOUT_NOM) in PFM.
- **Pass-through:** if VIN rises above the set VOUT, at 101 % of target the device stops
  switching and holds the high-side PMOS on; VOUT then = VIN − I × (L_DCR + RDS(on)).
  It resumes switching below 98 % of target. **There is no buck-boost regulation** — set
  VOUT above the maximum expected VIN if regulation matters.
- **OVP:** stops switching above ~4.35 V, resumes 0.1 V below. If FB < 0.2 V while
  VOUT > 2.9 V, valley current is clamped to ~100 mA (guards a mis-populated divider).
- **Short-to-ground:** output current folds back below VOUT = 1.6 V, clamped to ~100 mA
  below 1 V. Recovers through soft start once the short is released.
- **Shutdown:** EN low fully disconnects input from output (no body-diode path). EN has no
  internal pull — drive it, or fit a pull-down/pull-up resistor.
- **Thermal shutdown:** off at TJ = 150 °C, back on at ~130 °C.

## 8. External Component Selection

### 8.1 Feedback divider

```
R1 = (VOUT / VREF − 1) × R2        VREF = 0.795 V (PWM)
```

- Keep **R2 < 400 kΩ** so divider current is ≥ 100× the FB leakage.
- Lower R2 → better noise immunity; higher R2 → lower IQ, better light-load efficiency.
- 3.3 V example: R1 = 316 kΩ, R2 = 100 kΩ.

### 8.2 Feedforward capacitor (C3, VOUT → FB) — needed in most applications

```
C3 = 1 / (2π × fFFZ × R1)
```

- COUT(eff) < 40 µF → set fFFZ = **50 kHz**
- COUT(eff) > 40 µF → set fFFZ = **5 kHz**
- 3.3 V example (R1 = 316 kΩ, 50 kHz): C3 = 10 pF.

### 8.3 Inductor

Usable range **0.33 µH to 1.0 µH** (the operating-conditions table allows 0.2–1.3 µH
effective); 0.47 µH is the reference-design value.

```
D        = 1 − (VIN × η) / VOUT              (η ≈ 0.90)
IL(DC)   = (VOUT × IOUT) / (VIN × η)
ΔIL(P-P) = (VIN × D) / (L × fSW)
IL(peak) = IL(DC) + ΔIL(P-P) / 2
IOUT(CL) = (1 − D) × (ILIM − ΔIL(P-P) / 2)
```

Compute the worst case at **minimum VIN, maximum VOUT, maximum IOUT**, using −30 %
inductance tolerance and pessimistic efficiency. Target ΔIL(P-P) < 40 % of IL(DC).
Saturation current must exceed IL(peak).

**Recommended inductors:**

| Part number | L (µH) | DCR max (mΩ) | Isat (A) | Size L×W×H (mm) | Vendor |
|---|---|---|---|---|---|
| XFL4015-471ME | 0.47 | 8.36 | 6.6 | 4.0 × 4.0 × 1.5 | Coilcraft |
| 744383360047 | 0.47 | 22 | 8.0 | 3.0 × 3.0 × 2.0 | Würth Elektronik |
| DFE252012P-R47M | 0.47 | 27 | 5.7 | 2.5 × 2.0 × 1.2 | Toko |
| XFL4020-102ME | 1.0 | 11.9 | 5.4 | 4.0 × 4.0 × 2.1 | Coilcraft |

### 8.4 Output capacitor

```
COUT ≥ (IOUT × DMAX) / (fSW × VRIPPLE)
VRIPPLE(ESR) = IL(peak) × RESR      (only if tantalum/electrolytic)
```

- Use **X5R or X7R ceramics, 10 µF to 200 µF effective** (≥ 3.0 µF effective is allowed
  for IOUT < 300 mA).
- Derate for DC bias — a ceramic can lose more than 50 % of its capacitance at its rated
  voltage. Leave voltage-rating margin.
- Going below the recommended range can make the loop unstable.

### 8.5 Input capacitor

- X5R/X7R ceramic; **10 µF is sufficient for most applications**, larger is fine.
- Place as close as possible to VIN **and** PGND.
- If the supply feeds through long wires, add bulk (tantalum/aluminium, e.g. 100 µF)
  between the ceramic and the source. A load step can otherwise ring the VIN pin — easily
  mistaken for loop instability, and it can damage the part.

### 8.6 Reference design (2-cell alkaline → 3.3 V)

VIN 1.8–3.2 V, VOUT 3.3 V, IOUT 1.5 A, ripple ±50 mV:
L1 = 0.47 µH, C1 = 10 µF, C2 = 2 × 22 µF, R1 = 316 kΩ, R2 = 100 kΩ, C3 = 10 pF.

## 9. Layout Guidelines

1. **Minimise SW node length and area** — fast edges make it the main EMI radiator.
2. The **critical high-di/dt loop** is low-side FET → high-side FET → output capacitor →
   back to the low-side FET ground. Keep it as short and tight as possible.
3. **COUT close to both VOUT pins and to the PGND pad** — not just VOUT. This limits SW
   and VOUT overshoot.
4. **CIN close to both the VIN pin and the PGND pad** to reduce input supply ripple.
5. Use a **ground plane under the regulator** to reduce interplane coupling.
6. Solder the **exposed thermal pad (pin 9) to a large ground pour**, with thick copper.
   Add multiple vias tying top and bottom ground planes around the IC, solder mask
   removed, to improve thermal performance. See TI SLUA271 for thermal-pad soldering.
7. Route FB as a short, quiet trace away from SW and the inductor; place the divider and
   the feedforward cap near the FB pin.

## 10. Package / Footprint Data (DSG0008A, WSON-8)

**Body:** 1.9–2.1 mm × 1.9–2.1 mm; height 0.7–0.8 mm; standoff 0.00–0.05 mm.
**Pitch:** 0.5 mm (6 × 0.5 mm between the terminals of the two rows).
**Terminals:** 8 × (0.18–0.32 mm wide) × (0.2–0.4 mm long).
**Exposed thermal pad (pin 9):** 0.9 ± 0.1 mm × 1.6 ± 0.1 mm.
**Pin 1 ID:** 45° × 0.25 mm chamfer.

**Land pattern example:**
- 8 pads of 0.25 mm × 0.55 mm, 0.5 mm pitch, rows 1.9 mm apart
- Thermal land 0.9 mm × 1.6 mm
- Corner radius R0.05 mm typ
- Ø0.2 mm vias in the thermal land are optional; if used, fill, plug or tent them
- **Non-solder-mask-defined pads preferred**, 0.07 mm mask clearance all around

**Stencil example (0.125 mm thick):**
- Signal apertures 0.25 mm × 0.5 mm
- Thermal-pad aperture 0.45 mm × 0.7 mm → **87 % printed solder coverage by area** under
  the package. Do not print 100 % on the thermal pad or the part will float.
- Laser-cut apertures with trapezoidal walls and rounded corners release paste better
  (IPC-7525 may give alternative recommendations).

## 11. Tape & Reel (TPS61021ADSGR)

| Parameter | Value |
|---|---|
| SPQ | 3000 |
| Reel diameter | 178.0 mm (the 2016 addendum lists 180.0 mm) |
| Reel width W1 | 8.4 mm |
| A0 × B0 × K0 | 2.3 × 2.3 × 1.15 mm |
| Pitch P1 | 4.0 mm |
| Tape width W | 8.0 mm |
| Pin 1 quadrant | Q2 |
| Box L × W × H | 208 × 191 × 35 mm |

## 12. Power Supply Notes

The input supply must be well regulated over 0.5–4.4 V. If it sits more than a few inches
from the converter, add bulk capacitance (typically 100 µF tantalum/aluminium) on top of
the ceramic bypass. Rate the source current for the VIN/VOUT/IOUT combination — input
current is roughly `IOUT × VOUT / (VIN × η)`.
