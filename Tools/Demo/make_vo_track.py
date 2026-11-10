# Builds the guide's voice-over track of a recorded demo and muxes it into the MP4.
# The exhibits log every clip they start / stop ("VOCUE play <world s> <SoundWave>", "VOCUE stop <world s>") into
# Saved/DemoVideo/demo_run.log; markers.txt ties world time to video frames. Each clip is the staging WAV it was imported from
# (Saved/ImportStaging/exhibits.json), placed at its frame and cut where the guide stopped it or the next clip began.
# Usage: python Tools/Demo/make_vo_track.py [--video MuseumVR_Gallery_Demo.mp4]
import argparse
import json
import os
import re
import subprocess
import wave

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEMO = os.path.join(ROOT, "Saved", "DemoVideo")
STAGING = os.path.join(ROOT, "Saved", "ImportStaging", "exhibits.json")
FFMPEG = os.environ.get("FFMPEG", "ffmpeg")  # ffmpeg path; defaults to the one on PATH
FPS = 30
FADE = 0.08

ap = argparse.ArgumentParser()
ap.add_argument("--video", default="MuseumVR_Gallery_Demo.mp4")
ap.add_argument("--log", default=os.path.join(DEMO, "demo_run.log"))
a = ap.parse_args()

# World time -> video seconds, fitted on the markers (frame, video s, world s, name)
pairs = []
for row in open(os.path.join(DEMO, "markers.txt"), encoding="utf-8"):
    p = row.rstrip("\n").split("\t")
    if len(p) >= 4:
        pairs.append((float(p[2]), int(p[0]) / FPS))
n = len(pairs)
mx = sum(w for w, _ in pairs) / n
my = sum(v for _, v in pairs) / n
slope = sum((w - mx) * (v - my) for w, v in pairs) / sum((w - mx) ** 2 for w, _ in pairs)
offset = my - slope * mx
to_video = lambda world: slope * world + offset
print(f"video = {slope:.5f} * world + {offset:.3f}  ({n} markers)")

# SW_<NN>_<LANG>_<Title|Judul|PanelK> -> the staging WAV (audio.<LANG>[0] title, [K] card K)
with open(STAGING, encoding="utf-8") as f:
    exhibits = json.load(f)
exhibits = exhibits if isinstance(exhibits, list) else exhibits["exhibits"]
audio = {e["number"]: e["audio"] for e in exhibits}


def wav_of(sound):
    m = re.match(r"SW_(\d+)_([A-Z]+)_(Title|Judul|Panel(\d+))$", sound)
    if not m:
        return None
    index = int(m.group(4)) if m.group(4) else 0
    return audio[int(m.group(1))][m.group(2)][index]


def duration(path):
    with wave.open(path, "rb") as w:
        return w.getnframes() / w.getframerate()


# Clips: from each play to the next play / stop / the clip's own end
cue = re.compile(r"VOCUE (play|stop) ([\d.]+) ?(\S*)")
events = [(m.group(1), to_video(float(m.group(2))), m.group(3)) for m in map(cue.search, open(a.log, encoding="utf-8", errors="replace")) if m]
clips = []
for kind, at, sound in events:
    if clips and clips[-1]["end"] is None:
        clips[-1]["end"] = at
    if kind == "play":
        path = wav_of(sound)
        if path:
            clips.append({"start": at, "end": None, "path": path, "sound": sound})
for clip in clips:
    full = duration(clip["path"])
    clip["length"] = min(full, (clip["end"] or 1e9) - clip["start"])
clips = [c for c in clips if c["length"] > 0.15]
for c in clips:
    print(f"{int(c['start'] // 60)}:{c['start'] % 60:05.2f}  {c['length']:6.2f}s  {c['sound']}")

# Mix (48 kHz stereo), pad to the video's length, mux without re-encoding the picture
video = os.path.join(DEMO, a.video)
frames = len([f for f in os.listdir(os.path.join(DEMO, "Frames")) if f.endswith(".jpg")])
total = frames / FPS
track = os.path.join(DEMO, "vo_track.wav")
cmd = [FFMPEG, "-y"]
for c in clips:
    cmd += ["-i", c["path"]]
parts = []
for i, c in enumerate(clips):
    delay = int(round(c["start"] * 1000))
    fade = max(c["length"] - FADE, 0)
    parts.append(f"[{i}:a]aformat=sample_rates=48000:channel_layouts=stereo,atrim=0:{c['length']:.3f},"
                 f"afade=t=out:st={fade:.3f}:d={FADE},adelay={delay}|{delay}[a{i}]")
parts.append("".join(f"[a{i}]" for i in range(len(clips))) + f"amix=inputs={len(clips)}:normalize=0,apad,atrim=0:{total:.3f}[out]")
cmd += ["-filter_complex", ";".join(parts), "-map", "[out]", "-c:a", "pcm_s16le", track]
subprocess.check_call(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

muxed = video[:-4] + "_vo.mp4"
subprocess.check_call([FFMPEG, "-y", "-i", video, "-i", track, "-map", "0:v", "-map", "1:a", "-c:v", "copy", "-c:a", "aac", "-b:a", "192k",
                       "-shortest", "-movflags", "+faststart", muxed], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
os.replace(muxed, video)
print(f"{len(clips)} clips -> {video}")
