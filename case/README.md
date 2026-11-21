# 3D Printable Case (ESP32-C3 Super Mini + 1.28" GC9A01)

A basic, support-free 2-piece enclosure with:
- Round front opening for the **1.28" 240×240 GC9A01 TFT**
- **2 internal screw bosses** for the screen PCB
- Internal retention cradle for the **ESP32-C3 Super Mini** (`22.5 × 18.0 mm`)
- **USB-C port cutout** on the bottom wall
- Snap-aligned **back cover** with 4 corner M2 screw holes

## Files

- `generate_case.py` — Parametric Python (`build123d`) script that exports `flight_clock_case.step` and `flight_clock_case.stl`.
- `flight_clock_case.scad` — Parametric OpenSCAD version with the exact same dimensions.

## Generating the `.step` File

Generic 1.28" GC9A01 modules vary slightly in PCB size and screw-hole spacing depending on the vendor. Before generating, measure your screen PCB with calipers and update the top parameters in `generate_case.py` (or `flight_clock_case.scad`):

- `SCREEN_PCB_W` / `SCREEN_PCB_H`
- `SCREEN_HOLE_SPACING` (center-to-center distance between the 2 screw holes)
- `SCREEN_HOLE_DIA` (`2.2` mm for M2, `2.7` mm for M2.5)
- `SCREEN_HOLE_AXIS_Y` (`True` if holes are top/bottom, `False` if left/right)

Then run:

```bash
pip install build123d
python3 case/generate_case.py
```

This creates `case/flight_clock_case.step` and `case/flight_clock_case.stl`.
