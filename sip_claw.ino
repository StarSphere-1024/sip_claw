#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

#define SERIAL1_TX_PIN D6
#define SERIAL1_RX_PIN D7

const char *sta_ssid = "LZF";      // 在这里填入你的 WiFi 名称
const char *sta_password = "00008888";  // 在这里填入你的 WiFi 密码

const char *ap_ssid = "ClawMachine-AP"; // AP 热点名称
const char *ap_password = "12345678";   // AP 热点密码（至少8位）

// Web 服务器
WebServer server(80);

// 定义命令结构体
struct Command {
  const char* name;
  const byte* cmd_pgm;
  size_t len;
};

// --- 命令定义 ---
// 所有命令都存储在 PROGMEM (闪存) 中以节省 RAM.
// 格式: { "命令名称", (const byte[]) { 协议字节... }, 字节数 }

const Command commands[] = {
  {"connect",       (const byte[]){0x8A, 0x03, 0x01, 0x01, 0x05, 0x55}, 6},
  {"start",         (const byte[]){0x8A, 0x03, 0x02, 0x01, 0x06, 0x55}, 6},
  {"left",          (const byte[]){0x8A, 0x03, 0x03, 0x01, 0x07, 0x55}, 6},
  {"right",         (const byte[]){0x8A, 0x03, 0x04, 0x01, 0x08, 0x55}, 6},
  {"forward",       (const byte[]){0x8A, 0x03, 0x05, 0x01, 0x09, 0x55}, 6},
  {"backward",      (const byte[]){0x8A, 0x03, 0x06, 0x01, 0x0A, 0x55}, 6},
  {"stop",          (const byte[]){0x8A, 0x03, 0x07, 0x01, 0x0B, 0x55}, 6},
  {"grab200",       (const byte[]){0x8A, 0x04, 0x08, 0x01, 0xC8, 0xDD, 0x55}, 7},
  {"airclose160",   (const byte[]){0x8A, 0x04, 0x09, 0x01, 0xA0, 0xAE, 0x55}, 7},
  {"airopen",       (const byte[]){0x8A, 0x03, 0x0A, 0x01, 0x0E, 0x55}, 6},
  {"requeststatus", (const byte[]){0x8A, 0x03, 0x0C, 0x01, 0x10, 0x55}, 6},
  {"coinexample",   (const byte[]){0x8A, 0x04, 0x0F, 0x01, 0x01, 0x15, 0x55}, 7},
  {"basicsettings", (const byte[]){0x8A, 0x13, 0x10, 0x01, 0x01, 0x01, 0x00, 0x01, 0x01, 0x01, 0x14, 0x02, 0x0A, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x4F, 0x55}, 22},
  {"clawvoltage",   (const byte[]){0x8A, 0x13, 0x11, 0x01, 0x01, 0x0E, 0x00, 0x41, 0x00, 0x41, 0x01, 0x0E, 0x08, 0x08, 0x00, 0x0C, 0x02, 0x01, 0x00, 0x01, 0xE5, 0x55}, 22},
  {"motorspeed",    (const byte[]){0x8A, 0x06, 0x12, 0x01, 0x02, 0x02, 0x02, 0x1F, 0x55}, 9},
  {"cleardata",     (const byte[]){0x8A, 0x06, 0x13, 0x01, 0x00, 0x00, 0x00, 0x1A, 0x55}, 9},
  {"factoryreset",  (const byte[]){0x8A, 0x05, 0x14, 0x01, 0x00, 0x00, 0x1A, 0x55}, 8},
  {"queryaccount",  (const byte[]){0x8A, 0x03, 0x15, 0x01, 0x19, 0x55}, 6},
  {"querybasic",    (const byte[]){0x8A, 0x03, 0x16, 0x01, 0x1A, 0x55}, 6},
  {"queryclaw",     (const byte[]){0x8A, 0x03, 0x17, 0x01, 0x1B, 0x55}, 6},
  {"querymotor",    (const byte[]){0x8A, 0x03, 0x18, 0x01, 0x1C, 0x55}, 6},
  {"reset",         (const byte[]){0x8A, 0x03, 0x19, 0x01, 0x1D, 0x55}, 6},
  {"joystick",      (const byte[]){0x8A, 0x04, 0x1A, 0x01, 0xFE, 0x1D, 0x55}, 7},
  {"queryerror",    (const byte[]){0x8A, 0x03, 0x1B, 0x01, 0x1F, 0x55}, 6},
  {"outputport",    (const byte[]){0x8A, 0x04, 0x1C, 0x01, 0x00, 0x21, 0x55}, 7}
};
const int numCommands = sizeof(commands) / sizeof(commands[0]);

