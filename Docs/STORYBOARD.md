# Storyboard demo MR (rencana sebelum dibangun di Unreal)

Dibuat 2026-10-26 setelah rekaman pertama ditolak.

Masalah rekaman pertama:
- Panel dan hologram menempel seperti stiker dan tidak mengikuti sudut kamera.
- Panel kecil dan susah dibaca.
- Tidak ada navigasi waypoint dan tidak ada peta.
- Efek MR saat deteksi kurang terasa.

Frame dasar diambil dari footage asli di `Saved/Storyboard/base/`. Gambar storyboard dibuat di Gemini (web) dan disimpan di `Saved/Storyboard/`.

## Aturan visual

- Semua elemen MR adalah objek 3D di ruang footage. Masing-masing punya perspektif, ketebalan dan bayangan halus, dan mengikuti lantai dan sudut kamera. Tidak ada yang datar di layar seperti stiker.
- Gaya: kaca gelap transparan (glassmorphism), garis tepi cyan tipis, tipografi bersih. Warna aksen cyan `#4FD8FF` + emas tipis untuk elemen budaya.
- Panel info:
  - 2–4 panel per objek, cukup besar untuk dibaca.
  - Berdiri di samping objek dengan sudut miring menghadap penonton, seperti papan kaca melengkung.
  - Bisa diperluas (expand) dan di-scroll.
- HUD selalu ada (melengkung, 3D, di bawah pandangan):
  - Minimap 3D yang bisa diperkecil dan diperbesar.
  - Tombol navigasi.
  - Progres tur.

## Frame

| # | Momen | Frame dasar | Isi |
|---|---|---|---|
| 1 | Lobi / spawn | `walk01.jpg` | Passthrough galeri. HUD 3D melengkung di bawah: minimap kecil (minimized) di kiri bawah, tombol "Navigasi", progres "0/6". |
| 2 | Pilih tujuan | `walk01.jpg` | Menu tujuan 3D melengkung di depan, kartu 6 objek dengan thumbnail, jarak dan ETA. Laser dari tangan memilih "Arca Dwarapala". |
| 3 | Navigasi waypoint | `walk02.jpg` | Jalur panah bercahaya menempel di lantai mengikuti perspektif. Penanda waypoint melayang di kejauhan dengan jarak "18 m". Minimap diperbesar (expanded) melayang 3D di kiri: denah, rute, posisi visitor. |
| 4 | Deteksi objek (efek MR heboh) | `dwarapala.jpg` | Gelombang scan menyapu lingkungan dari arah arca. Wireframe/mesh cyan membungkus arca mengikuti bentuknya. Partikel, glitch RGB tipis, layar sedikit desaturasi/gelap kecuali arca. Label "OBJEK TERDETEKSI · Arca Dwarapala" dengan progress ring. |
| 5 | Hologram muncul (transisi ke MR biasa) | `dwarapala.jpg` | Hologram 3D arca terbentuk dari bawah ke atas (ring scan) di atas alas kaca di samping arca asli. Efek heboh memudar, warna passthrough kembali normal. |
| 6 | Panel info terbuka | `dwarapala.jpg` | 3 panel kaca melengkung di kiri arca, berbaris miring mengikuti kedalaman (yang dekat lebih besar). Judul jelas: "Identitas & Asal Usul", "Detail Visual & Makna Simbol", "Nilai Edukasi". Garis penghubung tipis ke arca. Hologram di kanan. |
| 7 | Panel diperluas + scroll | `dwarapala.jpg` | Panel 1 membesar (expand), isi teks panjang terbaca, scrollbar di sisi kanan, ikon suara VO aktif. Panel lain menyusut jadi tab. |
| 8 | Minimap diperbesar | `relief.jpg` | Minimap 3D diperbesar di depan visitor (seperti maket meja), 6 titik objek, 1 selesai (centang), rute ke objek berikutnya bercahaya, progres "1/6". |

