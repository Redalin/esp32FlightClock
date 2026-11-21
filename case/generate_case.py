"""
Parametric 3D-printable case generator for ESP32-C3 Super Mini + 1.28" GC9A01 Round TFT.
Exports both STEP (for CAD editing) and STL (for 3D printing).

Requires `build123d`:
    pip install build123d
Run:
    python3 generate_case.py
"""

from pathlib import Path
from build123d import (
    Align,
    Box,
    Cylinder,
    Part,
    Pos,
    RectangleRounded,
    Rot,
    export_step,
    export_stl,
    extrude,
)

# ==============================================================================
# USER-EDITABLE PARAMETERS (all dimensions in mm)
# ==============================================================================

# --- 1.28" GC9A01 Round TFT Screen ---
SCREEN_VIEW_DIA = 33.5           # Visible round opening diameter (active area ~32.4mm)
SCREEN_GLASS_DIA = 38.0          # Front glass/bezel diameter (for internal recess)
SCREEN_PCB_W = 38.0              # Screen PCB width
SCREEN_PCB_H = 45.0              # Screen PCB height (including bottom mounting holes)
SCREEN_GLASS_THICK = 2.2         # Glass/bezel thickness in front of PCB
SCREEN_Y_OFFSET = 3.5            # Shift screen opening upward (+Y) to leave room at bottom
SCREEN_HOLE_SPACING = 19.0       # Horizontal center-to-center distance between the 2 bottom holes
SCREEN_HOLE_Y_BELOW_CENTER = 22.0  # Vertical distance from screen center down to the 2 holes
SCREEN_HOLE_DIA = 2.2            # 2.2mm for M2 screws, 2.7mm for M2.5 screws

# --- ESP32-C3 Super Mini ---
ESP_W = 18.4                 # 18.0mm PCB + 0.4mm print clearance
ESP_L = 23.0                 # 22.5mm PCB + 0.5mm print clearance
USB_CUTOUT_W = 11.5          # USB-C port opening width (includes plug clearance)
USB_CUTOUT_H = 6.0           # USB-C port opening height
USB_Z_OFFSET = 1.5           # Height of USB-C cutout from back edge

# --- Case Shell & Lid ---
WALL_THICK = 2.4             # Side wall thickness
FRONT_THICK = 2.0            # Front bezel thickness
CASE_DEPTH = 18.0            # Total outer depth of main body
CORNER_RADIUS = 4.0          # Outer corner radius
CLEARANCE = 0.4              # General fit clearance
LID_THICK = 2.0              # Back cover thickness
LID_SCREW_DIA = 2.2          # Back cover M2 screw holes

# ==============================================================================
# DERIVED DIMENSIONS
# ==============================================================================
INNER_W = max(SCREEN_PCB_W, ESP_W) + 2 * CLEARANCE + 4.0
INNER_H = max(SCREEN_PCB_H, ESP_L) + 2 * CLEARANCE + 4.0
OUTER_W = INNER_W + 2 * WALL_THICK
OUTER_H = INNER_H + 2 * WALL_THICK


