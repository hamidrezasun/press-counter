"""Write KiCad project files (.kicad_pro, custom DRC rules, library tables) for one board."""
import json
import os


def netclass(name, clearance, track, via_d=0.6, via_dr=0.3, prio=0, color="rgba(0, 0, 0, 0.000)"):
    return {
        "bus_width": 12, "clearance": clearance, "diff_pair_gap": 0.25, "diff_pair_via_gap": 0.25, "diff_pair_width": 0.2,
        "line_style": 0, "microvia_diameter": 0.3, "microvia_drill": 0.1, "name": name, "pcb_color": color,
        "priority": prio, "schematic_color": "rgba(0, 0, 0, 0.000)", "track_width": track, "tuning_profile": "",
        "via_diameter": via_d, "via_drill": via_dr, "wire_width": 6,
    }


def write_project(dirpath, name, board):
    classes = [netclass("Default", 0.2, 0.25, prio=2147483647), netclass("Power", 0.2, 0.5, prio=3)]
    patterns = [{"netclass": "Power", "pattern": n} for n in board.power_nets]
    if board.mains_nets:
        classes.append(netclass("Mains", 1.5, 0.6, prio=1, color="rgb(255, 80, 0)"))
        classes.append(netclass("MainsDC", 0.4, 0.4, prio=0, color="rgb(255, 150, 0)"))
        patterns += [{"netclass": "Mains", "pattern": n} for n in board.mains_nets]
        patterns += [{"netclass": "MainsDC", "pattern": n} for n in board.mains_dc_nets]
    pro = {
        "board": {"design_settings": {
            "defaults": {"board_outline_line_width": 0.05, "copper_line_width": 0.2, "courtyard_line_width": 0.05,
                         "silk_line_width": 0.15, "silk_text_size_h": 1.0, "silk_text_size_v": 1.0,
                         "silk_text_thickness": 0.15, "other_text_size_h": 1.0, "other_text_size_v": 1.0,
                         "pads": {"drill": 0.8, "height": 1.6, "width": 1.6}, "zones": {"min_clearance": 0.3}},
            "rules": {"max_error": 0.005, "min_clearance": 0.15, "min_connection": 0.0, "min_copper_edge_clearance": 0.4,
                      "min_hole_clearance": 0.2, "min_hole_to_hole": 0.25, "min_through_hole_diameter": 0.3,
                      "min_track_width": 0.15, "min_via_annular_width": 0.13, "min_via_diameter": 0.5,
                      "min_silk_clearance": 0.0, "solder_mask_to_copper_clearance": 0.0, "min_resolved_spokes": 1, "min_text_height": 0.7},
        }},
        "meta": {"filename": name + ".kicad_pro", "version": 3},
        "net_settings": {"classes": classes, "meta": {"version": 5}, "net_colors": None, "netclass_assignments": None,
                         "netclass_patterns": patterns},
        "schematic": {"drawing": {"default_line_thickness": 6.0, "default_text_size": 50.0}},
        "sheets": [],
        "text_variables": {},
    }
    with open(os.path.join(dirpath, name + ".kicad_pro"), "w", encoding="utf-8") as f:
        json.dump(pro, f, indent=2)
    with open(os.path.join(dirpath, "sym-lib-table"), "w", encoding="utf-8") as f:
        f.write('(sym_lib_table\n  (version 7)\n)\n')
    with open(os.path.join(dirpath, "fp-lib-table"), "w", encoding="utf-8") as f:
        f.write('(fp_lib_table\n  (version 7)\n  (lib (name "press_counter")(type "KiCad")'
                '(uri "${KIPRJMOD}/../lib/press_counter.pretty")(options "")(descr "Press counter project footprints"))\n)\n')
    # NOTE: KiCad silently ignores the WHOLE rules file if one rule has an invalid property (e.g. A.parentFootprint),
    # so keep conditions simple and verify with a negative test (see docs/REVIEW.md).
    # Pad-to-pad pairs are exempt: the pad pitch inside a certified part (relay, opto, HLK module) is the manufacturer's.
    rules = ["(version 1)"]
    ex = "".join(f" && !(A.insideCourtyard('{r}') && B.insideCourtyard('{r}'))" for r in board.creepage_exempt)
    if board.mains_nets:
        rules += [f'''(rule mains_ac_to_lv
  (severity error)
  (condition "A.NetClass == 'Mains' && B.NetClass != 'Mains' && B.NetClass != 'MainsDC' && (A.Type != 'Pad' || B.Type != 'Pad'){ex}")
  (constraint clearance (min 5.0mm)))''',
                  f'''(rule mains_dc_to_lv
  (severity error)
  (condition "A.NetClass == 'MainsDC' && B.NetClass != 'Mains' && B.NetClass != 'MainsDC' && (A.Type != 'Pad' || B.Type != 'Pad'){ex}")
  (constraint clearance (min 5.0mm)))''']
    with open(os.path.join(dirpath, name + ".kicad_dru"), "w", encoding="utf-8") as f:
        f.write("\n".join(rules) + "\n")
