# Rencana: Museum Nasional Indonesia VR/MR (mode demo video)

Ditulis 2026-10-26. Acuan: project ZooEka2VR (read-only, sumber kode).

## Keputusan pemilik (2026-10-26)

| Hal | Keputusan |
|---|---|
| Basis project | Project UE 5.8 baru di folder ini (`Museum.uproject`). Bawa plugin `WzSystem` + kode Zoo yang dibutuhkan, ganti prefix `Zoo` jadi `Museum`. |
| Mode demo | Dijalankan di **desktop**, direkam jadi MP4 dengan autopilot (port `ZooVRDemoDirector`). Tidak perlu headset. |
| Latar "dunia nyata" | Wajib video rekaman objek, tampil seperti POV Meta Quest passthrough. Foto hanya untuk thumbnail. |
| Video portrait 03/05 | Diganti take landscape dari `Camera/Video/`: `Prasasti_Gajah_Mada_1351M_SidePan`, `Wayang_Golek_Wali_Songo_Showcase`. |
| Urutan tur | Nomor folder 01–06: Dwarapala, Nandiswara, Prasasti Gajah Mada, Relief Arjunawiwaha, Wayang Golek, Maket Masjid At-Thohir. |
| Bahasa | Indonesia default. Toggle ID/EN di menu destinasi dan panel info, mengganti teks dan VO. |
| Maskot | Tidak ada. |
| Belum diputuskan | Versi akhir MR passthrough: APK standalone atau Link. |

## Data aset (diperiksa 2026-10-26)

- GLB: tiap objek ~1.9–2.0 juta segitiga, 1 material, 3 tekstur 4096² (basecolor jpg, normal jpg, ORM png). Tinggi ternormalisasi ~1 m, pivot di dasar. Untuk demo desktop: impor sebagai static mesh Nanite, skala disesuaikan ukuran asli. Untuk Quest nanti: decimate ke ~100–200 rb segitiga + tekstur 2048².
- Video objek: semua tersimpan 1080x1920. Empat punya metadata rotate 90 (tampil landscape), dua tanpa rotasi (portrait: 03 Prasasti, 05 Wayang). Untuk 03 dan 05 dipakai take landscape dari `Camera/Video/`. Hasil ffmpeg (semua 1920x1080, 30 fps) di `Content/Movies/Exhibits/` (lihat `Tools/Video/prep_videos.py`).
- `Camera/Video/` punya take lain yang **landscape** untuk dua objek portrait: `Prasasti_Gajah_Mada_1351M_SidePan` (10.5 s) dan `Wayang_Golek_Wali_Songo_Showcase` (19.9 s) / `_Figur` (6.3 s). Juga ada video galeri (`Galeri_Prasasti_Kuno_Nusantara`, `Pameran_Jalur_Rempah_Nusantara`, `Paviliun_Kitab_Naskah_Kuno`) yang bisa dipakai sebagai footage navigasi antar-objek.
- Audio per objek per bahasa: `01_Judul`, `02_Panel_1`, `03_Panel_2`, `04_Panel_3` (mp3). Teks panel ada di `Skrip_AR_Voiceover.txt`.

## Arsitektur mode demo

```
CineCamera (dari camera tracking footage, di Level Sequence per objek)
 ├─ Plate video  : MediaTexture di plane yang menempel kamera, di belakang semua objek 3D
 ├─ Proxy objek  : mesh tak terlihat seukuran objek asli, di posisi hasil tracking
 │                 -> dipakai untuk kotak deteksi (bbox diproyeksikan ke layar) dan penempatan panel
 ├─ Hologram     : static mesh .glb + reveal scan, di samping/atas proxy
 ├─ Panel info   : world-locked di sisi proxy yang kosong (tidak menutupi objek asli)
 └─ HUD Quest    : menu destinasi, toast, panel misi, laser, lazy-follow seperti Zoo
Post-process "passthrough": grain tipis, saturasi turun, vignette lensa
```

Kalau tracking 3D satu video gagal, cadangannya: stabilkan footage, track 2D posisi objek untuk kotak deteksi, dan pasang hologram/panel di screen-space dengan offset dari titik track.

## Fase

1. **Persiapan aset**
   - [x] Video objek: bake rotasi, 30 fps CFR, H.264 (`Tools/Video/prep_videos.py`).
   - [x] Video portrait 03/05 diganti take landscape.
   - [x] Impor 6 GLB ke UE (Interchange glTF, Nanite). Buat data per objek dari `Skrip_AR_Voiceover.txt` + audio ID/EN + thumbnail `Gambar_Pilihan_*` (2026-10-26, `Tools/Editor/import_exhibits.py`).
