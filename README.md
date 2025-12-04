# SIP Claw Game Controller (极限蓄能)

这是一个基于 ESP32 的互动游戏控制器项目。玩家通过手机或电脑浏览器访问游戏页面，快速点击按钮积蓄能量。当能量充满时，ESP32 会触发继电器动作（模拟投币信号），启动抓娃娃机或其他设备。

## 功能特点

*   **Web 互动游戏**: 响应式的 HTML5 游戏界面，支持手机和电脑端。
*   **参数可配置**: 通过管理后台调整游戏难度（能量衰减速度、点击增加量、游戏时长）。
*   **WiFi 管理**: 支持连接现有 WiFi 网络，连接失败自动切换为 AP 热点模式。
*   **持久化存储**: 游戏参数和网络配置保存在 ESP32 的 NVS 中，掉电不丢失。
*   **硬件控制**: 游戏成功后自动触发继电器脉冲。
*   **mDNS 支持**: 可以通过 `http://claw-service.local` 访问设备，无需记忆 IP 地址。

## 硬件需求

*   ESP32 开发板 (如 Seeed Studio XIAO ESP32C3/S3 或其他标准 ESP32)
*   继电器模块 (连接到 GPIO D0)
*   物理按钮 (可选，连接到 GPIO 0，用于测试触发)
*   抓娃娃机或其他投币设备

## 快速开始

### 1. 环境准备

本项目使用 [PlatformIO](https://platformio.org/) 进行开发。请确保你已经安装了 VS Code 和 PlatformIO 插件。

### 2. 编译与上传

1.  克隆或下载本项目。
2.  在 VS Code 中打开项目文件夹。
3.  连接 ESP32 开发板。
4.  **上传文件系统**: 游戏页面和管理页面存储在 LittleFS 文件系统中。
    *   **方式一 (侧边栏)**: 点击侧边栏 PlatformIO 图标 -> `Project Tasks` -> `Platform` -> `Upload Filesystem Image`
    *   **方式二 (终端)**: 运行命令 `pio run -t uploadfs`
5.  **上传固件**:
    *   **方式一 (侧边栏)**: 点击侧边栏 PlatformIO 图标 -> `Project Tasks` -> `General` -> `Upload`
    *   **方式二 (终端)**: 运行命令 `pio run -t upload`

### 3. 连接与使用

**首次使用 / WiFi 连接失败时:**
1.  设备会启动 AP 热点模式。
2.  搜索并连接 WiFi: `Claw-Game-Fallback`
3.  密码: `12345678`
4.  浏览器访问: `http://192.168.4.1` 或 `http://claw-service.local`

**正常模式:**
1.  设备连接到预设的 WiFi (默认 SSID: `SEEED-MKT`, 密码: `edgemaker2023`)。
2.  确保你的手机/电脑在同一局域网内。
3.  浏览器访问: `http://claw-service.local`

## 管理后台

访问 `http://claw-service.local/admin` 进入管理页面。

*   **默认密码**: `admin`
*   **可配置项**:
    *   **能量衰减速度 (Decay Speed)**: 能量自动减少的速度，值越大越难。
    *   **点击增加能量 (Tap Power)**: 每次点击增加的能量值。
    *   **游戏时长**: 游戏倒计时时间。
    *   **WiFi 设置**: 修改设备连接的 WiFi 名称和密码 (重启生效)。
    *   **管理员密码**: 修改后台登录密码。

## API 接口

*   `GET /`: 游戏主页
*   `GET /admin`: 管理后台页面
*   `GET /insert_coin`: 触发投币脉冲 (返回 JSON)
*   `GET /api/config`: 获取当前游戏配置
*   `POST /api/admin/verify`: 验证管理员密码
*   `POST /api/admin/save`: 保存配置参数

## 引脚定义

| 功能 | 引脚 (Arduino) | 说明 |
| :--- | :--- | :--- |
| 继电器 | D0 | 高电平触发 (可配置) |
| 按钮 | 0 | 低电平触发 (内置上拉) |

## 开发说明

*   前端代码位于 `data/` 目录 (`game.html`, `admin.html`)。
*   核心逻辑位于 `src/main.cpp`。
*   修改前端代码后，必须重新执行 **Upload Filesystem Image**。
