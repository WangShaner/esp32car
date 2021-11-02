# ESP32遥控小车
## 硬件
- 小车底板、马达、轮子套装
- ESP32
- L298N电机驱动板模块
- 超声波检测模块, US-025A, HC-SR04均可
## 软件
### ESP32
server库使用[ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer)。
使用[platformio](https://platformio.org/)管理依赖
### 手机遥控
基于[wifi-car-esp8266](https://github.com/lacour-vincent/wifi-car-esp8266)开发