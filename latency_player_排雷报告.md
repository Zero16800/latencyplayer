# LatencyPlayer 排雷报告

> 排查对象：`E:\Android-SDK\sdk\src\main\cpp\latency_player_core.c`（1577 行）
> 排查时间：2026-09-30
> 结论：**8 颗雷，其中 2 颗 P0 上线必炸，2 颗 P1 高概率踩中，4 颗 R1 建议修。**

---

## 🔴 P0-1：live=1 用空格拼 URL，鉴权参数直接废（上线必炸）

**位置：** `latency_player_core.c:1514`

```c
// librtmp rtmpsrc: live streams need live=1; set via location space-arg
if (g_strcmp0(source_name, "rtmpsrc") == 0 && ...) {
    gchar *loc = NULL;
    g_object_get(source, "location", &loc, NULL);
    if (loc != NULL && strstr(loc, "live=") == NULL) {
        gchar *live_loc = g_strdup_printf("%s live=1", loc);   // ← 雷在这
        g_object_set(source, "location", live_loc, NULL);
    }
}
```

**雷点：**
- 第 1503 行的注释刚写了"**不在 URI 上拼空格参数，避免 URI 属性解析失败**"，第 1514 行自己就打脸，用空格把 `live=1` 拼进 location。
- 你的流地址大概率带鉴权：`rtmp://host/live/stream?token=xxx&sign=yyy`。
- 拼完变成 `...stream?token=xxx&sign=yyy live=1`——librtmp 解析时会把 ` live=1` 当成 URL 的一部分或直接解析失败。
- 结果：**token 失效、live 标志不生效、服务器拒连**。你费劲写的鉴权，全被这个空格毁掉。

**修法（正确姿势）：**
```c
gchar *live_loc = g_strdup_printf("%slive=1", loc);
//                       关键：用 & 连接，不是空格   ^
```
- 如果 loc 末尾是 `?`（无参数）：应该拼 `live=1`
- 如果 loc 末尾是 `?xxx=yyy`（有参数）：应该拼 `&live=1`
- 最稳做法：先判断 `strchr(loc, '?')` 再决定用 `?` 还是 `&`。

```c
gchar *sep = (strchr(loc, '?') != NULL) ? "&" : "?";
gchar *live_loc = g_strdup_printf("%s%slive=1", loc, sep);
```

---

## 🔴 P0-2：低延迟模式把源 latency 强压 0，弱网必断流

**位置：** `latency_player_core.c:1558-1563`

```c
/* 低延迟：源上还有 latency 则压到 0（jitterbuffer 等），rtspsrc 已按 ms 单独处理 */
if (ctx->low_latency && g_strcmp0(source_name, "rtspsrc") != 0 &&
    g_object_class_find_property(G_OBJECT_GET_CLASS(source), "latency")) {
    g_object_set(source, "latency", (guint64)0, NULL);   // ← 雷在这
    LOGI("source latency forced to 0 (low-latency)");
}
```

**雷点：**
- 这个 `latency` 属性是**源缓冲队列的时长**，不是延迟上限。压到 0 = 源端完全不缓冲 = **网络抖一下立即断流**。
- 它对 rtmpsrc、souphttpsrc（HTTP 拉流）等无差别生效——HTTP 拉流本来就是弱网大户，压 0 后 4G 网络下一个抖动就卡死。
- 你的 `latency_player_core.c:1527-1528` 已经有正常逻辑：
  ```c
  guint lat = ctx->low_latency ? 80u : 200u;
  g_object_set(source, "latency", lat, NULL);
  ```
  这里本来已经给 RTMP 设了 80ms 的下限，结果 1558 行的这段又把所有非 rtspsrc 的源强行覆盖回 0。**两段代码互相打架，后面这段把前面那段的活全否了。**

**修法：**
- 直接删掉 1558-1563 这段，或加下限保护：

```c
if (ctx->low_latency && g_strcmp0(source_name, "rtspsrc") != 0 &&
    g_object_class_find_property(G_OBJECT_GET_CLASS(source), "latency")) {
    guint64 min_latency = 80;   // RTMP/HTTP 下限 80ms，别压到 0
    g_object_set(source, "latency", min_latency, NULL);
}
```

---

## 🟠 P1-3：低延迟模式把 rtmpsrc 的 timeout 设成 0，直播必超时

**位置：** `latency_player_core.c:1527`（结合 1505-1506 的 timeout 设置）

```c
guint lat = ctx->low_latency ? 80u : 200u;
g_object_set(source, "latency", lat, NULL);
```

**雷点：**
- 如果低延迟模式下 `rtmp_timeout` 也被压到 0 或极小值（需要核对初始化处），rtmpsrc 的 timeout=0 表示**立即超时**——直播流一个包没及时到就判定失败。
- 注释里写了 rtspsrc "已按 ms 单独处理"，但 rtmpsrc 走的是 `latency` 单位换算，低延迟 80ms 对直播服务器来说太激进。

