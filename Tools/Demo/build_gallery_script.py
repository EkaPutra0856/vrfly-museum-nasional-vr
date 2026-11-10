# Writes Content/MuseumVR/Demo/MuseumVRGalleryScript.json: the recorded tour of the 3D gallery L_MuseumGallery
# (Tools/Editor/build_gallery_level.py), exhibits 01-06 in order. Per exhibit: choose it (odd exhibits from the destination menu
# "Mau ke mana?", even ones on the tabletop tour map: press the stop, "Ya, pandu saya", the route draws itself; at exhibit 02 the
# map is also spun and tilted first), follow the dotted floor path to the pin, step onto the viewing spot (StationNN), watch the
# detection and the hologram, then the cards: card 1 open and heard, card 2 above, card 3 (below the object or in the side column), the
# video card (enlarged to the big screen at 01 and 04), ID/EN at 04; then end the visit. At the end the map shows 6/6.
# Usage: python Tools/Demo/build_gallery_script.py, then (editor closed)
#   set MUSEUM_MAP=/Game/Museum/Maps/L_MuseumGallery
#   Tools\Demo\run_demo.cmd Content\MuseumVR\Demo\MuseumVRGalleryScript.json
#   python Tools\Demo\make_mp4.py --out MuseumVR_Gallery_Demo.mp4
import json
import os

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = os.path.join(ROOT, "Content", "MuseumVR", "Demo", "MuseumVRGalleryScript.json")
STAGING = os.path.join(ROOT, "Saved", "ImportStaging", "exhibits.json")

# Exhibits whose hologram stands tall: card 3 sits in the side column, not below (Tools/Editor/build_gallery_level.py card_slots).
# Every card is opened: the visit (and the tour's n/6) counts once all three information cards were heard.
TALL = {1, 2, 3, 6}
# A rope runs below the hologram: the video card is in the column, look less far down
ROPE = {1, 2, 3}
SPIN_DEMO = 2
LANGUAGE_DEMO = 4
SCREEN_DEMO = {1, 4}


def marker(name):
    return {"Type": "marker", "Name": name}


def wait(seconds):
    return {"Type": "wait", "Seconds": seconds}


def rest():
    return {"Type": "rest"}


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


def aim(target, seconds=1.0):
    return {"Type": "aim", "Target": target, "Seconds": seconds, "bHead": False}


def click(target, seconds=1.2):
    # The hand points, the head stays
    return [{"Type": "click", "Target": target if ":" in target else "ui:" + target, "Seconds": seconds, "bHead": False}, rest()]


def drag(yaw, pitch, seconds=1.4):
    # Hold the trigger on the map's tray and sweep: spins / tilts the table
    return {"Type": "drag", "Target": "mapstop:0", "Seconds": seconds, "bHead": False, "DragYaw": yaw, "DragPitch": pitch}


def choose_from_menu(number, tag):
    steps = [marker(f"{tag}_menu"), *click("NavigateButton"), until("menu_open", 5, 1.2)]
    if number == 1:
        steps += [aim("ui:Place2"), aim("ui:Place3")]
    steps += [*click(f"Place{number}"), until("guiding", 6, 1.5)]
    return steps


def choose_on_map(number, tag):
    steps = [marker(f"{tag}_peta"), *click("MinimapButton"), wait(1.2), look("mapstop:0", 1.2, 4.0), wait(0.8)]
    if number == SPIN_DEMO:
        steps += [marker(f"{tag}_peta_putar"), drag(20, 0), wait(0.6), drag(-14, 7), wait(0.6), drag(-6, -7), wait(0.6)]
    steps += [
        aim(f"mapstop:{number}"), wait(0.8),
        *click(f"mapstop:{number}"), wait(1.2),
        marker(f"{tag}_peta_konfirmasi"), wait(0.6),
        *click("MapConfirmYesButton"),
        marker(f"{tag}_peta_rute"),
        until("guiding", 6),
        # The route draws itself on the map, then the map goes beside the view
        wait(2.8),
    ]
    return steps


