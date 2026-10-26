"""Design discovery, building, caching and review state."""

from __future__ import annotations

import hashlib
import json
import os
import runpy
import shutil
import time
import traceback
from typing import Any, Dict, List, Optional, Tuple

from .design import Design
from .mapio import design_to_map, load_materials
from .validate import validate

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PKG = os.path.dirname(os.path.abspath(__file__))
DESIGNS = os.path.join(ROOT, "designs")
OUT = os.path.join(ROOT, "out")
APPROVED = os.path.join(ROOT, "approved")
REVIEW_FILE = os.path.join(ROOT, "review.json")
MATERIALS_FILE = os.path.join(ROOT, "materials.json")

STATUSES = ("proposed", "approved", "changes", "rejected")


class Params(dict):
    """A dict whose keys can also be read as attributes: p.width == p["width"]."""

    def __getattr__(self, key: str) -> Any:
        try:
            return self[key]
        except KeyError:
            raise AttributeError(f"unknown parameter '{key}'") from None


def _hash_file(path: str) -> str:
    with open(path, "rb") as f:
        return hashlib.sha1(f.read()).hexdigest()


def _library_hash() -> str:
    h = hashlib.sha1()
    for name in sorted(os.listdir(PKG)):
        if name.endswith(".py"):
            with open(os.path.join(PKG, name), "rb") as f:
                h.update(f.read())
    if os.path.exists(MATERIALS_FILE):
        h.update(open(MATERIALS_FILE, "rb").read())
    return h.hexdigest()


def list_designs() -> List[str]:
    if not os.path.isdir(DESIGNS):
        return []
    return sorted(
        n[:-3] for n in os.listdir(DESIGNS)
        if n.endswith(".py") and not n.startswith("_")
    )


def design_path(name: str) -> str:
    return os.path.join(DESIGNS, name + ".py")


def load_module(name: str) -> Dict[str, Any]:
    path = design_path(name)
    if not os.path.exists(path):
        raise FileNotFoundError(f"no design named '{name}' ({path})")
    return runpy.run_path(path, run_name=f"brushlab_design_{name}")


def variants_of(mod: Dict[str, Any]) -> Dict[str, Dict[str, Any]]:
    variants = mod.get("VARIANTS") or {"default": {}}
    return {str(k): dict(v) for k, v in variants.items()}


def parse_ref(ref: str) -> Tuple[str, Optional[str]]:
    if "@" in ref:
        d, v = ref.split("@", 1)
        return d, v
    return ref, None


# ---------------------------------------------------------------------------------------
# Review state
# ---------------------------------------------------------------------------------------


def load_review() -> Dict[str, Dict[str, Dict[str, Any]]]:
    if os.path.exists(REVIEW_FILE):
        with open(REVIEW_FILE) as f:
            return json.load(f)
    return {}


def save_review(data) -> None:
    with open(REVIEW_FILE, "w") as f:
        json.dump(data, f, indent=2, sort_keys=True)
        f.write("\n")


def review_entry(review, design: str, variant: str) -> Dict[str, Any]:
    return review.get(design, {}).get(variant, {})


def effective_status(review, design: str, variant: str,
                     meta: Optional[Dict[str, Any]] = None) -> Tuple[str, bool]:
    """Returns (status, revised). `revised` means the variant's generated geometry changed
    after the last review, so the reviewer should look again. Pass the build metadata
    if you have it; otherwise the variant is built (cached) to find out."""
    e = review_entry(review, design, variant)
    status = e.get("status", "proposed")
    if status == "proposed" or "map_hash" not in e:
        return status, False
    if meta is None:
        try:
            meta = build_variant(design, variant, render=False)
        except Exception:
            return status, False
    return status, meta.get("map_hash") not in (None, e["map_hash"])


def set_review(design: str, variant: str, status: str, note: str = "") -> Dict[str, Any]:
    if status not in STATUSES:
        raise ValueError(f"status must be one of {', '.join(STATUSES)}")
    mod = load_module(design)
    if variant not in variants_of(mod):
        raise ValueError(f"design '{design}' has no variant '{variant}'")
    result = build_variant(design, variant)
    if result.get("error"):
        raise RuntimeError(f"cannot review a design that fails to build: {result['error']}")
    review = load_review()
    entry = review.setdefault(design, {}).setdefault(variant, {})
    history = entry.setdefault("history", [])
    history.append({"status": status, "note": note, "time": time.strftime("%Y-%m-%d %H:%M")})
    entry.update({
        "status": status,
        "note": note,
        "map_hash": result["map_hash"],
        "time": time.strftime("%Y-%m-%d %H:%M"),
    })
    entry.pop("design_hash", None)
    save_review(review)

    approved_map = os.path.join(APPROVED, f"{design}--{variant}.map")
    if status == "approved":
        os.makedirs(APPROVED, exist_ok=True)
        shutil.copyfile(result["map"], approved_map)
        entry["approved_map"] = os.path.relpath(approved_map, ROOT)
        save_review(review)
    elif os.path.exists(approved_map) and status == "rejected":
        os.remove(approved_map)
    return entry


