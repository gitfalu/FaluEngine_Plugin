# FaluEngine Architecture

## 概要

| 項目 | 内容 |
|------|------|
| 言語 | C++20 |
| グラフィックス | DirectX 11 |
| ビルド | CMake 3.25+ / vcpkg |
| ECS | EnTT |
| スクリプト | Lua 5.4 / sol2 |
| UI (エディタ) | Dear ImGui |

## レイヤー構成

```
Application (Game / Editor)
      │
Engine Core (EventBus, GameLoop, Memory, Logger)
      │
Subsystems (Renderer, Physics, Audio, Input, Script, UI)
      │
Platform Abstraction Layer (Window, FileIO, Thread)
      │
OS / Hardware (Win32, DirectX 11, GPU)
```

## モジュール依存関係

- `engine_core` — 他全モジュールの基盤。外部依存は spdlog / glm / nlohmann_json のみ。
- `engine_scene` → `engine_core`, EnTT
- `engine_renderer` → `engine_core`, `engine_scene`, DirectX::D3D11
- `engine_asset` → `engine_core`, assimp
- `engine_platform` → `engine_core`, DirectX::D3D11 (DXGI/Swapchain)
- `engine_plugin` → `engine_core`
- `engine_script` → `engine_core`, `engine_scene`, sol2/Lua

## プラグイン規約

1. `IPlugin` を継承したクラスを実装する
2. DLL に `createPlugin()` / `destroyPlugin()` をエクスポートする
3. `ENGINE_PLUGIN_EXPORT` マクロを使うことで Win32 / Linux 両対応になる
4. `engine_add_plugin()` マクロで `plugins/` 以下に登録するだけでビルド対象に追加される

## セットアップ手順

```bash
# 1. vcpkg のインストール（初回のみ）
git clone https://github.com/microsoft/vcpkg "$env:VCPKG_ROOT"
& "$env:VCPKG_ROOT/bootstrap-vcpkg.bat"

# 2. CMake 構成
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"

# 3. ビルド
cmake --build build --config Release

# 4. テスト
ctest --test-dir build -C Release --output-on-failure
```
