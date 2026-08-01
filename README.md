# Remote Clipboard

Remote Clipboard 是一个 C++17/Qt 跨设备剪贴板同步项目，提供 Linux Wayland GUI、Linux CLI、Windows GUI，以及共享同一服务端核心的 64 MiB / 512 MiB 两种服务端产物。

当前仓库处于 **v0.2 桌面体验版本**。本版本沿用 JSON Lines v1 的安全与文件完整性基线，新增共享锐利风格界面、亮色/暗色主题、中英双语和更可靠的窗口尺寸适配；JWT、设备配对、Protobuf v2、离线队列和插件系统属于后续里程碑，尚未实现。安全基线边界见 [`docs/18_remediation/v0.1-remediation-plan.md`](docs/18_remediation/v0.1-remediation-plan.md)。

## v0.2 能力

- UTF-8 文本剪贴板广播
- 小文件 `file_bundle` 传输与 Base64/SHA-256 校验
- 大文件 512 KiB 顺序分块、大小校验、SHA-256 校验和原子提交
- TLS 1.3 服务端和严格证书校验客户端
- 有界 JSON Lines 帧、连接数、TLS 握手数、发送队列和会话上传量
- 可 join 的会话线程与确定的停服语义
- SQLite WAL 事件元数据、schema migration 和参数化 SQL
- Windows/Linux 根构建、协议契约、注入回归和真实服务端集成测试
- Linux/Windows 共享 Qt 界面、亮色/暗色主题和 English/简体中文切换
- 主窗口与设置窗口在较小尺寸下的重排、伸缩和滚动适配

## 安全默认值

生产配置默认要求 TLS 1.3、用户名和外部密码来源。服务端不打印密码，也不把密码写入 JSON 配置。GUI/CLI 读取旧配置中的密码用于一次迁移，但后续保存会移除明文密码。

明文模式和忽略证书错误只允许在显式开发 profile 中使用：

```bash
remote-clipboard-server-64mb \
  --development \
  --no-tls \
  --storage-dir ./received_files \
  --sqlite ./remote_clipboard.sqlite3
```

该命令在开发模式下使用兼容凭据 `admin/admin`。不要把开发模式暴露到不可信网络。

生产运行应通过权限为 `0600` 的 secret 文件或 `REMOTE_CLIPBOARD_PASSWORD` 注入密码：

```bash
remote-clipboard-server-64mb \
  --username clipboard-service \
  --password-file /run/secrets/remote_clipboard_password \
  --tls-cert /etc/remote-clipboard/server.crt \
  --tls-key /etc/remote-clipboard/server.key \
  --storage-dir /var/lib/remote-clipboard/files \
  --sqlite /var/lib/remote-clipboard/events.sqlite3
```

服务端配置优先级为命令行、`REMOTE_CLIPBOARD_*` 环境变量、版本化 JSON 配置、内置默认值。配置写入使用临时文件并保留 `.bak` 备份；保存权限在 POSIX 平台收紧为 `0600`。

## 构建

根 `CMakeLists.txt` 是推荐且由 CI 验证的构建入口。Linux 依赖示例：

```bash
sudo apt-get install \
  build-essential cmake \
  ninja-build \
  libssl-dev libsqlite3-dev nlohmann-json3-dev \
  qt6-base-dev qt6-tools-dev-tools wl-clipboard
```

配置、构建并测试：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Linux 默认产物：

- `build/remote-clipboard-server-64mb`
- `build/remote-clipboard-server-512mb`
- `build/remote-clipboard-cli`
- `build/remote-clipboard-gui`

根构建使用全局唯一 CMake target：

- `remote_clipboard_server_64`
- `remote_clipboard_server_512`
- `remote_clipboard_linux_gui`
- `remote_clipboard_windows_gui`（仅 Windows）

常用选项：

```bash
cmake -S . -B build \
  -DRC_BUILD_SERVERS=ON \
  -DRC_BUILD_CLI=ON \
  -DRC_BUILD_WAYLAND_GUI=ON \
  -DRC_BUILD_WINDOWS_GUI=OFF \
  -DRC_BUILD_TESTS=ON
```