2. **Skeleton project**: `Museum.uproject`, modul `Museum`, plugin `WzSystem` (Source + Content), config dari Zoo yang dirapikan, map kosong, build editor.
3. **Port kode Zoo** (ganti nama, buang Duma) — C++ selesai dan build OK 2026-10-26, rincian di `Docs/CHANGES.md`. Belum: aset Blueprint/WBP/input/material/Niagara dan tes di editor.
   - `ZooVRComponent`, `ZooVRPanelHost`, `ZooVRGameMode` -> `MuseumVR*` (laser, hover, panel lazy-follow).
   - `ZooHologramComponent` + `ZooHologramProfile` -> `MuseumHologramComponent` untuk **static mesh** (reveal naik dari dasar, ring scan, glow).
   - `ZooARExhibit` -> `MuseumExhibit`: 3 panel accordion + scroll, VO per panel, `01_Judul` saat hologram muncul, toggle ID/EN, kartu MEDIA opsional.
   - `ZooPathGuideWidgetController` + `ZooPlaceCardsAsset` -> menu destinasi "Mau ke mana?" dengan thumbnail, jarak, ETA.
   - `ZooQuestTrackerPeek` (progres tur 0/6), toast tiba, `ZooViewClutter`.
   - `ZooVRDemoDirector` + `Tools/VRDemo` -> autopilot rekam demo dari JSON.
4. **Mode demo video**: camera tracking per video (Blender 4.5 motion tracking -> kamera FBX -> Level Sequence), plate video, proxy objek, kotak deteksi + label + progress bar, post-process passthrough, tangan/controller Quest dengan laser.
   - [x] 2026-10-26: diganti plate yang distabilkan ke objek, world-locked per stasiun + video jalan head-locked di antara stasiun (`AMuseumPassthrough`). Level `L_MuseumDemo` dan skrip demo dibangun dari skrip (rincian di `Docs/CHANGES.md`).
5. **Rekam demo** 6 objek berurutan -> MP4.
6. **(Nanti)** MR passthrough asli di Quest: passthrough, anchor/scene, skala dunia nyata.

## Pertanyaan terbuka

1. Footage navigasi antar-objek: pakai video galeri dari `Camera/Video/`, atau awal video objek + rute di lantai?
2. Versi akhir MR: APK standalone atau Link?

## Langkah berikut

Lapisan MR sesuai storyboard 8 frame sudah dibangun dan direkam (rincian di `Docs/CHANGES.md`, 2026-10-26). Berikutnya:
1. Owner menonton `Saved/DemoVideo/MuseumVR_Demo.mp4` dan memberi koreksi.
2. Audio di MP4: VO perlu digabung dari `markers.txt` (atau rekam audio terpisah).
3. Lubang NavMesh di origin level demo (PlayerStart sekarang di -250, 0): cari sumbernya.
4. Packaging: aset MR dimuat lewat path string, tambahkan ke daftar cook.
5. Video demo galeri (`L_MuseumGallery`) setelah pemilik setuju screenshot panel baru; `build_demo_script.py` disesuaikan dengan kartu baru (kartu 1 terbuka di awal, bar `HeaderButton<N>`, kartu video, layar besar).
6. Laser pointer kini hanya ke panel dan hologram (2026-10-27). Di video galeri, tangan yang tidak menunjuk panel/hologram tidak lagi menampilkan garis; saat menu tujuan atau peta depan terbuka laser tetap nyala.
7. Peta tur diorama yang bisa diklik (pop-up konfirmasi navigasi) dan waypoint titik-titik + pin (2026-10-27). Menunggu persetujuan pemilik dari screenshot `Saved/Screenshots/Gallery/NavMap/`. Video galeri bisa memakai alur klik peta (`mapstop:N` lalu `ui:MapConfirmYesButton`) selain menu tujuan.
8. Peta tur: putar 360° (tahan trigger + geser), miring sedikit, default landscape, "Anda di sini", animasi rute setelah Ya (2026-10-27). Menunggu persetujuan dari `Saved/Screenshots/Gallery/NavMap/Putar/`. Video galeri bisa memakai step `drag` (target `mapstop:0`).
9. Video demo galeri direkam (2026-10-27): `Saved/DemoVideo/MuseumVR_Gallery_Demo.mp4`, skrip dari `python Tools/Demo/build_gallery_script.py`. Audio VO sudah dimuks (`python Tools/Demo/make_vo_track.py`).

Level dan skrip demo dibangun ulang dengan `Tools/Editor/build_demo_level.py` dan `python Tools/Demo/build_demo_script.py`. Rekam: `Tools\Demo\run_demo.cmd`, lalu `python Tools\Demo\make_mp4.py`.

Skrip Python editor jalan headless lewat `Tools/Editor/run_py.ps1` (editor tertutup).
