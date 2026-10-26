"""Prepara .glb com esqueleto (ex.: Mixamo) para a raylib 5.5.

A raylib ignora as inverseBindMatrices e a transformação de nós-pai que não são ossos
(ex.: "Armature"/"Character" com escala 0.01). Este script:
  1. redefine a pose de repouso de cada osso para a pose de bind (inversa das inverseBindMatrices);
  2. embute a transformação dos nós-pai no osso raiz e nas animações dele;
  3. coloca a transformação de mundo da malha direto no nó da malha.
Uso: python fix_skin.py arquivo.glb [mais.glb ...]   (é idempotente)
"""
import json, struct, sys
import numpy as np


def q2m(q):
    x, y, z, w = q
    return np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                     [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                     [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])


def m2q(m):
    t = np.trace(m)
    if t > 0:
        s = np.sqrt(t + 1) * 2
        q = [(m[2, 1] - m[1, 2]) / s, (m[0, 2] - m[2, 0]) / s, (m[1, 0] - m[0, 1]) / s, 0.25 * s]
    elif m[0, 0] > m[1, 1] and m[0, 0] > m[2, 2]:
        s = np.sqrt(1 + m[0, 0] - m[1, 1] - m[2, 2]) * 2
        q = [0.25 * s, (m[0, 1] + m[1, 0]) / s, (m[0, 2] + m[2, 0]) / s, (m[2, 1] - m[1, 2]) / s]
    elif m[1, 1] > m[2, 2]:
        s = np.sqrt(1 + m[1, 1] - m[0, 0] - m[2, 2]) * 2
        q = [(m[0, 1] + m[1, 0]) / s, 0.25 * s, (m[1, 2] + m[2, 1]) / s, (m[0, 2] - m[2, 0]) / s]
    else:
        s = np.sqrt(1 + m[2, 2] - m[0, 0] - m[1, 1]) * 2
        q = [(m[0, 2] + m[2, 0]) / s, (m[1, 2] + m[2, 1]) / s, 0.25 * s, (m[1, 0] - m[0, 1]) / s]
    q = np.array(q)
    return list(q / np.linalg.norm(q))


def node_mat(n):
    if "matrix" in n:
        return np.array(n["matrix"], dtype=float).reshape(4, 4).T
    M = np.eye(4)
    M[:3, :3] = q2m(n.get("rotation", [0, 0, 0, 1])) * np.array(n.get("scale", [1, 1, 1]))
    M[:3, 3] = n.get("translation", [0, 0, 0])
    return M


def decompose(M):
    t = M[:3, 3].tolist()
    sc = np.linalg.norm(M[:3, :3], axis=0)
    R = M[:3, :3] / sc
    if np.linalg.det(R) < 0:
        sc[0] = -sc[0]; R[:, 0] = -R[:, 0]
    return t, m2q(R), sc.tolist()


def set_trs(n, M):
    n.pop("matrix", None)
    n["translation"], n["rotation"], n["scale"] = decompose(M)


