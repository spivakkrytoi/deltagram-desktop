# DeltaGram Desktop (Windows)

A fork of [Telegram Desktop](https://github.com/telegramdesktop/tdesktop) rebranded
for the **DeltaGram** private server, with operator-granted cosmetic avatar frames.

* App identity: `DeltaGram`, `DeltaGram.exe`, version `1.0.0`
* Server: `150.241.69.39:2398` (IPv4 only, built-in DC 2)
* API credentials: baked into `Telegram/cmake/telegram_options.cmake`
* Branding: `Telegram/SourceFiles/core/version.h`, `core/launcher.cpp`,
  `platform/win/windows_app_user_model_id.cpp`, `Resources/art/*`
* Avatar frames: `Telegram/SourceFiles/deltagram/`, painted from
  `Data::PeerData::paintUserpic`

## Build with GitHub Actions (recommended)

This repository is a complete source tree. Submodules are **not** included in the
archive; the workflow checks them out recursively.

1. Create an empty **private** repository on GitHub.
2. Extract the archive and push it:

   ```bat
   tar -xzf DeltaGram-Desktop-src.tar.gz
   cd deltagram-src
   git remote add origin https://github.com/<you>/<repo>.git
   git push -u origin main
   ```

   (The archive is already a git repository with one commit and all 38
   submodule entries registered, so `git push` is enough — do **not** re-run
   `git init` or `git add`.)

3. Open **Actions → DeltaGram Windows → Run workflow**.
4. When the job finishes, download the `DeltaGram Windows x64 qt6` artifact.
   It contains `DeltaGram.exe` plus `DeltaGram-win64.zip`, a portable build with
   the Qt runtime deployed by `windeployqt`.

The workflow builds x64 / Qt 6 / Release with autoupdate and crash reporting
disabled. Caches for third-party libraries and Qt keep subsequent runs fast.

## Build locally on Windows

Requirements are the same as upstream (`docs/building-win.md`): Visual Studio
2026 with the 10.0.26100 SDK, Python 3.10, Git.

```bat
git clone --recursive <this-repo> tdesktop
tdesktop\Telegram\build\prepare\win.bat
cd tdesktop\Telegram
configure.bat x64 qt6
cmake --build ..\out --config Release --parallel
```

The binary is `out\Release\DeltaGram.exe`. API credentials are already set, but
you can override them with `-D TDESKTOP_API_ID=... -D TDESKTOP_API_HASH=...`.

## DeltaGram-only changes

| Area | File |
| --- | --- |
| App name / version / GUID | `Telegram/SourceFiles/core/version.h` |
| Process and application name | `Telegram/SourceFiles/core/launcher.cpp` |
| Windows AUMID | `Telegram/SourceFiles/platform/win/windows_app_user_model_id.cpp` |
| API id / hash defaults | `Telegram/cmake/telegram_options.cmake` |
| Built-in DC endpoint (IPv4 only) | `Telegram/SourceFiles/mtproto/mtproto_dc_options.cpp` |
| `DeltaGram.exe` output name | `Telegram/CMakeLists.txt` (end of file) |
| Window/icon/tray artwork | `Telegram/Resources/art/icon256.ico`, `logo_256*.png` |
| Avatar frames | `Telegram/SourceFiles/deltagram/deltagram_frames.{h,cpp}` |
| Frame paint hook | `Telegram/SourceFiles/data/data_peer.cpp` |
| CI build | `.github/workflows/deltagram-win.yml` |

## Avatar frames

Frames are cosmetic and never travel over MTProto. The client polls the public
registry `https://dltagram.duckdns.org/api/frames/registry` every 10 minutes and
paints a ring around matching avatars. Operators grant frames from the admin
panel (Accounts → account → *Обводка аватара*); an empty value removes the frame.

Known ids: `scratch`, `ember`, `neon`, `gold`, `sakura`, `void`, `frost`,
`rainbow`. They must stay in sync with `internal/domain/avatar_frame.go` in the
server repository.

Upstream Telegram Desktop is licensed under GPL-3.0 (see `LICENSE`); this fork
keeps that license.
