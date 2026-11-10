# Log perubahan

Semua perubahan project dicatat di sini (aturan pemilik dari project Zoo). Rencana ada di `Docs/PLAN.md`.

## 2026-10-26: skeleton project + persiapan video

### Video demo
- `Tools/Video/prep_videos.py`: memproses `Hasil_Model_3D/0x_*/Video_Referensi_*.mp4` ke `Content/Movies/Exhibits/Exhibit0x_<Nama>.mp4`.
  - Rotasi HP di-bake ke piksel, tidak ada lagi metadata rotate.
  - Frame rate variabel jadi 30 fps tetap. H.264 High, CRF 17, keyframe tiap 1 s, tanpa audio.
  - Aspek asli dipertahankan: 01, 02, 04, 06 = 1920x1080; 03, 05 = 1080x1920.
  - File asli tidak diubah. Jalankan ulang dengan `python -I Tools/Video/prep_videos.py --force`.

### Project UE 5.8
- `Museum.uproject`, modul runtime `Museum` (`Source/Museum`), target `Museum` dan `MuseumEditor`.
  - Dependensi modul sama dengan modul Zoo, ditambah `MediaAssets` dan `CinematicCamera` untuk plate video dan kamera tracking.
  - Plugin: sama dengan Zoo (OpenXR, OculusXR, MovieRenderPipeline, toolset MCP editor, WzSystem), ditambah `MediaPlate`.
- `Plugins/WzSystem`: disalin dari ZooEka2VR (hanya `Source`, `Content`, `.uplugin`; tanpa Binaries/Intermediate). Tidak ada perubahan kode.
- Config:
  - `DefaultEngine.ini`: renderer sama dengan Zoo, `vr.InstancedStereo=False`, kanal collision `PathGuideGround`. Map dan game mode belum diset.
  - `DefaultGameplayTags.ini`: tag tempat `PathGuide.Place.Museum.0x.*` untuk 6 objek.
  - `DefaultInput.ini`: Enhanced Input. Mapping context VR ditambahkan saat `IMC_MuseumVR` dibuat.
- Build `MuseumEditor Win64 Development` berhasil (102 s). Dari PowerShell, karena path project berisi spasi:
  `& '<UE_ROOT>\Engine\Build\BatchFiles\Build.bat' MuseumEditor Win64 Development "-Project=<path>\Museum.uproject" -WaitMutex -NoHotReload`
  Compile hanya saat editor tertutup.

## 2026-10-26: video 03/05 landscape + port kode Zoo

### Video
- Keputusan pemilik: objek 03 dan 05 memakai take landscape dari `Camera/Video/`, bukan video portrait di `Hasil_Model_3D`.
  - `Exhibit03_PrasastiGajahMada.mp4` dari `Prasasti_Gajah_Mada_1351M_SidePan_VID20261017154303.mp4` (10.4 s).
  - `Exhibit05_WayangGolek.mp4` dari `Wayang_Golek_Wali_Songo_Showcase_VID20261017162923.mp4` (19.9 s).
  - Sekarang keenam video 1920x1080, 30 fps. `prep_videos.py` punya kolom override sumber per objek.

### Kode yang diport dari Zoo (`Zoo` diganti `Museum`, tanpa Duma)
Disalin otomatis lalu disesuaikan. Asal file: `ZooEka2VR/Source/Zoo/<sama>`.
- Tanpa perubahan logika: `Player/MuseumCharacter`, `Game/MuseumGameMode`, `UI/MuseumHUD`, `UI/MuseumQuestTrackerPeek`,
  `UI/MuseumViewClutter`, `VR/MuseumVRGameMode`, `VR/MuseumVRPanelHost`.
- `Player/MuseumPlayerController`: bagian Duma dan `ARVisitEvents` dihapus.
  - Event exhibit: hologram muncul -> kursor untuk panel; semua kartu didengar -> `Data->VisitedEvent` ke narrative; kunjungan selesai -> mode input game.
  - Input opsional `ToggleLanguageAction` (ID/EN).
  - Tag narrative jadi `Narrative.Event.Museum.*` (terdaftar di `DefaultGameplayTags.ini`, termasuk `Visited.01`–`06`).
- `Hologram/MuseumHologramComponent`: target sekarang `UMeshComponent` (static mesh artefak atau skeletal). Ukuran diambil dari `CalcBounds`, bukan aset skeletal.
- `VR/MuseumVRComponent`: menunjuk `AMuseumExhibit` (trace laser, log klik demo). Selebihnya sama dengan Zoo.
- `UI/PathGuide/MuseumPlaceCardsAsset`: `Name` dan `Description` jadi dua bahasa (`FMuseumText`).
- `UI/PathGuide/MuseumPathGuideWidgetController`:
  - Kartu dan tombol Navigasi mengikuti bahasa tur. `NavigateButtonText` dan `GuidingButtonFormat` jadi `FMuseumText`.
  - Baru: `ToggleLanguage()`, `GetLanguageCode()`, event `OnLanguageChanged` (untuk tombol ID/EN di WBP).
- `VR/Demo/MuseumVRDemoDirector`: Duma dihapus.
  - Langkah baru: `scan` (mulai deteksi objek), `end_visit`.
  - Target baru: `real:<Label>` (objek asli).
  - Kondisi baru: `exhibit_shown:`, `voice_done:`. `exhibit_hidden:` = fase Idle.
  - CVar: `museum.vr.Demo`, `museum.vr.DemoCapture`; argumen `-MuseumVRDemo`. Skrip default `Content/MuseumVR/Demo/MuseumVRDemoScript.json`.

Tidak diport: `AR/ZooAnimal`, `ZooARPresentationComponent`, `ZooARCameraFocusComponent`, `ZooARHabitatMath`, `UI/ZooARInfoWidget`, `Duma/*`, `Game/ZooCinematic*`.

### Kode baru
- `Language/MuseumLanguageSubsystem`: bahasa tur (default Indonesia), `ToggleLanguage()`, event `OnLanguageChanged`. Struct `FMuseumText` dan `FMuseumSound` (ID/EN; EN kosong jatuh ke ID).
- `Exhibit/MuseumExhibitData` (data asset per objek): nomor, `PlaceTag`, nama, subjudul, `VisitedEvent`, mesh, tinggi asli + skala hologram, profil hologram, VO judul, 3 panel (heading, judul, isi, VO), video MEDIA, thumbnail.
- `Exhibit/MuseumExhibit` (pengganti `AZooARExhibit`):
  - Fase Idle, Scanning, Appearing, Shown, Hiding.
  - Kunjungan dimulai saat masuk `ViewingZone`, atau dari `BeginScan()` / `StartPresentation()`.
  - Komponen `RealObject` (box objek asli di footage), `HologramRoot` + `ArtifactMesh` (kembaran hologram di samping), panel info world-space (tegak, menghadap pengunjung), audio guide 2D.
  - Hologram berputar pelan (turntable), berhenti halus saat kartu dibuka, dan melayang.
  - Accordion: satu kartu terbuka, VO kartu diputar. Ganti bahasa memutar ulang VO kartu yang terbuka. `bAutoAdvance` opsional.
  - `OnVisited` setelah semua kartu dibuka sekali.
  - Klik mouse / laser VR dengan pola yang sama seperti Zoo (`WidgetInteraction` custom hit).