## Prompt Gemini (per frame, frame dasar diunggah)

Prompt umum (ditambahkan ke tiap frame):
> Edit this real museum photo into a mixed-reality headset view (Meta Quest 3 passthrough). Keep the photo realistic and unchanged except for the added MR elements. All MR elements must be 3D objects anchored in the scene's perspective, matching the camera angle, floor plane and depth, with subtle light and shadow. Never flat stickers. Style: dark translucent glass panels, thin cyan (#4FD8FF) edge glow, subtle gold accents, clean sans-serif Indonesian text, readable. 16:9.

1. Add a curved 3D HUD bar floating low in front of the viewer: on the left a small minimized 3D minimap disc, in the middle a button "NAVIGASI", on the right "Tur 0/6". The curve follows the viewer's field of view.
2. Add a curved 3D glass menu panel floating in front at eye level titled "Mau ke mana?", with 6 destination cards (thumbnail, name, "35 m · 1 mnt"). A VR controller hand at bottom right casts a cyan laser onto the card "Arca Dwarapala", which is highlighted.
3. Add a glowing cyan arrow path lying flat on the floor, following the floor perspective into the distance and turning right. Far ahead, a floating waypoint diamond marker with "Arca Dwarapala · 18 m". On the left, an expanded 3D floating minimap (small tabletop-style floor plan with route line and a "you are here" dot), tilted toward the viewer.
4. Dramatic MR object detection on the stone statue: a cyan wireframe mesh tightly wraps the statue's exact shape; a bright scan wave sweeps outward across the ground and hedges; particles and light streaks rise from the statue; slight RGB glitch at the edges; the surroundings are slightly darkened and desaturated while the statue stays lit. Floating label above it in 3D: "OBJEK TERDETEKSI · Arca Dwarapala" with a circular progress ring.
5. To the right of the real statue, a translucent cyan-gold hologram replica of the same statue materializing from bottom to top on a small floating glass pedestal, with a horizontal scan ring at the build line. The detection effect is fading out; colors are returning to normal.
6. To the left of the real statue, three tall curved glass info panels stand in a staggered row receding in depth (the nearest one larger), angled toward the viewer. Titles: "IDENTITAS & ASAL USUL", "DETAIL VISUAL & MAKNA SIMBOL", "NILAI EDUKASI & FAKTA MENARIK". Thin leader lines connect to the statue. A small hologram replica stands on a glass pedestal to the right of the statue. Panels never cover the statue.
7. Same layout as frame 6, but the first panel is expanded: larger, showing a title "Mengenal Arca Dwarapala", several readable paragraphs of Indonesian text, a scrollbar on its right edge, and a small speaker icon "▶ Suara". The other two panels collapse into slim tabs.
8. A large expanded 3D minimap floats in front of the viewer like a tabletop architectural model: museum floor plan with 6 numbered glowing points, point 1 with a check mark, a glowing route to point 4, and a header "Tur 1/6".

## Hasil (2026-10-26)

- Dibuat di aplikasi Gemini desktop, satu chat, tiap frame diunggah ulang frame dasarnya dengan awalan "Use ONLY the newly attached photo as the base".
- Prompt akhir = prompt umum + teks frame di `Tools/Storyboard/gemini_frames.py`. Frame 3 memakai tujuan "Arca Dwarapala - 18 m" (sama dengan pilihan di frame 2).
- File: `Saved/Storyboard/frame_01.jpg` … `frame_08.jpg`, lembar ringkas `Saved/Storyboard/storyboard_sheet.jpg`.
- Catatan cacat gambar AI (bukan bagian rencana):
  - Frame 6: panel ketiga terlalu dekat ke arca. Di Unreal panel dijaga tidak menutupi objek asli.
  - Frame 8: angka "4" muncul dua kali di minimap.
  - Frame 4: wireframe menutupi seluruh arca. Ini disengaja hanya saat deteksi (beberapa detik).
