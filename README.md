# Remote Clipboard

Remote Clipboard 是一个轻量级的跨设备剪贴板同步工具，面向日常办公与开发场景，支持在多台设备之间同步文本内容与文件内容。项目当前包含 Linux Wayland GUI 客户端、Linux CLI 客户端、Windows GUI 客户端，以及两个不同消息缓冲上限的原生 C++ 服务端实现。

## 项目目标

Remote Clipboard 致力于解决多设备协作中的一个高频问题：在不同系统、不同桌面环境之间，快速、稳定地共享临时文本和文件，而不依赖聊天软件、云笔记或重型同步服务。

项目设计重点如下：

- 低依赖、轻量化，基于直接网络通信完成同步
- 面向多平台使用场景，提供 GUI 与 CLI 两类客户端
- 默认开箱即用，TLS 作为可选能力按需启用
- 在网络波动场景下具备自动重连与心跳保活能力
- 支持文件落盘与服务端保留策略，便于审计和运维

## 核心特性

- 支持文本剪贴板同步
- 支持文件剪贴板同步
- 客户端可配置接收目录，并将收到的文件保存到本地指定路径
- 服务端可配置接收目录，并支持基于天数的文件保留与自动清理
- 默认使用明文 TCP，TLS 可选启用
- 客户端支持心跳检测、断线重连和退避重试
- 提供 Debian/Ubuntu、Arch、Fedora、openSUSE 安装脚本

## 仓库结构

- `RemoteClipboard_Linux_wayland`
  Linux Wayland GUI 客户端
- `RemoteclipboardCliForLinux`
  Linux CLI 客户端
- `RemoteClipboardWindows`
  Windows GUI 客户端
- `RemoteClipboardServer-64MB`
  适合较小消息负载的服务端版本
- `RemoteClipboardServer-512MB`
  适合更大消息负载的服务端版本
- `server_common`
  服务端共享传输层与文件处理实现
- `scripts`
  Linux 发行版安装脚本

## 协议与传输

当前实现使用“每行一条 JSON”的分帧协议。主要消息类型包括：

- `auth`
- `auth_response`
- `clipboard_text`
- `file_bundle`
- `file_transfer_start`
- `file_transfer_chunk`
- `file_transfer_complete`
- `ping`
- `pong`

其中：

- 文本内容通过 `clipboard_text` 传输
- 小文件内容通过 `file_bundle` 传输
- 大文件内容通过 `file_transfer_start/chunk/complete` 分片传输
- `file_bundle.files[*].data` 和 `file_transfer_chunk.data` 都采用 Base64 表示

当前默认策略如下：

- 4MB 以下文件优先走 `file_bundle`
- 4MB 及以上文件自动切换到分片传输
- 默认分片大小为 512KB

这样做是为了兼顾图片类文件的高频传输和 `64MB` 服务端版本的内存安全边界。

## TLS 设计

TLS 为可选能力，客户端与服务端默认均关闭 TLS，以降低首次部署门槛。需要加密传输时，可由用户手动启用。

### 服务端启用 TLS

```bash
./RemoteClipboardServer \
  --tls \
  --tls-cert /path/to/server.crt \
  --tls-key /path/to/server.key
```

### 客户端启用 TLS

- GUI 客户端：勾选 `Use TLS (optional)`
- CLI 客户端：增加 `--tls`

### 证书校验说明

为兼顾易用性与部署成本，当前 GUI 和 CLI 客户端在启用 TLS 后默认允许较宽松的证书校验策略，适合内网、自签名证书和快速试运行场景。若需要严格校验服务端证书，请显式提供 CA 文件：

- GUI 客户端：填写 `CA Cert`
- CLI 客户端：使用 `--tls-ca /path/to/ca.pem --strict-tls`

### 自签名证书示例

```bash
openssl req -x509 -nodes -newkey rsa:2048 \
  -keyout server.key \
  -out server.crt \
  -days 365
```

## 文件接收与保留策略

### 客户端

客户端收到文件后，会将其写入用户指定目录。

- Linux GUI / Windows GUI：通过设置界面配置，支持手动输入路径或文件管理器选择目录
- Linux CLI：首次启动时提示输入目录，后续默认读取配置文件

若目录不存在：

- GUI 客户端会询问是否创建，并提示创建结果
- CLI 客户端会在终端询问是否创建，并提示创建结果

### 服务端

服务端同样会保存收到的文件，并支持保留周期控制：

- `--storage-dir /path/to/dir`
  指定服务端文件保存目录
- `--retention-days N`
  指定文件保留天数
- `--retention-days -1`
  表示不限时保留

服务端接收到文件后会执行以下流程：

1. 将文件保存到服务端本地目录。
2. 将消息广播给其他已认证客户端。
3. 清理超过保留天数的历史文件。

## 断线重连机制

客户端针对以下情况实现了自动恢复能力：

- 短时网络抖动
- 服务端重启
- 心跳超时
- TLS 连接中断

重连采用递增退避策略，以避免在服务端故障或网络不稳定时产生持续高频连接请求。

## 快速开始

### 1. 启动服务端

明文模式示例：

```bash
./RemoteClipboardServer \
  --port 8080 \
  --username admin \
  --password admin \
  --storage-dir ./server-files \
  --retention-days 7
```

TLS 模式示例：