- `UI/Exhibit/MuseumInfoPanelWidget`: panel info dibangun di C++ (tanpa WBP):
  - Badge nomor, nama, subjudul, tombol ID/EN.
  - 3 kartu accordion. Status kartu: `+` tertutup, `−` terbuka, `♪` VO berjalan, `✓` sudah didengar. Isi kartu di-scroll.
  - Font dan warna bisa diganti dari Blueprint turunan.
- `UI/Exhibit/MuseumScanWidget`: overlay "deteksi objek" digambar di C++.
  - Bracket sudut mengunci ke proyeksi `RealObject` tiap frame, jadi ikut footage yang di-track.
  - Garis sapuan, label "MEMINDAI · Nama" lalu "TERDETEKSI", progress bar, fade.

Build `MuseumEditor` berhasil. Belum dites di editor/PIE.

## 2026-10-26: aset Zoo + impor 6 objek (Python headless)

### Menjalankan Python editor
- `Tools/Editor/run_py.ps1 <script.py>`: commandlet `UnrealEditor-Cmd -run=pythonscript`, editor harus TERTUTUP. Log di `Saved/Logs/py_<script>.log`, baris skrip berawalan `[Museum`.
- `-script=` terpotong di spasi pertama dan path project ini punya spasi. Skrip `.py` disalin dulu ke `%TEMP%\museum_py\` lalu dijalankan dari sana.

### Aset Zoo yang dipakai ulang
- `Tools/Assets/zoo_asset_closure.py` (read-only ke ZooEka2VR): 143 paket disalin ke `Content/Zoo` dan `Content/ZooVR`, path sama dengan Zoo. Isi: VR (hands, input, BP VR component/game mode), input jalan + PathGuide, BP character/controller/game mode, widget PathGuide, font, material dan suara PathGuide, material UI, hologram (`M_ZooHologram`, MF, Niagara), `M_ARWidget_OnTop`, `WBP_Zoo_NarrativeOverlay`.
- Tidak disalin (khusus Zoo): map, sinematik, hewan, Duma, `WBP_AR_*`, `DA_PG_Places`, `DA_ZooPlaceCards`, thumbnail Zoo.
- `Config/DefaultEngine.ini` `[CoreRedirects]`: kelas/struct/fungsi `/Script/Zoo.Zoo*` -> `/Script/Museum.Museum*`, paket `/Script/Zoo` -> `/Script/Museum`, plus 32 redirect WzSystem dari Zoo.
- `Tools/Editor/resave_shared.py`: load + compile + simpan ulang semua aset salinan dengan kelas Museum. Semua BP compile, kecuali `BP_Zoo_HUD` (node AR Media Zoo hilang).
- `BP_Zoo_HUD` diganti `/Game/Museum/Blueprints/BP_MuseumHUD`: induk `AMuseumHUD`, widget PathGuide + narrative disalin dari CDO lama. `BP_ZooGameMode` dan `BP_ZooVRGameMode` sekarang memakai HUD ini. Salinan `BP_Zoo_HUD` dihapus (tidak ada referensi, aslinya tetap di ZooEka2VR).
- `BP_ZooPathGuideWC`: `PlaceCards` = `DA_MuseumPlaceCards`, tombol `Navigasi` / `Navigate`, format `Menuju: {Place}` / `Heading to: {Place}` (dulu FText, sekarang FMuseumText, jadi nilainya diisi ulang).
- `M_ZooHologram` (translucent, unlit): parameter tekstur `BaseColorTex`, skalar termasuk `RevealHeight`, `HoloHeight`, `Glitch`, `Speaking`.

### Impor 6 objek
- `Tools/Assets/stage_exhibits.py` (python biasa + ffmpeg, sumber tidak diubah) -> `Saved/ImportStaging/`:
  - teks dari `Keterangan_Objek.txt` baris 1 (nama + subjudul ID) dan `Skrip_AR_Voiceover.txt` (3 panel ID + EN: heading, judul, isi),
  - mp3 VO -> wav 16-bit (48 file), `Gambar_Pilihan_*` -> jpg 1024 px,
  - nama + subjudul EN tidak ada di sumber: diterjemahkan di `NAMES_EN` (perlu dicek museum).
- `Tools/Editor/import_exhibits.py` (aman diulang; GLB diimpor ulang hanya dengan `-MuseumReimport`). Per objek di `/Game/Museum/Exhibits/<nn>_<Nama>/`:
  - `Mesh/...`: GLB lewat Interchange (static mesh + material bertekstur + 3 tekstur 4K). Tinggi hasil impor ~98 cm (Maket 84 cm), pivot di dasar.
  - Nanite aktif, fallback 10% segitiga. Slot 0 mesh = `MI_Holo_<nn>_<Nama>` (induk `M_ZooHologram`, `BaseColorTex` = basecolor GLB). Material translucent, jadi yang dirender mesh fallback (~200 rb segitiga). Material bertekstur asli tetap ada di `Mesh/`.
  - `Audio/SW_<nn>_ID_Judul|Panel1..3`, `SW_<nn>_EN_Title|Panel1..3`.
  - `T_<nn>_<Nama>`: thumbnail UI (tanpa mip, tidak di-stream).
  - `DA_Exhibit_<nn>_<Nama>` (`UMuseumExhibitData`): nomor, `PlaceTag`, `VisitedEvent`, nama/subjudul ID+EN, mesh, `RealHeight` 0 (sementara), `HologramScale` 0.6 (sementara), profil hologram, VO judul, 3 panel + VO, thumbnail.
- Bersama:
  - `/Game/Museum/Hologram/DA_MuseumHologramProfile`: ambient `NS_ZooHolo_Ambient`, suara muncul/hilang/glitch dari suara PathGuide (seperti profil hewan Zoo). `NS_ZooHolo_Materialize` tidak dipasang: Niagara itu hanya sampling skeletal mesh.
  - `/Game/Museum/PathGuide/DA_MuseumPlaceCards`: 6 kartu (nomor, nama, subjudul, thumbnail).
  - `/Game/Museum/Media/FMS_Exhibit<nn>_<Nama>`: FileMediaSource ke `Content/Movies/Exhibits/*.mp4` (plate demo).
- `Tools/Editor/verify_exhibits.py`: cek 6 DA (tag, mesh, Nanite, material hologram, 8/8 VO, thumbnail), kartu, HUD game mode. Semua lolos.
- Ukuran: `Content/Museum` 453 MB, `Content/Zoo` 9 MB, `Content/ZooVR` 14 MB.

Belum: dibuka di editor/PIE, efek materialize untuk static mesh, tinggi asli objek.

## 2026-10-26: level demo, plate passthrough, skrip demo

### Ganti rencana: plate distabilkan, bukan camera tracking 3D
- Rencana awal (CineCamera dari tracking Blender per video) diganti. Tiap video objek distabilkan ke objeknya, lalu diputar di plate yang diam di dunia (world-locked).
- Kamera game tetap bebas (kepala/tangan autopilot). `RealObject`, hologram dan panel adalah aktor statis dan tetap menempel di objek di footage.
- Batas: kepala tidak boleh menoleh jauh (tepi plate kelihatan). Skrip demo menjaga pandangan dalam ±12° dari titik `GazeNN`.

### Video
- `Tools/Video/stabilize_plates.py` (OpenCV, sumber tidak diubah):
  - Optical flow LK (cek maju/mundur) + `estimateAffinePartial2D` RANSAC, transformasi berantai, dihaluskan Gaussian.
  - Zoom minimal di sekitar objek supaya bingkai penuh (maks 1.5), tepi `BORDER_REPLICATE`.
  - Loop ping-pong (maju lalu mundur).
- Keluaran:
  - `Content/Movies/Plates/PlateNN_<Nama>.mp4` (6 objek).
  - `Content/Movies/Walk/WalkNN_<Nama>.mp4` (3 video jalan galeri).
  - `Tools/Video/plates.json`: kotak objek di bingkai plate, zoom, durasi loop. Pratinjau di `Saved/PlatePreview/`.
- Potongan: 01 mulai 1.5 s (awal terpotong atas), 04 mulai 2.6 s (awal tiang/dinding), 05 9.9–14.6 s dan hanya figur ke-3 (yang dimodelkan), 06 hanya kelompok masjid.

### C++
- `Passthrough/MuseumPassthrough` (`AMuseumPassthrough`, tick `TG_PostUpdateWork`), "dunia nyata" pengganti passthrough Quest:
  - Walk plate: video jalan, head-locked 40 m di depan, ukuran dari FOV + aspek viewport. Klip berikutnya tiap guidance mulai. Hanya diputar saat visitor bergerak.
  - Station plate: `PlateVideo` exhibit, world-locked di `GetPlateTransform()`. Tampil saat visitor di zona exhibit dan tidak sedang dipandu. Menggantikan walk plate setelah frame pertama ter-decode (tidak ada layar hitam).
  - Material: parameter `Video` + `ContentScale`.
- `AMuseumExhibit`:
  - Properti baru `PlateVideo`, `PlateOffset` (ruang aktor, plate menghadap -X), `PlateSize`, `PlateContentScale` (0.3–1: rekaman diperkecil di plate, sisanya isian blur).
  - Fungsi baru `GetPlateTransform()`.
- `Museum.Build.cs`: + `ProceduralMeshComponent`.

### Config
- `DefaultEngine.ini`:
  - Map default dan map startup editor = `/Game/Museum/Maps/L_MuseumDemo`.
  - `[/Script/NavigationSystem.RecastNavMesh] RuntimeGeneration=Dynamic`: NavMesh dibangun saat jalan, tidak perlu build di editor.
  - `r.SkinCache.CompileShaders=True`, sama seperti Zoo. Tanpa ini `-game` crash: "Ray tracing requires skin cache to be enabled". Shader akan dikompilasi ulang sekali.
- `DefaultGame.ini` `WzPathGuideSettings`: `PlacesAsset` = `DA_PG_Places_Museum`; config + UI config tetap dari Zoo.

### Level (`Tools/Editor/build_demo_level.py`, headless, membangun ulang dari nol)
- `/Game/Museum/Passthrough/M_PassthroughPlate`: unlit, Custom HLSL.
  - Rekaman tajam di tengah (skala `ContentScale`). Sisanya diisi rekaman yang diblur dan digelapkan.
  - Tampilan passthrough: desaturasi, kontras, kecerahan, vignette, grain per frame.
- `/Game/Museum/Media/FMS_PlateNN_*`, `FMS_WalkNN_*`.
- `/Game/Museum/PathGuide/DA_PG_Places_Museum`: 6 tempat, radius tiba 150.
- `L_MuseumDemo`:
  - Lantai tak terlihat (collision), NavMeshBounds, 2 lampu arah, post process (exposure manual, vignette, bloom rendah, tanpa motion blur), PlayerStart, `Passthrough`.
  - Game mode `BP_ZooVRGameMode`.
- Rute:
  - Kaki 2000 cm (kecepatan jalan pawn 250 × 8 s), belok kiri 45° di tiap stasiun.
  - Visitor tiba menghadap objek.
- Per stasiun:
  - `StationNN`: tujuan PathGuide.
  - `ZoneNN`: TriggerBox 120×120 cm, kunjungan mulai di sini.
  - `ExhibitNN`: `PointerIndex` = 10+nomor. Kalau semua 0, pointer panel saling membatalkan dan kartu tidak bisa diklik.
  - `GazeNN`: titik pandang autopilot.
- Penempatan, dalam sudut dari titik pandang:
  - Plate selebar 120° pada jarak 15 m. Kotak objek dari `plates.json` menjadi `RealObject` pada kedalaman 4 m.
  - Hologram (kedalaman 2.2 m) dan panel (1.85 m) dicari pada grid 1° dalam ±40° × ±25°, jarak minimal 2.5° dari objek dan satu sama lain.
  - Urutan kompromi: panel tetap terbaca (skala 0.062/0.056), lalu rekaman diperkecil.
  - `HologramScale` di DA diisi dari tinggi target, `RealHeight` 0.
- Hasil:

  | # | Rekaman | Hologram | Panel |
  |---|---|---|---|
  | 01 | 0.7 | kanan, 94 cm | kiri ×0.056 |
  | 02 | 0.5 | kanan, 94 cm | kiri ×0.056 |
  | 03 | 0.5 | kanan, 94 cm | kiri ×0.056 |
  | 04 | 0.3 | kiri, 84 cm | kanan ×0.056 |
  | 05 | 1.0 | kanan, 94 cm | kanan ×0.062 |
  | 06 | 1.0 | bawah, 60 cm | kiri ×0.062 |

- `Tools/Editor/verify_demo_level.py`: aktor, wiring exhibit, file video, tag tempat. Lolos.

### Demo
- `Tools/Demo/build_demo_script.py` -> `Content/MuseumVR/Demo/MuseumVRDemoScript.json` (240 langkah):
  - Lobi, lalu per objek: menu, kartu tempat, rute, langkah ke `StationNN`, scan + hologram, 3 kartu (kartu 1 didengar sampai habis, 2–3 dibaca + scroll), akhiri kunjungan.
  - Objek 03 juga memakai toggle ID/EN.
- `Tools/Demo/run_demo.cmd`: 1080p, 30 fps tetap, rekam frame ke `Saved/DemoVideo/Frames`.
- `Tools/Demo/make_mp4.py`: MP4 + `timestamps.md` dari `markers.txt`.
- Tes pendek (lobi + objek 01) lolos:
  - Rute PathGuide sampai tiba, zona memicu scan dan hologram.
  - Klik kartu membuka panel, VO main sampai habis.
  - Plate dan hologram tampil benar.
- Catatan log:
  - `MI_Holo_* missing usage flag Nanite` (material translucent di mesh Nanite; hologram tetap tampil lewat fallback).
  - `LogGameFeatures: Error` saat startup membuat commandlet keluar dengan kode 1. Error ini sudah ada sebelumnya.
- Rekaman penuh (`run_demo.cmd` + `make_mp4.py`):
  - Hasil: `Saved/DemoVideo/MuseumVR_Demo.mp4`, 7:25, 13375 frame, 711 MB, tanpa audio. Timestamp di `Saved/DemoVideo/timestamps.md`.
  - 240 langkah, tanpa timeout. Ke-6 objek lewat: rute, scan, hologram, 3 kartu, toggle EN di objek 03.
- Kekurangan yang terlihat:
  - 04: rekaman di skala 0.3 terlihat seperti gambar dalam bingkai (tepi tajam di atas isian blur).
  - 05: hologram dan panel menutupi figur wayang tetangga, bukan figur yang dimodelkan.
  - 06: hologram kecil (60 cm, di bawah) dan sebagian tertutup tangan.
  - Panel HUD `MISI` dan `NAVIGASI` kadang melayang di tengah pandangan.

## 2026-10-26: storyboard visual MR (sebelum dibangun ulang di Unreal)

- Rekaman pertama ditolak owner (panel seperti stiker, tidak ikut sudut kamera, tanpa waypoint/peta, efek deteksi lemah).
- Rencana visual baru di `Docs/STORYBOARD.md`, 8 frame.
- Gambar dibuat di aplikasi Gemini desktop (akun Pro, mode "Buat gambar", 16:9) dari frame footage asli `Saved/Storyboard/base/`. API key gratis tidak bisa (kuota gambar 0, HTTP 429).
- Hasil: `Saved/Storyboard/frame_01.jpg` … `frame_08.jpg` + `storyboard_sheet.jpg` (2x4).
- Belum ada perubahan di Unreal. Menunggu persetujuan owner.

## 2026-10-26: lapisan MR dibangun ulang sesuai storyboard (8 frame)

Owner menyetujui storyboard (`Docs/STORYBOARD.md`). Semua UI MR sekarang 3D di dunia (bukan widget layar), dibuat dari kode (tanpa WBP).

### Kode baru (`Source/Museum/MR`)
- `MuseumMRStyle`: warna, font (Inter untuk simbol, Rajdhani untuk label), pembuat mesh MR (`MakeMesh`: tanpa collision, tidak memengaruhi NavMesh).
- `MuseumMRWidget` (dasar widget kode) + `MuseumMRLabelWidget`: pil kaca, ring progres digambar di `NativePaint`.
- `MuseumMRInfoCardWidget`: kartu kaca tinggi; tertutup (seluruh kartu tombol), terbuka (teks panjang + scrollbar, `Suara`, `ID | EN`, tutup `—`), tab tipis (judul diputar).
- `MuseumMRHudWidgets`:
  - Bar HUD melengkung (frame 1): cakram `PETA n/6`, `NAVIGASI`/`MEMANDU`, `Tur n/6` + titik, pesan. Nama panjang pakai huruf lebih kecil.
  - Menu "Mau ke mana?" (frame 2): kaca melengkung 3x2, thumbnail, nomor `01`, `✓ Dikunjungi`, jarak; tombol `Place<N>`, `ID | EN`, `×`.
  - Header peta: "PETA TUR · n/6 dikunjungi" + tombol `–`.
- `AMuseumMRHud` (di-spawn `AMuseumHUD`, WBP layar tidak dipakai lagi: `bShowScreenWidgets=false`):
  - Bar 95 cm, pitch -19°, ikut kepala secara malas (ambang 18°). Diam saat visitor menunduk ke bar (pitch < -9°) atau menunjuknya; tanpa ini bar "lari" saat kepala menoleh ke tombol di ujungnya.
  - Bar sembunyi saat menu terbuka, peta di depan, atau objek sedang presentasi.
  - Menu 130 cm, muncul di arah pandang saat dibuka. Nomor, nama, thumbnail menu diambil dari data exhibit (sama dengan kartu).
- `AMuseumMRMinimap` (frame 3 & 8): meja kaca bundar, 6 titik bernomor (masing-masing sekali), centang hijau untuk yang dikunjungi, rute bercahaya saat memandu, titik visitor.
  - Samping kiri saat memandu (yaw -44°, skala 0.6); depan seperti meja (72 cm, turun 46 cm) saat cakram PETA ditekan; `–` melipat.
- `AMuseumMRWaypoints` (frame 3): 36 chevron di lantai mengalir ke tujuan, diamond + sinar + cincin di tujuan, label "{nama} · {m} m" yang tetap terbaca dari jauh. Rute bawaan plugin disembunyikan.

### Exhibit (`Exhibit/MuseumExhibit`, ditulis ulang)
- Dihapus: panel info lama, `MouseInteraction`, kelas widget scan/info (`UI/Exhibit/*` dihapus).
- Deteksi (frame 4): label "MEMINDAI OBJEK…" → "OBJEK TERDETEKSI" + nama, ring progres; parameter `DetectAmount/DetectTime` untuk plate.
- Hologram (frame 5): pedestal kaca, ring yang naik ikut reveal, tag "Hologram · nama".
- Kartu (frame 6 & 7): 3 kartu kaca tebal di zona bebas samping objek (`PanelZone`, dihitung builder level), bertingkat kedalamannya, miring ke penonton, garis penunjuk ke objek. Hologram di sisi lain. Satu kartu terbuka, dua lainnya jadi tab tipis. Animasi tata letak 0.55 s.
- Label deteksi 1800x320 x0.085, tag hologram 1100x160 x0.07 (versi pertama terlalu kecil di rekaman).

### Passthrough / level
- `M_PassthroughPlate`: efek deteksi di atas objek asli (RGB split, glow tepi Sobel, sekeliling digelapkan + desaturasi, gelombang scan + grid segitiga, gelombang lantai, bracket lock-on, percikan). Tepi rekaman yang diperkecil dibuat lebih halus (`smoothstep(-0.09, 0)`).
- `build_demo_level.py`: plate 150°, zona kartu 30–38° di sisi yang lebih bebas, hologram di sisi lain; `view_offset` + `panel_zone` per exhibit. `verify_demo_level.py` mengecek parameter plate dan zona kartu.
- `build_mr_assets.py` (baru): `M_MR_Glass`, `M_MR_Glow`, `M_MR_Ring` di `/Game/Museum/MR`. Glow memakai alpha vertex lewat input terpisah (`VA`); `VC.a` gagal compile di Lumen card PS.
- PlayerStart pindah ke (-250, 0): NavMesh di titik origin punya lubang radius ~1 m (sumbernya belum ketemu; diagnosa tidak menemukan komponen nav-relevant di sana, rebuild area pun tidak menutupnya). Dari origin, PathGuide gagal ("no route") dan jarak menu kosong.
- Catatan packaging: aset MR dimuat lewat path string (`/Game/Museum/MR/*`, font `/Game/Zoo/Fonts/*`); perlu masuk daftar cook kalau dipaket.

### Demo
- `VR/Demo/MuseumVRDemoDirector`: target `ui:` juga mencari widget dunia MR (tag `MuseumMRInteractive`), target baru `panels:<Label>`, titik klik widget silinder.
- `build_demo_script.py`: per objek tekan `NavigateButton` di bar, pilih `Place<N>` di menu, ikuti chevron, deteksi + hologram, lihat kartu, buka kartu 1 (didengar sampai habis, scroll `CardBody`), kartu 2–3 lewat tab. Objek 03: `ID | EN` setelah kartu 1 terbuka, kembali ID di akhir. Setelah objek 01: peta di depan lalu `–`.

### Rekaman
- `Saved/DemoVideo/MuseumVR_Demo.mp4`: 7:53, 14205 frame, 738 MB, tanpa audio. Timestamp di `Saved/DemoVideo/timestamps.md`.
- 266 langkah, tanpa timeout. Keenam objek lewat: menu 3D, chevron + diamond, deteksi, hologram, 3 kartu, EN di objek 03, peta di depan setelah objek 01.
- Kekurangan yang terlihat:
  - Peta samping saat memandu masih sedikit menimpa ujung kiri bar.
  - Peta di depan (setelah objek 01) menutup bagian bawah arca.
  - Pinggir rekaman yang diperkecil (blur) masih terlihat saat kepala menoleh ke kartu di objek dengan skala rekaman kecil.

## 2026-10-27: peta samping dan peta depan dirapikan, rekam ulang

- `AMuseumMRMinimap`:
  - Peta samping (saat memandu): yaw -40°, 80 cm, turun 2 cm, skala 0.5, sehingga di kiri dan di atas ujung bar HUD. Tidak lagi menimpa bar atau terpotong tepi layar.
  - Peta depan: skala 0.8.
- `AMuseumMRHud::GetFreeYaw`: saat visitor berdiri di objek, peta depan dibuka di sisi bebas objek (tepi dalam `PanelZone` + 17°), bukan lurus ke depan, sehingga arca tidak tertutup. Di luar objek tetap di arah bar.
- Rekaman: `Saved/DemoVideo/MuseumVR_Demo.mp4`, 7:54, 14249 frame, 736 MB, tanpa audio, tanpa timeout.
- Masih terlihat: teks "Menuju …" untuk nama yang sangat panjang (objek 04, 06) masih terpotong di bar.

## 2026-10-27: efek deteksi dikalemkan, teks bar tidak terpotong, rekam ulang

- `M_PassthroughPlate` (`PLATE_HLSL` di `build_demo_level.py`): deteksi awal tinggal scan 3D saja: pita cyan yang naik dari dasar ke puncak objek, grid segitiga yang tertinggal di belakang pita, glow tepi Sobel hanya di dalam pita. Dihapus: RGB split, sekeliling digelapkan, gelombang lantai, bracket lock-on, percikan.
- `UMuseumMRHudBarWidget`: pesan bar ("Menuju …") di dalam `UScaleBox` (ScaleToFit, DownOnly), mengecil ke sisa ruang bar sehingga nama panjang (objek 04, 06) tetap utuh.
- `AMuseumMRHud`: bar tidak lagi beku saat visitor berjalan dengan kepala menunduk (melihat chevron). Beku karena pitch hanya saat berdiri (kecepatan ≤ 20 cm/s); saat berjalan bar mengikuti kepala mulai 6° (berdiri: 18°). Sebelumnya bar tertinggal di kanan dan ujungnya (teks "Menuju …") keluar layar.
- `AMuseumExhibit`: draw area label deteksi 2400x320 (dari 1800) dan tag hologram 1600x160 (dari 1100). Plate label memeluk teks di tengah; nama terpanjang (objek 06) sebelumnya terpotong di ujung kanan.
- `AMuseumMRWaypoints`: draw area label di atas diamond 2200x220 (dari 1300); "Maket Miniatur Arsitektur Masjid At-Thohir · n m" sebelumnya terpotong.
- Rekaman: `Saved/DemoVideo/MuseumVR_Demo.mp4`, 7:51, 14151 frame, 724 MB, tanpa audio, tanpa timeout. Dicek: bar "Menuju …" utuh dan tetap di tengah saat berjalan (objek 04, 06), label deteksi/hologram/waypoint utuh, deteksi hanya scan 3D.

## 2026-10-27: map galeri 3D nyata (L_MuseumGallery)

Map baru untuk show-off: museum seolah lingkungan nyata. Map ini tidak memakai rekaman passthrough; keenam objek berdiri sebagai objek 3D di ruang pamer yang diterangi lampu. Lapisan MR tetap sama: deteksi, hologram, kartu, bar HUD, menu, peta, dan chevron.

- `Tools/Editor/build_gallery_level.py` (baru; editor harus tertutup):
  - Material prosedural tanpa tekstur di `/Game/Museum/Gallery`:
    - `M_Gallery_Surface`: lantai beton poles bernat 120 cm, dinding charcoal, dinding aksen, plafon, plinth, kayu, kuningan;
    - `M_Gallery_Glass`: vitrine;
    - `M_Gallery_Emissive`: panel lampu;
    - `M_Gallery_Print`: foto dinding.
  - `/Game/Museum/MR/M_MR_Scan`: scan deteksi di permukaan objek 3D. Pita naik dari dasar ke puncak, grid segitiga, dan rim glow di dalam pita.
  - Ruang pamer 43 x 19 m, tinggi 6 m:
    - sirip dinding di antara bay, rel lampu track, panel lampu di atas lorong, bangku;
    - judul "JEJAK PERADABAN NUSANTARA" di dinding ujung.
  - Penempatan objek: 01–03 di dinding kiri, 04–06 di dinding kanan, urutan tur memutar.
  - Per objek:
    - mesh Tripo dengan material bertekstur aslinya, pada skala nyata (01 150 cm, 02 140, 03 130, 04 75, 05 88, 06 lebar 150), menghadap pengunjung;
    - plinth; vitrine kaca berangka untuk 05 dan 06; tiang + tali pembatas untuk 01–03;
    - plakat dinding (nomor, nama, asal);
    - foto dokumentasi berbingkai. `Gambar_Pilihan_*.jpg` diimpor ke `/Game/Museum/Gallery/Prints` (maks. 2048); file asli hanya dibaca.
    - Lampu: spot track key + fill (3100 K) dengan badan lampu, dan washer untuk plakat dan foto.
  - Lampu dan eksposur memakai skala yang sama dengan level demo: manual, bias 0, intensitas dalam candela. Cahaya UI MR jadi tidak berubah.
  - Aktor tur memakai nama yang sama dengan level demo (`StationNN`, `ZoneNN`, `ExhibitNN`, `GazeNN`), jadi skrip tur `build_demo_script.py` jalan di sini tanpa diubah.
  - Hologram dan zona kartu diletakkan dengan sudut dari mata pengunjung, sama seperti di level demo. Bedanya, outline objek di sini dihitung dari bounds 3D-nya.
- `AMuseumExhibit`:
  - `RealActor` + `ScanMaterial`: salinan mesh objek nyata ("scan shell", skala 1.004, aditif) yang tampil selama deteksi. Parameter: `DetectAmount`, `DetectTime`, `ScanBottom`, `ScanTop`.
  - `TwinHeight`: tinggi hologram per instance. `DA_Exhibit_*` dipakai bersama dengan level demo, jadi `HologramScale` di data asset tidak disentuh.
- `Tools/Demo/run_demo.cmd`: map dipilih lewat `MUSEUM_MAP` (default `L_MuseumDemo`).

## 2026-10-27: panel info baru (minimize/extend, posisi di sekitar objek, panel video + layar besar)

Permintaan pemilik: ketiga panel bisa di-minimize dan di-extend; awalnya 1 panel terbuka dan sisanya kecil dengan tombol; panel tidak selalu berjajar di samping (misalnya samping, atas, bawah objek); ada panel video yang bisa diperbesar ke layar 3D sekitar 70% layar dengan tombol tutup; panel tidak semuanya teks, ada foto pendukung.

- Media: tidak mengunduh dari internet. Dipakai media milik pemilik:
  - foto `Camera/*.jpg` (2 per objek) diimpor ke `/Game/Museum/Gallery/Photos/T_Photo_NN_2|3` (maks. 2048, grup UI); file asli hanya dibaca;
  - video `Content/Movies/Exhibits/ExhibitNN_*.mp4` lewat `FMS_ExhibitNN_*` yang sudah ada.
- `FMuseumExhibitPanel.Image` (baru): foto pendukung kartu. Kartu 1 tetap teks penuh; kartu 2 dan 3 foto + teks lebih pendek. `UMuseumExhibitData.MediaVideo` kini dipakai untuk kartu video.
- `UMuseumMRInfoCardWidget` ditulis ulang:
  - Mode `Minimized` (bar ramping: nomor, heading, judul yang mengecil agar muat, tombol "+"/"✓") dan `Expanded`.
  - Jenis `Text`, `Image` (landscape: foto 16:9 kiri, judul + teks bergulir kanan), `Video` (video, "Putar/Jeda", "+ Perbesar", keterangan).
  - Nama tombol untuk demo tetap: `HeaderButton<N>` (bar kecil dan tombol "—"), `LanguageButton`, `VoiceButton`, `CardBody`. Baru: `VideoPlayButton`, `VideoEnlargeButton`.
  - Latar gelap tipis di belakang isi kartu: kaca saja pudar di depan plinth putih.
- `UMuseumMRVideoScreenWidget` (baru): layar video besar dengan `TheaterPlayButton` dan `TheaterCloseButton` ("× Tutup").
- `UMuseumMRWidget`: `MakePlayMark` (segitiga play digambar; font Rajdhani tidak punya glyph ▶, tampil kotak) dan `MakeFittedText` (teks mengecil, tidak terpotong).
- `AMuseumExhibit`:
  - Kartu = kartu data + kartu video (jika `MediaVideo` ada). Satu kartu terbuka pada satu waktu; `InitialPanel` (0) terbuka saat kartu muncul dan langsung dibacakan.
  - `PanelSlots` (baru, `FMuseumPanelSlot`: `Area` sudut dari mata + `Anchor`): kartu dengan area berdiri di sana (bar kecil di titik anchor, kartu terbuka tumbuh dari situ, skala agar kartu terbuka muat). Kartu tanpa area masuk kolom di samping objek (`PanelZone`), ditumpuk vertikal, garis leader dari kartu kolom pertama.
  - Video: `UMediaPlayer` + `UMediaTexture` per objek, loop, tanpa suara. Kartu video terbuka = video diputar; pindah kartu = jeda. Sumber video disimpan di `VideoSource` (UPROPERTY): soft pointer saja membuat sumber ter-GC dan video objek 02–06 hitam.
  - Layar besar: `OpenVideoScreen` / `CloseVideoScreen`. Lebar = `VideoScreenShare` (0.7) x lebar pandang pada `VideoScreenDistance` (150 cm), di depan arah pandang (pitch dibatasi ±12°). Kartu lain memudar selama layar terbuka; pointer hanya ke layar.
  - Turntable hologram berhenti saat suara pemandu atau layar video aktif (dulu: saat ada kartu terbuka, sekarang selalu ada).
  - `Fan` 8° untuk kolom; `ExpandedFan` dihapus.
- `Tools/Editor/build_gallery_level.py`:
  - `dress_cards`: foto ke kartu 2 dan 3, `MediaVideo` ke data. Ini menulis `DA_Exhibit_*` (dipakai juga oleh level demo; di level demo semua kartu masuk kolom samping karena `PanelSlots` kosong).
  - `card_slots`: kartu 2 di atas objek + hologram (sampai pitch 23°); 04 dan 05: kartu 3 di bawah objek (di bawah label hologram, sampai -34°); 06: video di bawah hologram; 01–03: video di kolom (tali pembatas lewat di bawah hologram).
- Screenshot: `Saved/Screenshots/Gallery/Panel/` (00_ringkasan_panel + 12 gambar). Tur cek 6 objek: semua klik (bar, "—", bahasa, video, perbesar, tutup) jalan, tanpa timeout, video 6 objek tampil.
- Belum: skrip tur video `Tools/Demo/build_demo_script.py` masih mengikuti kartu lama (klik judul kartu 1–3). Akan diubah saat rekam video galeri, setelah screenshot disetujui.

## 2026-10-27: laser pointer hanya ke panel dan hologram
Permintaan pemilik: garis pointer muncul hanya saat menunjuk objek hologram atau panel; selain itu disembunyikan.

- `UMuseumVRComponent`:
  - Garis debug `UWidgetInteractionComponent` (`bShowDebug`) tidak dipakai lagi. Dulu garis itu selalu sepanjang `PointerDistance` (15 m) untuk panel AR, dan tetap tampil saat menu tujuan terbuka atau ada panel tambahan.
  - Laser baru: balok glow `MuseumVR_LaserBeam` (tebal 0,35 cm, warna `LaserColor`) dari tangan sampai titik kena, plus titik `MuseumVR_LaserDot` (1,4 cm) di titik kena.
  - Tampil hanya jika sinar kena panel VR, widget dunia (kartu info, layar video, bar HUD, menu, peta) atau `ArtifactClickBounds` hologram. Objek asli, vitrin, lantai, dinding: laser hilang.
  - `LaserLingerTime` 0,6 → 0,15 s (menjembatani celah antar tombol), lalu memudar cepat.
- Cek: tur `L_MuseumGallery` objek 01, 04, 06 (panel, tombol, hologram, objek asli, lantai). Screenshot: `Saved/Screenshots/Gallery/Laser/`.
- Tambahan (permintaan pemilik: pointer juga nyala saat navigasi): selama menu tujuan "Mau ke mana?" atau peta tur di depan terbuka, laser tetap nyala walau menunjuk celah antar kartu atau badan peta 3D (tanpa collision). Panjang sampai benda pertama, maks. `NavigationLaserLength` 300 cm; titik ujung hanya muncul di atas panel/hologram. Saat memandu (jalan ikut panah lantai) laser tetap mati kecuali menunjuk panel.
  - `AMuseumMRHud::IsNavigationOpen()` dan `AMuseumHUD::GetMRHud()` (baru) untuk membaca status menu/peta.
  - Cek tur navigasi: kartu menu, celah, luar menu, rute, badan peta, tombol peta, peta ditutup. Screenshot `laser_navigasi_peta.jpg`.

## 2026-10-27: referensi baru waypoint dan peta tur (Gemini)
Permintaan pemilik: waypoint dan peta masih jelek, buat gambar referensi lain dengan Gemini.

- `Tools/Storyboard/gemini_nav_refs.py` (baru): 8 prompt (W1–W4 waypoint, M1–M4 peta). Mode API (edit frame galeri di `Saved/Storyboard/NavRefs/base/`) gagal: key free tier, kuota gambar 0 (HTTP 429). `text_prompt()` = versi teks penuh (deskripsi galeri) untuk aplikasi Gemini; daftar di `Saved/Storyboard/NavRefs/text_prompts.json`.
- Dibuat di Gemini web (model 3.1 Pro) di panel browser, satu chat "Museum Nasional Mixed-Reality Tour", 8 gambar:
  - W1 pita cahaya di lantai, W2 titik-titik + berkas cahaya + pin, W3 bola pemandu + jejak, W4 jalur dua garis + pelat panah + papan tujuan;
  - M1 diorama 3D galeri di nampan kaca, M2 kartu kaca denah + daftar, M3 peta hologram di tangan kiri, M4 denah terproyeksi di lantai.
- Unggah frame galeri ke Gemini web tidak bisa (fetch ke server lokal diblokir, clipboard ditolak, dialog file tidak bisa diakses). Gambar belum diunduh ke proyek.

## 2026-10-27: peta tur diorama (M1) yang bisa diklik dan waypoint titik-titik + pin (W2)
Permintaan pemilik: pakai referensi M1 dan W2; objek di peta tur bisa diklik, animasi hover dan klik (scale up + glow), klik memunculkan pop-up "Aktifkan navigasi ke destinasi ini?".

- `AMuseumMRMinimap` (ditulis ulang): nampan kaca miring berisi diorama galeri (bingkai glow, dinding rendah). Tiap stop: alas putih, miniatur mesh artefak (6 cm), nomor (teks unlit `/Engine/VREditor/Fonts/VRTextMaterial`, sebelumnya tampil hitam), cincin, cakram glow, kotak klik (`MuseumMRInteractive`, hanya saat peta bisa ditunjuk).
  - Hover: stop membesar 30 %, alas berubah cyan, cakram glow lembut. Klik: pop 25 % lagi.
  - Klik stop memunculkan pop-up di atas header peta (tumbuh dari nampan); pop-up tertutup otomatis saat peta tidak di depan.
- `UMuseumMRMapConfirmWidget` (baru, 1500 x 560): foto + nomor, "NAVIGASI", "Aktifkan navigasi ke destinasi ini?" / "Start navigation to this destination?", nama (emas), tombol `MapConfirmYesButton` "Ya, pandu saya" dan `MapConfirmNoButton` "Batal".
- `AMuseumMRHud`: tombol trigger yang tidak kena widget menekan stop yang sedang di-hover (`PressStop`). Ya memanggil `SelectPlace` (mulai memandu) dan menutup peta depan.
- `AMuseumMRWaypoints` (ditulis ulang): titik glow pipih tiap 50 cm di lantai, tiap titik ke-3 panah kecil, gelombang cahaya berjalan ke tujuan. Di tujuan: cincin lantai + denyut, berkas cahaya tipis, pin peta melayang (membesar sesuai jarak di atas 5 m), label "Arca Dwarapala · 11 m" di atas pin.
- Director demo: target baru `mapstop:N` (stop N di peta tur).
- Laser: komponen bertag `MuseumMRInteractive` dihitung sebagai sasaran (titik ujung tampil di stop peta).
- Cek: tur `L_MuseumGallery` (buka peta, hover stop 3 dan 5, klik stop 2, Batal, klik stop 1, Ya, rute + pin). Screenshot: `Saved/Screenshots/Gallery/NavMap/`.

## 2026-10-27: peta tur bisa diputar 360°, default landscape, posisi kita, animasi rute
Permintaan pemilik: peta 3D bisa diputar penuh 360°, miring ke atas/samping hanya sedikit; default landscape (sebelumnya potret); tampilkan posisi kita; setelah klik navigasi di peta muncul animasi rute.

- `AMuseumMRMinimap`:
  - Default landscape. Dari 4 putaran 90°, dipilih yang sisi panjang galeri melintang di depan mata dan titik awal tur paling dekat ke pengunjung (`BaseYaw`). Peta tidak lagi diselaraskan dengan arah museum asli. Tiap kali peta dibuka di depan, posisinya kembali ke default.
  - Putar: tekan dan tahan trigger di nampan (bukan di stop), lalu geser. Geser kiri/kanan memutar meja tanpa batas (5° per 1° gerak tangan). Geser naik/turun memiringkan maks. ±8°. Saat diputar meja juga condong ke samping maks. ±5°, lalu kembali tegak saat dilepas. Gerakannya dihaluskan.
  - "Anda di sini" / "You are here": titik + denyut + panah arah hadap pengunjung + label cyan (teks unlit).
  - Rute baru menggambar diri dalam 1,4 s dari posisi pengunjung ke stop, dengan ujung terang di depan; stop tujuan "pop" saat rute sampai. Setelah itu kilau berjalan di sepanjang rute. Rute digambar ulang setiap peta dibuka.
  - `GetStopLocation(0)` = titik di nampan untuk dipegang (target demo `mapstop:0`).
- `AMuseumMRHud`: setelah "Ya, pandu saya", peta tetap di depan 2,6 s (`MapRouteShowTime`) agar animasi rute terlihat, lalu pindah ke samping. `BeginMapDrag` / `EndMapDrag` (mouse atau trigger). `FrontMapHalfWidth` 17° → 22° (peta landscape lebih lebar).
- `UMuseumVRComponent`: trigger yang ditekan di nampan peta memulai drag (bukan klik). Saat dilepas, drag berhenti dan tidak ada klik. Sebelumnya trigger VR hanya menjadi klik saat dilepas, jadi tidak bisa ditahan untuk drag.
- Director demo: step baru `drag` (`Target`, `Seconds`, `DragYaw`, `DragPitch`): tahan trigger lalu sapu laser.
- Cek: tur `L_MuseumGallery` (buka peta, putar 3x, miring, klik stop 4, Ya, rute tergambar). Screenshot: `Saved/Screenshots/Gallery/NavMap/Putar/`.

## 2026-10-27: video demo galeri 3D
Permintaan pemilik: rekam video demo setelah peta dan waypoint disetujui.

- `Tools/Demo/build_gallery_script.py` (baru) menulis `Content/MuseumVR/Demo/MuseumVRGalleryScript.json` untuk `L_MuseumGallery`.
  - Objek ganjil dipilih lewat menu "Mau ke mana?". Objek genap dipilih di peta tur: tekan stop, "Ya, pandu saya", lalu rute tergambar. Di objek 02 peta juga diputar dan dimiringkan.
  - Setiap objek: jalan ikut titik lantai ke pin, deteksi, hologram, lalu kartu 1 (didengar), kartu 2, kartu 3 dan kartu video. Layar besar di 01 dan 04, ID/EN di 04. Di akhir peta menunjukkan 6/6.
  - Semua kartu dibuka karena kunjungan baru dihitung setelah ketiga kartu info didengar. Rekaman pertama hanya 2/6 karena kartu 3 dilewati di 01, 02, 03 dan 06.
- Perbaikan yang ditemukan saat merekam:
  - `AMuseumExhibit::UpdateVideoScreen`: outline hologram (post-process dari custom depth) terlihat tembus layar video besar yang translucent. Custom depth objek kini mati selama layar terbuka.
  - `UMuseumMRInfoCardWidget`: crash `Assertion failed: Index >= 0` di `IsResourceObjectValid`, karena foto kartu (`LoadSynchronous`) hanya dipegang pointer biasa dan kena GC. Kini ditahan di `HeldPicture` (UPROPERTY).
- Hasil: `Saved/DemoVideo/MuseumVR_Gallery_Demo.mp4` (7:44, 1080p30, tanpa audio). Daftar waktu ada di `Saved/DemoVideo/timestamps_gallery.md`.

## 2026-10-27: audio suara pemandu di video demo galeri
Permintaan pemilik: tambahkan audio VO ke video demo galeri.

- `AMuseumExhibit`: setiap klip suara pemandu yang mulai/berhenti dicatat ke log: `VOCUE play <waktu dunia> <SoundWave>` dan `VOCUE stop <waktu dunia>`. `StopVoiceOver()` (baru) menggantikan semua `VoiceOver->Stop()` langsung.
- `Tools/Demo/make_vo_track.py` (baru):
  - Waktu dunia dipetakan ke frame video lewat `markers.txt` (regresi linear; hasil: video = waktu dunia − 0,068 s).
  - Tiap `SW_NN_<ID|EN>_<Judul|Title|PanelK>` dipetakan ke WAV staging-nya (`Saved/ImportStaging/exhibits.json`).
  - Klip ditempatkan di frame-nya, dipotong saat pemandu berhenti atau klip berikutnya mulai (fade 80 ms), lalu digabung ke `vo_track.wav` dan dimuks ke MP4 (video tidak di-encode ulang).
- Video direkam ulang (tur sama): `Saved/DemoVideo/MuseumVR_Gallery_Demo.mp4`, 7:45, 1080p30, AAC 192 kb/s, 26 klip VO (judul + kartu 1–3 tiap objek, kartu 3 objek 04 juga dalam EN). Level: rata-rata −21,4 dB, puncak −4,0 dB.
- Urutan bikin ulang: `run_demo.cmd` → `make_mp4.py --out MuseumVR_Gallery_Demo.mp4` → `make_vo_track.py`.
