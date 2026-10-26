"""Baixa texturas PBR CC0 do Poly Haven (polyhaven.com) para game/assets.
Se algo falhar, o jogo gera texturas equivalentes por código."""
import json, os, sys, urllib.request

OUT = sys.argv[1] if len(sys.argv) > 1 else "assets"
UA = {"User-Agent": "NeonCoast-build/1.0"}
WANT = {  # material -> palavras-chave aceitas no id do Poly Haven
    "asphalt": ["asphalt"],
    "paving": ["paving", "pavement", "sidewalk"],
    "sand": ["sand"],
    "plaster": ["plaster", "stucco"],
}
EXCLUDE = ["sandstone", "brick", "rock"]

def get(url):
    return urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=60).read()

os.makedirs(OUT, exist_ok=True)
assets = json.loads(get("https://api.polyhaven.com/assets?t=textures"))
for mat, keys in WANT.items():
    cands = [(a.get("download_count", 0), i) for i, a in assets.items()
             if any(k in i for k in keys) and not any(e in i for e in EXCLUDE)]
    for _, aid in sorted(cands, reverse=True)[:3]:
        try:
            files = json.loads(get(f"https://api.polyhaven.com/files/{aid}"))
            diff = files["Diffuse"]["2k"]["jpg"]["url"]
            nor = files["nor_gl"]["2k"]["jpg"]["url"]
            open(os.path.join(OUT, f"{mat}_diff.jpg"), "wb").write(get(diff))
            open(os.path.join(OUT, f"{mat}_nor.jpg"), "wb").write(get(nor))
            print(f"{mat}: {aid}")
            break
        except Exception as e:
            print(f"{mat}: falhou {aid}: {e}")

# ---------------------------------------------------------------------------------------
# Catálogo de modelos 3D do Poly Haven (impresso no log do build) e download dos escolhidos
# ---------------------------------------------------------------------------------------
try:
    models = json.loads(get("https://api.polyhaven.com/assets?t=models"))
    print("=== CATALOGO POLY HAVEN (modelos) ===")
    for mid, a in sorted(models.items()):
        print(f"{mid} | {','.join(a.get('categories', []))} | {','.join(a.get('tags', [])[:6])}")
except Exception as e:
    print("catalogo falhou:", e)