```bash
./RemoteClipboardServer \
  --port 8080 \
  --username admin \
  --password admin \
  --storage-dir ./server-files \
  --retention-days 7 \
  --tls \
  --tls-cert ./server.crt \
  --tls-key ./server.key
```

### 2. 启动 Linux CLI 客户端

```bash
./clipboard_sync \
  --host 127.0.0.1 \
  --port 8080 \
  --username admin \
  --password admin \
  --receive-dir ~/Downloads/RemoteClipboard
```

启用 TLS：

```bash
./clipboard_sync \
  --host 127.0.0.1 \
  --port 8080 \
  --username admin \
  --password admin \
  --receive-dir ~/Downloads/RemoteClipboard \
  --tls
```

### 3. 启动 GUI 客户端

GUI 客户端启动后，填写以下信息即可连接：

- 服务端地址
- 端口
- 用户名与密码
- 接收目录
- TLS 相关选项（可选）

认证成功后，客户端会开始监听本地剪贴板，并自动同步文本或文件内容。

## 配置文件

项目现在为 GUI、CLI 和服务端都引入了正式的 JSON 配置文件。

### GUI 客户端

- Windows GUI：`AppConfigLocation/RemoteClipboardWindowsClient/config.json`
- Linux GUI：`AppConfigLocation/RemoteClipboardLinuxClient/config.json`

GUI 配置文件保存以下内容：

- 多组服务器分组
- 当前分组与上次成功连接分组
- 接收目录
- 自启动开关
- 自启动后是否自动使用上次连接
- 快捷键

### Linux CLI

- `~/.config/RemoteClipboard/linux-cli/config.json`

CLI 配置文件保存默认服务器连接参数和默认接收目录。若要更改默认接收目录，也可以直接编辑这个文件。

### 服务端

- 64MB 版默认：`~/.config/RemoteClipboard/server-64mb/config.json`
- 512MB 版默认：`~/.config/RemoteClipboard/server-512mb/config.json`

也可以通过 `--config-name <name>` 使用不同命名空间的配置文件。

## 设置能力

GUI 客户端现在支持：

- 多组中转广播服务器分组管理与快速切换
- 设置接收文件目录
- 自启动注册
- 自启动后默认使用上一次成功连接信息
- 快捷键设置

快捷键说明：

- Windows：支持全局热键弹出窗口、切换配置
- Linux Wayland：提供窗口内快捷键和托盘切换；全局热键受桌面环境与 Wayland 限制

## 构建指南

### Linux GUI 客户端

依赖示例：

```bash
# Debian / Ubuntu
sudo apt-get install qt6-base-dev qt6-tools-dev-tools wl-clipboard
```

构建：

```bash
cmake -S RemoteClipboard_Linux_wayland -B build/linux-gui
cmake --build build/linux-gui -j
```

### Linux CLI 客户端

依赖示例：

```bash
# Debian / Ubuntu
sudo apt-get install cmake nlohmann-json3-dev libssl-dev wl-clipboard
```

构建：

```bash
cmake -S RemoteclipboardCliForLinux -B build/linux-cli
cmake --build build/linux-cli -j
```

### 服务端

依赖示例：

```bash
# Debian / Ubuntu
sudo apt-get install cmake nlohmann-json3-dev libssl-dev
```

构建 `64MB` 版本：

```bash
cmake -S RemoteClipboardServer-64MB -B build/server-64
cmake --build build/server-64 -j
```

构建 `512MB` 版本：

```bash
cmake -S RemoteClipboardServer-512MB -B build/server-512
cmake --build build/server-512 -j
```

## Linux 安装脚本

项目提供了按发行版划分的安装脚本，以及一个自动识别入口脚本：

- `scripts/install-linux.sh`
- `scripts/install-debian.sh`
- `scripts/install-arch.sh`
- `scripts/install-fedora.sh`
- `scripts/install-opensuse.sh`

自动识别发行版并安装：

```bash
./scripts/install-linux.sh
```

也可以直接执行对应发行版脚本，例如：

```bash
./scripts/install-debian.sh
```

安装完成后，二进制文件会被放置到 `/usr/local/bin`：

- `remote-clipboard-server-64mb`
- `remote-clipboard-server-512mb`
- `remote-clipboard-cli`
- `remote-clipboard-gui`

## 运行建议

- 内网快速部署场景可以直接使用默认明文模式
- 对传输安全有要求时，建议启用 TLS，并为客户端配置 CA 校验
- 若主要同步文本内容，可优先使用 `64MB` 服务端
- 若存在更大的文件传输需求，可使用 `512MB` 服务端
- 若需要长期运行，建议将服务端纳入 systemd 或其他进程管理工具

## 当前状态与验证范围

目前已在本地完成以下模块的构建验证：

- `RemoteClipboardServer-64MB`
- `RemoteClipboardServer-512MB`
- `RemoteclipboardCliForLinux`
- `RemoteClipboard_Linux_wayland`

Windows GUI 代码已经同步到当前协议与功能模型，但在当前开发环境下未完成本机构建验证。因此，README 当前主要提供 Linux 侧的构建与运行说明。

## 后续可继续完善的方向

- 更细粒度的权限与访问控制
- 更完善的文件类型与大文件传输策略
- 更清晰的客户端状态提示与日志输出
- 更完整的 Windows 与 macOS 构建和发行说明

## License

当前仓库尚未声明许可证。如计划公开分发或接受外部贡献，建议补充明确的开源许可证文件。
