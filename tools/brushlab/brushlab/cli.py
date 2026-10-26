"""Command line interface. Run `brushlab --help`."""

from __future__ import annotations

import argparse
import os
import subprocess
import sys

from . import project

TEMPLATE = '''"""{description}"""

from brushlab import *

TITLE = "{title}"
TAGS = []

# Every parameter the reviewer might want to tweak. Variants override some of them.
PARAMS = dict(
    width=256,
    depth=256,
    height=128,
)

VARIANTS = {{
    "default": {{}},
    "wide": dict(width=384),
}}


def build(p):
    d = Design()
    d.add(room(rect(0, 0, p.width, p.depth), height=p.height,
               openings=[Opening(edge=0, width=64, height=96)]), "Room")
    d.entity("light", (p.width / 2, p.depth / 2, p.height - 32), light=250)
    return d
'''


def _print_result(r, verbose: bool = False) -> bool:
    ref = f"{r.get('design')}@{r.get('variant')}"
    if r.get("error"):
        print(f"  FAIL  {ref}: {r['error']}")
        if verbose and r.get("traceback"):
            print(r["traceback"])
        return False
    errors = [i for i in r.get("issues", []) if i["level"] == "error"]
    warnings = [i for i in r.get("issues", []) if i["level"] == "warning"
                or (verbose and i["level"] == "info")]
    size = " x ".join(f"{s:g}" for s in r["size"])
    flag = "ERR " if errors else "ok  "
    print(f"  {flag}  {ref}: {r['brushes']} brushes, {size} units")
    for i in errors + warnings:
        print(f"          {i['level']}: {i['message']}")
    if r.get("sheet"):
        print(f"          preview: {os.path.relpath(r['sheet'])}")
    return not errors


def cmd_build(args) -> int:
    names = args.designs or project.list_designs()
    ok = True
    for ref in names:
        name, variant = project.parse_ref(ref)
        for r in project.build_design(name, force=args.force, render=not args.no_render,
                                      only_variant=variant):
            ok = _print_result(r, args.verbose) and ok
    return 0 if ok else 1


def cmd_new(args) -> int:
    path = project.design_path(args.name)
    if os.path.exists(path):
        print(f"{path} already exists", file=sys.stderr)
        return 1
    os.makedirs(project.DESIGNS, exist_ok=True)
    title = args.title or args.name.replace("_", " ").capitalize()
    with open(path, "w") as f:
        f.write(TEMPLATE.format(title=title, description=args.description or title))
    print(f"created {os.path.relpath(path)}")
    return 0


def cmd_status(args) -> int:
    review = project.load_review()
    for name in project.list_designs():
        try:
            variants = project.variants_of(project.load_module(name))
        except Exception as e:
            print(f"{name}: cannot load ({e})")
            continue
        for v in variants:
            status, revised = project.effective_status(review, name, v)
            if args.pending and not (status in ("proposed", "changes") or revised):
                continue
            entry = project.review_entry(review, name, v)
            extra = " (revised since review)" if revised else ""
            print(f"{name}@{v}: {status}{extra}")
            if entry.get("note"):
                print(f"    reviewer: {entry['note']}")
    return 0


def cmd_review(args) -> int:
    name, variant = project.parse_ref(args.ref)
    if variant is None:
        variants = list(project.variants_of(project.load_module(name)))
        if len(variants) != 1:
            print(f"specify a variant: {', '.join(name + '@' + v for v in variants)}", file=sys.stderr)
            return 1
        variant = variants[0]
    entry = project.set_review(name, variant, args.status, args.message or "")
    print(f"{name}@{variant}: {entry['status']}")
    if entry.get("approved_map"):
        print(f"  saved {entry['approved_map']}")
    return 0


def _resolve(ref: str):
    name, variant = project.parse_ref(ref)
    if variant is None:
        variant = next(iter(project.variants_of(project.load_module(name))))
    r = project.build_variant(name, variant, render=False)
    if r.get("error"):
        raise SystemExit(f"{ref}: {r['error']}")
    return r


