"""Make the passthrough plates of the demo level from the prepared recordings (prep_videos.py output).

Station plates (Content/Movies/Plates/PlateNN_<Name>.mp4): the exhibit recording locked on the real object, so the object
stays still in the frame while the phone moves. That replaces 3D camera tracking: the scan brackets, the hologram twin and
the panel are placed once, in the frame's coordinates, and stay on the object. Per recording:
  1. track the object from a reference frame forward and backward: features inside its box (LK optical flow, checked
     both ways), a similarity transform per frame step (RANSAC), chained, lightly smoothed;
  2. undo that motion and zoom about the object just enough that no frame shows an empty border (capped at MAX_ZOOM;
     past the cap the edge pixels are stretched, the plate's vignette hides them);
  3. write it forward then backward, a seamless loop for a visit of any length.
Where the object ends up in the frame (normalized box) goes to Tools/Video/plates.json; the level builder places the
real-object box, the hologram and the panel from it.

Walk plates (Content/Movies/Walk/WalkNN_<Name>.mp4): gallery walks from Camera/Video, rotation baked, 30 fps, trimmed
to the steady part; played behind the visitor while guided between exhibits.

Sources are never modified. Needs opencv-python, numpy, scipy, ffmpeg.
Usage: python stabilize_plates.py [--only 01,05] [--preview]
"""
import json
import pathlib
import subprocess
import sys

import cv2
import numpy as np
from scipy.ndimage import gaussian_filter1d

ROOT = pathlib.Path(__file__).resolve().parents[3]  # ...\Museum Nasional Indonesia
PROJECT = ROOT / "UnrealProj"
SRC = PROJECT / "Content" / "Movies" / "Exhibits"
PLATES = PROJECT / "Content" / "Movies" / "Plates"
WALK = PROJECT / "Content" / "Movies" / "Walk"
CAMERA_VIDEO = ROOT / "Camera" / "Video"
LAYOUT = pathlib.Path(__file__).with_name("plates.json")
PREVIEW = PROJECT / "Saved" / "PlatePreview"

FPS = 30
MAX_ZOOM = 1.5
SMOOTH_FRAMES = 2.0   # gaussian sigma of the tracked motion (jitter only)
TRACK_SCALE = 0.5     # tracking runs on half-size grey frames

# (number, name, start s, end s, reference s, object box at the reference frame: u0, v0, u1, v1 normalized)
# Boxes read from gridded reference frames (2026-10-26). 01 and 04 skip the approach (object cut off / pillars); 05 keeps the
# showcase before the pan and the visitors walking in front, and its object is the figure the GLB models (3rd from the left:
# brown jacket, teal robe, checked turban); 06's object is the mosque on the maket, not the whole maket table.
EXHIBITS = [
    ("01", "Dwarapala", 1.5, None, 7.3, (0.400, 0.185, 0.535, 0.665)),
    ("02", "Nandiswara", 0.0, None, 5.0, (0.360, 0.100, 0.570, 0.790)),
    ("03", "PrasastiGajahMada", 0.0, None, 5.2, (0.355, 0.080, 0.585, 0.975)),
    ("04", "ReliefArjunawiwaha", 2.6, None, 8.3, (0.383, 0.225, 0.717, 0.765)),
    ("05", "WayangGolek", 9.9, 14.6, 12.4, (0.399, 0.352, 0.461, 0.667)),
    ("06", "MaketMasjidAtThohir", 0.0, None, 5.6, (0.460, 0.325, 0.645, 0.510)),
]

# (number, name, source glob in Camera/Video, start s, end s)
WALKS = [
    ("01", "PameranJalurRempah", "Pameran_Jalur_Rempah_Nusantara_*.mp4", 0.0, 9.2),
    ("02", "GaleriPrasasti", "Galeri_Prasasti_Kuno_Nusantara_*.mp4", 0.0, 19.3),
    ("03", "PaviliunKitab", "Paviliun_Kitab_Naskah_Kuno_*.mp4", 1.2, 10.2),
]


def read_frames(path, start, end):
    """Yields the frames in [start, end] (full size; read twice instead of held, 1080p x 600 frames would be 3.7 GB)."""
    cap = cv2.VideoCapture(str(path))
    cap.set(cv2.CAP_PROP_POS_MSEC, start * 1000.0)
    while True:
        ok, frame = cap.read()
        if not ok or (end is not None and cap.get(cv2.CAP_PROP_POS_MSEC) / 1000.0 > end):
            break
        yield frame
    cap.release()


def similarity(src, dst):
    """2x3 similarity from point pairs (RANSAC); None when it cannot be trusted."""
    if len(src) < 12:
        return None
    m, inliers = cv2.estimateAffinePartial2D(src, dst, method=cv2.RANSAC, ransacReprojThreshold=1.5, maxIters=2000, confidence=0.99)
    if m is None or inliers is None or inliers.sum() < 10:
        return None
    return m


