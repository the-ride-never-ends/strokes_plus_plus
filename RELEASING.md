# Publishing a Strokes++ release

1. Review the version in `CMakeLists.txt`, the version resource, `CHANGELOG.md`, and
   `RELEASE_NOTES.md`. Commit the release source and confirm the working tree is clean.
2. On Windows 11 x64 with Visual Studio C++ tools, CMake, and NSIS available, run
   `./package.ps1 -Format Both` in PowerShell. The script configures Release, builds it, runs the
   full CTest suite, and creates both packages under `build-package/packages`.
3. Confirm that the `.exe` and `.zip` have matching `.sha256` files. Inspect both packages for
   `StrokesPlusPlus.exe`, `LICENSE`, `README.md`, `THIRD_PARTY_NOTICES.txt`, and the Visual C++
   runtime DLLs.
4. On a clean Windows 11 x64 machine or VM, install the executable, launch from its shortcut,
   open Settings, exercise one built-in gesture and one Lua action, then uninstall. Confirm that
   user configuration remains under `%LOCALAPPDATA%\StrokesPlusPlus`. Extract and launch the
   portable ZIP separately.
5. If a code-signing certificate is available, sign the installer and verify the signature before
   publishing. Otherwise, identify it as unsigned in the release description.
6. Publish the source tag and upload the installer, portable ZIP, and both checksum files with the
   text from `RELEASE_NOTES.md`. Verify the public links and checksums after upload.

Do not upload local `build-*` directories, `log.txt`, or files from Local AppData. The repository
ignores build output and local runtime logs.