def build_main_body() -> Part:
    """Build the main enclosure body (print face-down, z=0 is front face)."""
    inner_r = max(1.0, CORNER_RADIUS - WALL_THICK)

    # Outer shell
    body = extrude(RectangleRounded(OUTER_W, OUTER_H, CORNER_RADIUS), CASE_DEPTH)

    # Main internal cavity
    cavity = Pos(0, 0, FRONT_THICK) * extrude(
        RectangleRounded(INNER_W, INNER_H, inner_r), CASE_DEPTH - FRONT_THICK + 1.0
    )
    body -= cavity

    # Round screen viewing hole (shifted upward by SCREEN_Y_OFFSET)
    body -= Pos(0, SCREEN_Y_OFFSET, -0.5) * Cylinder(
        radius=SCREEN_VIEW_DIA / 2,
        height=FRONT_THICK + 1.0,
        align=(Align.CENTER, Align.CENTER, Align.MIN),
    )

    # Shallow recess for screen glass/bezel
    body -= Pos(0, SCREEN_Y_OFFSET, FRONT_THICK - 0.8) * Cylinder(
        radius=(SCREEN_GLASS_DIA + CLEARANCE) / 2,
        height=0.9,
        align=(Align.CENTER, Align.CENTER, Align.MIN),
    )

    # USB-C port cutout on -Y wall
    usb_center_z = CASE_DEPTH - USB_Z_OFFSET - USB_CUTOUT_H / 2
    usb_slot = (
        Pos(0, -OUTER_H / 2 + WALL_THICK / 2, usb_center_z)
        * Rot(90, 0, 0)
        * extrude(
            RectangleRounded(USB_CUTOUT_W, USB_CUTOUT_H, 1.5),
            WALL_THICK + 2.0,
            both=True,
        )
    )
    body -= usb_slot

    # 2 Screen mounting bosses at the bottom of the screen
    boss_dia = SCREEN_HOLE_DIA + 3.6
    hole_x = SCREEN_HOLE_SPACING / 2
    hole_y = SCREEN_Y_OFFSET - SCREEN_HOLE_Y_BELOW_CENTER
    hole_pts = [(-hole_x, hole_y), (hole_x, hole_y)]

    for hx, hy in hole_pts:
        boss = Pos(hx, hy, FRONT_THICK) * Cylinder(
            radius=boss_dia / 2,
            height=SCREEN_GLASS_THICK,
            align=(Align.CENTER, Align.CENTER, Align.MIN),
        )
        hole = Pos(hx, hy, FRONT_THICK - 0.1) * Cylinder(
            radius=SCREEN_HOLE_DIA / 2,
            height=SCREEN_GLASS_THICK + 0.2,
            align=(Align.CENTER, Align.CENTER, Align.MIN),
        )
        body += boss
        body -= hole

    # ESP32-C3 Super Mini retention cradle (aligned to -Y wall)
    rail_thick = 1.6
    rail_h = 4.5
    rail_z = CASE_DEPTH - rail_h
    esp_y_center = -INNER_H / 2 + ESP_L / 2

    for sx in (-1, 1):
        rail = Pos(sx * (ESP_W / 2 + rail_thick / 2), esp_y_center, rail_z) * Box(
            rail_thick, ESP_L, rail_h, align=(Align.CENTER, Align.CENTER, Align.MIN)
        )
        body += rail

    backstop = Pos(0, -INNER_H / 2 + ESP_L + rail_thick / 2, rail_z) * Box(
        ESP_W + 2 * rail_thick,
        rail_thick,
        rail_h,
        align=(Align.CENTER, Align.CENTER, Align.MIN),
    )
    body += backstop

    # 4 Corner lid screw bosses
    cx = INNER_W / 2 - 2.5
    cy = INNER_H / 2 - 2.5
    boss_h = CASE_DEPTH - FRONT_THICK - LID_THICK
    for sx in (-1, 1):
        for sy in (-1, 1):
            c_boss = Pos(sx * cx, sy * cy, FRONT_THICK) * Cylinder(
                radius=2.75,
                height=boss_h,
                align=(Align.CENTER, Align.CENTER, Align.MIN),
            )
            c_hole = Pos(sx * cx, sy * cy, FRONT_THICK + boss_h - 6.0) * Cylinder(
                radius=LID_SCREW_DIA / 2,
                height=6.2,
                align=(Align.CENTER, Align.CENTER, Align.MIN),
            )
            body += c_boss
            body -= c_hole

    return body


def build_back_cover() -> Part:
    """Build the flat back cover with alignment lip and 4 corner screw holes."""
    inner_r = max(1.0, CORNER_RADIUS - WALL_THICK)
    cover = extrude(RectangleRounded(OUTER_W, OUTER_H, CORNER_RADIUS), LID_THICK)

    # Alignment lip
    lip_outer = extrude(
        RectangleRounded(INNER_W - 0.6, INNER_H - 0.6, inner_r), 1.6
    )
    lip_inner = extrude(
        RectangleRounded(INNER_W - 3.0, INNER_H - 3.0, max(0.5, inner_r - 1.2)), 1.6
    )
    cover += Pos(0, 0, LID_THICK) * (lip_outer - lip_inner)

    cx = INNER_W / 2 - 2.5
    cy = INNER_H / 2 - 2.5
    for sx in (-1, 1):
        for sy in (-1, 1):
            cover -= Pos(sx * cx, sy * cy, -0.1) * Cylinder(
                radius=(LID_SCREW_DIA + 0.4) / 2,
                height=LID_THICK + 2.0,
                align=(Align.CENTER, Align.CENTER, Align.MIN),
            )

    return cover


if __name__ == "__main__":
    out_dir = Path(__file__).parent
    body = build_main_body()
    cover = build_back_cover()

    # Combine side-by-side for single-file export
    assembly = body + Pos(OUTER_W + 10, 0, 0) * cover

    step_path = out_dir / "flight_clock_case.step"
    stl_path = out_dir / "flight_clock_case.stl"

    export_step(assembly, str(step_path))
    export_stl(assembly, str(stl_path))
    print(f"Exported: {step_path}")
    print(f"Exported: {stl_path}")
