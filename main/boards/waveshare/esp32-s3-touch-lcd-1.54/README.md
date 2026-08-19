# 产品链接

[微雪电子 ESP32-S3-Touch-LCD-1.54](https://www.waveshare.net/shop/ESP32-S3-Touch-LCD-1.54.htm)
[微雪电子 ESP32-S3-LCD-1.54](https://www.waveshare.net/shop/ESP32-S3-LCD-1.54.htm)

# 编译配置命令

**克隆工程**

```bash
git clone https://github.com/78/xiaozhi-esp32.git
```

**进入工程**

```bash
cd xiaozhi-esp32
```

**配置编译目标为 ESP32S3**

```bash
idf.py set-target esp32s3
```

**打开 menuconfig**

```bash
idf.py menuconfig
```

**选择板子**

```bash
Xiaozhi Assistant -> Board Type -> Waveshare ESP32-S3-Touch-LCD-1.54
```

**编译**

```ba
idf.py build
```

**下载并打开串口终端**

```bash
idf.py build flash monitor
```

## Companion Device baseline

This board is the pinned production target for the Companion Device. See
[`docs/companion-baseline.md`](../../../../docs/companion-baseline.md) for the
shared macOS simulator, device build, hardware diagnostics, and factory recovery
procedure. Do not flash hardware until the external factory backup checksum has
been verified and the flash has been approved.

The Clock Screen reads the IANA time-zone name from the `clock/timezone` NVS
setting at startup. The current release supports `America/Chicago` and uses it
as the default. The device stores UTC system time; the platform time adapter
maps the configured name to the POSIX daylight-saving rule used by ESP newlib.
An unsupported setting is reported and falls back to `America/Chicago`.

Swipe left to select the Clock Screen and right to select the Avatar Screen.
Navigation does not wrap at either edge. Conversation activity can temporarily
present the Avatar Screen without replacing the screen selected by the user.
