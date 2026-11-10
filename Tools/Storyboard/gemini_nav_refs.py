# Reference images for the redesign of the floor waypoints and the tour map: Gemini image editing of real frames of the
# gallery level (Saved/Storyboard/NavRefs/base, taken from the check tour). Output: Saved/Storyboard/NavRefs/<id>.png.
# Key: GEMINI_API_KEY (sent in a header). Usage: python Tools/Storyboard/gemini_nav_refs.py [--model <id>] [ids...]
import argparse
import base64
import json
import os
import sys
import urllib.request
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
REFS = os.path.join(ROOT, "Saved", "Storyboard", "NavRefs")
BASE = os.path.join(REFS, "base")
API = "https://generativelanguage.googleapis.com/v1beta"
MODEL = "models/gemini-3-pro-image"

COMMON = (
    "This is a first-person view inside a VR museum gallery (Museum Nasional Indonesia, a long dark hall with warm spot "
    "lights; exhibits 1, 2, 3 stand along the right wall from near to far, exhibits 6, 5, 4 along the left wall from near "
    "to far). Redesign ONLY the navigation graphics as a premium mixed-reality interface, like Apple Vision Pro or Meta "
    "Quest 3 system UI. Keep the room, the exhibits, the lighting, the camera angle and the controller hands exactly as "
    "they are. All new elements are real 3D objects anchored in the scene's perspective and floor plane, with soft "
    "emissive glow, gentle reflections on the floor and depth; calm and elegant, never flat stickers, never cluttered. "
    "Palette: cyan #4FD8FF light with subtle warm gold #E7B860 accents, dark smoked glass. Text in clean sans-serif, "
    "Indonesian, short and readable. Keep the 16:9 frame. "
)

REMOVE_ROUTE = ("First remove the existing thin cyan chevrons on the floor, the ring with the vertical line and diamond "
                "on the right, the long floating label \"Arca Dwarapala Penjaga Gerbang Suci · 11 m\" and the round "
                "map on the left. Keep the curved HUD bar at the bottom. ")
REMOVE_MAP = ("First remove the existing round cyan map disc with white cylinders and numbers, and its header \"PETA "
              "TUR\". Keep the floor route only if the new design needs it. ")

REFS_SPEC = {
    # Waypoints: the route from the visitor to exhibit 1 (Arca Dwarapala, right wall, near)
    "W1_pita_cahaya": ("rute.jpg", REMOVE_ROUTE +
        "Add a continuous soft light ribbon lying flat on the floor, about 40 cm wide, starting a meter in front of the "
        "viewer and curving smoothly to the right toward the stone guardian statue on the right (exhibit 1). Small arrow "
        "shapes flow inside the ribbon like moving light. The ribbon fades out near the viewer and ends in a soft glowing "
        "circle on the floor in front of the statue's plinth. Above that circle, a small elegant glass tag at eye height: "
        "\"01 · Arca Dwarapala\" and \"11 m\"."),
    "W2_titik_jejak": ("rute.jpg", REMOVE_ROUTE +
        "Add a dotted path on the floor: a line of small round glowing dots, evenly spaced (about 50 cm apart), getting "
        "smaller with distance, curving to the right to the stone guardian statue (exhibit 1). Every third dot is a "
        "small arrow pointing along the path. At the end, a slim vertical beam of soft light rises from a floor ring in "
        "front of the plinth, with a compact 3D map-pin icon floating at head height and a small label "
        "\"Arca Dwarapala · 11 m\"."),
    "W3_bola_pemandu": ("rute.jpg", REMOVE_ROUTE +
        "Add a small glowing guide orb (like a firefly, size of a tennis ball) floating at chest height two meters ahead "
        "of the viewer, slightly to the right, leaving a faint luminous trail that curves toward the stone guardian "
        "statue (exhibit 1). A very subtle thin line on the floor below the trail shows the same route. Next to the orb, "
        "a tiny label \"Arca Dwarapala · 11 m\". Around the statue's plinth, a soft ring of light on the floor marks the "
        "destination."),
    "W4_lantai_garis": ("rute.jpg", REMOVE_ROUTE +
        "Add a museum-wayfinding style floor route: two thin parallel light lines on the floor (like a lane, 60 cm apart) "
        "with wide flat arrow plates between them every 1.5 m, all in perspective, turning right toward the stone "
        "guardian statue (exhibit 1). The arrows near the viewer are brighter, far ones softer. The route ends in a "
        "rounded floor plate in front of the plinth printed with \"01\". A slim standing glass sign beside the plate "
        "reads \"Arca Dwarapala\" and \"11 m · ±15 detik\"."),
    # Tour map: opened in front, the visitor looks down a little
    "M1_diorama": ("peta.jpg", REMOVE_MAP +
        "Add a 3D tabletop diorama of this gallery floating at waist height in front of the viewer, tilted slightly "
        "toward them, about 60 cm long: a rectangular dark glass tray with the hall's low translucent walls, six tiny "
        "white plinths with tiny glowing miniatures of the exhibits (statues, relief, wayang puppet, mosque model), "
        "numbered 1 to 6 with small round badges. A glowing route line runs from a \"you are here\" arrow marker to "
        "exhibit 1. Header floating above the tray: \"Peta Tur · 0/6 dikunjungi\" with a small \"–\" button."),
    "M2_kartu_kaca": ("peta.jpg", REMOVE_MAP +
        "Add a floating curved smoked-glass card (like a tablet, 60 x 40 cm) in front of the viewer at chest height, "
        "angled toward them: on its left two thirds a clean top-down floor plan of the rectangular hall with six "
        "numbered circular photo thumbnails of the exhibits along the two walls, a cyan route line from a \"Anda di "
        "sini\" dot to exhibit 1; on its right third a short list of the six exhibits with check marks and distances. "
        "Header \"Peta Tur · 0/6 dikunjungi\" and a \"–\" button on the card's top edge."),
    "M3_peta_tangan": ("peta.jpg", REMOVE_MAP +
        "Make the tour map appear above the viewer's LEFT controller hand, like a hologram held in the palm: a compact "
        "glowing 3D miniature floor plan of the rectangular hall, about 30 cm wide, tilted toward the viewer, with six "
        "numbered pins (photo thumbnails on top), the route to exhibit 1 and a pulsing \"you are here\" dot. A slim "
        "header above it \"Peta Tur · 0/6\"."),
    "M4_lantai_proyeksi": ("peta.jpg", REMOVE_MAP +
        "Add the tour map as a large holographic floor plan projected ON the floor just in front of the viewer, about "
        "1.5 m wide, in perspective: thin cyan outlines of the hall's walls, six numbered glowing circles where the "
        "exhibits stand, each with a tiny vertical light pin and a small floating name label, the route line to exhibit "
        "1 continuing into the real floor route ahead, a \"you are here\" arrow at the viewer's feet. A small floating "
        "header at eye level: \"Peta Tur · 0/6 dikunjungi\" with a \"–\" button."),
}