def fix(path):
    d = open(path, "rb").read()
    jl = struct.unpack("<I", d[12:16])[0]
    j = json.loads(d[20:20 + jl])
    if j.get("extras", {}).get("raylib_skin_fixed"):
        print(f"{path}: ja corrigido"); return
    if not j.get("skins"):
        print(f"{path}: sem esqueleto, nada a fazer"); return
    bo = 20 + jl
    bl = struct.unpack("<I", d[bo:bo + 4])[0]
    binb = bytearray(d[bo + 8:bo + 8 + bl])
    nodes = j["nodes"]
    parent = {c: i for i, n in enumerate(nodes) for c in n.get("children", [])}
    local = [node_mat(n) for n in nodes]
    world = {}

    def W(i):
        if i not in world:
            world[i] = (W(parent[i]) if i in parent else np.eye(4)) @ local[i]
        return world[i]

    def read(acc_i, comp):
        acc = j["accessors"][acc_i]
        bv = j["bufferViews"][acc["bufferView"]]
        stride = bv.get("byteStride", comp * 4)
        base = bv.get("byteOffset", 0) + acc.get("byteOffset", 0)
        return [list(struct.unpack_from("<%df" % comp, binb, base + i * stride)) for i in range(acc["count"])]

    def write(acc_i, rows, comp):
        while len(binb) % 4: binb.append(0)
        off = len(binb)
        for r in rows: binb.extend(struct.pack("<%df" % comp, *r))
        j["bufferViews"].append({"buffer": 0, "byteOffset": off, "byteLength": len(rows) * comp * 4})
        acc = j["accessors"][acc_i]
        acc["bufferView"] = len(j["bufferViews"]) - 1; acc["byteOffset"] = 0
        acc.pop("min", None); acc.pop("max", None)

    joints = {jt for s in j["skins"] for jt in s["joints"]}
    # a raylib aceita só uma skin: une as demais na skin 0 remapeando JOINTS_0
    if len(j["skins"]) > 1:
        base = j["skins"][0]["joints"]
        for s in j["skins"][1:]:
            for jt in s["joints"]:
                if jt not in base: raise ValueError("skins com ossos diferentes")
        for i, n in enumerate(nodes):
            sk = n.get("skin", 0)
            if sk == 0: continue
            remap = [base.index(jt) for jt in j["skins"][sk]["joints"]]
            for prim in j["meshes"][n["mesh"]]["primitives"]:
                for key, acc_i in list(prim["attributes"].items()):
                    if not key.startswith("JOINTS_"): continue
                    acc = j["accessors"][acc_i]
                    fmt = {5121: "B", 5123: "H"}[acc["componentType"]]
                    bv = j["bufferViews"][acc["bufferView"]]
                    sz = struct.calcsize(fmt)
                    stride = bv.get("byteStride", 4 * sz)
                    off0 = bv.get("byteOffset", 0) + acc.get("byteOffset", 0)
                    rows = [[remap[x] for x in struct.unpack_from("<4" + fmt, binb, off0 + k * stride)] for k in range(acc["count"])]
                    while len(binb) % 4: binb.append(0)
                    off = len(binb)
                    for r in rows: binb.extend(struct.pack("<4H", *r))
                    j["bufferViews"].append({"buffer": 0, "byteOffset": off, "byteLength": len(rows) * 8})
                    acc.update(bufferView=len(j["bufferViews"]) - 1, byteOffset=0, componentType=5123)
                    acc.pop("min", None); acc.pop("max", None)
            n["skin"] = 0
        j["skins"] = j["skins"][:1]
    roots = [jt for jt in joints if parent.get(jt) not in joints]
    ancestors = set()
    for r in roots:
        p = parent.get(r)
        while p is not None: ancestors.add(p); p = parent.get(p)
    root_A = {r: (W(parent[r]) if r in parent else np.eye(4)) for r in roots}
    uscale = lambda M: float(np.cbrt(abs(np.linalg.det(M[:3, :3]))))
    # A raylib compõe os ossos sem aplicar a escala do pai às translações e ignora as
    # inverseBindMatrices. Deixamos todos os ossos sem escala, com translações em unidades
    # de mundo, e a escala que sobra (k) vai para a malha.
    r0 = roots[0]
    s_w = uscale(W(r0))                                   # escala de mundo dos ossos em repouso
    ib_root = np.linalg.inv(np.array(read(j["skins"][0]["inverseBindMatrices"], 16)[j["skins"][0]["joints"].index(r0)]).reshape(4, 4).T) \
        if "inverseBindMatrices" in j["skins"][0] else np.eye(4)
    k = s_w / uscale(ib_root)
    bind = {}
    for s in j["skins"]:
        ibms = read(s["inverseBindMatrices"], 16) if "inverseBindMatrices" in s else [list(np.eye(4).flatten())] * len(s["joints"])
        for jt, ib in zip(s["joints"], ibms):
            if jt in bind: continue
            B = np.linalg.inv(np.array(ib).reshape(4, 4).T)
            t, q, _ = decompose(B)
            M = np.eye(4); M[:3, :3] = q2m(q); M[:3, 3] = np.array(t) * k
            bind[jt] = M

    # 1) animações
    for an in j.get("animations", []):
        for ch in an["channels"]:
            nd = ch["target"].get("node")
            if nd not in joints: continue
            acc_i = an["samplers"][ch["sampler"]]["output"]
            pth = ch["target"]["path"]
            if nd in root_A:
                A = root_A[nd]; tA, qA, sA = decompose(A)
                if pth == "translation": write(acc_i, [(A @ np.array(v + [1.0]))[:3].tolist() for v in read(acc_i, 3)], 3)
                elif pth == "rotation": write(acc_i, [m2q(q2m(qA) @ q2m(v)) for v in read(acc_i, 4)], 4)
                elif pth == "scale": write(acc_i, [[sA[c] * v[c] / s_w for c in range(3)] for v in read(acc_i, 3)], 3)
            elif pth == "translation":
                write(acc_i, [[x * s_w for x in v] for v in read(acc_i, 3)], 3)
            elif pth == "scale":
                write(acc_i, [[1.0, 1.0, 1.0] for _ in read(acc_i, 3)], 3)
    # 2) pose de repouso = pose de bind (sem escala)
    for jt in joints:
        pw = bind[parent[jt]] if parent.get(jt) in joints else np.eye(4)
        set_trs(nodes[jt], np.linalg.inv(pw) @ bind[jt])
    # 3) nós-pai viram identidade; malhas recebem só a escala k
    for a in ancestors:
        for kk in ("translation", "rotation", "scale", "matrix"): nodes[a].pop(kk, None)
    for i, n in enumerate(nodes):
        if "skin" in n:
            for kk in ("translation", "rotation", "matrix"): n.pop(kk, None)
            n["scale"] = [k, k, k]
            p = parent.get(i)
            while p is not None and p not in ancestors:
                for kk in ("translation", "rotation", "scale", "matrix"): nodes[p].pop(kk, None)
                p = parent.get(p)

    j.setdefault("extras", {})["raylib_skin_fixed"] = True
    j["buffers"][0]["byteLength"] = len(binb)
    js = json.dumps(j, separators=(",", ":")).encode()
    js += b" " * ((4 - len(js) % 4) % 4)
    while len(binb) % 4: binb.append(0)
    body = struct.pack("<I4s", len(js), b"JSON") + js + struct.pack("<I4s", len(binb), b"BIN\0") + bytes(binb)
    open(path, "wb").write(struct.pack("<4sII", b"glTF", 2, 12 + len(body)) + body)
    print(f"{path}: esqueleto corrigido")


for p in sys.argv[1:]:
    try:
        fix(p)
    except Exception as e:
        print(f"{p}: ignorado ({e})")
