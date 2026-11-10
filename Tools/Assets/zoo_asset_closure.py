"""Finds which ZooEka2VR assets the Museum reuses, with every asset they reference, by reading the
package names stored in each .uasset (read-only: the Zoo project is never opened or written).

Usage:
  python -I zoo_asset_closure.py report        # list the closure, sizes and /Script/Zoo classes used
  python -I zoo_asset_closure.py copy          # copy the closure into this project's Content (same paths)
"""
import os
import pathlib
import re
import shutil
import sys

ZOO_CONTENT = pathlib.Path(os.environ.get("ZOO_CONTENT", "../ZooEka2VR/Content"))  # Content folder of the Zoo project
MUSEUM_CONTENT = pathlib.Path(__file__).resolve().parents[2] / "Content"

# Package paths or folders (ending in /) reused by the Museum.
SEEDS = [
    "/Game/ZooVR/",                                   # VR component + game mode Blueprints, VR input, hands
    "/Game/Zoo/Input/",                               # walking + Path Guide input
    "/Game/Zoo/Blueprints/BP_ZooCharacter",
    "/Game/Zoo/Blueprints/BP_ZooPlayerController",
    "/Game/Zoo/Blueprints/BP_Zoo_HUD",
    "/Game/Zoo/Blueprints/BP_ZooGameMode",
    "/Game/Zoo/Blueprints/Widgets/PathGuide/",        # destination list, Navigasi button, toast, pin
    "/Game/Zoo/Blueprints/Widgets/Fonts/",
    "/Game/Zoo/PathGuide/Data/DA_PG_Config",
    "/Game/Zoo/PathGuide/Data/DA_PG_UIConfig",
    "/Game/Zoo/PathGuide/Materials/",
    "/Game/Zoo/PathGuide/UI/Sounds/",
    "/Game/Zoo/PathGuide/UI/Textures/",
    "/Game/Zoo/Materials/UI/",
    "/Game/Zoo/Assets/Hologram/",                     # M_ZooHologram, halo, functions, effects
    "/Game/Zoo/Assets/Materials/M_ARWidget_OnTop",
    "/Game/Zoo/Assets/Narrative/Widgets/WBP_Zoo_NarrativeOverlay",
]

# Packages that must not be pulled in (Zoo-only content); a hit is reported, not copied.
# The animal info widgets (WBP_AR_*), the Zoo's places, place cards and thumbnails are replaced by museum ones.
EXCLUDE = re.compile(r"/Game/Zoo/(Maps|Cinematics|Assets/Animals|Assets/Duma|Blueprints/Animal|Blueprints/Duma|Blueprints/DataTable|Blueprints/Struct"
                     r"|Blueprints/Widgets/WBP_AR|PathGuide/Data/DA_PG_Places|PathGuide/Data/DA_ZooPlaceCards|PathGuide/UI/Thumbnails)"
                     r"|/Game/(Cine|CityPark|Marketplace|TripoModels|Assets)/")

PACKAGE_RE = re.compile(rb"/Game/[A-Za-z0-9_/\-]+")
SCRIPT_RE = re.compile(rb"/Script/Zoo\.[A-Za-z0-9_]+|\bZoo[A-Z][A-Za-z0-9_]+")


def package_file(package):
    rel = package[len("/Game/"):]
    for ext in (".uasset", ".umap"):
        f = ZOO_CONTENT / (rel + ext)
        if f.exists():
            return f
    return None


def seed_packages():
    out = set()
    for seed in SEEDS:
        if seed.endswith("/"):
            folder = ZOO_CONTENT / seed[len("/Game/"):]
            for f in folder.rglob("*.uasset"):
                out.add("/Game/" + f.relative_to(ZOO_CONTENT).with_suffix("").as_posix())
        else:
            out.add(seed)
    return out


def closure():
    todo = list(seed_packages())
    seen, excluded, missing, scripts = set(), {}, set(), set()
    while todo:
        pkg = todo.pop()
        if pkg in seen:
            continue
        f = package_file(pkg)
        if not f:
            missing.add(pkg)
            continue
        seen.add(pkg)
        data = f.read_bytes()
        scripts.update(m.decode() for m in SCRIPT_RE.findall(data))
        for m in PACKAGE_RE.findall(data):
            ref = m.decode().split(".")[0]
            if ref == pkg or ref in seen:
                continue
            if EXCLUDE.search(ref):
                excluded.setdefault(ref, set()).add(pkg)
                continue
            if package_file(ref):
                todo.append(ref)
    return seen, excluded, missing, scripts


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "report"
    seen, excluded, missing, scripts = closure()
    total = sum(package_file(p).stat().st_size for p in seen)
    if mode == "report":
        for p in sorted(seen):
            print(p)
        print(f"\n{len(seen)} packages, {total / 1e6:.1f} MB")
        print("\nExcluded references (Zoo-only, not copied):")
        for ref, by in sorted(excluded.items()):
            print(f"  {ref}  <- {', '.join(sorted(by))}")
        print("\nUnresolved names (engine/plugin or not a package):", len(missing))
        print("\nZoo classes referenced:", " ".join(sorted(s for s in scripts if s.startswith("Zoo") or s.startswith("/Script"))))
    elif mode == "copy":
        for p in sorted(seen):
            src = package_file(p)
            dst = MUSEUM_CONTENT / src.relative_to(ZOO_CONTENT)
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dst)
        print(f"copied {len(seen)} packages, {total / 1e6:.1f} MB")


if __name__ == "__main__":
    main()
