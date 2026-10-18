# Keymap 运行时可视化编辑

连接 Android 设备并启动投屏，在投屏窗口按 **Ctrl+E**，或点击右侧工具栏的键盘图标。

编辑时投屏继续更新，设备的键鼠输入暂停，已有触点和延迟动作会被释放。群控主机进入编辑时，也会释放从机上先前传播的输入；正在编辑的设备不接收新的群控输入。

## 编辑与应用

1. 从映射列表选择控件，或点击投屏上的标记。拖动标记可以调整位置，坐标按实际视频区域归一化为 0–1，不包含窗口边框和黑边。
2. 在类型下拉框中选择类型，点击“添加”。支持单击、按下/松开双击、一键多点、方向轮盘、拖动、Android 按键以及鼠标视角/小眼睛。Android 按键没有触摸坐标，通过列表选择并设置数字 Android 键码。
3. 双击属性值修改参数。按键属性是录制按钮：点击后按一个键盘按键或鼠标按钮；Esc 取消录制。修饰键分别映射，不使用组合快捷键格式。需要绑定 Esc 时可使用高级 JSON 中的 `Key_Escape`。
4. 方向轮盘中心与四个方向的距离均可拖动。一键多点的每个点击点都可拖动，延迟在属性中设置；“添加点击点”和“删除点击点”编辑序列，删除时先在属性树选择对应序列项。
5. “高级 JSON”编辑当前所选对象；选择“全局”时可编辑完整配置。验证失败不会替换当前文档。普通编辑会保留未修改的字段与自定义扩展字段。
6. 撤销/重做按钮支持所有文档修改。一次拖动对应一个撤销步骤。在编辑面板和投屏编辑层均可按 Ctrl+Z 撤销；重做使用平台默认快捷键（Windows 为 Ctrl+Y）。
7. 点击“应用并恢复控制”立即替换当前设备的映射，无需重连，并进入自定义映射模式。“应用”不会自动写文件。再次按 Ctrl+E 可继续编辑。

## 文件与恢复

- “保存”写入当前文件，“另存为”选择新文件。写入使用原子文件替换，失败时保留未保存状态。
- “导入”读取已有 JSON，“新建”创建空配置；有未保存修改时先提示处理。
- “取消编辑”恢复进入编辑前运行的映射。保存文件与应用设备是两个独立动作：取消不会撤销已经明确保存的文件。
- 新文件默认保存到用户数据目录的 `keymap` 子目录。主窗口“打开 Keymap 目录”可以打开它。也可另存到指定目录。
- 断开连接或关闭投屏时，活动编辑草稿与已经应用但未保存的配置会写入该目录的 `recovery-日期时间-序号.json`。两份内容不同时分别保留；已经明确取消的草稿不会恢复。恢复路径会显示在主窗口日志中，之后可以通过“导入”打开。
- 初次生成的映射默认按键可在属性中重新录制；配置允许同一按键被多次使用，实际执行遵循原有运行时的映射规则，请自行避免非预期冲突。

## Windows Portable

完整解压目录后运行 `QtScrcpy.exe`，无需安装 Qt、Python、FFmpeg 或 VC 运行库。支持 Windows 10/11 x64；Android 设备仍须启用 USB 调试，并按设备需要安装 USB 驱动及接受调试授权。

目录内包含 ADB、scrcpy-server、Qt 插件、VC 运行库，以及从 FFmpeg 4.4.2 源码构建的动态库。替换 FFmpeg 时须保持库 ABI 与功能兼容。源码和构建参数放在 `sources/`，第三方说明和许可文本放在 `licenses/`。

## 开发与验证

保留 C++11、Qt Widgets 和 Qt 5/6 兼容接口。当前已在 Windows x64、Qt 5.15.2、MSVC 工具链上构建；Linux/macOS 尚未做本次变更的构建验证。

```powershell
# 在 MSVC 开发环境中，使用原生 Windows CMake。
cmake -S . -B build/windows -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=<Qt SDK>
cmake --build build/windows
cmake -S tests/keymapeditor -B build/keymap-tests -G Ninja -DCMAKE_PREFIX_PATH=<Qt SDK>
cmake --build build/keymap-tests
ctest --test-dir build/keymap-tests --output-on-failure
cmake -S QtScrcpy/QtScrcpyCore/tests/inputlifecycle -B build/input-tests -G Ninja -DCMAKE_PREFIX_PATH=<Qt SDK>
cmake --build build/input-tests
ctest --test-dir build/input-tests --output-on-failure
```

测试进程需要能从 PATH 加载 Qt 的 `bin` 目录。`tests/keymapeditor/preview.cpp` 是独立编辑器渲染测试，不模拟 Android 设备。

Portable 打包使用 `ci/win/package-portable.ps1`，明确传入 Qt、FFmpeg 安装目录、对应源码与源码归档、x64 VC CRT 目录。脚本拒绝覆盖既有目标目录，也拒绝启用 GPL/nonfree/version3 的 FFmpeg 配置。

FFmpeg 构建脚本为 `ci/win/build-ffmpeg-lgpl.ps1` 与同名 `.sh` 文件。需要 MSVC 开发环境、MSYS2 UCRT64、make、nasm、diffutils，输出目录可指定；本次使用 4.4.2 未修改的官方源码。