**排查点：** 核对 `ctx->rtmp_timeout` 的赋值逻辑（在 init 处），确认低延迟模式下有没有把 timeout 也压成 0。

---

## 🟠 P1-4：复制粘贴漏改——函数是 video，取的属性是 audio

**位置：** `latency_player_core.c:1566-1576`

```c
static void on_video_decoder_changed(GstElement *element, GParamSpec *pspec, gpointer user_data) {
    LatencyPlayerContext *ctx = (LatencyPlayerContext *)user_data;

    gchar *decoder_name = NULL;
    g_object_get(element, "current-audio", &decoder_name, NULL);   // ← 雷在这
    ...
    LOGI("Video decoder changed to: %s", decoder_name);
}
```

**雷点：**
- 回调名字是 `on_video_decoder_changed`，日志打印 "Video decoder changed to"，**取出来的却是 `current-audio` 属性**。
- 典型的复制粘贴漏改。日志永远显示音频解码器名，排查视频解码问题时会被误导。

**修法：**
```c
g_object_get(element, "current-video", &decoder_name, NULL);
```

---

## 🟡 R1-5：低延迟模式对 souphttpsrc 也压 0 缓冲（同上文 P0-2 的扩散面）

**位置：** 1558-1563 的生效范围含 `souphttpsrc`（HTTP/HTTPS 拉流）。

**雷点：** HTTP 拉流（点播文件、HLS 分片）网络波动更大，压 0 缓冲 = 秒断。点播场景根本不需要低延迟，**低延迟只该对直播源生效**。

**修法：** 把 `souphttpsrc` 也加进排除列表，或者只对 `rtmpsrc` 生效：

```c
if (ctx->low_latency && g_strcmp0(source_name, "rtspsrc") != 0 &&
    g_strcmp0(source_name, "souphttpsrc") != 0 && ...) {
```

---

## 🟡 R1-6：live=1 的 strstr 检查误伤 token 参数

**位置：** `latency_player_core.c:1513`

```c
if (loc != NULL && strstr(loc, "live=") == NULL) {
```

**雷点：**
- 如果 URL 里恰好带 `?live=0` 或 `token=xxx` 里含 `live=` 子串（概率低但存在），`strstr` 会误判"已有 live 参数"而跳过设置。
- 正确做法是解析 query 参数，而不是子串匹配。

---

## 🟡 R1-7：`g_object_set(latency, (guint64)0)` 类型不匹配隐患

**位置：** `latency_player_core.c:1561`

```c
g_object_set(source, "latency", (guint64)0, NULL);
```

**雷点：**
- GStreamer 源元素的 `latency` 属性类型是 `G_TYPE_UINT`（guint，32 位），你传 `guint64`（64 位）。
- g_object_set 是 varargs，类型不匹配在 32 位对齐下可能读到垃圾值；虽然传 0 侥幸没事，但**这是未定义行为**，一旦改成非 0 值就炸。
- 对照 1528 行 `g_object_set(source, "latency", lat, NULL)` 用的是 `guint lat`，类型是对的。1558 这段是漏改。

**修法：**
```c
g_object_set(source, "latency", 0u, NULL);   // guint，不是 guint64
```

---

## 🟡 R1-8：所有低延迟强制逻辑缺少开关粒度

**位置：** 1527、1558 两处。

**雷点：**
- 低延迟是全局开关 `ctx->low_latency`，没有按源类型/协议细分。
- 未来加 HLS、WebRTC 源时，这套"一刀切压 0"的逻辑会继续误伤新协议。
- 建议把延迟策略收敛成一张表：`协议 → 默认延迟 → 低延迟下限`。

---

## 修复优先级总结

| 级别 | 编号 | 一句话 | 修法 |
|---|---|---|---|
| 🔴 P0 | 1 | live=1 空格拼 URL 毁鉴权 | `&` 或 `?` 拼接 |
| 🔴 P0 | 2 | latency 压 0 弱网断流 | 删掉强制覆盖或加 80ms 下限 |
| 🟠 P1 | 3 | timeout 疑似被压 0 | 核对 rtmp_timeout 初始化 |
| 🟠 P1 | 4 | video 回调取 audio 属性 | 改 `current-video` |
| 🟡 R1 | 5 | souphttpsrc 也被压 0 | 排除 HTTP 源 |
| 🟡 R1 | 6 | strstr 检查误伤参数 | 解析 query 参数 |
| 🟡 R1 | 7 | guint64 传 guint 属性 | 改 `0u` |
| 🟡 R1 | 8 | 无协议粒度开关 | 收敛成策略表 |

---

## 一句话总结

**你最自信的低延迟模块，恰恰是全文件最危险的地方**：强压 0 缓冲（P0-2）+ 空格拼 live=1（P0-1），一个毁体验，一个毁鉴权。修完这两个再谈优化，不然上线就是事故现场。
