# Generates the storyboard frames of Docs/STORYBOARD.md with Gemini image editing: each frame = a real footage frame
# (Saved/Storyboard/base) + its prompt. Output: Saved/Storyboard/frame_NN.png. Key: GEMINI_API_KEY (sent in a header).
# Usage: python Tools/Storyboard/gemini_frames.py [--list] [--model <id>] [frame numbers...]
import argparse
import base64
import json
import os
import sys
import urllib.request

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
BASE = os.path.join(ROOT, "Saved", "Storyboard", "base")
OUT = os.path.join(ROOT, "Saved", "Storyboard")
API = "https://generativelanguage.googleapis.com/v1beta"

COMMON = (
    "Edit this real museum photo into a mixed-reality headset view (Meta Quest 3 passthrough). Keep the photo realistic and "
    "unchanged except for the added MR elements. All MR elements must be 3D objects anchored in the scene's perspective, "
    "matching the camera angle, floor plane and depth, with subtle light, reflection and shadow - never flat stickers. "
    "Style: dark translucent glass panels with real thickness, thin cyan (#4FD8FF) edge glow, subtle gold accents, clean "
    "sans-serif Indonesian text, large and readable. Keep the 16:9 frame. "
)

FRAMES = {
    1: ("walk01.jpg", "Add a curved 3D HUD bar floating low in front of the viewer, bending around the field of view: on the "
        "left a small minimized 3D minimap disc with a tiny floor plan, in the middle a button \"NAVIGASI\", on the right "
        "\"Tur 0/6\" with six small dots. Add a Quest controller hand at the bottom right."),
    2: ("walk01.jpg", "Add a large curved 3D glass menu panel floating in front at eye level, titled \"Mau ke mana?\", with 6 "
        "destination cards in a 3x2 grid (small photo thumbnail, name, \"35 m · 1 mnt\"): Arca Dwarapala, Arca Nandiswara, "
        "Prasasti Gajah Mada, Relief Arjunawiwaha, Wayang Golek, Maket Masjid At-Thohir. A Quest controller hand at the "
        "bottom right casts a thin cyan laser onto the card \"Arca Dwarapala\", which glows highlighted."),
    3: ("walk02.jpg", "Add a glowing cyan chevron arrow path lying flat ON the floor, following the floor perspective into "
        "the distance between the exhibits and turning right. Far ahead, a floating 3D waypoint diamond marker with a label "
        "\"Arca Nandiswara · 18 m\". On the left, an expanded 3D floating minimap like a small tabletop floor-plan model, "
        "tilted toward the viewer, with the route line and a pulsing \"you are here\" dot."),
    4: ("dwarapala.jpg", "Dramatic MR object detection on the stone guardian statue: a bright cyan wireframe mesh tightly wraps "
        "the statue's exact 3D shape; a glowing scan wave ring sweeps outward across the ground, plinth and hedges; light "
        "particles and thin light streaks rise from the statue; slight RGB-split glitch at the frame edges; the surroundings "
        "are darkened and desaturated while the statue stays lit. A 3D label floating above the statue: \"OBJEK TERDETEKSI · "
        "Arca Dwarapala\" with a circular progress ring at 100%."),
    5: ("dwarapala.jpg", "To the right of the real statue, on the plinth, a translucent cyan-gold hologram replica of the same "
        "statue (about half its size) materializes from bottom to top on a small floating glass pedestal, with a bright "
        "horizontal scan ring at the build line; the top half is still wireframe. Faint remains of a detection effect are "
        "fading out; the photo colors are back to normal."),
    6: ("dwarapala.jpg", "To the left of the real statue, three tall curved glass info panels stand in a staggered row "
        "receding in depth (the nearest one larger), each angled toward the viewer like real glass signs with thickness. "
        "Titles: \"IDENTITAS & ASAL USUL\", \"DETAIL VISUAL & MAKNA SIMBOL\", \"NILAI EDUKASI & FAKTA MENARIK\", each with a "
        "one-line subtitle and a \"+\" expand icon. Thin cyan leader lines connect them to the statue. To the right of the "
        "statue a small translucent hologram replica stands on a glass pedestal. Nothing covers the real statue."),
    7: ("dwarapala.jpg", "Same layout as an MR info display: to the left of the real statue one large curved glass panel is "
        "expanded, angled toward the viewer in perspective, titled \"Mengenal Arca Dwarapala\" with three readable "
        "paragraphs of Indonesian text about the guardian statue, a scrollbar on its right edge, and a small speaker "
        "button \"Suara\" playing. Two other panels are collapsed into slim tabs beside it. A Quest controller hand at the "
        "bottom right points a thin cyan laser at the scrollbar. A small hologram replica on a glass pedestal to the right."),
    8: ("relief.jpg", "A large expanded 3D minimap floats in front of the viewer at chest height like a glowing tabletop "
        "architectural model of a museum floor, seen in perspective: six numbered glowing exhibit points, point 1 with a "
        "check mark, a glowing route line to point 4, small icons. Header floating above it: \"Peta Tur · 1/6 dikunjungi\" "
        "with a \"–\" minimize button. A Quest controller hand at the bottom right."),
}


def request(path, body=None):
    headers = {"x-goog-api-key": os.environ["GEMINI_API_KEY"], "Content-Type": "application/json"}
    data = json.dumps(body).encode() if body is not None else None
    req = urllib.request.Request(f"{API}/{path}", data=data, headers=headers, method="POST" if data else "GET")
    try:
        with urllib.request.urlopen(req, timeout=300) as response:
            return json.load(response)
    except urllib.error.HTTPError as error:
        sys.exit(f"HTTP {error.code}: {error.read().decode()[:800]}")


def image_models():
    models = request("models?pageSize=200")["models"]
    return [m["name"] for m in models if "image" in m["name"] and "generateContent" in m.get("supportedGenerationMethods", [])]


def generate(model, number):
    base, prompt = FRAMES[number]
    with open(os.path.join(BASE, base), "rb") as f:
        image = base64.b64encode(f.read()).decode()
    body = {
        "contents": [{"parts": [{"inline_data": {"mime_type": "image/jpeg", "data": image}}, {"text": COMMON + prompt}]}],
        "generationConfig": {"responseModalities": ["TEXT", "IMAGE"]},
    }
    result = request(f"{model}:generateContent", body)
    for candidate in result.get("candidates", []):
        for part in candidate.get("content", {}).get("parts", []):
            blob = part.get("inline_data") or part.get("inlineData")
            if blob:
                ext = "png" if "png" in blob.get("mime_type", blob.get("mimeType", "")) else "jpg"
                path = os.path.join(OUT, f"frame_{number:02d}.{ext}")
                with open(path, "wb") as f:
                    f.write(base64.b64decode(blob["data"]))
                print(f"frame {number}: {path}")
                return
    print(f"frame {number}: no image. {json.dumps(result)[:600]}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("frames", nargs="*", type=int)
    parser.add_argument("--model")
    parser.add_argument("--list", action="store_true")
    args = parser.parse_args()
    models = image_models()
    if args.list:
        print("\n".join(models))
        return
    model = args.model or next((m for m in models if "pro-image" in m), None) or models[0]
    print(f"model {model}")
    for number in args.frames or sorted(FRAMES):
        generate(model, number)


if __name__ == "__main__":
    main()
