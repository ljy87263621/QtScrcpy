# Third-party components

QtScrcpy and QtScrcpyCore retain their upstream Apache-2.0 licenses. The bundled scrcpy server is from the QtScrcpyCore source distribution; see its upstream source and license. ADB originates from the Android Open Source Project.

The portable build uses Qt 5.15.2 dynamically. Qt is available under LGPL-3.0 (and alternative GPL/commercial terms). License texts are included. Corresponding Qt sources: https://download.qt.io/archive/qt/5.15/5.15.2/single/ and https://code.qt.io/cgit/qt/qtbase.git/tag/?h=v5.15.2 . Applications may be relinked against compatible replacement Qt libraries.

This software uses libraries from the FFmpeg project under LGPL-2.1-or-later. FFmpeg 4.4.2 source is included in `sources/ffmpeg-4.4.2.tar.xz`, with the exact configure/build script and generated configuration. The source was not modified. This build disables GPL, nonfree, and version3 options and dynamically links the libraries. Original source: https://ffmpeg.org/releases/ffmpeg-4.4.2.tar.xz . FFmpeg information: https://ffmpeg.org/ . Compatible library replacement is permitted under the component's license.

The DLLs originally present in the upstream QtScrcpyCore vendor directory report a nonfree/unredistributable configuration. They are deliberately replaced in this portable package. The source repository keeps those upstream files unchanged; use the packaging script and the separately built LGPL libraries when distributing this build.

Microsoft Visual C++ runtime DLLs are included from the Visual Studio x64 Redistributable directory under Microsoft's redistribution terms. Windows system libraries are supplied by the operating system.
