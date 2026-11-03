"""Render the source-checked OTA overview as one landscape image."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

OUT = Path(r'D:\笔记\STM32嵌入式知识库\23-综合项目\附件\环境监测-运行与OTA一屏总图.png')
OUT.parent.mkdir(parents=True, exist_ok=True)
S = 2
im = Image.new('RGB', (1800*S, 1000*S), '#f5f7fa')
d = ImageDraw.Draw(im)
def font(n, bold=False):
    return ImageFont.truetype('C:/Windows/Fonts/msyhbd.ttc' if bold else 'C:/Windows/Fonts/msyh.ttc', n*S)
def txt(x,y,t,n=23,color='#243448',bold=False):
    d.text((x*S,y*S),t,font=font(n,bold),fill=color)
def rect(box,fill,outline=None):
    d.rounded_rectangle(tuple(int(v*S) for v in box),radius=12*S,fill=fill,outline=outline,width=1*S)
def arrow(x1,y1,x2,y2,color='#61748a'):
    d.line((x1*S,y1*S,x2*S,y2*S),fill=color,width=3*S)
    import math
    a=math.atan2(y2-y1,x2-x1)
    p=[(x2*S,y2*S)]+[((x2-11*math.cos(a+b))*S,(y2-11*math.sin(a+b))*S) for b in (-.5,.5)]
    d.polygon(p,fill=color)
def box(col,y,title,lines,kind='normal'):
    x=30+col*445
    fill,accent=('#eaf2fc','#245e9a') if kind=='normal' else ('#eaf5f0','#247351')
    rect((x,y,x+415,y+176),fill)
    txt(x+15,y+10,title,25,accent,True)
    for i,line in enumerate(lines):
        assert d.textlength(line,font=font(22)) < 385*S, line
        txt(x+15,y+48+i*29,line,22)
    if col<3: arrow(x+418,y+85,x+441,y+85)

txt(30,18,'环境监测工程 · 正常运行 + 华为云 OTA + 元数据',35,bold=True)
txt(30,65,'读图：各行从左到右；OTA 按 ①—⑧ 阅读。箭头表示调用 / 消息 / 处理先后，不都属于同一个函数调用栈。',22)
txt(30,105,'正常运行｜STM32 的任务并行调度，下面并非四个任务顺序执行',24,'#245e9a',True)
box(0,145,'上电 → 选择并启动应用',[
    'Bootloader main()', 'Bootloader_SelectSlot()', 'Bootloader_JumpToSlot()', '读取元数据，跳到有效的应用'])
box(1,145,'应用 main() → 创建任务',[
    'HAL / 外设初始化', 'MX_FREERTOS_Init()', 'osThreadNew() 创建四个任务', 'osKernelStart() 启动调度'])
box(2,145,'采集 + 显示 + 上报 + OTA',[
    'StartDefaultTask → 传感器更新', 'StartTask02 → AppUi_Render', 'StartTask03 → 发送 ENV', 'StartOtaTask → OtaUart_Process'])
box(3,145,'ESP32 → 华为云',[
    'loop() → huawei.update()', 'processStm32Lines() 接收 ENV', 'publishEnvironment() 上报', 'STATUS 返回 STM32 更新状态'])

txt(30,340,'OTA 升级｜示例：当前 A 正在运行，把新固件写进 B；成功确认后才正式切换',24,'#247351',True)
box(0,380,'① 云指令：谁来接收？',[
    'ESP32：mqttClient_.loop()', 'mqttCallback()', 'handleMqttMessage()', '解析升级消息，暂存待处理包'], 'ota')
box(1,380,'② ESP32 下载并发起升级',[
    'processPendingOta()', 'downloadAndTransfer()', 'HTTP GET → beginTransfer()', '发 OTA_BEGIN；等待 OTA_READY'], 'ota')
box(2,380,'③ STM32 收到：准备 B 区',[
    'HAL_UART_RxCpltCallback()', '接收缓存 → OtaUart_Process()', 'OtaUart_HandleBegin()', 'OtaApp_Begin() 校验参数并擦除'], 'ota')
box(3,380,'④ 分包传输并写入 Flash',[
    'ESP：transfer() → sendFrame()', 'STM：OtaUart_PushFrameByte()', 'OtaUart_AcknowledgeFrame()', 'OtaApp_Write() → ACK / NACK'], 'ota')

box(0,588,'⑤ 收齐 → 校验 → 写待升级',[
    'endTransfer() 发送 OTA_END', 'OtaApp_Finalize()', '检查大小 / SHA256 / 向量表', '计算 CRC32；OtaStorage_Save()'], 'ota')
box(1,588,'⑥ 传完不等于升级成功',[
    'STM 回复 OTA_OK', 'OtaPort_ResetToBootloader()', 'NVIC_SystemReset() 重启', 'ESP 上报 99%，等待 BOOT_OK'], 'ota')
box(2,588,'⑦ Bootloader 尝试启动 B',[
    'Bootloader_SelectSlot()', 'Bootloader_PendingImageIsValid()', '校验通过：次数 +1，保存记录', 'Bootloader_JumpToSlot() → B'], 'ota')
box(3,588,'⑧ B 确认 → 云端 100%',[
    'RtosOta_ConfirmWhenStable()', '10 秒后 OtaApp_ConfirmRunning()', '发 BOOT_OK → processStm32Lines()', '版本匹配 → reportProgress(100)'], 'ota')

rect((30,787,1770,961),'#fff5e4')
txt(45,797,'元数据 = 掉电后仍保留的“启动交接单”｜OtaBootRecord_t：8 × uint32_t = 32 字节',25,'#865b19',True)
txt(45,835,'magic 标识  ·  generation 新旧代数  ·  active_slot 已确认槽  ·  pending_slot 待试启动槽',23)
txt(45,869,'pending_size 固件长度  ·  pending_crc32 固件校验  ·  attempt_count 尝试次数  ·  record_crc32 记录校验',23)
txt(45,906,'⑤ active=A / pending=B / 次数=0  →  ⑦ 次数=1 或 2  →  ⑧ active=B / pending=NONE / 次数清零',24,'#865b19',True)
txt(30,969,'双页保存元数据，加载有效新记录。校验失败或次数用尽：清除 pending，尝试回 A；卡死必须再次复位才有回退机会。依据源码静态分析。',20)
im.save(OUT)
print(OUT)