Windows 的服务器与测试依赖记录在 `vcpkg.json`；Windows GUI 由宿主平台显式启用，不再硬编码本机 Qt 或编译器路径。

### GitHub Windows 构建

`.github/workflows/ci.yml` 使用 GitHub 托管的 Windows runner 和 MSYS2 UCRT64 验证完整 Windows 构建。每次 push、pull request 或手动运行都会：

- 构建 Windows GUI、64 MiB / 512 MiB 服务端和全部测试；
- 在真实 Windows 环境执行四个 CTest 目标；
- 使用 `windeployqt6` 部署 Qt 插件及运行库；
- 上传 `RemoteClipboard-client-windows-x64` 客户端制品，其中包含可分发 ZIP。

已登录 GitHub CLI 时可手动触发并监控：

```bash
gh workflow run ci.yml --ref main
gh run watch
```

最近一次成功构建的 Windows 制品可下载到当前目录：

```bash
gh run download --name RemoteClipboard-client-windows-x64
```

若 `gh auth status` 显示未登录，先在交互式终端运行 `gh auth login`。工作流必须先提交并推送到目标分支，GitHub 才能执行该版本。

## 发布包

`.github/workflows/release.yml` 从版本 tag 构建、测试并发布下列 x86_64 / amd64 制品：

| 用途 | 平台 | 制品 |
| --- | --- | --- |
| 服务端 | Debian 12 | `remote-clipboard-server_<version>_amd64.deb` |
| 服务端 | Fedora 42 | `remote-clipboard-server-<version>-1.x86_64.rpm` |
| Linux 客户端 | Debian 12 | `remote-clipboard-client_<version>_amd64.deb` |
| Linux 客户端 | Fedora 42 | `remote-clipboard-client-<version>-1.x86_64.rpm` |
| Linux 客户端 | Arch Linux | `remote-clipboard-client-<version>-1-x86_64.pkg.tar.zst` |
| Windows 客户端 | Windows x64 | `RemoteClipboard-client-windows-x64.zip` |
| 服务端与客户端源码 | 通用 | `RemoteClipboard-source-<version>.zip` |

Linux 原生包安装示例：

```bash
sudo apt install ./remote-clipboard-server_0.2.0_amd64.deb
sudo dnf install ./remote-clipboard-client-0.2.0-1.x86_64.rpm
sudo pacman -U ./remote-clipboard-client-0.2.0-1-x86_64.pkg.tar.zst
```

统一源码 ZIP 同时包含服务端和 Linux 客户端构建脚本。安装 README“构建”章节列出的依赖后运行：

```bash
./scripts/build-server-from-source.sh
./scripts/build-client-from-source.sh
```

脚本默认运行测试并分别暂存到 `.dist/server/`、`.dist/client/`；可通过 `RUN_TESTS=0` 跳过测试。Arch Linux 本地包也可在普通用户下执行 `./scripts/build-arch-client-package.sh` 重新生成。

每个 Release 附带 `SHA256SUMS`，tag 构建还生成 GitHub/Sigstore 构建证明：

```bash
sha256sum --check SHA256SUMS
gh attestation verify RemoteClipboard-client-windows-x64.zip \
  --repo ParadoxMoranis/RemoteClipboard
```

## 快速验证

启动开发服务端：

```bash
./build/remote-clipboard-server-64mb \
  --development --no-tls \
  --storage-dir ./received_files \
  --sqlite ./remote_clipboard.sqlite3
```

启动开发 CLI：

```bash
./build/remote-clipboard-cli \
  --development \
  --host 127.0.0.1 \
  --receive-dir "$PWD/received_cli"
```

生产 CLI 默认使用 TLS 1.3、系统信任链和主机名验证：

```bash
./build/remote-clipboard-cli \
  --host clipboard.example.com \
  --username alice \
  --password-file /run/secrets/remote_clipboard_password \
  --tls-ca /etc/remote-clipboard/ca.pem \
  --receive-dir "$PWD/received_cli"
```

GUI 默认开启 TLS 且不忽略证书错误。密码只保留在当前进程内；重新启动后需再次输入，直到后续版本接入平台密钥环。

## v1 协议