const char HTML_CONTENT[] = R"rawliteral(<!DOCTYPE html><html><head><title>抓娃娃机控制面板</title><meta charset="UTF-8"><style>body { font-family: Arial, sans-serif; margin: 20px; } button { padding: 10px 20px; margin: 5px; background-color: #4CAF50; color: white; border: none; cursor: pointer; } button:hover { background-color: #45a049; } input[type="text"] { padding: 10px; margin: 5px; width: 200px; } select { padding: 10px; margin: 5px; } .status { color: green; font-weight: bold; }</style></head><body><h1>抓娃娃机控制面板</h1><p class="status">连接状态: 已就绪</p><h2>基本控制命令</h2><button onclick="sendCommand('connect')">连接下位机</button><button onclick="sendCommand('start')">开始游戏</button><button onclick="sendCommand('left')">向左移动</button><button onclick="sendCommand('right')">向右移动</button><button onclick="sendCommand('forward')">向前移动</button><button onclick="sendCommand('backward')">向后移动</button><button onclick="sendCommand('stop')">停止移动</button><button onclick="sendCommand('grab200')">执行抓取 (抓力200)</button><button onclick="sendCommand('airclose160')">悬空收拢 (抓力160)</button><button onclick="sendCommand('airopen')">悬空张开</button><button onclick="sendCommand('requeststatus')">请求状态</button><button onclick="sendCommand('coinexample')">发送币数示例</button><h2>设置命令</h2><button onclick="sendCommand('basicsettings')">基础数据设置</button><button onclick="sendCommand('clawvoltage')">爪力电压设置</button><button onclick="sendCommand('motorspeed')">马达速度设置</button><button onclick="sendCommand('cleardata')">资料清除</button><button onclick="sendCommand('factoryreset')">恢复工厂设置</button><h2>查询命令</h2><button onclick="sendCommand('queryaccount')">查账</button><button onclick="sendCommand('querybasic')">查询基础数据</button><button onclick="sendCommand('queryclaw')">查询爪力电压</button><button onclick="sendCommand('querymotor')">查询马达速度</button><button onclick="sendCommand('reset')">复位</button><button onclick="sendCommand('joystick')">摇杆示例</button><button onclick="sendCommand('queryerror')">查询错误状态</button><button onclick="sendCommand('outputport')">输出端口示例</button><h2>自定义输入</h2><input type="text" id="customCmd" placeholder="输入自定义命令 (如: start)"><button onclick="sendCustom()">发送自定义命令</button><p id="response"></p><script>function sendCommand(cmd) {    fetch('/command?cmd=' + cmd).then(function(response) { return response.text(); }).then(function(data) {        document.getElementById('response').innerHTML = '回应: ' + data;    });}function sendCustom() {    var cmd = document.getElementById('customCmd').value;    if (cmd) {        sendCommand(cmd);    }}</script></body></html>)rawliteral";