def to3(m):
    return np.vstack([m, [0.0, 0.0, 1.0]])


def box_mask(shape, box, shrink=0.06):
    x0, y0, x1, y1 = box
    dx, dy = (x1 - x0) * shrink, (y1 - y0) * shrink
    mask = np.zeros(shape, np.uint8)
    h, w = shape
    mask[max(0, int(y0 + dy)):min(h, int(y1 - dy)), max(0, int(x0 + dx)):min(w, int(x1 - dx))] = 255
    return mask


def transform_box(m3, box):
    x0, y0, x1, y1 = box
    corners = np.array([[x0, y0, 1], [x1, y0, 1], [x1, y1, 1], [x0, y1, 1]], float).T
    p = (m3 @ corners)[:2]
    return p[0].min(), p[1].min(), p[0].max(), p[1].max()


def track(grey, ref, box):
    """A[t]: 3x3 map from reference-frame pixels to frame t pixels (tracking resolution)."""
    n = len(grey)
    a = [None] * n
    a[ref] = np.eye(3)
    lk = dict(winSize=(21, 21), maxLevel=3, criteria=(cv2.TERM_CRITERIA_EPS | cv2.TERM_CRITERIA_COUNT, 30, 0.01))
    lost = 0
    for step in (1, -1):
        t = ref
        while 0 <= t + step < n:
            current_box = transform_box(a[t], box)
            pts = cv2.goodFeaturesToTrack(grey[t], maxCorners=500, qualityLevel=0.01, minDistance=6, mask=box_mask(grey[t].shape, current_box))
            m = None
            if pts is not None:
                nxt, st, _ = cv2.calcOpticalFlowPyrLK(grey[t], grey[t + step], pts, None, **lk)
                back, st2, _ = cv2.calcOpticalFlowPyrLK(grey[t + step], grey[t], nxt, None, **lk)
                good = (st.ravel() == 1) & (st2.ravel() == 1) & (np.linalg.norm((back - pts).reshape(-1, 2), axis=1) < 1.0)
                m = similarity(pts[good], nxt[good])
            if m is None:
                lost += 1
                m = np.array([[1.0, 0, 0], [0, 1.0, 0]])
            a[t + step] = to3(m) @ a[t]
            t += step
    return a, lost


def smooth(a):
    """Gaussian-smoothed similarity parameters (log scale, angle, translation)."""
    p = np.array([[np.log(np.hypot(m[0, 0], m[1, 0])), np.arctan2(m[1, 0], m[0, 0]), m[0, 2], m[1, 2]] for m in a])
    p[:, 1] = np.unwrap(p[:, 1])
    p = gaussian_filter1d(p, SMOOTH_FRAMES, axis=0, mode="nearest")
    out = []
    for ls, ang, tx, ty in p:
        s = np.exp(ls)
        out.append(np.array([[s * np.cos(ang), -s * np.sin(ang), tx], [s * np.sin(ang), s * np.cos(ang), ty], [0, 0, 1.0]]))
    return out


def zoom_about(c, z):
    return np.array([[z, 0, c[0] * (1 - z)], [0, z, c[1] * (1 - z)], [0, 0, 1.0]])


def covered(warps, w, h):
    """Frames whose warped source fills the whole output."""
    corners = np.array([[0, 0, 1], [w, 0, 1], [w, h, 1], [0, h, 1]], float).T
    count = 0
    for wt in warps:
        p = (np.linalg.inv(wt) @ corners)[:2]
        count += bool((p[0] >= -0.5).all() and (p[0] <= w + 0.5).all() and (p[1] >= -0.5).all() and (p[1] <= h + 0.5).all())
    return count


def encoder(out, w, h):
    out.parent.mkdir(parents=True, exist_ok=True)
    return subprocess.Popen(["ffmpeg", "-y", "-v", "error", "-f", "rawvideo", "-pix_fmt", "bgr24", "-s", f"{w}x{h}", "-r", str(FPS), "-i", "-",
                             "-c:v", "libx264", "-preset", "slow", "-crf", "18", "-pix_fmt", "yuv420p", "-g", str(FPS), "-an",
                             "-movflags", "+faststart", str(out)], stdin=subprocess.PIPE)