# ---------------------------------------------------------------------------------------
# Building
# ---------------------------------------------------------------------------------------


def out_dir(design: str, variant: str) -> str:
    return os.path.join(OUT, design, variant)


def build_variant(design: str, variant: str, force: bool = False, mod=None,
                  render: bool = True) -> Dict[str, Any]:
    """Builds one variant. Results are cached on the design file and library contents."""
    d_out = out_dir(design, variant)
    meta_path = os.path.join(d_out, "meta.json")
    key = _hash_file(design_path(design)) + _library_hash() + variant
    if not force and os.path.exists(meta_path):
        with open(meta_path) as f:
            meta = json.load(f)
        if meta.get("key") == key and (meta.get("rendered") or not render):
            return meta

    os.makedirs(d_out, exist_ok=True)
    meta: Dict[str, Any] = {"design": design, "variant": variant, "key": key}
    t0 = time.time()
    try:
        mod = mod or load_module(design)
        params = Params(mod.get("PARAMS", {}))
        params.update(variants_of(mod)[variant])
        meta["params"] = dict(params)
        built = mod["build"](params)
        if not isinstance(built, Design):
            raise TypeError("build(p) must return a brushlab.Design")
        built.name = built.name if built.name != "Design" else mod.get("TITLE", design)
    except Exception as e:  # report design errors instead of crashing the whole run
        meta["error"] = f"{type(e).__name__}: {e}"
        meta["traceback"] = traceback.format_exc()
        meta["key"] = None
        with open(meta_path, "w") as f:
            json.dump(meta, f, indent=2)
        return meta

    materials = load_materials(MATERIALS_FILE)
    issues = validate(built)
    map_path = os.path.join(d_out, f"{design}--{variant}.map")
    map_text = design_to_map(built, materials)
    with open(map_path, "w") as f:
        f.write(map_text)
    meta["map_hash"] = hashlib.sha1(map_text.encode()).hexdigest()
    paste_path = os.path.join(d_out, "paste.map")
    with open(paste_path, "w") as f:
        f.write(design_to_map(built, materials, worldspawn=False))

    lo, hi = built.bounds
    meta.update({
        "title": mod.get("TITLE", design),
        "description": (mod.get("__doc__") or "").strip(),
        "tags": list(mod.get("TAGS", [])),
        "brushes": len(built.brushes),
        "groups": {k: len(v) for k, v in built.groups.items()},
        "entities": [e.classname for e in built.entities],
        "bounds": [list(lo), list(hi)],
        "size": [hi[i] - lo[i] for i in range(3)],
        "issues": [i.to_json() for i in issues],
        "notes": built.notes,
        "map": map_path,
        "paste": paste_path,
        "build_seconds": round(time.time() - t0, 2),
    })

    if render:
        from .render import render_sheet, render_thumbnail
        from .viewer import mesh_json

        sheet = os.path.join(d_out, "sheet.png")
        thumb = os.path.join(d_out, "thumb.png")
        subtitle = f"{design}@{variant}"
        render_sheet(built, sheet, title=meta["title"], subtitle=subtitle)
        render_thumbnail(built, thumb)
        with open(os.path.join(d_out, "mesh.json"), "w") as f:
            json.dump(mesh_json(built), f, separators=(",", ":"))
        meta.update({"sheet": sheet, "thumb": thumb, "rendered": True})
        meta["build_seconds"] = round(time.time() - t0, 2)

    with open(meta_path, "w") as f:
        json.dump(meta, f, indent=2)
    return meta


def build_design(design: str, force: bool = False, render: bool = True,
                 only_variant: Optional[str] = None) -> List[Dict[str, Any]]:
    try:
        mod = load_module(design)
    except Exception as e:
        return [{"design": design, "variant": "?", "error": f"{type(e).__name__}: {e}",
                 "traceback": traceback.format_exc()}]
    results = []
    for variant in variants_of(mod):
        if only_variant and variant != only_variant:
            continue
        results.append(build_variant(design, variant, force=force, mod=mod, render=render))
    return results


def build_all(force: bool = False, render: bool = True) -> List[Dict[str, Any]]:
    results = []
    for name in list_designs():
        results.extend(build_design(name, force=force, render=render))
    return results