def cmd_copy(args) -> int:
    r = _resolve(args.ref)
    text = open(r["paste"]).read()
    for tool in (["pbcopy"], ["wl-copy"], ["xclip", "-selection", "clipboard"], ["clip"]):
        try:
            subprocess.run(tool, input=text.encode(), check=True)
            print(f"copied {r['brushes']} brushes; paste into TrenchBroom with Cmd/Ctrl+V")
            return 0
        except (FileNotFoundError, subprocess.CalledProcessError):
            continue
    print(text)
    return 0


def cmd_export(args) -> int:
    from .mapio import design_to_map, load_materials

    name, variant = project.parse_ref(args.ref)
    mod = project.load_module(name)
    variant = variant or next(iter(project.variants_of(mod)))
    params = project.Params(mod.get("PARAMS", {}))
    params.update(project.variants_of(mod)[variant])
    design = mod["build"](params)
    text = design_to_map(design, load_materials(project.MATERIALS_FILE), valve=args.valve,
                         worldspawn=not args.snippet)
    if args.output == "-":
        sys.stdout.write(text)
    else:
        with open(args.output, "w") as f:
            f.write(text)
        print(f"wrote {args.output}")
    return 0


def cmd_gallery(args) -> int:
    from .gallery import write_gallery

    path = args.output or os.path.join(project.OUT, "gallery.html")
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    write_gallery(path, force=args.force, inline_images=args.inline)
    print(f"wrote {path}")
    return 0


def cmd_serve(args) -> int:
    from .server import serve

    serve(args.port)
    return 0


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(prog="brushlab", description="Design, preview and review brush prefabs.")
    sub = ap.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("build", help="build designs, validate them and render previews")
    p.add_argument("designs", nargs="*", help="design or design@variant (default: all)")
    p.add_argument("--force", action="store_true", help="ignore the build cache")
    p.add_argument("--no-render", action="store_true", help="skip preview rendering")
    p.add_argument("-v", "--verbose", action="store_true", help="print tracebacks")
    p.set_defaults(fn=cmd_build)

    p = sub.add_parser("new", help="scaffold a new design file")
    p.add_argument("name")
    p.add_argument("--title")
    p.add_argument("--description")
    p.set_defaults(fn=cmd_new)

    p = sub.add_parser("status", help="list designs with review status and reviewer notes")
    p.add_argument("--pending", action="store_true", help="only proposed, changes or revised")
    p.set_defaults(fn=cmd_status)

    p = sub.add_parser("review", help="record a review decision")
    p.add_argument("ref", help="design@variant")
    p.add_argument("status", choices=project.STATUSES)
    p.add_argument("-m", "--message", help="feedback for the agent")
    p.set_defaults(fn=cmd_review)

    p = sub.add_parser("copy", help="copy a design to the clipboard for pasting into TrenchBroom")
    p.add_argument("ref")
    p.set_defaults(fn=cmd_copy)

    p = sub.add_parser("export", help="write a .map file")
    p.add_argument("ref")
    p.add_argument("-o", "--output", default="-")
    p.add_argument("--valve", action="store_true", help="Valve 220 format")
    p.add_argument("--snippet", action="store_true", help="omit worldspawn (for pasting)")
    p.set_defaults(fn=cmd_export)

    p = sub.add_parser("gallery", help="write a static HTML gallery")
    p.add_argument("-o", "--output")
    p.add_argument("--force", action="store_true")
    p.add_argument("--inline", action="store_true", help="embed images (single portable file)")
    p.set_defaults(fn=cmd_gallery)

    p = sub.add_parser("serve", help="run the review gallery with approve/reject buttons")
    p.add_argument("--port", type=int, default=8765)
    p.set_defaults(fn=cmd_serve)

    args = ap.parse_args(argv)
    return args.fn(args)
