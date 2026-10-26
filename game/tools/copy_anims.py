"""Copia animações entre esqueletos com nomes de ossos do Mixamo (com ou sem prefixo "mixamorig:").
Uso: python copy_anims.py origem.glb destino.glb Walk Idle ...
Rode ANTES do fix_skin.py (os dois arquivos devem estar no formato original)."""
import json, struct, sys
import numpy as np


def load(path):
    d = open(path, "rb").read()
    jl = struct.unpack("<I", d[12:16])[0]
    j = json.loads(d[20:20 + jl])
    bo = 20 + jl
    bl = struct.unpack("<I", d[bo:bo + 4])[0]
    return j, bytearray(d[bo + 8:bo + 8 + bl])


def save(path, j, binb):
    j["buffers"][0]["byteLength"] = len(binb)
    js = json.dumps(j, separators=(",", ":")).encode()
    js += b" " * ((4 - len(js) % 4) % 4)
    while len(binb) % 4: binb.append(0)
    body = struct.pack("<I4s", len(js), b"JSON") + js + struct.pack("<I4s", len(binb), b"BIN\0") + bytes(binb)
    open(path, "wb").write(struct.pack("<4sII", b"glTF", 2, 12 + len(body)) + body)


def q2m(q):
    x, y, z, w = q
    return np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                     [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                     [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])


def m2q(m):
    t = np.trace(m)
    if t > 0:
        s = np.sqrt(t + 1) * 2; q = [(m[2, 1] - m[1, 2]) / s, (m[0, 2] - m[2, 0]) / s, (m[1, 0] - m[0, 1]) / s, 0.25 * s]
    elif m[0, 0] > m[1, 1] and m[0, 0] > m[2, 2]:
        s = np.sqrt(1 + m[0, 0] - m[1, 1] - m[2, 2]) * 2; q = [0.25 * s, (m[0, 1] + m[1, 0]) / s, (m[0, 2] + m[2, 0]) / s, (m[2, 1] - m[1, 2]) / s]
    elif m[1, 1] > m[2, 2]:
        s = np.sqrt(1 + m[1, 1] - m[0, 0] - m[2, 2]) * 2; q = [(m[0, 1] + m[1, 0]) / s, 0.25 * s, (m[1, 2] + m[2, 1]) / s, (m[0, 2] - m[2, 0]) / s]
    else:
        s = np.sqrt(1 + m[2, 2] - m[0, 0] - m[1, 1]) * 2; q = [(m[0, 2] + m[2, 0]) / s, (m[1, 2] + m[2, 1]) / s, 0.25 * s, (m[1, 0] - m[0, 1]) / s]
    q = np.array(q); return list(q / np.linalg.norm(q))


def node_mat(n):
    if "matrix" in n: return np.array(n["matrix"], dtype=float).reshape(4, 4).T
    M = np.eye(4)
    M[:3, :3] = q2m(n.get("rotation", [0, 0, 0, 1])) * np.array(n.get("scale", [1, 1, 1]))
    M[:3, 3] = n.get("translation", [0, 0, 0])
    return M


def world_of(j, idx):
    parent = {c: i for i, n in enumerate(j["nodes"]) for c in n.get("children", [])}
    M = np.eye(4)
    while idx is not None:
        M = node_mat(j["nodes"][idx]) @ M; idx = parent.get(idx)
    return M


def parent_of(j, idx):
    return next((i for i, n in enumerate(j["nodes"]) if idx in n.get("children", [])), None)


def key(name):
    return name.split(":")[-1].lower().replace("mixamorig", "")


def read(j, binb, acc_i):
    acc = j["accessors"][acc_i]
    comp = {"SCALAR": 1, "VEC3": 3, "VEC4": 4}[acc["type"]]
    bv = j["bufferViews"][acc["bufferView"]]
    stride = bv.get("byteStride", comp * 4)
    base = bv.get("byteOffset", 0) + acc.get("byteOffset", 0)
    return [list(struct.unpack_from("<%df" % comp, binb, base + i * stride)) for i in range(acc["count"])], acc["type"]


def add_acc(j, binb, rows, typ):
    while len(binb) % 4: binb.append(0)
    off = len(binb)
    comp = len(rows[0])
    for r in rows: binb.extend(struct.pack("<%df" % comp, *r))
    j["bufferViews"].append({"buffer": 0, "byteOffset": off, "byteLength": len(rows) * comp * 4})
    acc = {"bufferView": len(j["bufferViews"]) - 1, "componentType": 5126, "count": len(rows), "type": typ}
    if typ == "SCALAR": acc["min"] = [min(r[0] for r in rows)]; acc["max"] = [max(r[0] for r in rows)]
    j["accessors"].append(acc)
    return len(j["accessors"]) - 1


def main(src, dst, names):
    sj, sb = load(src)
    dj, db = load(dst)
    dnodes = {key(n.get("name", "")): i for i, n in enumerate(dj["nodes"])}
    snodes = sj["nodes"]
    hs = next(i for i, n in enumerate(snodes) if key(n.get("name", "")) == "hips")
    hd = dnodes["hips"]
    Ps = world_of(sj, parent_of(sj, hs)) if parent_of(sj, hs) is not None else np.eye(4)
    Pd = world_of(dj, parent_of(dj, hd)) if parent_of(dj, hd) is not None else np.eye(4)
    hs_w, hd_w = world_of(sj, hs)[:3, 3], world_of(dj, hd)[:3, 3]
    ratio = abs(hd_w[1]) / max(abs(hs_w[1]), 1e-6)            # altura do quadril no mundo
    L = np.linalg.inv(Pd[:3, :3]) @ Ps[:3, :3]                  # espaço do pai: origem -> destino
    Lr = L / np.cbrt(abs(np.linalg.det(L)))
    dj.setdefault("animations", [])
    for an in sj.get("animations", []):
        if an.get("name") not in names: continue
        new = {"name": an["name"], "channels": [], "samplers": []}
        for ch in an["channels"]:
            k = key(snodes[ch["target"]["node"]].get("name", ""))
            path = ch["target"]["path"]
            if k not in dnodes or path == "scale": continue
            if path == "translation" and k != "hips": continue   # proporções diferentes: só o quadril se move
            smp = an["samplers"][ch["sampler"]]
            tin, _ = read(sj, sb, smp["input"])
            tout, typ = read(sj, sb, smp["output"])
            if k == "hips" and path == "translation":
                tout = [(L @ np.array(v) * ratio).tolist() for v in tout]
            elif k == "hips" and path == "rotation":
                tout = [m2q(Lr @ q2m(v)) for v in tout]
            new["samplers"].append({"input": add_acc(dj, db, tin, "SCALAR"), "output": add_acc(dj, db, tout, typ),
                                    "interpolation": smp.get("interpolation", "LINEAR")})
            new["channels"].append({"sampler": len(new["samplers"]) - 1, "target": {"node": dnodes[k], "path": path}})
        dj["animations"].append(new)
        print(f"{dst}: animacao {an['name']} copiada ({len(new['channels'])} canais)")
    save(dst, dj, db)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], sys.argv[3:])