def exhibit_steps(number, short):
    label, gaze, station = f"Exhibit{number:02d}", f"actor:Gaze{number:02d}", f"actor:Station{number:02d}"
    tag = f"{number:02d}_{short.lower()}"
    steps = choose_from_menu(number, tag) if number % 2 else choose_on_map(number, tag)
    steps += [
        rest(),
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
        # Card 1 is open and heard; read a little, scroll it
        marker(f"{tag}_kartu_1"),
        look("ui:CardBody", 1.4),
        wait(4.0),
        {"Type": "scroll", "Target": "scrollof:CardBody", "Notches": -4, "Timeout": 8, "bHead": False},
        wait(2.0),
    ]
    if number == 1:
        steps += [until(f"voice_done:{label}", 60, 0.8)]
    # Card 2 above the object
    steps += [look(gaze, 0.8), *click("HeaderButton1"), look("ui:CardBody", 1.0, -45.0), wait(1.0), marker(f"{tag}_kartu_2"), wait(3.0)]
    # Card 3 below the object, or in the side column
    steps += [look(gaze, 0.8), *click("HeaderButton2"), look("ui:CardBody", 1.0, 0.0 if number in TALL else 25.0), wait(1.0),
              marker(f"{tag}_kartu_3"), wait(3.0)]
    if number == LANGUAGE_DEMO:
        steps += [*click("LanguageButton"), wait(1.2), marker(f"{tag}_bahasa_en"), wait(2.5), *click("LanguageButton"), wait(1.0)]
    # The video card; at some, the big screen
    steps += [look(gaze, 0.8), *click("HeaderButton3"), look("ui:VideoPlayButton", 1.0, 20.0 if number in ROPE else 60.0), wait(1.0),
              marker(f"{tag}_video"), wait(2.5)]
    if number in SCREEN_DEMO:
        steps += [look(gaze, 0.8), *click("VideoEnlargeButton"), wait(1.5), marker(f"{tag}_layar_besar"), wait(4.0),
                  *click("TheaterCloseButton"), wait(1.2)]
    steps += [
        rest(),
        look(f"exhibit:{label}", 2.0),
        marker(f"{tag}_selesai"),
        {"Type": "end_visit", "Target": label},
        until(f"exhibit_hidden:{label}", 6, 0.5),
    ]
    return steps


def main():
    with open(STAGING, encoding="utf-8") as f:
        exhibits = json.load(f)
    exhibits = exhibits if isinstance(exhibits, list) else exhibits["exhibits"]
    shorts = {exhibit["number"]: exhibit["short"] for exhibit in exhibits}

    steps = [
        marker("mulai"),
        {"Type": "hands", "Left": "relaxed", "Right": "relaxed"},
        {"Type": "look", "Yaw": 0, "Pitch": 0, "Seconds": 1.5},
        {"Type": "look", "Yaw": 25, "Pitch": 4, "Seconds": 1.8},
        {"Type": "look", "Yaw": -20, "Pitch": 2, "Seconds": 2.0},
        {"Type": "look", "Yaw": 0, "Pitch": 0, "Seconds": 1.2},
    ]
    for number in range(1, 7):
        steps += exhibit_steps(number, shorts[number])
    # The map once more: all six visited
    steps += [
        marker("peta_akhir"), *click("MinimapButton"), wait(1.2), look("mapstop:0", 1.2, 4.0), wait(3.0),
        *click("MapMinimizeButton"), wait(1.0),
        marker("selesai"), {"Type": "look", "Yaw": 0, "Pitch": 0, "Seconds": 2.0}, wait(2.0), {"Type": "end"},
    ]

    script = {"StartDelay": 1.5, "EyeHeight": 165.0, "Steps": steps}
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as f:
        json.dump(script, f, ensure_ascii=False, indent=1)
    print(f"{OUT}: {len(steps)} steps")


if __name__ == "__main__":
    main()
