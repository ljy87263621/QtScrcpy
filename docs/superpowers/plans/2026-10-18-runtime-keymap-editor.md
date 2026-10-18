# Runtime Keymap Editor Implementation Plan

> Execute the tasks in this chat, in order, with review and verification between tasks.

**Goal:** Edit every existing QtScrcpy key mapping over a live device screen, apply it without reconnecting, and save compatible JSON.

**Architecture:** Keep JSON as the authoritative document so unedited and extension fields survive round trips. Separate a document with undo history, a transparent screen overlay, and an accompanying editor/property tool window. Integrate with the existing device script update interface; explicitly release active input before replacing the running mapping.

**Tech Stack:** C++11, Qt Widgets 5.15.2 (retain Qt 6 compatible APIs), QJsonDocument, QUndoStack, Qt Test, CMake, MSVC x64.

## Confirmed requirements

- Windows x64 Portable is the first delivery; retain cross-platform source compatibility.
- Overlay controls on the live screen, with drag handles and an editable property panel.
- Cover click, click twice, multiple clicks, steering wheel, drag, mouse movement/small eyes, Android keys, and the mapping switch key.
- Add/delete, keyboard/mouse binding capture, undo/redo, import, save, and save as.
- Pause mapped device input while editing. Apply explicitly and resume without reconnecting.
- Preserve existing JSON formats and unedited fields. Use normalized video coordinates, excluding window borders and letterboxing.
- Ship runtime dependencies in the portable directory. No Qt/Python/toolchain installation required for end users.

## Task 1: Repository and build baseline

- [x] Create `ljy87263621/QtScrcpy` as a fork of `barry-ran/QtScrcpy`.
- [x] Finish fetching the upstream default branch `dev` and its pinned `QtScrcpyCore` submodule.
- [x] Create `Beta/runtime-keymap-editor` from `origin/dev`; keep `origin` as the user's fork and `upstream` as the original.
- [x] Initialize CodeGraph and inspect the script, rendering, event, and configuration paths before editing code.
- [x] Install the Windows Qt SDK under `D:\Develope\Qt` and configure a separate build directory with an explicit MSVC generator.
- [x] Record the unmodified build result before feature changes.

## Task 2: Compatible document and undo history

**Create:** `QtScrcpy/keymapeditor/keymapdocument.h`, `QtScrcpy/keymapeditor/keymapdocument.cpp`.
**Test:** `tests/keymapeditor/tst_keymapeditor.cpp`, `tests/keymapeditor/CMakeLists.txt`.

Document responsibilities: parse and validate JSON; preserve the complete root object; expose typed paths to editable values and normalized coordinate handles; create valid defaults for each existing mapping type; support atomic replacement, node addition/removal, and undoable edits; serialize only after validation.

- [x] First add tests for loading the shipped configurations, lossless semantic round trips, rejected malformed imports, and retained unknown fields.
- [x] Add tests for independent undo/redo, clean/dirty state after save, add/delete, coordinate bounds, and drag edits.
- [x] Run the tests against a minimal document implementation and observe the missing behavior failures.
- [x] Implement the document and re-run the tests.

## Task 3: Overlay and property editor

**Create:** `QtScrcpy/keymapeditor/keymapoverlay.h/.cpp`, `QtScrcpy/keymapeditor/keymapeditor.h/.cpp`.
**Modify:** `QtScrcpy/CMakeLists.txt`.

The overlay is a sibling above the rendering widget and matches its actual geometry. It paints keys, drag paths, multiple-click points, mouse anchors, and steering controls. A drag changes the selected document coordinate, with one undo entry per completed drag. Selection is synchronized with a mapping list. The property panel edits all existing scalar/nested fields, with key capture and coordinate controls. Multiple-click steps can be added and removed. Save uses `QSaveFile` and reports write errors without clearing dirty state.

- [x] Add Qt Test coverage for hit selection, normalized dragging under different viewport sizes, keyboard capture, import failures, and save/reopen equivalence.
- [x] Implement selection, dragging, labels, and edit gestures; swallow editor input before it reaches the device.
- [x] Implement mapping creation/deletion, properties, undo/redo, and file actions.
- [x] Verify the editor against all bundled scripts and inspect its rendered layout.

## Task 4: Runtime integration and input lifecycle

**Modify:** `QtScrcpy/ui/videoform.h/.cpp`, `QtScrcpy/ui/dialog.h/.cpp`, `QtScrcpy/ui/toolform.h/.cpp` as required by their existing signal connections.
**Potential dependency change:** `QtScrcpyCore/src/device/controller/inputconvert/inputconvertgame.h/.cpp` for active-input cleanup on script replacement.

- [x] Test that replacing a mapping releases held touches/Android keys, stops delayed clicks and drags, and releases cursor capture.
- [x] If the core needs a change, fork that dependency too and pin the parent to a committed dependency revision, so a recursive clone remains reproducible.
- [x] Add an editor entry point to the projection window and associate the selected script/path with that device.
- [x] On entry, suspend input and release running actions; keep video updating.
- [x] On Apply, validate and replace the script, close the overlay, and retain explicit access to re-enter editing.
- [x] On cancellation/close/disconnect, consistently resolve unsaved changes and restore the appropriate script.
- [x] Keep editor and runtime changes isolated to the selected device, including group-control mode.

## Task 5: Delivery and verification

**Create:** `docs/RuntimeKeymapEditor_zh.md` and a portable packaging script if the existing publisher cannot provide a reproducible local package.

- [x] Run the document and widget tests with the Qt platform plugin available.
- [x] Build the full Windows Release application with the explicit MSVC toolchain.
- [ ] Verify resize, fullscreen, landscape/portrait, high DPI, all mapping types, cancel/apply/save, undo after drag, and invalid/read-only files.
- [ ] Exercise real-device behavior when an authorized Android device is connected; record any unavailable device verification explicitly.
- [x] Package Qt plugins/libraries, FFmpeg, ADB, server, configuration, keymaps, notices/licenses, and VC runtime DLLs.
- [x] Launch the package using a sanitized PATH and local dependencies to detect accidental SDK dependencies.
- [x] Review the complete Git diff, record checks and remaining limitations, and deliver the portable directory/ZIP.
