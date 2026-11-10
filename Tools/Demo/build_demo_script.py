# Writes Content/MuseumVR/Demo/MuseumVRDemoScript.json: the recorded tour of L_MuseumDemo (Tools/Editor/build_demo_level.py),
# exhibits 01-06 in order, along Docs/STORYBOARD.md's 8 frames. Per exhibit: NAVIGASI on the HUD bar, pick the place in the
# 3D menu (button Place<N>), follow the floor chevrons, step onto the viewing spot (StationNN; the zone there starts the
# detection), watch the hologram build up, open the 3 cards (the first heard to the end, the others read and scrolled), then
# end the visit. Exhibit 03 also shows the ID/EN toggle; after exhibit 01 the tour map opens in front and folds back.
# Usage: python Tools/Demo/build_demo_script.py
import json
import os

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = os.path.join(ROOT, "Content", "MuseumVR", "Demo", "MuseumVRDemoScript.json")
STAGING = os.path.join(ROOT, "Saved", "ImportStaging", "exhibits.json")

LANGUAGE_DEMO = 3
MAP_DEMO = 1


def marker(name):
    return {"Type": "marker", "Name": name}


def wait(seconds):
    return {"Type": "wait", "Seconds": seconds}


def until(cond, timeout, seconds=0.0):
    step = {"Type": "wait_until", "Cond": cond, "Timeout": timeout}
    if seconds:
        step["Seconds"] = seconds
    return step


def look(target, seconds, z=0.0):
    step = {"Type": "look", "Target": target, "Seconds": seconds}
    if z:
        step["Offset"] = {"X": 0, "Y": 0, "Z": z}
    return step


def click(target, seconds=1.3):
    # The hand points, the head stays on the exhibit (bHead false keeps the passthrough plate edge out of view)
    return {"Type": "click", "Target": "ui:" + target, "Seconds": seconds, "bHead": False}


def scroll(target, notches, timeout=8):
    return {"Type": "scroll", "Target": "scrollof:" + target, "Notches": notches, "Timeout": timeout, "bHead": False}


def exhibit_steps(number, short, titles, first):
    label, gaze, station = f"Exhibit{number:02d}", f"actor:Gaze{number:02d}", f"actor:Station{number:02d}"
    tag = f"{number:02d}_{short.lower()}"
    steps = [
        marker(f"{tag}_menu"),
        # The HUD bar sits low in front (in view): the hand presses NAVIGASI, the head stays
        click("NavigateButton"),
        until("menu_open", 5, 1.2),
        {"Type": "rest"},
    ]
    if first:
        steps += [{"Type": "aim", "Target": "ui:Place2", "Seconds": 1.0, "bHead": False},
                  {"Type": "aim", "Target": "ui:Place3", "Seconds": 1.0, "bHead": False}]
    steps += [
        click(f"Place{number}"),
        until("guiding", 6, 1.5),
        {"Type": "rest"},
        marker(f"{tag}_rute"),
        {"Type": "route", "Timeout": 60},
        until("arrived", 5, 0.4),
        # The exact viewing spot (the zone is small), then the head on the exhibit
        {"Type": "walk", "Target": station, "Radius": 25, "Timeout": 10},
        look(gaze, 1.6),
        until(f"exhibit_started:{label}", 15),
        marker(f"{tag}_deteksi_hologram"),
        look(f"real:{label}", 2.2),
        look(gaze, 1.6),
        until(f"exhibit_panel:{label}", 25, 1.0),
        marker(f"{tag}_kartu"),
        look(f"panels:{label}", 1.6),
    ]

    # Card 1 heard to the end (its collapsed card is a button with its title), cards 2 and 3 read (their tabs)
    steps += [marker(f"{tag}_kartu_1"), click(titles[0][0]), wait(2.5)]
    if number == LANGUAGE_DEMO:
        steps += [marker(f"{tag}_bahasa_en"), click("LanguageButton"), wait(2.5)]
    steps += [scroll("CardBody", -4), wait(1.5), until(f"voice_done:{label}", 90, 0.8)]
    for index in (1, 2):
        steps += [marker(f"{tag}_kartu_{index + 1}"), click(titles[1][index]), wait(3.0), scroll("CardBody", -5), wait(3.0)]
    if number == LANGUAGE_DEMO:
        steps += [marker(f"{tag}_bahasa_id"), click("LanguageButton"), wait(1.5)]

    steps += [
        {"Type": "rest"},
        look(f"exhibit:{label}", 2.0),
        marker(f"{tag}_selesai"),
        {"Type": "end_visit", "Target": label},
        until(f"exhibit_hidden:{label}", 6, 0.5),
    ]
    if number == MAP_DEMO:
        # The tour map in front like a tabletop ("1/6 dikunjungi"), then folded back to the side
        steps += [
            marker(f"{tag}_peta"),
            click("MinimapButton"),
            wait(1.0),
            look("ui:MapMinimizeButton", 1.4, -12.0),
            wait(3.5),
            click("MapMinimizeButton"),
            wait(1.0),
            {"Type": "rest"},
        ]
    return steps


def main():
    with open(STAGING, encoding="utf-8") as f:
        exhibits = json.load(f)
    exhibits = exhibits if isinstance(exhibits, list) else exhibits["exhibits"]

    steps = [
        marker("mulai_lobi"),
        {"Type": "hands", "Left": "relaxed", "Right": "relaxed"},
        {"Type": "look", "Yaw": 0, "Pitch": 0, "Seconds": 1.5},
        {"Type": "look", "Yaw": 25, "Pitch": 4, "Seconds": 1.8},
        {"Type": "look", "Yaw": -20, "Pitch": 2, "Seconds": 2.0},
        {"Type": "look", "Yaw": 0, "Pitch": 0, "Seconds": 1.2},
    ]
    for i, exhibit in enumerate(sorted(exhibits, key=lambda e: e["number"])):
        # Card 1 is opened in Indonesian; the language demo switches to English before cards 2 and 3
        titles = [panel["title"] for panel in exhibit["panels"]["ID"]]
        later = titles
        if exhibit["number"] == LANGUAGE_DEMO:
            later = [panel["title"] for panel in exhibit["panels"]["EN"]]
        steps += exhibit_steps(exhibit["number"], exhibit["short"], (titles, later), i == 0)
    steps += [marker("selesai"), {"Type": "look", "Yaw": 0, "Pitch": 0, "Seconds": 2.0}, wait(2.0), {"Type": "end"}]

    script = {"StartDelay": 1.5, "EyeHeight": 165.0, "Steps": steps}
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as f:
        json.dump(script, f, ensure_ascii=False, indent=1)
    print(f"{OUT}: {len(steps)} steps")


if __name__ == "__main__":
    main()
