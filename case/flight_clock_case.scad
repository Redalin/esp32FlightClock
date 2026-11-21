// Parametric 3D-Printable Case for ESP32-C3 Super Mini + 1.28" GC9A01 Round TFT
// Print orientation:
//   - Main body: face-down on the build plate (no supports needed)
//   - Back cover: flat on the build plate (no supports needed)

// ============================================================================
// USER-EDITABLE PARAMETERS (all dimensions in mm)
// ============================================================================

// --- 1.28" GC9A01 Round TFT Screen ---
screen_view_dia            = 33.5;   // Visible round opening diameter (active area ~32.4mm)
screen_glass_dia           = 38.0;   // Front glass/bezel diameter (for internal recess)
screen_pcb_w               = 38.0;   // Screen PCB width
screen_pcb_h               = 45.0;   // Screen PCB height (including bottom mounting holes)
screen_pcb_thick           = 1.6;    // Screen PCB thickness
screen_glass_thick         = 2.2;    // Glass/bezel thickness in front of PCB
screen_y_offset            = 3.5;    // Shift screen opening upward (+Y) to leave room at bottom
screen_hole_spacing        = 19.0;   // Horizontal center-to-center distance between the 2 bottom holes
screen_hole_y_below_center = 22.0;   // Vertical distance from screen center down to the 2 holes
screen_hole_dia            = 2.2;    // M2 clearance/pilot hole (2.2mm for M2 screw, 2.7mm for M2.5) (left/right)

// --- ESP32-C3 Super Mini ---
esp_w                 = 18.4;   // 18.0mm PCB + 0.4mm print clearance
esp_l                 = 23.0;   // 22.5mm PCB + 0.5mm print clearance
esp_h                 = 6.0;    // Clearance height for ESP32 + wires/headers
usb_cutout_w          = 11.5;   // USB-C port opening width (allows cable plug clearance)
usb_cutout_h          = 6.0;    // USB-C port opening height
usb_z_offset          = 1.5;    // Height of USB-C center above ESP32 cradle floor

// --- Case Shell & Lid ---
wall_thick            = 2.4;    // Side wall thickness
front_thick           = 2.0;    // Front bezel thickness
case_depth            = 18.0;   // Total outer depth of main body (excluding back lid)
corner_radius         = 4.0;    // Outer corner radius
clearance             = 0.4;    // General fit clearance
lid_thick             = 2.0;    // Back cover thickness
lid_screw_dia         = 2.2;    // Back cover M2 screw holes

$fn = 64;

// ============================================================================
// DERIVED DIMENSIONS
// ============================================================================
inner_w = max(screen_pcb_w, esp_w) + 2 * clearance + 4.0;
inner_h = max(screen_pcb_h, esp_l) + 2 * clearance + 4.0;
outer_w = inner_w + 2 * wall_thick;
outer_h = inner_h + 2 * wall_thick;

// ============================================================================
// MODULES
// ============================================================================
module rounded_rect(w, h, r) {
    offset(r = r) offset(delta = -r)
        square([w, h], center = true);
}

module main_body() {
    difference() {
        union() {
            // Outer shell
            linear_extrude(height = case_depth)
                rounded_rect(outer_w, outer_h, corner_radius);
        }

        // Main internal cavity (leaves front_thick at z = 0..front_thick)
        translate([0, 0, front_thick])
            linear_extrude(height = case_depth - front_thick + 0.1)
                rounded_rect(inner_w, inner_h, max(1.0, corner_radius - wall_thick));

        // Round screen viewing opening through front bezel (shifted upward)
        translate([0, screen_y_offset, -0.1])
            cylinder(d = screen_view_dia, h = front_thick + 0.2);

        // Shallow recess for screen glass/metal bezel if needed
        translate([0, screen_y_offset, front_thick - 0.8])
            cylinder(d = screen_glass_dia + clearance, h = 0.9);

        // USB-C port cutout on the bottom wall (-Y edge)
        usb_center_z = case_depth - usb_z_offset - usb_cutout_h / 2;
        translate([0, -outer_h / 2 + wall_thick / 2, usb_center_z])
            rotate([90, 0, 0])
                linear_extrude(height = wall_thick + 1.0, center = true)
                    rounded_rect(usb_cutout_w, usb_cutout_h, 1.5);
    }

    // 2 Screen mounting bosses at the bottom of the screen
    boss_z_h = screen_glass_thick;
    boss_dia = screen_hole_dia + 3.6;
    hole_y = screen_y_offset - screen_hole_y_below_center;
    for (s = [-1, 1]) {
        translate([s * screen_hole_spacing / 2, hole_y, front_thick])
            difference() {
                cylinder(d = boss_dia, h = boss_z_h);
                translate([0, 0, -0.1])
                    cylinder(d = screen_hole_dia, h = boss_z_h + 0.2);
            }
    }

    // ESP32-C3 Super Mini side retention rails (along -Y wall)
    rail_thick = 1.6;
    rail_h = 4.5;
    rail_z = case_depth - rail_h;
    for (s = [-1, 1]) {
        translate([s * (esp_w / 2 + rail_thick / 2), -inner_h / 2 + esp_l / 2, rail_z])
            cube([rail_thick, esp_l, rail_h], center = false);
        // Backstop at +Y end of ESP32
    }
    translate([-esp_w / 2 - rail_thick, -inner_h / 2 + esp_l, rail_z])
        cube([esp_w + 2 * rail_thick, rail_thick, rail_h]);

    // 4 Corner bosses for back cover screws
    corner_x = inner_w / 2 - 2.5;
    corner_y = inner_h / 2 - 2.5;
    for (sx = [-1, 1], sy = [-1, 1]) {
        translate([sx * corner_x, sy * corner_y, front_thick])
            difference() {
                cylinder(d = 5.5, h = case_depth - front_thick - lid_thick);
                translate([0, 0, case_depth - front_thick - lid_thick - 6.0])
                    cylinder(d = lid_screw_dia, h = 6.2);
            }
    }
}

module back_cover() {
    corner_x = inner_w / 2 - 2.5;
    corner_y = inner_h / 2 - 2.5;

    difference() {
        union() {
            // Outer plate
            linear_extrude(height = lid_thick)
                rounded_rect(outer_w, outer_h, corner_radius);

            // Inner alignment lip
            translate([0, 0, lid_thick])
                linear_extrude(height = 1.6)
                    difference() {
                        rounded_rect(inner_w - 0.6, inner_h - 0.6, max(1.0, corner_radius - wall_thick));
                        rounded_rect(inner_w - 3.0, inner_h - 3.0, max(0.5, corner_radius - wall_thick - 1.2));
                    }
        }

        // 4 Corner screw holes
        for (sx = [-1, 1], sy = [-1, 1]) {
            translate([sx * corner_x, sy * corner_y, -0.1])
                cylinder(d = lid_screw_dia + 0.4, h = lid_thick + 2.0);
        }
    }
}

// ============================================================================
// RENDER LAYOUT (side-by-side for printing)
// ============================================================================
main_body();

translate([outer_w + 10, 0, 0])
    back_cover();
