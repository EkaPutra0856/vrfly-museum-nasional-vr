"""Prepare the 6 exhibit recordings for UE Media Player (demo mode).

Source files in Hasil_Model_3D are never modified. For each exhibit this:
  - bakes the phone rotation metadata into the pixels (ffmpeg autorotate),
  - converts variable frame rate to constant 30 fps,
  - re-encodes H.264 High, yuv420p, keyframe every 1 s (cheap seeking), no audio.
Output keeps the native aspect; all six are landscape 1920x1080 because 03 and 05
use the landscape takes from Camera/Video (owner decision 2026-10-26: the takes in
Hasil_Model_3D for those two are portrait). Framing into the Quest POV is done in UE,
so camera tracking runs on the full frame.

Usage: python -I prep_videos.py [--force]
"""
import json
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]  # ...\Museum Nasional Indonesia
SRC = ROOT / "Hasil_Model_3D"
CAMERA_VIDEO = ROOT / "Camera" / "Video"
DST = ROOT / "UnrealProj" / "Content" / "Movies" / "Exhibits"

# (number, output name, source override in Camera/Video or None = Video_Referensi_* in the exhibit folder)
EXHIBITS = [
    ("01", "Dwarapala", None),
    ("02", "Nandiswara", None),
    ("03", "PrasastiGajahMada", "Prasasti_Gajah_Mada_1351M_SidePan_*.mp4"),
    ("04", "ReliefArjunawiwaha", None),
    ("05", "WayangGolek", "Wayang_Golek_Wali_Songo_Showcase_*.mp4"),
    ("06", "MaketMasjidAtThohir", None),
]


def probe(path):
    out = subprocess.run(
        ["ffprobe", "-v", "error", "-select_streams", "v:0",
         "-show_entries", "stream=width,height,avg_frame_rate:format=duration",
         "-of", "json", str(path)],
        check=True, capture_output=True, text=True).stdout
    return json.loads(out)


def main():
    force = "--force" in sys.argv
    DST.mkdir(parents=True, exist_ok=True)
    for num, name, override in EXHIBITS:
        if override:
            src = next(CAMERA_VIDEO.glob(override))
        else:
            folder = next(SRC.glob(f"{num}_*"))
            src = next(folder.glob("Video_Referensi_*.mp4"))
        dst = DST / f"Exhibit{num}_{name}.mp4"
        if dst.exists() and not force:
            print(f"skip {dst.name} (exists)")
            continue
        subprocess.run(
            ["ffmpeg", "-y", "-v", "error", "-i", str(src),
             "-vf", "fps=30,format=yuv420p",
             "-fps_mode", "cfr", "-an",
             "-c:v", "libx264", "-preset", "slow", "-crf", "17",
             "-profile:v", "high", "-g", "30", "-keyint_min", "30", "-sc_threshold", "0",
             "-movflags", "+faststart", str(dst)],
            check=True)
        s = probe(dst)
        st = s["streams"][0]
        print(f"{dst.name} <- {src.name}: {st['width']}x{st['height']} {st['avg_frame_rate']} "
              f"{float(s['format']['duration']):.2f}s")


if __name__ == "__main__":
    main()