def plate(num, name, start, end, ref_time, box_norm, preview):
    src = SRC / f"Exhibit{num}_{name}.mp4"
    grey = []
    for f in read_frames(src, start, end):
        h, w = f.shape[:2]
        grey.append(cv2.cvtColor(cv2.resize(f, None, fx=TRACK_SCALE, fy=TRACK_SCALE, interpolation=cv2.INTER_AREA), cv2.COLOR_BGR2GRAY))
    n = len(grey)
    ref = min(n - 1, max(0, round((ref_time - start) * FPS)))
    gh, gw = grey[0].shape
    box_track = (box_norm[0] * gw, box_norm[1] * gh, box_norm[2] * gw, box_norm[3] * gh)
    a, lost = track(grey, ref, box_track)
    a = smooth(a)

    # Tracking-resolution maps to full resolution
    up = np.diag([1 / TRACK_SCALE, 1 / TRACK_SCALE, 1.0])
    a_full = [up @ m @ np.linalg.inv(up) for m in a]
    box = (box_norm[0] * w, box_norm[1] * h, box_norm[2] * w, box_norm[3] * h)
    centre = ((box[0] + box[2]) / 2, (box[1] + box[3]) / 2)

    # Smallest zoom about the object that fills every frame (or the cap)
    zoom = MAX_ZOOM
    for z in np.arange(1.0, MAX_ZOOM + 1e-6, 0.01):
        if covered([zoom_about(centre, z) @ np.linalg.inv(m) for m in a_full], w, h) == n:
            zoom = float(z)
            break
    warps = [zoom_about(centre, zoom) @ np.linalg.inv(m) for m in a_full]
    filled = covered(warps, w, h)

    out = PLATES / f"Plate{num}_{name}.mp4"
    enc = encoder(out, w, h)
    stabilized = []
    for f, wt in zip(read_frames(src, start, end), warps):
        g = cv2.warpAffine(f, wt[:2], (w, h), flags=cv2.INTER_CUBIC, borderMode=cv2.BORDER_REPLICATE)
        stabilized.append(cv2.imencode(".jpg", g, [cv2.IMWRITE_JPEG_QUALITY, 96])[1])
    for i in list(range(n)) + list(range(n - 2, 0, -1)):  # forward, then back (no repeated end frames)
        enc.stdin.write(cv2.imdecode(stabilized[i], cv2.IMREAD_COLOR).tobytes())
    enc.stdin.close()
    enc.wait()

    final = transform_box(zoom_about(centre, zoom), box)
    final_norm = [round(float(max(0.0, final[0] / w)), 4), round(float(max(0.0, final[1] / h)), 4), round(float(min(1.0, final[2] / w)), 4), round(float(min(1.0, final[3] / h)), 4)]
    if preview:
        PREVIEW.mkdir(parents=True, exist_ok=True)
        picks = [cv2.imdecode(stabilized[i], cv2.IMREAD_COLOR) for i in np.linspace(0, n - 1, 8).astype(int)]
        for p in picks:
            cv2.rectangle(p, (int(final[0]), int(final[1])), (int(final[2]), int(final[3])), (0, 0, 255), 4)
        tiles = [cv2.resize(p, (480, 270)) for p in picks]
        cv2.imwrite(str(PREVIEW / f"Plate{num}_{name}.jpg"), np.vstack([np.hstack(tiles[:4]), np.hstack(tiles[4:])]))
    print(f"{num} {name}: {n} frames, ref {ref}, lost steps {lost}, zoom {zoom:.2f}, filled {filled}/{n}, box {final_norm}")
    return {"number": int(num), "short": name, "file": f"Movies/Plates/{out.name}", "box": final_norm, "zoom": round(zoom, 3),
            "filled_frames": filled, "frames": n, "loop_seconds": round((2 * n - 2) / FPS, 2), "lost_steps": lost}


def walk(num, name, pattern, start, end):
    src = next(CAMERA_VIDEO.glob(pattern))
    out = WALK / f"Walk{num}_{name}.mp4"
    out.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(["ffmpeg", "-y", "-v", "error", "-ss", str(start), "-to", str(end), "-i", str(src),
                    "-vf", f"scale=1920:1080:force_original_aspect_ratio=increase,crop=1920:1080,fps={FPS},format=yuv420p",
                    "-c:v", "libx264", "-preset", "slow", "-crf", "18", "-g", str(FPS), "-an", "-movflags", "+faststart", str(out)], check=True)
    print(f"walk {num} {name}: {src.name} {start}-{end} s")
    return {"number": int(num), "short": name, "file": f"Movies/Walk/{out.name}", "seconds": round(end - start, 2)}


def main():
    only = None
    if "--only" in sys.argv:
        only = set(sys.argv[sys.argv.index("--only") + 1].split(","))
    preview = "--preview" in sys.argv
    layout = json.loads(LAYOUT.read_text(encoding="utf-8")) if LAYOUT.exists() else {"plates": [], "walks": []}
    plates = {p["number"]: p for p in layout.get("plates", [])}
    for spec in EXHIBITS:
        if only and spec[0] not in only:
            continue
        plates[int(spec[0])] = plate(*spec, preview=preview)
    walks = layout.get("walks") or []
    if not only:
        walks = [walk(*spec) for spec in WALKS]
    layout = {"note": "written by stabilize_plates.py; boxes are the object in the plate frame (u0, v0, u1, v1)",
              "aspect": 16 / 9, "plates": [plates[k] for k in sorted(plates)], "walks": walks}
    LAYOUT.write_text(json.dumps(layout, indent=1), encoding="utf-8")
    print(f"wrote {LAYOUT}")


if __name__ == "__main__":
    main()
