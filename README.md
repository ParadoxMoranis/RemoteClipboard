# Remote Clipboard: 高效跨设备剪贴板同步系统

## 项目背景

在现代开发与办公环境中，多设备协作已成为常态。然而，在不同设备间传输临时文本内容往往依赖聊天软件或云笔记，效率低下且操作繁琐。特别是在 Linux 下，Wayland 对剪贴板访问的限制，使得跨设备复制粘贴更具挑战性。

在配置 Arch Linux 并使用 Hyprland 桌面环境的过程中，我感受到缺乏高效跨设备剪贴板同步工具的困扰。因此，我决定做一个 Remote Clipboard，一个轻量级、高效、跨平台的实时剪贴板共享解决方案。

## 项目概述

Remote Clipboard 通过 TCP 网络通信，实现跨设备的剪贴板同步，支持 Windows、Linux（Wayland/CLI）、macOS（CLI）等主流操作系统。其核心目标包括：

- **实时同步**：监听剪贴板变化，自动广播到所有连接的设备。
- **轻量级架构**：无需数据库，采用直接 TCP 传输，避免额外开销。
- **跨平台兼容**：针对不同系统提供相应的实现方案，确保高可用性。
- **高安全性**：后续版本将支持 TLS 加密，确保传输数据安全。

## 系统架构与技术实现

### 1. 客户端架构

**Windows 客户端**

- 使用 Qt（C++）构建 GUI，提高用户体验。
- 通过 Win32 API 监听剪贴板事件。
- 采用 Winsock 实现 TCP 数据传输。

**Linux 客户端**

- 采用 Qt 实现 GUI 版本，提供良好交互体验。
- 通过 `wl-clipboard` 监听 Wayland 剪贴板，兼容 CLI 场景。
- 使用标准 socket 实现 TCP 通信。
- 提供了命令行版本，提高可用性。

**macOS 客户端**

- 采用 C++，并通过 Objective-C 与 NSPasteboard 交互。
- 采用标准 socket 进行数据传输。

### 2. 服务器端架构

服务器采用原生 C++ 开发，负责接收和广播剪贴板数据。其主要功能包括：

- 处理客户端连接管理。
- 转发剪贴板数据至所有在线设备。
- 可选身份认证机制（用户名/密码）。

## 使用方法

### 客户端编译

#### Windows:

```bash
mkdir build
cd build
C:\path\to\your\qt\bin\cmake.exe .. -G "MinGW Makefiles"
mingw32-make
powershell -ExecutionPolicy Bypass -File deploy.ps1
.\build\dmo.exe
```

#### Linux/macOS:

```bash
mkdir build
cd build
cmake ..
make
```

### 服务器端编译

安装依赖（示例）：

```bash
# Ubuntu/Debian
sudo apt-get install nlohmann-json3-dev

# Arch Linux
sudo pacman -S nlohmann-json
```

构建并运行：

```bash
mkdir build
cd build
cmake ..
make
./server -p <port> [-u <user> -w <password>]
```

## 未来优化方向

1. **安全性增强**
   - 引入 TLS 加密，防止中间人攻击和数据窃取。
2. **自动重连机制**
   - 解决网络波动导致的连接断开问题，提升稳定性。
3. **更广泛的跨平台支持**
   - 增强对 Android 和 iOS 设备的兼容性。

## 结语

Remote Clipboard 以轻量级、高效、跨平台的特性，解决了多设备间临时数据传输的痛点。未来，该项目将不断优化，朝着更安全、更智能的方向发展，助力高效开发与办公环境。