# Text-only variant for the Gemini app (no base frame upload): the scene is described instead of edited
SCENE = (
    "Generate a photorealistic 16:9 first-person view from a Meta Quest 3 mixed-reality headset, standing in a modern "
    "museum gallery of Museum Nasional Indonesia: a long dark hall (about 12 m wide, 30 m long) with dark brown walls, "
    "a black ceiling with rows of small spotlights and a few rectangular light panels, a dark grey polished concrete "
    "floor with warm pools of light. Exhibits stand on white plinths in pools of warm spotlight: on the right a stone "
    "Dwarapala guardian statue behind a red rope barrier (nearest), a Nandiswara statue and a stone inscription further; "
    "on the left a white miniature mosque model in a glass vitrine (nearest), a wayang golek puppet and a stone relief "
    "further. A terracotta wall at the far end reads \"JEJAK PERADABAN NUSANTARA\". Two dark VR controller hands are "
    "visible at the bottom. "
)
STYLE = (
    "Only the navigation graphics are MR: real 3D objects anchored in the scene's perspective and floor plane, soft "
    "emissive glow, gentle reflections on the floor, depth; premium and calm like Apple Vision Pro or Meta Quest 3 system "
    "UI, never flat stickers, never cluttered. Palette: cyan #4FD8FF light with subtle warm gold #E7B860 accents, dark "
    "smoked glass. Short readable Indonesian sans-serif text. "
)


def text_prompt(key):
    """The prompt for the Gemini app: scene + style + the design (the 'remove the existing ...' part dropped)."""
    _, prompt = REFS_SPEC[key]
    for lead in (REMOVE_ROUTE, REMOVE_MAP):
        prompt = prompt.replace(lead, "")
    if key.startswith("M"):
        prompt = "The visitor looks slightly down at the floor in front. " + prompt
    return SCENE + STYLE + prompt


def request(path, body):
    headers = {"x-goog-api-key": os.environ["GEMINI_API_KEY"], "Content-Type": "application/json"}
    req = urllib.request.Request(f"{API}/{path}", data=json.dumps(body).encode(), headers=headers, method="POST")
    try:
        with urllib.request.urlopen(req, timeout=300) as response:
            return json.load(response)
    except urllib.error.HTTPError as error:
        return {"error": f"HTTP {error.code}: {error.read().decode()[:600]}"}


def generate(model, key):
    base, prompt = REFS_SPEC[key]
    with open(os.path.join(BASE, base), "rb") as f:
        image = base64.b64encode(f.read()).decode()
    body = {
        "contents": [{"parts": [{"inline_data": {"mime_type": "image/jpeg", "data": image}}, {"text": COMMON + prompt}]}],
        "generationConfig": {"responseModalities": ["TEXT", "IMAGE"], "imageConfig": {"aspectRatio": "16:9"}},
    }
    result = request(f"{model}:generateContent", body)
    for candidate in result.get("candidates", []):
        for part in candidate.get("content", {}).get("parts", []):
            blob = part.get("inline_data") or part.get("inlineData")
            if blob:
                ext = "png" if "png" in blob.get("mime_type", blob.get("mimeType", "")) else "jpg"
                path = os.path.join(REFS, f"{key}.{ext}")
                with open(path, "wb") as f:
                    f.write(base64.b64decode(blob["data"]))
                return f"{key}: {path}"
    return f"{key}: no image. {json.dumps(result)[:500]}"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("refs", nargs="*")
    parser.add_argument("--model", default=MODEL)
    args = parser.parse_args()
    keys = args.refs or list(REFS_SPEC)
    with ThreadPoolExecutor(4) as pool:
        for line in pool.map(lambda k: generate(args.model, k), keys):
            print(line, flush=True)


if __name__ == "__main__":
    main()
