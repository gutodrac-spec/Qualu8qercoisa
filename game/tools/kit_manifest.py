"""Gera <kit>.kit ao lado de um .gltf modular (ex.: fachadas do Poly Haven) para o jogo montar prédios.
Formato:  mat <indice_raylib> <nome>   |   piece <nome> <primeira_malha> <qtd_malhas> <tx> <ty> <tz>
A raylib cria uma malha por primitiva, na ordem dos nós, e coloca o material i do glTF no índice i+1."""
import json, sys, os

for path in sys.argv[1:]:
    j = json.load(open(path))
    out = [f"mat {i + 1} {m.get('name', '')}" for i, m in enumerate(j.get("materials", []))]
    idx = 0
    for n in j["nodes"]:
        if "mesh" not in n: continue
        prims = [p for p in j["meshes"][n["mesh"]]["primitives"] if p.get("mode", 4) == 4]
        rot, sc = n.get("rotation", [0, 0, 0, 1]), n.get("scale", [1, 1, 1])
        if max(abs(x) for x in rot[:3]) > 1e-4 or max(abs(x - 1) for x in sc) > 1e-4:
            print(f"aviso: {n.get('name')} tem rotacao/escala, ignorado"); idx += len(prims); continue
        t = n.get("translation", [0, 0, 0])
        out.append(f"piece {n.get('name')} {idx} {len(prims)} {t[0]:.4f} {t[1]:.4f} {t[2]:.4f}")
        idx += len(prims)
    dst = os.path.splitext(path)[0] + ".kit"
    open(dst, "w").write("\n".join(out) + "\n")
    print(f"{dst}: {idx} malhas")
