"""Baixa modelos 3D CC0 do Poly Haven em glTF para uma pasta (uso: python fetch_models.py destino)."""
import json, os, sys, urllib.request

OUT = sys.argv[1] if len(sys.argv) > 1 else "polyhaven"
UA = {"User-Agent": "NeonCoast-build/1.0"}
IDS = {   # id -> resolução
    "modular_urban_apartments_facade": "1k", "modular_factory_facade": "1k", "modular_fire_escape": "1k",
    "rollershutter_door": "1k", "rollershutter_window_01": "1k", "modular_metal_gutter": "1k",
    "street_lamp_01": "1k", "street_lamp_02": "1k", "fire_hydrant": "1k", "utility_box_01": "1k", "utility_box_02": "1k",
    "concrete_road_barrier": "1k", "modular_street_seating": "1k", "trashbag": "1k", "water_manhole_cover": "1k",
    "modular_electricity_poles": "1k", "security_light": "1k", "standing_chalkboard_01": "1k", "outdoor_table_chair_set_01": "1k",
    "island_tree_02": "1k",
    "shrub_04": "1k", "potted_plant_01": "1k", "potted_plant_02": "1k", "coast_rocks_01": "1k", "barrel_03": "1k", "covered_car": "1k",
}

def get(url):
    return urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=120).read()

for mid, res in IDS.items():
    try:
        files = json.loads(get(f"https://api.polyhaven.com/files/{mid}"))
        g = files["gltf"]
        r = res if res in g else sorted(g.keys())[0]
        entry = g[r]["gltf"]
        d = os.path.join(OUT, mid); os.makedirs(d, exist_ok=True)
        open(os.path.join(d, os.path.basename(entry["url"])), "wb").write(get(entry["url"]))
        for rel, inc in entry.get("include", {}).items():
            p = os.path.join(d, rel); os.makedirs(os.path.dirname(p), exist_ok=True)
            open(p, "wb").write(get(inc["url"]))
        print(f"ok {mid} ({r})")
    except Exception as e:
        print(f"FALHOU {mid}: {e}")
