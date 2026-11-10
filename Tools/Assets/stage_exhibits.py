"""Stage the 6 exhibits' sources for the UE import (Tools/Editor/import_exhibits.py).

Source files in Hasil_Model_3D are never modified. Into UnrealProj/Saved/ImportStaging this writes:
  - Audio/<nn>/<ID|EN>/*.wav   the voice-over mp3s as 16-bit PCM wav (UE imports wav, not mp3),
  - Thumbs/T_<nn>_<Name>.jpg   Gambar_Pilihan_* scaled to 1024 px wide (destination card, MEDIA card),
  - exhibits.json              per exhibit: GLB path, name/subtitle, the 3 panels (ID + EN) and the staged files.

Text sources:
  - Keterangan_Objek.txt line 1  "JUDUL OBJEK: <Name> (<Subtitle>)"  (Indonesian only),
  - Skrip_AR_Voiceover.txt       "--- [BAHASA INDONESIA] ---" then "--- [ENGLISH] ---", each with 3 blocks
                                 "[PANEL n: HEADING]" / "Judul Panel:"|"Panel Title:" / "Voiceover:" + quoted text.
English name/subtitle are not in the sources: NAMES_EN below (translated here, review with the museum).

Usage: python -I stage_exhibits.py [--force]
"""
import json
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]  # ...\Museum Nasional Indonesia
SRC = ROOT / "Hasil_Model_3D"
PROJECT = ROOT / "UnrealProj"
DST = PROJECT / "Saved" / "ImportStaging"

# (number, short name = asset/tag suffix, source folder prefix)
EXHIBITS = [
    ("01", "Dwarapala", "01_"),
    ("02", "Nandiswara", "02_"),
    ("03", "PrasastiGajahMada", "03_"),
    ("04", "ReliefArjunawiwaha", "04_"),
    ("05", "WayangGolek", "05_"),
    ("06", "MaketMasjidAtThohir", "06_"),
]

NAMES_EN = {
    "01": ("Dwarapala Gate Guardian Statue", "East Java, 14th century CE"),
    "02": ("Nandiswara Statue", "Guardian of the Main Chamber of a Shiva Temple"),
    "03": ("Gajah Mada Inscription", "Singhasari Inscription, 1351 CE"),
    "04": ("Arjunawiwaha Water Spout Relief", "Kediri, 14th century CE"),
    "05": ("Wayang Golek Menak: Wali Songo Figure", "A Sunan character"),
    "06": ("At-Thohir Mosque Architectural Model", "Depok"),
}

# Staged name -> source file, per language
AUDIO = {
    "ID": ("Bahasa_Indonesia", ["01_Judul", "02_Panel_1", "03_Panel_2", "04_Panel_3"]),
    "EN": ("English", ["01_Title", "02_Panel_1", "03_Panel_2", "04_Panel_3"]),
}

PANEL_RE = re.compile(r"\[PANEL\s*(\d)\s*:\s*(.+?)\]\s*\n\s*(?:Judul Panel|Panel Title)\s*:\s*(.+?)\s*\n\s*Voiceover\s*:\s*\n\s*\"(.+?)\"", re.S)


def read(path):
    return path.read_text(encoding="utf-8-sig")


def parse_panels(script):
    """Returns {"ID": [3 panels], "EN": [3 panels]} with heading, title, body."""
    parts = re.split(r"---\s*\[ENGLISH\]\s*---", script)
    if len(parts) != 2:
        raise ValueError("no '--- [ENGLISH] ---' separator")
    out = {}
    for lang, text in zip(("ID", "EN"), parts):
        panels = [{"heading": h.strip(), "title": t.strip(), "body": " ".join(b.split())}
                  for _, h, t, b in sorted(PANEL_RE.findall(text), key=lambda m: m[0])]
        if len(panels) != 3:
            raise ValueError(f"{lang}: {len(panels)} panels, expected 3")
        out[lang] = panels
    return out


def parse_title(description):
    line = description.splitlines()[0]
    m = re.match(r"JUDUL OBJEK:\s*(.+?)\s*\((.+)\)\s*$", line)
    return (m.group(1), m.group(2)) if m else (line.split(":", 1)[-1].strip(), "")


def ffmpeg(args, out, force):
    if out.exists() and not force:
        return
    out.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(["ffmpeg", "-y", "-v", "error", *args, str(out)], check=True)


def main():
    force = "--force" in sys.argv
    exhibits = []
    for num, short, prefix in EXHIBITS:
        folder = next(SRC.glob(prefix + "*"))
        name_id, subtitle_id = parse_title(read(folder / "Keterangan_Objek.txt"))
        panels = parse_panels(read(folder / "Skrip_AR_Voiceover.txt"))

        audio = {}
        for lang, (sub, names) in AUDIO.items():
            staged = []
            for name in names:
                wav = DST / "Audio" / num / lang / (name + ".wav")
                ffmpeg(["-i", str(folder / "Audio" / sub / (name + ".mp3")), "-c:a", "pcm_s16le"], wav, force)
                staged.append(str(wav))
            audio[lang] = staged

        thumb = DST / "Thumbs" / f"T_{num}_{short}.jpg"
        ffmpeg(["-i", str(next(folder.glob("Gambar_Pilihan_*.jpg"))), "-vf", "scale=1024:-2", "-q:v", "2"], thumb, force)

        name_en, subtitle_en = NAMES_EN[num]
        exhibits.append({
            "number": int(num), "short": short, "folder": folder.name,
            "glb": str(next(folder.glob("Model_3D_*.glb"))),
            "name": {"ID": name_id, "EN": name_en}, "subtitle": {"ID": subtitle_id, "EN": subtitle_en},
            "panels": panels, "audio": audio, "thumbnail": str(thumb),
            "video": str(PROJECT / "Content" / "Movies" / "Exhibits" / f"Exhibit{num}_{short}.mp4"),
        })
        print(f"{num} {short}: {name_id} ({subtitle_id}) | " + " / ".join(p["title"] for p in panels["ID"]))

    DST.mkdir(parents=True, exist_ok=True)
    (DST / "exhibits.json").write_text(json.dumps(exhibits, ensure_ascii=False, indent=1), encoding="utf-8")
    print(f"wrote {DST / 'exhibits.json'}")


if __name__ == "__main__":
    main()