// 尝试连接到指定的 WiFi，如果失败则启动 AP 模式
void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(sta_ssid, sta_password);
  Serial.print("正在连接到 WiFi: ");
  Serial.println(sta_ssid);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) { // 尝试连接10秒
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi 连接成功!");
    Serial.print("IP 地址: ");
    Serial.println(WiFi.localIP());
    Serial.println("在浏览器输入以上 IP 地址访问 Web 控制页面。");
  } else {
    Serial.println("\nWiFi 连接失败。正在启动 AP 模式...");
    setupAP(); // 连接失败，启动 AP
  }
}

// AP 模式连接函数
void setupAP()
{
  WiFi.mode(WIFI_AP); // 明确设置模式
  Serial.print("正在设置 AP 热点: ");
  Serial.println(ap_ssid);

  // 设置 AP 模式
  WiFi.softAP(ap_ssid, ap_password);

  // 等待 AP 启动
  delay(100); // 短暂等待

  // AP 设置成功后，输出行成功消息和 IP 地址
  Serial.println("\nAP 热点设置成功!");
  Serial.print("AP IP 地址: ");
  Serial.println(WiFi.softAPIP());
  Serial.print("请用手机或电脑连接热点 '");
  Serial.print(ap_ssid);
  Serial.print("'，密码 '");
  Serial.print(ap_password);
  Serial.println("'。");
  Serial.println("然后在浏览器输入 IP 地址 (如 192.168.4.1) 访问 Web 控制页面。");
}

// 发送命令的辅助函数
void sendCommand(const byte *cmd, size_t len, const char *cmdName)
{
  size_t bytesSent = Serial1.write(cmd, len);
  if (bytesSent == len)
  {
    Serial.print("命令 '");
    Serial.print(cmdName);
    Serial.println("' 发送成功!");
  }
  else
  {
    Serial.print("发送命令 '");
    Serial.print(cmdName);
    Serial.println("' 失败。");
  }
}

// 处理 Web 请求 - 根页面
void handleRoot()
{
  server.send(200, "text/html", HTML_CONTENT);
}

// 处理命令请求
void handleCommand()
{
  if (server.hasArg("cmd"))
  {
    String cmdName = server.arg("cmd");
    Serial.print("收到 Web 命令: ");
    Serial.println(cmdName);

    for (int i = 0; i < numCommands; i++)
    {
      if (cmdName == commands[i].name)
      {
        sendCommand(commands[i].cmd_pgm, commands[i].len, commands[i].name);
        server.send(200, "text/plain", "命令 '" + cmdName + "' 已发送至抓娃娃机");
        return;
      }
    }

    server.send(400, "text/plain", "未知命令: " + cmdName);
  }
  else
  {
    server.send(400, "text/plain", "缺少 'cmd' 参数");
  }
}

void setup()
{
  // 启动标准 USB 串口，用于调试和打印消息到 Arduino IDE 串口监视器。
  Serial.begin(115200);
  Serial.println("Xiao S3 启动中...");

  setupWiFi(); // 尝试连接 WiFi，如果失败则启动 AP 模式

  Serial1.begin(9600, SERIAL_8N1, SERIAL1_RX_PIN, SERIAL1_TX_PIN);

  delay(2000);

  // 发送连接命令
  sendCommand(commands[0].cmd_pgm, commands[0].len, commands[0].name);

  // Web 服务器路由
  server.on("/", handleRoot);
  server.on("/command", handleCommand);
  server.begin();
  Serial.println("Web 服务器启动，访问根页面以控制。");

  Serial.println("Xiao S3 Upper Computer Initialized.");
}

void loop()
{
  // 处理 Web 服务器请求
  server.handleClient();

  // 监听并处理串口回应
  handleSerialResponse();
}

void handleSerialResponse()
{
  if (Serial1.available() > 0) {
    Serial.print("从娃娃机收到数据: ");
    while (Serial1.available() > 0) {
      byte incomingByte = Serial1.read();
      Serial.print(incomingByte, HEX);
      Serial.print(" ");
    }
    Serial.println();
  }
}
//
