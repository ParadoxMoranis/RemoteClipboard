#include "uilanguage.h"

#include <QApplication>
#include <QHash>
#include <QTranslator>

namespace {
class ChineseUiTranslator final : public QTranslator
{
public:
    QString translate(const char* context,
                      const char* sourceText,
                      const char* disambiguation = nullptr,
                      int n = -1) const override
    {
        Q_UNUSED(context)
        Q_UNUSED(disambiguation)
        Q_UNUSED(n)

        if (sourceText == nullptr) {
            return {};
        }

        static const QHash<QString, QString> translations = {
            {QStringLiteral("Remote Clipboard // Client"), QStringLiteral("远程剪贴板 // 客户端")},
            {QStringLiteral("SECURE SYNC // DESKTOP CLIENT"), QStringLiteral("安全同步 // 桌面客户端")},
            {QStringLiteral("REMOTE / CLIPBOARD"), QStringLiteral("远程 / 剪贴板")},
            {QStringLiteral("CROSS-DEVICE TRANSFER CHANNEL · v0.2"), QStringLiteral("跨设备传输通道 · v0.2")},
            {QStringLiteral("● STANDBY"), QStringLiteral("● 待机")},
            {QStringLiteral("◆ CONNECTING"), QStringLiteral("◆ 正在连接")},
            {QStringLiteral("◆ AUTHENTICATING"), QStringLiteral("◆ 正在认证")},
            {QStringLiteral("× RECONNECTING"), QStringLiteral("× 正在重连")},
            {QStringLiteral("× NETWORK ERROR"), QStringLiteral("× 网络错误")},
            {QStringLiteral("● SYNC ONLINE"), QStringLiteral("● 同步在线")},
            {QStringLiteral("× AUTH FAILED"), QStringLiteral("× 认证失败")},
            {QStringLiteral("DARK MODE ↗"), QStringLiteral("暗色模式 ↗")},
            {QStringLiteral("LIGHT MODE ↗"), QStringLiteral("亮色模式 ↗")},
            {QStringLiteral("CONNECTION // CONTROL"), QStringLiteral("连接 // 控制台")},
            {QStringLiteral("SERVER HOST"), QStringLiteral("服务器地址")},
            {QStringLiteral("host or IP address"), QStringLiteral("主机名或 IP 地址")},
            {QStringLiteral("PORT"), QStringLiteral("端口")},
            {QStringLiteral("USERNAME"), QStringLiteral("用户名")},
            {QStringLiteral("account name"), QStringLiteral("账户名称")},
            {QStringLiteral("PASSWORD"), QStringLiteral("密码")},
            {QStringLiteral("required"), QStringLiteral("必填")},
            {QStringLiteral("PROFILE"), QStringLiteral("档案")},
            {QStringLiteral("SETTINGS"), QStringLiteral("设置")},
            {QStringLiteral("HIDE"), QStringLiteral("隐藏")},
            {QStringLiteral("RECEIVE DIR"), QStringLiteral("接收目录")},
            {QStringLiteral("BROWSE"), QStringLiteral("浏览")},
            {QStringLiteral("USE TLS // OPTIONAL"), QStringLiteral("使用 TLS // 可选")},
            {QStringLiteral("CA CERT"), QStringLiteral("CA 证书")},
            {QStringLiteral("CONNECT // START"), QStringLiteral("连接 // 启动")},
            {QStringLiteral("DISCONNECT // STOP"), QStringLiteral("断开 // 停止")},
            {QStringLiteral("EVENT STREAM // LIVE"), QStringLiteral("事件流 // 实时")},
            {QStringLiteral("Waiting for connection events..."), QStringLiteral("等待连接事件...")},
            {QStringLiteral("Remote Clipboard // Settings"), QStringLiteral("远程剪贴板 // 设置")},
            {QStringLiteral("CONFIGURATION // WORKSPACE"), QStringLiteral("配置 // 工作区")},
            {QStringLiteral("CLIENT / SETTINGS"), QStringLiteral("客户端 / 设置")},
            {QStringLiteral("PROFILES · STARTUP · SHORTCUTS · TRANSPORT SECURITY"), QStringLiteral("档案 · 启动 · 快捷键 · 传输安全")},
            {QStringLiteral("SERVER PROFILES // LIST"), QStringLiteral("服务器档案 // 列表")},
            {QStringLiteral("ADD"), QStringLiteral("添加")},
            {QStringLiteral("REMOVE"), QStringLiteral("移除")},
            {QStringLiteral("PROFILE // DETAILS"), QStringLiteral("档案 // 详情")},
            {QStringLiteral("Profile Name"), QStringLiteral("档案名称")},
            {QStringLiteral("Server Host"), QStringLiteral("服务器地址")},
            {QStringLiteral("Port"), QStringLiteral("端口")},
            {QStringLiteral("Username"), QStringLiteral("用户名")},
            {QStringLiteral("Password"), QStringLiteral("密码")},
            {QStringLiteral("Enable TLS"), QStringLiteral("启用 TLS")},
            {QStringLiteral("CA Certificate"), QStringLiteral("CA 证书")},
            {QStringLiteral("CLIENT // BEHAVIOR"), QStringLiteral("客户端 // 行为")},
            {QStringLiteral("Receive Directory"), QStringLiteral("接收目录")},
            {QStringLiteral("Language"), QStringLiteral("语言")},
            {QStringLiteral("Launch on system startup"), QStringLiteral("系统启动时运行")},
            {QStringLiteral("Auto-connect last profile on startup"), QStringLiteral("启动时自动连接上次档案")},
            {QStringLiteral("Show Window Hotkey"), QStringLiteral("显示窗口快捷键")},
            {QStringLiteral("Switch Profile Hotkey"), QStringLiteral("切换档案快捷键")},
            {QStringLiteral("SAVE // APPLY"), QStringLiteral("保存 // 应用")},
            {QStringLiteral("CANCEL"), QStringLiteral("取消")},
            {QStringLiteral("Show"), QStringLiteral("显示")},
            {QStringLiteral("Switch Profile"), QStringLiteral("切换档案")},
            {QStringLiteral("Quit"), QStringLiteral("退出")},
            {QStringLiteral("Browse"), QStringLiteral("浏览")},
            {QStringLiteral("Add"), QStringLiteral("添加")},
            {QStringLiteral("Remove"), QStringLiteral("移除")},
            {QStringLiteral("Save Failed"), QStringLiteral("保存失败")},
            {QStringLiteral("Autostart setup failed: %1"), QStringLiteral("自动启动设置失败：%1")},
            {QStringLiteral("Directory Ready"), QStringLiteral("目录可用")},
            {QStringLiteral("Receive directory found:\n%1"), QStringLiteral("已找到接收目录：\n%1")},
            {QStringLiteral("Create Directory"), QStringLiteral("创建目录")},
            {QStringLiteral("The directory does not exist:\n%1\n\nCreate it now?"), QStringLiteral("目录不存在：\n%1\n\n现在创建吗？")},
            {QStringLiteral("Receive directory was not created"), QStringLiteral("未创建接收目录")},
            {QStringLiteral("Directory Created"), QStringLiteral("目录已创建")},
            {QStringLiteral("Directory created successfully:\n%1"), QStringLiteral("目录创建成功：\n%1")},
            {QStringLiteral("Create Failed"), QStringLiteral("创建失败")},
            {QStringLiteral("Failed to create directory:\n%1"), QStringLiteral("无法创建目录：\n%1")},
            {QStringLiteral("File Bundle Too Large"), QStringLiteral("文件包过大")},
            {QStringLiteral("Selected clipboard files exceed the small-file transfer threshold."), QStringLiteral("所选剪贴板文件超过小文件传输阈值。")},
            {QStringLiteral("Skipped unreadable file: %1"), QStringLiteral("已跳过无法读取的文件：%1")},
            {QStringLiteral("Sent %1 file(s) using chunked transfer"), QStringLiteral("已通过分块传输发送 %1 个文件")},
            {QStringLiteral("Receive directory is unavailable, skipped incoming files"), QStringLiteral("接收目录不可用，已跳过传入文件")},
            {QStringLiteral("Saved %1 file(s) to %2"), QStringLiteral("已将 %1 个文件保存到 %2")},
            {QStringLiteral("Receive directory is unavailable, skipped chunked transfer"), QStringLiteral("接收目录不可用，已跳过分块传输")},
            {QStringLiteral("Failed to open file for incoming transfer: %1"), QStringLiteral("无法打开传入文件：%1")},
            {QStringLiteral("Receiving file: %1"), QStringLiteral("正在接收文件：%1")},
            {QStringLiteral("Transfer target is not writable"), QStringLiteral("传输目标不可写")},
            {QStringLiteral("Failed while writing incoming chunk"), QStringLiteral("写入传入分块时失败")},
            {QStringLiteral("Saved file but failed to verify: %1"), QStringLiteral("文件已保存但校验失败：%1")},
            {QStringLiteral("Discarded file with mismatched size: %1"), QStringLiteral("已丢弃大小不匹配的文件：%1")},
            {QStringLiteral("Discarded file with mismatched hash: %1"), QStringLiteral("已丢弃哈希不匹配的文件：%1")},
            {QStringLiteral("Saved chunked file to %1"), QStringLiteral("分块文件已保存到 %1")},
            {QStringLiteral("Disconnect requested"), QStringLiteral("已请求断开连接")},
            {QStringLiteral("Missing Credentials"), QStringLiteral("缺少凭据")},
            {QStringLiteral("Please enter username and password."), QStringLiteral("请输入用户名和密码。")},
            {QStringLiteral("Connecting to server..."), QStringLiteral("正在连接服务器...")},
            {QStringLiteral("Sent text clipboard update"), QStringLiteral("已发送文本剪贴板更新")},
            {QStringLiteral("Sent %1 file(s) from clipboard"), QStringLiteral("已从剪贴板发送 %1 个文件")},
            {QStringLiteral("Transport connected, sending authentication"), QStringLiteral("传输通道已连接，正在发送认证信息")},
            {QStringLiteral("Connection lost"), QStringLiteral("连接已中断")},
            {QStringLiteral("Disconnected"), QStringLiteral("已断开连接")},
            {QStringLiteral("Network error: %1"), QStringLiteral("网络错误：%1")},
            {QStringLiteral("Authenticated successfully"), QStringLiteral("认证成功")},
            {QStringLiteral("Remote Clipboard"), QStringLiteral("远程剪贴板")},
            {QStringLiteral("Auto-start connected using the last saved server profile."), QStringLiteral("已使用上次保存的服务器档案自动连接。")},
            {QStringLiteral("Authentication Failed"), QStringLiteral("认证失败")},
            {QStringLiteral("Unknown error"), QStringLiteral("未知错误")},
            {QStringLiteral("Received text clipboard update"), QStringLiteral("已接收文本剪贴板更新")},
            {QStringLiteral("Select Receive Directory"), QStringLiteral("选择接收目录")},
            {QStringLiteral("Select CA Certificate"), QStringLiteral("选择 CA 证书")},
            {QStringLiteral("Certificate Files (*.pem *.crt *.cer);;All Files (*)"), QStringLiteral("证书文件 (*.pem *.crt *.cer);;所有文件 (*)")},
            {QStringLiteral("Reconnect attempt %1 scheduled in %2 seconds"), QStringLiteral("第 %1 次重连将在 %2 秒后进行")},
            {QStringLiteral("Settings updated. Config file: %1"), QStringLiteral("设置已更新。配置文件：%1")},
            {QStringLiteral("The client is still running in the background."), QStringLiteral("客户端仍在后台运行。")},
            {QStringLiteral("Switched to profile: %1"), QStringLiteral("已切换到档案：%1")},
            {QStringLiteral("Missing Profile"), QStringLiteral("缺少档案")},
            {QStringLiteral("Please keep at least one server profile."), QStringLiteral("请至少保留一个服务器档案。")},
            {QStringLiteral("Profile %1"), QStringLiteral("档案 %1")},
            {QStringLiteral("Cannot Remove"), QStringLiteral("无法移除")},
            {QStringLiteral("At least one profile must remain."), QStringLiteral("必须至少保留一个档案。")}
        };

        const auto translation = translations.constFind(QString::fromUtf8(sourceText));
        return translation == translations.cend() ? QString() : *translation;
    }
};

ChineseUiTranslator chineseTranslator;
bool chineseTranslatorInstalled = false;
}

QString normalizedUiLanguage(const QString& language)
{
    return language.compare(QStringLiteral("zh_CN"), Qt::CaseInsensitive) == 0
        ? QStringLiteral("zh_CN")
        : QStringLiteral("en");
}

void applyUiLanguage(QApplication& application, const QString& language)
{
    if (chineseTranslatorInstalled) {
        application.removeTranslator(&chineseTranslator);
        chineseTranslatorInstalled = false;
    }

    if (normalizedUiLanguage(language) == QStringLiteral("zh_CN")) {
        chineseTranslatorInstalled = application.installTranslator(&chineseTranslator);
    }
}