v1 使用 TCP/TLS 上的 UTF-8 JSON Lines，每条消息以 `\n` 结束。唯一共享契约实现在 `client_common/protocol.*`，服务端和 CLI 均使用该实现。

已支持消息：

- `auth` / `auth_response`
- `clipboard_text`
- `file_bundle`
- `file_transfer_start`
- `file_transfer_chunk`
- `file_transfer_complete`
- `ping` / `pong`
- `error`

分块字段固定为：

- start：`transfer_id`、`name`、无符号 `size`、可选 `sha256` / `mime`
- chunk：`transfer_id`、从 `0` 开始严格递增的无符号 `seq`、`data`
- complete：`transfer_id`

完整帧超过服务端上限时，连接状态会被清空并断开；未知 transfer、错误 Base64、超量、跳号、重复、大小或摘要不匹配都不会广播。文件先写入 `.transfers` 工作目录，只有最终大小与 SHA-256 通过后才原子移动到接收目录。

## SQLite 元数据

`--sqlite <path>` 启用 SQLite WAL 元数据。v0.1 记录成功接收的 `clipboard_text`、已验证 bundle 以及有效分块控制事件，不保存剪贴板正文到数据库。

核心字段包括 workspace、event id、device、sender、消息类型、sequence、内容 SHA-256、负载大小和时间。唯一约束 `(workspace_id, event_id)` 提供 v1 可用的幂等入口；所有不可信字段均使用 `sqlite3_bind_*` 参数绑定。

## 测试证据

CTest 当前包含：

- `frame_parser_test`：带/不带换行的超限帧和边界帧
- `protocol_contract_test`：共享 v1 构造与真实服务端字段契约
- `sqlite_storage_test`：单引号、分号和恶意 device/sender 的注入回归
- `tcpserver_integration_test`：真实服务端认证、广播、拒绝无效文件、乱序/重复分块、超限断开、SQLite 写入、TLS 慢握手并发接入和反复启停

真实集成测试只绑定 `127.0.0.1` 的随机空闲端口。核心格式门禁可单独运行：

```bash
bash scripts/check-format.sh
```

## Linux 安装与 Debian 12

自动识别发行版并安装依赖、构建根项目：

```bash
./scripts/install-linux.sh
```

也可使用 `install-debian.sh`、`install-arch.sh`、`install-fedora.sh` 或 `install-opensuse.sh`。默认安装到 `/usr/local/bin`。

Linux 二进制依赖构建环境的 `glibc`。面向 Debian 12 分发时，应在 Debian 12 本机构建，或使用容器脚本：

```bash
./scripts/build-debian12-container.sh
```

产物位于 `.dist/debian12/`，包含两个服务端、CLI 和 GUI。不要把仓库中的旧 `build/` 或 `.build-linux/` 产物当作发布证据。

## 仓库结构

- `client_common`：共享 v1 协议契约
- `server_common`：帧、路由、会话、文件、SQLite 和配置核心
- `gui_common`：GUI 配置、设置和自启动适配
- `RemoteClipboardServer-*`：两个薄服务端入口
- `RemoteClipboard_Linux_wayland`：Linux Wayland GUI
- `RemoteClipboardWindows`：Windows GUI
- `RemoteclipboardCliForLinux`：Linux CLI
- `tests`：单元、契约、安全和真实服务端集成测试
- `docs`：产品、架构、安全、协议、测试和路线图文档

GUI 目录中的旧 `tcpserver.*` / `server_main.cpp` 以及服务器目录中的重复实现已移除；`server_common` 是唯一服务端实现。

## 当前限制

- v1 仍是单工作区广播兼容协议，不具备设备级授权或离线可靠投递。
- 密码认证仍是 bootstrap 方案；生产身份、JWT 和设备证明属于后续里程碑。
- SQLite 当前只承担事件元数据，不提供用户可见历史 API。
- Windows CI 构建 GUI、两个服务端和全部测试；Release ZIP 只分发带完整 Qt/MinGW 运行库的 x64 客户端。
- Release 提供校验和与 GitHub/Sigstore 构建证明；后续仍需补充 SBOM 和自动更新机制。
- 仓库尚未声明许可证，公开分发前需完成依赖许可证审查并补充许可证。
