from pathlib import Path
from datetime import date

from PIL import Image, ImageDraw, ImageFont
from docx import Document
from docx.enum.style import WD_STYLE_TYPE
from docx.enum.table import WD_CELL_VERTICAL_ALIGNMENT, WD_TABLE_ALIGNMENT
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Cm, Pt, RGBColor


ROOT = Path(r"D:\aiguigu\huanjing_freeRTOS_HAL\huanjing_freeRTOS_HAL")
OUTPUT_DIR = ROOT / "huanjing_HAL" / "output"
QA_DIR = OUTPUT_DIR / ".bootloader_plain_guide_qa"
OUTPUT = OUTPUT_DIR / "Bootloader代码逐函数大白话手册.docx"

BLACK = "000000"
NAVY = "1F4E78"
PALE = "EAF2F8"
GRAY = "F2F2F2"
BORDER = "D9D9D9"
WHITE = "FFFFFF"


def set_font(run, size=10.5, bold=False, color=BLACK, code=False):
    run.font.name = "Consolas" if code else "Aptos"
    rpr = run._element.get_or_add_rPr()
    rpr.rFonts.set(qn("w:eastAsia"), "Microsoft YaHei")
    rpr.rFonts.set(qn("w:ascii"), "Consolas" if code else "Aptos")
    rpr.rFonts.set(qn("w:hAnsi"), "Consolas" if code else "Aptos")
    run.font.size = Pt(size)
    run.bold = bold
    run.font.color.rgb = RGBColor.from_string(color)


def add_picture_with_alt(paragraph, image_path, width, alt_text):
    """Insert an inline picture and give it useful screen-reader text."""
    shape = paragraph.add_run().add_picture(str(image_path), width=width)
    shape._inline.docPr.set("descr", alt_text)
    shape._inline.docPr.set("title", alt_text)
    return shape


def shade(cell, fill):
    tcpr = cell._tc.get_or_add_tcPr()
    shd = tcpr.find(qn("w:shd"))
    if shd is None:
        shd = OxmlElement("w:shd")
        tcpr.append(shd)
    shd.set(qn("w:fill"), fill)


def cell_margins(cell, value=110):
    tcpr = cell._tc.get_or_add_tcPr()
    mar = tcpr.first_child_found_in("w:tcMar")
    if mar is None:
        mar = OxmlElement("w:tcMar")
        tcpr.append(mar)
    for tag in ("top", "start", "bottom", "end"):
        node = mar.find(qn(f"w:{tag}"))
        if node is None:
            node = OxmlElement(f"w:{tag}")
            mar.append(node)
        node.set(qn("w:w"), str(value))
        node.set(qn("w:type"), "dxa")


def table_borders(table):
    pr = table._tbl.tblPr
    borders = pr.find(qn("w:tblBorders"))
    if borders is None:
        borders = OxmlElement("w:tblBorders")
        pr.append(borders)
    for edge in ("top", "left", "bottom", "right", "insideH", "insideV"):
        n = OxmlElement(f"w:{edge}")
        n.set(qn("w:val"), "single")
        n.set(qn("w:sz"), "6")
        n.set(qn("w:color"), BORDER)
        borders.append(n)


def no_split(row, repeat=False):
    pr = row._tr.get_or_add_trPr()
    pr.append(OxmlElement("w:cantSplit"))
    if repeat:
        h = OxmlElement("w:tblHeader")
        h.set(qn("w:val"), "true")
        pr.append(h)


def set_cell(cell, text, bold=False, color=BLACK, center=False, code=False, size=9.3):
    cell.text = ""
    p = cell.paragraphs[0]
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER if center else WD_ALIGN_PARAGRAPH.LEFT
    p.paragraph_format.space_after = Pt(0)
    p.paragraph_format.line_spacing = 1.18
    set_font(p.add_run(str(text)), size=size, bold=bold, color=color, code=code)
    cell.vertical_alignment = WD_CELL_VERTICAL_ALIGNMENT.CENTER
    cell_margins(cell)


def add_table(doc, headers, rows, widths, centers=None):
    t = doc.add_table(rows=1, cols=len(headers))
    t.alignment = WD_TABLE_ALIGNMENT.CENTER
    t.autofit = False
    table_borders(t)
    for i, h in enumerate(headers):
        set_cell(t.rows[0].cells[i], h, bold=True, color=WHITE, center=True)
        shade(t.rows[0].cells[i], NAVY)
        t.rows[0].cells[i].width = Cm(widths[i])
    no_split(t.rows[0], repeat=True)
    for ridx, values in enumerate(rows):
        cells = t.add_row().cells
        no_split(t.rows[-1])
        for i, value in enumerate(values):
            center = bool(centers and centers[i])
            set_cell(cells[i], value, center=center, code=isinstance(value, str) and value.startswith("0x"))
            cells[i].width = Cm(widths[i])
            if ridx % 2:
                shade(cells[i], PALE)
    p = doc.add_paragraph()
    p.paragraph_format.space_after = Pt(1)
    return t


def heading(doc, text, level=1):
    p = doc.add_heading(text, level=level)
    p.paragraph_format.keep_with_next = True
    return p


def body(doc, text, lead=None):
    p = doc.add_paragraph()
    p.paragraph_format.first_line_indent = Cm(0.74)
    p.paragraph_format.space_after = Pt(5)
    p.paragraph_format.line_spacing = 1.4
    if lead and text.startswith(lead):
        set_font(p.add_run(lead), bold=True)
        set_font(p.add_run(text[len(lead):]))
    else:
        set_font(p.add_run(text))
    return p


def bullet(doc, text):
    p = doc.add_paragraph(style="List Bullet")
    p.paragraph_format.space_after = Pt(2)
    p.paragraph_format.line_spacing = 1.25
    set_font(p.add_run(text), size=10.2)


def step(doc, number, text):
    p = doc.add_paragraph()
    p.paragraph_format.left_indent = Cm(0.8)
    p.paragraph_format.first_line_indent = Cm(-0.8)
    p.paragraph_format.space_after = Pt(3)
    p.paragraph_format.line_spacing = 1.3
    set_font(p.add_run(f"{number}  "), bold=True, color=NAVY)
    set_font(p.add_run(text), size=10.3)


def code(doc, text):
    p = doc.add_paragraph(style="Code Block")
    p.paragraph_format.keep_together = True
    p.paragraph_format.left_indent = Cm(0.45)
    p.paragraph_format.right_indent = Cm(0.25)
    p.paragraph_format.space_before = Pt(3)
    p.paragraph_format.space_after = Pt(7)
    p.paragraph_format.line_spacing = 1.05
    pr = p._p.get_or_add_pPr()
    shd = OxmlElement("w:shd")
    shd.set(qn("w:fill"), GRAY)
    pr.append(shd)
    set_font(p.add_run(text), size=8.5, code=True, color="202020")
    return p


def function_section(doc, name, file_name, called_by, calls, plain, professional, params, why, failure, snippet=None):
    heading(doc, name, 2)
    add_table(doc, ["位置", "由谁调用", "继续调用"], [[file_name, called_by, calls]], [4.2, 5.6, 6.5])
    body(doc, "大白话  " + plain, lead="大白话  ")
    body(doc, "专业说法  " + professional, lead="专业说法  ")
    body(doc, "参数和返回值  " + params, lead="参数和返回值  ")
    if snippet:
        code(doc, snippet)
    body(doc, "为什么这样写  " + why, lead="为什么这样写  ")
    body(doc, "如果不这样写  " + failure, lead="如果不这样写  ")


def page_number(paragraph):
    paragraph.alignment = WD_ALIGN_PARAGRAPH.CENTER
    set_font(paragraph.add_run("第 "), size=9, color="666666")
    r = paragraph.add_run()
    for kind, value in (("begin", None), (None, " PAGE "), ("separate", None), (None, "1"), ("end", None)):
        if kind:
            e = OxmlElement("w:fldChar")
            e.set(qn("w:fldCharType"), kind)
        elif value == " PAGE ":
            e = OxmlElement("w:instrText")
            e.set(qn("xml:space"), "preserve")
            e.text = value
        else:
            e = OxmlElement("w:t")
            e.text = value
        r._r.append(e)
    set_font(r, size=9, color="666666")
    set_font(paragraph.add_run(" 页"), size=9, color="666666")


def font(size, bold=False):
    options = [
        Path(r"C:\Windows\Fonts\msyhbd.ttc" if bold else r"C:\Windows\Fonts\msyh.ttc"),
        Path(r"C:\Windows\Fonts\simhei.ttf"),
        Path(r"C:\Windows\Fonts\simsun.ttc"),
    ]
    for p in options:
        if p.exists():
            return ImageFont.truetype(str(p), size)
    return ImageFont.load_default()


def centered(draw, box, text, f, fill="#000000"):
    x1, y1, x2, y2 = box
    b = draw.multiline_textbbox((0, 0), text, font=f, spacing=8, align="center")
    draw.multiline_text(((x1+x2-b[2]+b[0])/2, (y1+y2-b[3]+b[1])/2), text, font=f, fill=fill, spacing=8, align="center")


def make_diagrams():
    QA_DIR.mkdir(parents=True, exist_ok=True)
    title = font(40, True)
    regular = font(25)
    small = font(21)

    flash = Image.new("RGB", (1400, 900), "white")
    d = ImageDraw.Draw(flash)
    d.text((50, 30), "STM32F103ZET6 Bootloader 使用的 Flash 布局", font=title, fill="#000000")
    regions = [
        ("Bootloader", "0x08000000 至 0x08006FFF", "28 KB", 130, "#D9EAF7"),
        ("元数据 A", "0x08007000 至 0x080077FF", "2 KB", 85, "#FFF2CC"),
        ("元数据 B", "0x08007800 至 0x08007FFF", "2 KB", 85, "#FFE699"),
        ("Slot A", "0x08008000 至 0x08043FFF", "240 KB", 210, "#E2F0D9"),
        ("Slot B", "0x08044000 至 0x0807FFFF", "240 KB", 210, "#FCE4D6"),
    ]
    y = 110
    for name, address, size_text, h, fill in regions:
        d.rectangle((280, y, 1110, y+h), fill=fill, outline="#666666", width=3)
        d.text((310, y+22), name, font=regular, fill="#000000")
        d.text((580, y+22), address, font=small, fill="#333333")
        d.text((1010, y+22), size_text, font=small, fill="#333333")
        y += h
    d.text((280, 850), "总容量 512 KB   SRAM 0x20000000 至 0x20010000", font=small, fill="#333333")
    flash_path = QA_DIR / "flash-layout.png"
    flash.save(flash_path, dpi=(180, 180))

    flow = Image.new("RGB", (1500, 1180), "white")
    d = ImageDraw.Draw(flow)
    d.text((55, 30), "从复位到应用程序的完整选择流程", font=title, fill="#000000")
    def box(x1, y1, x2, y2, text, fill="#EAF2F8", outline="#4472C4"):
        d.rounded_rectangle((x1,y1,x2,y2), radius=18, fill=fill, outline=outline, width=4)
        centered(d, (x1,y1,x2,y2), text, regular)
    def arrow(x1,y1,x2,y2,color="#4472C4"):
        d.line((x1,y1,x2,y2), fill=color, width=6)
        d.polygon([(x2,y2),(x2-12,y2-18),(x2+12,y2-18)], fill=color)
    box(480,95,1020,180,"芯片复位进入 Bootloader")
    arrow(750,180,750,225)
    box(480,225,1020,320,"读取元数据 A 和 B\n选出最新有效记录")
    arrow(750,320,750,365)
    box(480,365,1020,460,"有有效元数据吗", "#FFF2CC", "#BF9000")
    d.text((315,390),"没有",font=small,fill="#000000")
    d.line((480,412,235,412,235,520),fill="#4472C4",width=6)
    d.polygon([(235,520),(223,502),(247,502)],fill="#4472C4")
    box(55,520,420,615,"只检查 Slot A")
    d.text((1090,390),"有",font=small,fill="#000000")
    d.line((1020,412,1260,412,1260,520),fill="#4472C4",width=6)
    d.polygon([(1260,520),(1248,502),(1272,502)],fill="#4472C4")
    box(1080,520,1440,615,"存在 pending 吗", "#FFF2CC", "#BF9000")
    arrow(1260,615,1260,680)
    box(1080,680,1440,785,"校验 pending 记录\n向量表和 CRC32")
    d.line((1080,565,750,565,750,680),fill="#4472C4",width=6)
    d.polygon([(750,680),(738,662),(762,662)],fill="#4472C4")
    box(570,680,930,785,"没有 pending\n验证 active")
    arrow(1260,785,1260,850)
    box(1080,850,1440,955,"有效且次数小于 2", "#FFF2CC", "#BF9000")
    d.line((1080,902,915,902,915,1015),fill="#C00000",width=6)
    d.polygon([(915,1015),(903,997),(927,997)],fill="#C00000")
    box(710,1015,1120,1110,"否则清除 pending\n回到 active", "#FCE4D6", "#C00000")
    d.line((1440,902,1470,902,1470,1015,1320,1015),fill="#4472C4",width=6)
    d.polygon([(1320,1015),(1338,1003),(1338,1027)],fill="#4472C4")
    box(1135,1015,1480,1110,"先保存次数加一\n再启动 pending", "#E2F0D9", "#548235")
    arrow(750,785,750,850)
    box(570,850,930,945,"启动 active", "#E2F0D9", "#548235")
    arrow(235,615,235,680)
    box(55,680,420,775,"A 有效就启动 A\n否则留在 Bootloader", "#E2F0D9", "#548235")
    flow_path = QA_DIR / "boot-flow.png"
    flow.save(flow_path, dpi=(180,180))

    jump = Image.new("RGB", (1400, 1050), "white")
    d = ImageDraw.Draw(jump)
    d.text((55, 30), "Bootloader 向应用程序交接 CPU", font=title, fill="#000000")
    items = [
        ("1", "检查向量表", "先确认 MSP 和入口地址合理"),
        ("2", "关闭中断和 SysTick", "阻止旧运行环境突然打断交接"),
        ("3", "清理 NVIC", "关闭外设中断并清除挂起状态"),
        ("4", "设置 VTOR", "让中断去应用自己的向量表"),
        ("5", "设置 MSP", "换成应用自己的主栈"),
        ("6", "调用 Reset Handler", "从应用的复位入口开始初始化"),
    ]
    y = 120
    for i,(n,name,why) in enumerate(items):
        d.rounded_rectangle((170,y,1230,y+120),radius=18,fill="#EAF2F8",outline="#4472C4",width=3)
        centered(d,(190,y+15,290,y+105),n,font(31,True),"#1F4E78")
        centered(d,(310,y+15,650,y+105),name,regular)
        centered(d,(675,y+15,1210,y+105),why,small,"#333333")
        if i < len(items)-1:
            d.line((700,y+120,700,y+145),fill="#4472C4",width=5)
            d.polygon([(700,y+145),(688,y+127),(712,y+127)],fill="#4472C4")
        y += 145
    jump_path = QA_DIR / "jump-flow.png"
    jump.save(jump_path, dpi=(180,180))
    return flash_path, flow_path, jump_path


def configure(doc):
    sec = doc.sections[0]
    sec.page_width = Cm(21)
    sec.page_height = Cm(29.7)
    sec.top_margin = Cm(2.0)
    sec.bottom_margin = Cm(1.9)
    sec.left_margin = Cm(2.2)
    sec.right_margin = Cm(2.2)
    sec.footer_distance = Cm(0.8)
    styles = doc.styles
    normal = styles["Normal"]
    normal.font.name = "Aptos"
    normal._element.rPr.rFonts.set(qn("w:eastAsia"), "Microsoft YaHei")
    normal.font.size = Pt(10.5)
    normal.font.color.rgb = RGBColor(0,0,0)
    for name,size,before,after in (("Title",28,0,16),("Heading 1",17,14,7),("Heading 2",13,10,5),("Heading 3",11,7,3)):
        s=styles[name]
        s.font.name="Aptos Display"
        s._element.rPr.rFonts.set(qn("w:eastAsia"),"Microsoft YaHei")
        s.font.size=Pt(size)
        s.font.bold=True
        s.font.color.rgb=RGBColor(0,0,0)
        s.paragraph_format.space_before=Pt(before)
        s.paragraph_format.space_after=Pt(after)
        s.paragraph_format.keep_with_next=True
        if name == "Title":
            ppr=s._element.get_or_add_pPr()
            b=ppr.find(qn("w:pBdr"))
            if b is not None: ppr.remove(b)
    for name in ("List Bullet","List Bullet 2"):
        rpr = styles[name]._element.get_or_add_rPr()
        rfonts = rpr.find(qn("w:rFonts"))
        if rfonts is None:
            rfonts = OxmlElement("w:rFonts")
            rpr.append(rfonts)
        rfonts.set(qn("w:eastAsia"),"Microsoft YaHei")
    if "Code Block" not in styles:
        styles.add_style("Code Block", WD_STYLE_TYPE.PARAGRAPH)
    cb=styles["Code Block"]
    cb.font.name="Consolas"
    cb._element.rPr.rFonts.set(qn("w:eastAsia"),"Microsoft YaHei")
    cb.font.size=Pt(8.5)


def build():
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    flash_path, flow_path, jump_path = make_diagrams()
    doc=Document()
    configure(doc)

    for _ in range(4): doc.add_paragraph()
    p=doc.add_paragraph(style="Title")
    p.alignment=WD_ALIGN_PARAGRAPH.CENTER
    set_font(p.add_run("Bootloader 代码逐函数大白话手册"),size=28,bold=True)
    p=doc.add_paragraph(); p.alignment=WD_ALIGN_PARAGRAPH.CENTER
    set_font(p.add_run("STM32F103 A B 双槽启动 试运行 回滚与程序跳转"),size=14,color="333333")
    p.paragraph_format.space_after=Pt(18)
    p=doc.add_paragraph(); p.alignment=WD_ALIGN_PARAGRAPH.CENTER
    set_font(p.add_run("面向能够阅读基础 C 代码但不熟悉 Cortex M 启动过程的读者"),size=10.5,color="666666")
    for _ in range(8): doc.add_paragraph()
    p=doc.add_paragraph(); p.alignment=WD_ALIGN_PARAGRAPH.CENTER
    set_font(p.add_run(f"依据当前工程源码整理  {date.today().year} 年 {date.today().month} 月 {date.today().day} 日"),size=10,color="666666")
    doc.add_page_break()

    heading(doc,"这份手册帮你看懂什么",1)
    body(doc,"Bootloader 是芯片复位后先运行的一小段程序。它不负责传感器、显示、联网和 FreeRTOS 业务，只负责判断哪一份应用程序可以启动，并把 CPU 控制权交给那份应用。当前工程采用 Slot A 和 Slot B 两个应用槽，因此升级失败时仍能回到原来的稳定程序。")
    body(doc,"读完整份手册后，你应该能够从 main 函数出发，解释每个判断为什么存在；能够读懂 active、pending、attempt 和 generation；能够说明向量表、MSP、Reset Handler 与 VTOR 在程序跳转中的作用；也能够在调试器里判断设备为什么启动 A、启动 B 或停在 Bootloader。")
    body(doc,"本文只讲 Bootloader 和它直接依赖的启动支持模块。应用程序如何联网、下载固件和执行业务不在本文范围内。为了说明 pending 最终如何结束，只会提到一句：新应用稳定后需要把自己确认成 active。")
    heading(doc,"建议阅读方法",2)
    bullet(doc,"第一次先读第 1 至第 4 章，建立整体地图，不要急着抠寄存器。")
    bullet(doc,"第二次读第 5 章 main 和槽位选择，再沿调用关系进入其他函数。")
    bullet(doc,"第三次读程序跳转、元数据双副本和典型场景，并在 Keil 中跟着断点走一遍。")
    heading(doc,"目录",1)
    for item in ["1 Bootloader 的角色","2 Flash 和 SRAM 地图","3 启动记录结构体","4 完整运行逻辑","5 boot main 中的全部函数","6 ota metadata 中的全部函数","7 ota storage 中的全部函数","8 ota integrity 中的全部函数","9 程序跳转的底层原理","10 典型启动场景","11 为什么采用这些设计","12 Keil 调试方法","13 当前代码的边界和风险","14 源码阅读顺序和术语"]:
        bullet(doc,item)
    doc.add_page_break()

    heading(doc,"1 Bootloader 的角色",1)
    body(doc,"大白话  可以把 Bootloader 想成设备开机时的门卫。Flash 里可能有旧程序和刚升级的新程序，门卫先检查登记表，再检查候选程序是否像一份能运行的 STM32 程序，最后只放行一个。",lead="大白话  ")
    body(doc,"专业说法  Bootloader 是位于 Flash 起始地址 0x08000000 的第一阶段启动程序。复位向量把 CPU 带入 Bootloader。Bootloader 根据持久化元数据执行镜像选择、完整性校验、试启动计数和回滚，然后重定位向量表并跳转到目标应用的 Reset Handler。",lead="专业说法  ")
    add_table(doc,["Bootloader 负责","Bootloader 不负责"],[["读取 A B 两份启动记录并选出最新有效记录","连接 WiFi 或云端"],["验证向量表和待启动镜像 CRC32","接收并下载升级包"],["限制新固件的试启动次数","初始化 LCD 传感器和业务任务"],["在新固件失败时回滚稳定槽位","比较版本字符串决定谁更新"],["清理 CPU 中断环境并跳转应用","长期运行产品业务"]],[8.15,8.15])
    body(doc,"最重要的一句话是：active 是已经确认的稳定程序，pending 是允许试运行但尚未取得正式身份的新程序。Bootloader 始终优先保护 active，只有安全条件满足时才试启动 pending。")

    heading(doc,"2 Flash 和 SRAM 地图",1)
    p=doc.add_paragraph(); p.alignment=WD_ALIGN_PARAGRAPH.CENTER
    add_picture_with_alt(
        p,
        flash_path,
        Cm(16.2),
        "STM32F103 Flash 分区图：Bootloader、元数据 A/B、应用 Slot A/B 的地址和大小。",
    )
    p.paragraph_format.space_after=Pt(4)
    body(doc,"Flash 是掉电不丢失的存储空间。代码和启动记录都在 Flash 中。SRAM 是程序运行时使用的内存，掉电会丢失。Bootloader 检查应用向量表时，会确认应用给出的初始栈顶确实位于 SRAM。")
    add_table(doc,["区域","起始地址","结束地址","大小","用途"],[["Bootloader","0x08000000","0x08006FFF","28 KB","复位后最先运行的启动程序"],["元数据 A","0x08007000","0x080077FF","2 KB","第一份 OtaBootRecord"],["元数据 B","0x08007800","0x08007FFF","2 KB","第二份 OtaBootRecord"],["Slot A","0x08008000","0x08043FFF","240 KB","应用镜像 A"],["Slot B","0x08044000","0x0807FFFF","240 KB","应用镜像 B"]],[3.0,3.2,3.2,2.0,4.9],[True,True,True,True,False])
    body(doc,"为什么地址必须写死  链接器按照这些地址放置程序，Bootloader 又按照同一组地址寻找向量表和镜像。如果链接地址与 Ota_SlotBase 返回的地址不一致，程序本身可能完好，Bootloader 仍会从错误位置读取 MSP 和 Reset Handler。",lead="为什么地址必须写死  ")

    heading(doc,"3 启动记录结构体",1)
    body(doc,"OtaBootRecord_t 是 Bootloader 的小账本，共有 8 个 uint32_t 字段，因此通常占 32 字节。它告诉 Bootloader 哪个槽位已经稳定、哪个槽位正在试运行，以及这份记录有没有损坏。")
    code(doc,"typedef struct\n{\n    uint32_t magic;\n    uint32_t generation;\n    uint32_t active_slot;\n    uint32_t pending_slot;\n    uint32_t pending_size;\n    uint32_t pending_crc32;\n    uint32_t attempt_count;\n    uint32_t record_crc32;\n} OtaBootRecord_t;")
    add_table(doc,["字段","大白话","专业含义","为什么需要"],[["magic","这是 OTA 账本，不是随机数据","固定魔数 0x4F544131，即 OTA1","快速排除空白 Flash 或错误区域"],["generation","第几次改账本","单调递增的记录代数","两份记录都有效时判断谁更新"],["active_slot","正式在岗的程序","已确认稳定的启动槽","回滚时必须知道安全目标"],["pending_slot","试用期的新程序","待确认镜像槽位","让新固件先试运行"],["pending_size","新程序实际多大","参与镜像范围和 CRC 计算的字节数","不把整个 240 KB 都当成固件"],["pending_crc32","新程序的校验号码","待启动镜像的 IEEE CRC32","发现传输或 Flash 偶然损坏"],["attempt_count","已经试了几次","持久化试启动次数","防止坏固件无限重启"],["record_crc32","账本自己的校验号码","前 28 字节的 CRC32","发现启动决策数据损坏"]],[2.7,4.5,4.5,4.6])
    heading(doc,"3 1 两种 CRC 不要混淆",2)
    add_table(doc,["名称","校验对象","何时检查","回答的问题"],[["pending_crc32","pending 槽中的固件字节","每次准备试启动 pending 时","这份新固件有没有发生偶然损坏"],["record_crc32","OtaBootRecord_t 前 28 字节","每次读取或保存记录时","启动账本里的字段有没有损坏"]],[3.2,5.0,4.5,3.6])
    body(doc,"CRC32 是完整性校验，不是身份认证。它擅长发现误码、写入中断和 Flash 损坏，但攻击者如果能够同时替换固件和 CRC，CRC 本身无法证明固件来自可信发布者。")

    heading(doc,"4 完整运行逻辑",1)
    p=doc.add_paragraph(); p.alignment=WD_ALIGN_PARAGRAPH.CENTER
    add_picture_with_alt(
        p,
        flow_path,
        Cm(16.3),
        "Bootloader 完整启动流程：加载元数据、校验 pending、记录尝试次数、回滚或跳转应用。",
    )
    p.paragraph_format.space_after=Pt(5)
    for n,text in enumerate(["CPU 复位后从 0x08000000 对应的 Bootloader 启动代码进入 main。","Bootloader 初始化 USART1，只用于输出简短启动结果。","OtaStorage_Load 检查元数据 A 和 B，选出有效且 generation 更新的一份。","没有任何有效元数据时，只把 Slot A 当作出厂候选；A 的向量表也无效就返回 NONE。","存在 pending 时，先验证记录、镜像长度、向量表和 pending_size 范围内的 CRC32。","pending 无效或已经尝试两次时，创建一条清除 pending 的新记录，然后回到 active。","pending 有效且次数未用完时，先把 attempt_count 加一并保存；保存成功后才允许启动 pending。","得到目标槽后打印 A、B 或 no valid app。","Bootloader_JumpToSlot 清理中断环境，设置应用向量表和主栈，调用应用 Reset Handler。","如果没有有效应用或 Reset Handler 异常返回，程序停在死循环中。"],1): step(doc,n,text)
    heading(doc,"4 1 为什么先保存次数再启动",2)
    body(doc,"假设新固件一运行就崩溃。如果 Bootloader 先跳转、以后再想办法记录次数，那么后面的记录代码永远没有机会执行。设备每次复位都会把它当作第一次试启动，形成无限重启。当前代码先把次数写入 Flash，哪怕新应用马上崩溃，下次复位也能看到这次失败。")

    heading(doc,"5 boot main 中的全部函数",1)
    body(doc,"本章按照代码的实际调用关系解释 boot_main.c。先读 main，再读 Bootloader_SelectSlot，最后读跳转函数，最容易形成整体理解。")

    function_section(doc,"BootloaderEntry_t","boot_main.c","Bootloader_JumpToSlot","无","它给应用入口地址起了一个类型名字，意思是可以把这个地址当成一个无参数、无返回值的函数来调用。","它是函数指针类型 typedef。Cortex M 向量表第二项保存带 Thumb 位的 Reset Handler 地址，代码把该整数地址转换为 BootloaderEntry_t 后执行间接调用。","它没有参数和返回值，因为应用的 Reset Handler 按启动约定不接收 C 参数，也不会返回。","直接用整数表达式跳转既难读，也容易出现错误的函数签名；命名类型让强制转换的意图更清楚。","若函数签名与实际入口约定不一致，编译器可能按错误的调用约定准备参数或返回现场。", "typedef void (*BootloaderEntry_t)(void);")

    function_section(doc,"Bootloader_DebugInit","boot_main.c","main","直接操作 RCC GPIOA USART1 寄存器","它打开 GPIOA 和 USART1 的时钟，把 PA9 配成串口发送脚，然后设置 115200 波特率。","函数在 HAL_Init 之前使用 STM32F1 寄存器完成最小 USART1 TX 初始化。复位后的 HSI 为 8 MHz，APB2 也为 8 MHz；BRR 写入 0x45，CR1 只打开 USART 和发送器。","为什么不用 CubeMX 和 HAL  UART  初始化  Bootloader 只需要输出几个固定字符串，直接寄存器初始化依赖少、代码体积小，也避免把完整 HAL 初始化流程带入启动程序。","如果时钟树不再是复位默认的 8 MHz，而这里仍写 0x45，串口波特率会错误；如果忘记打开 GPIO 或 USART 时钟，寄存器配置不会正常工作。", "RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_USART1EN;\nGPIOA->CRH &= ~(0xFUL << 4U);\nGPIOA->CRH |=  (0xBUL << 4U);\nUSART1->BRR = 0x0045U;\nUSART1->CR1 = USART_CR1_UE | USART_CR1_TE;")

    function_section(doc,"Bootloader_DebugText","boot_main.c","main","USART1 状态寄存器和数据寄存器","它把字符串里的字符一个一个塞进串口，直到遇到字符串结尾的零字符。","这是轮询式 USART 发送。函数等待 TXE 置位，表示发送数据寄存器可以接收下一个字节，然后写 DR。它不使用中断、DMA 或 printf。","参数 text 是以零结尾的 C 字符串；NULL 时直接返回；函数没有返回值。","启动日志很短，轮询最简单可靠。printf 会引入格式化库并扩大 Bootloader；中断发送又需要额外状态和中断初始化。","若不等待 TXE 就连续写 DR，前一个字节可能被覆盖；若不判断 NULL，会访问地址 0；它只等 TXE，不保证最后一位已经完全从引脚发出，但当前代码马上跳转前通常有足够时间。", "while (*text != '\\0')\n{\n    while ((USART1->SR & USART_SR_TXE) == 0U) { }\n    USART1->DR = (uint8_t)*text;\n    text++;\n}")

    function_section(doc,"Bootloader_VectorIsValid","boot_main.c","Bootloader_SelectSlot Bootloader_PendingImageIsValid Bootloader_JumpToSlot","Ota_SlotBase Ota_VectorWordsAreValid","它先找到槽位开头，拿出应用留下的两个最重要数字：栈顶地址和第一条启动函数地址，然后判断它们像不像真的应用。","STM32 Flash 是内存映射的，因此可以直接解引用固定地址。Cortex M 向量表第 0 项是初始 MSP，第 1 项是 Reset Handler。函数把这两个字交给纯算法函数做范围、对齐和 Thumb 位检查。","输入是 OTA_SLOT_A 或 OTA_SLOT_B；返回 true 表示向量表数值基本合理，false 表示槽位编号或向量表不可信。","全 0xFF 的空白 Flash 不是应用。只检查某个位置非空也不够，MSP 和 Reset Handler 的结构性检查可以尽早拒绝空槽、错误链接地址和明显损坏的镜像。","如果直接跳到未检查的地址，CPU 可能使用非法栈或执行非法指令，通常立即进入 HardFault。", "msp = *(const uint32_t *)base;\nreset_handler = *(const uint32_t *)(base + sizeof(uint32_t));\nreturn Ota_VectorWordsAreValid(slot, msp, reset_handler);")

    function_section(doc,"Bootloader_PendingImageIsValid","boot_main.c","Bootloader_SelectSlot","OtaRecord_IsValid Ota_SlotBase Bootloader_VectorIsValid Ota_Crc32","它对试用的新程序做三道检查：账本要是真的、程序开头像应用、整份程序算出来的 CRC 要和账本一致。","这是 pending 镜像的启动前完整性检查。它验证元数据字段组合和 record_crc32，确认 pending_slot 非 NONE，验证目标向量表，再对 pending_size 个字节计算 IEEE CRC32。","输入指向 OtaBootRecord_t；只有所有检查都通过才返回 true。","向量表合理只说明前 8 字节像应用，不能证明后面的代码没有损坏。CRC 覆盖实际镜像长度，可以发现下载不完整或 Flash 位翻转。","若只检查向量表，固件中部损坏仍可能被启动；若不先验证 pending_size，CRC 可能越过槽位边界读取不该读取的区域。", "if (!OtaRecord_IsValid(record)) return false;\nif (pending_slot == OTA_SLOT_NONE) return false;\nif (!Bootloader_VectorIsValid(pending_slot)) return false;\nreturn Ota_Crc32((const uint8_t *)base, record->pending_size)\n       == record->pending_crc32;")

    function_section(doc,"Bootloader_ClearPending","boot_main.c","Bootloader_SelectSlot","OtaRecord_Make OtaStorage_Save","它重新写一张账本：稳定程序不变，把试用程序、大小、CRC 和次数全部清零。","函数创建 generation 加一的回滚记录，保留 active_slot，把 pending_slot 设为 OTA_SLOT_NONE。OtaRecord_Make 会同步生成有效的 record_crc32，然后写入另一元数据页。","输入必须是已经加载的可信记录；函数没有返回值，并且当前实现忽略 OtaStorage_Save 的状态。","不能在旧记录内只改一个字段，因为其他 pending 字段和记录 CRC 必须保持一致。创建完整新记录能避免残留状态。","如果只清 pending_slot 而保留 pending_size 或 attempt_count，OtaRecord_IsValid 会判定字段组合矛盾。当前函数忽略保存失败，因此 Flash 写入失败时，下次复位仍可能看到旧 pending；这是需要注意的限制。", "rollback_record = OtaRecord_Make(record->generation + 1U,\n                                 (OtaSlot_t)record->active_slot,\n                                 OTA_SLOT_NONE, 0U, 0U);\n(void)OtaStorage_Save(&rollback_record);")

    function_section(doc,"Bootloader_SelectSlot","boot_main.c","main","OtaStorage_Load Bootloader_PendingImageIsValid Bootloader_ClearPending Bootloader_VectorIsValid OtaRecord_SetAttemptCount OtaStorage_Save","它是整个门卫的决策中心。它先看账本，再决定试新程序、回旧程序、首次启动 A，或者谁也不启动。","该函数实现 A B OTA 的启动状态机。它区分无元数据、无 pending、有合法 pending、pending 无效、次数耗尽和元数据保存失败等路径，并在每条路径返回候选槽位。","没有参数；返回 OTA_SLOT_A、OTA_SLOT_B 或 OTA_SLOT_NONE。返回槽位只代表决策结果，真正跳转由 Bootloader_JumpToSlot 完成。","选择和跳转分开后，选择逻辑可以独立测试；同时每条返回路径都再次验证 active 或 pending，避免把损坏的槽位交给跳转函数。","如果把所有判断揉进 main 或边判断边跳转，状态更新失败后的回退路径会更难保证，也更难做单元测试。", "if (!OtaStorage_Load(&record))\n    return VectorValid(A) ? A : NONE;\n\nif (pending != NONE)\n{\n    if (pending 无效 || attempt_count >= 2)\n        清除 pending 并返回 active;\n    先把 attempt_count 加一并保存;\n    保存成功才返回 pending;\n}\nreturn 有效的 active 或 NONE;")
    heading(doc,"5 1 SelectSlot 决策表",3)
    add_table(doc,["元数据","pending","镜像和次数","保存结果","返回"],[["无有效记录","不适用","Slot A 向量表有效","不写记录","A"],["无有效记录","不适用","Slot A 无效","不写记录","NONE"],["有效","NONE","active 向量表有效","不写记录","active"],["有效","有","镜像无效或次数达到 2","尝试清除 pending","有效 active 或 NONE"],["有效","有","镜像有效且次数小于 2","次数保存成功","pending"],["有效","有","镜像有效且次数小于 2","次数保存失败","有效 active 或 NONE"]],[3.0,2.0,5.2,3.4,2.7],[True,True,False,False,True])

    function_section(doc,"Bootloader_JumpToSlot","boot_main.c","main","Bootloader_VectorIsValid Cortex M 内核寄存器 应用 Reset Handler","它把 Bootloader 留下的闹钟和中断收拾干净，换上应用的中断电话簿和栈，最后跳到应用的开机入口。","函数执行 Cortex M 上的裸机镜像交接：验证向量表，关闭全局中断，停止 SysTick，禁用并清除 NVIC，设置 SCB VTOR，执行屏障，设置 MSP，重新开中断并调用 Reset Handler。","输入是目标槽位；无返回值。槽位无效时立即返回；正常情况下 Reset Handler 永不返回。","Bootloader 和应用是两个独立链接的程序。仅修改 PC 跳转不够，应用还需要自己的主栈和中断向量表；旧 SysTick 或挂起中断也必须清除。","未清理旧中断会在应用初始化完成前执行错误的处理函数；未设置 VTOR 会继续查 Bootloader 向量表；未设置 MSP 会让应用使用错误的栈。", "__disable_irq();\nSysTick->CTRL = SysTick->LOAD = SysTick->VAL = 0U;\nfor (index = 0U; index < 2U; index++)\n{\n    NVIC->ICER[index] = 0xFFFFFFFFUL;\n    NVIC->ICPR[index] = 0xFFFFFFFFUL;\n}\nSCB->VTOR = base;\n__DSB(); __ISB();\n__set_MSP(new_msp);\n__enable_irq();\n((BootloaderEntry_t)reset_handler)();")

    function_section(doc,"main","boot_main.c","启动文件中的 Reset Handler","Bootloader_DebugInit Bootloader_SelectSlot Bootloader_DebugText Bootloader_JumpToSlot","它只做四件事：准备串口、选程序、打印选择结果、跳过去。如果跳不过去，就一直停着。","main 是 Bootloader 的顶层编排函数。底层检查和状态机被封装在独立函数中，因此 main 保持线性流程，并把 OTA_SLOT_NONE 交给跳转函数的二次防护。","返回类型是 int，但嵌入式 main 正常不会返回；局部变量 slot 保存本次选择。","顶层函数越短，复位后的控制流越容易审查。日志放在选择后、跳转前，现场调试时可以直接看到决策结果。","若没有最后的 while 死循环，异常返回路径可能继续执行未定义区域；如果在 main 中直接解引用无效槽位，NONE 路径可能导致总线错误。", "Bootloader_DebugInit();\nslot = Bootloader_SelectSlot();\nBootloader_DebugText(...);\nBootloader_JumpToSlot(slot);\nwhile (1) { }")

    heading(doc,"6 ota metadata 中的全部函数",1)
    body(doc,"ota_metadata.c 不直接擦写 Flash。它只处理 OtaBootRecord_t 的格式、合法性和新旧比较，因此这些规则可以脱离硬件做单元测试。")
    function_section(doc,"OtaRecord_IsApplicationSlot","ota_metadata.c","OtaRecord_IsValid","无","它只回答一个问题：这个值是不是 A 或 B。NONE 不是应用槽。","这是文件内部的枚举成员验证函数，集中定义可启动槽位集合。","输入 OtaSlot_t；A 或 B 返回 true，其余返回 false。","把规则集中后，active 和 pending 的检查不会各写一套容易漂移的条件。","若把任意非零值都当成槽位，损坏的元数据可能被转换为错误 Flash 地址。", "return (slot == OTA_SLOT_A) || (slot == OTA_SLOT_B);")
    function_section(doc,"OtaRecord_CalculateCrc","ota_metadata.c","OtaRecord_Make OtaRecord_SetAttemptCount OtaRecord_IsValid","Ota_Crc32","它给账本前七个字段算校验值，故意不把最后的 record_crc32 自己算进去。","offsetof 得到 record_crc32 在结构体中的字节偏移，当前是 28。CRC 输入范围从结构体首地址到该偏移之前。","输入记录指针，返回 32 位 CRC。","校验值不能包含自身，否则计算会形成循环依赖。使用 offsetof 比手写 28 更能适应字段布局变化。","若把 record_crc32 也算进去，生成和验证无法用同一条简单规则；若手写长度，结构体变更后可能忘记同步。", "return Ota_Crc32((const uint8_t *)record,\n                 (uint32_t)offsetof(OtaBootRecord_t, record_crc32));")
    function_section(doc,"OtaRecord_Make","ota_metadata.c","Bootloader_ClearPending 应用侧写入 pending 时","OtaRecord_CalculateCrc","它从零开始填好一张完整账本，并在最后算好账本 CRC。","这是记录构造函数。它明确赋值全部八个字段，将 attempt_count 初始化为 0，并在其他字段确定后计算 record_crc32。","输入代数、active、pending、镜像长度和镜像 CRC；按值返回完整结构体。","逐字段构造避免未初始化的栈垃圾进入 CRC，也保证无 pending 时调用方可以传入成组的零值。","若只修改旧结构体的几个成员，遗留字节或不一致字段会使记录不可重复，甚至被误判有效。", "record.magic = OTA_RECORD_MAGIC;\n...\nrecord.attempt_count = 0U;\nrecord.record_crc32 = OtaRecord_CalculateCrc(&record);")
    function_section(doc,"OtaRecord_SetAttemptCount","ota_metadata.c","Bootloader_SelectSlot","OtaRecord_CalculateCrc","它修改尝试次数后，顺手重算账本 CRC。","这是保持结构体不变量的更新函数。任何被 CRC 覆盖的字段改变后，record_crc32 都必须刷新。","输入记录指针和新次数；NULL 时不处理；无返回值。","把更新与 CRC 刷新绑定，调用者不会出现只改次数忘记改 CRC 的半成品记录。","若次数改变但 CRC 不变，OtaStorage_Save 会拒绝记录，或下次启动时记录会被判坏。", "record->attempt_count = attempt_count;\nrecord->record_crc32 = OtaRecord_CalculateCrc(record);")
    function_section(doc,"OtaRecord_IsValid","ota_metadata.c","Bootloader_PendingImageIsValid OtaStorage_Save OtaRecord_SelectNewest","OtaRecord_IsApplicationSlot Ota_ImageSizeIsValid OtaRecord_CalculateCrc","它不只看 CRC，还检查账本里的话是否自相矛盾。例如说没有新程序，却又写着新程序大小和尝试次数，就判无效。","这是元数据的语义验证加完整性验证。依次检查 NULL、magic、active 槽、pending 字段组合、镜像大小和 record_crc32。","输入记录指针；全部条件满足返回 true。","CRC 只能说明字节是否和保存时一致，不能说明字段组合在业务上合理。语义检查能阻止错误状态进入启动决策。","只校验 CRC 时，一条合法编码但逻辑矛盾的记录仍可能让 Bootloader 启动错误槽位。", "if (pending_slot == OTA_SLOT_NONE)\n{\n    size crc attempt 必须全部为 0;\n}\nelse\n{\n    pending 必须是另一个应用槽且长度合法;\n}\nreturn record_crc32 == CalculateCrc(record);")
    function_section(doc,"OtaRecord_SelectNewest","ota_metadata.c","OtaStorage_Load OtaStorage_Save","OtaRecord_IsValid","它先把两本账都验一遍。只有一本真的就用那本；两本都真的，就用代数更大的那本。","这是双副本仲裁函数。它分别验证两条记录，并用有符号的 generation 差值判断新旧，从而支持 uint32_t 回绕。","输入两条可能位于 Flash 的记录指针；返回其中一条的 const 指针，或在两条都无效时返回 NULL。","写新页时不会立即删除旧页，所以正常情况下两页都可能有效。必须有稳定规则确定最新状态。","如果简单比较 second generation 大于 first，计数从 0xFFFFFFFF 回绕到 0 后会把旧记录误认为更新。", "if ((int32_t)(second->generation - first->generation) > 0)\n    return second;\nreturn first;")
    function_section(doc,"OtaRecord_BootSlot","ota_metadata.c","当前 boot_main.c 没有直接调用 可供纯逻辑测试或其他端口使用","无","它只根据账本说应该优先选 pending 还是 active，不负责检查镜像。","这是轻量的纯状态选择辅助函数。pending 存在且 attempt_count 小于最大次数时返回 pending，否则返回 active。","输入记录指针；NULL 返回 NONE。","把最基本的状态规则做成纯函数便于单测和复用。当前 Bootloader 需要额外执行向量表、CRC 和保存次数，因此使用了更完整的 Bootloader_SelectSlot。","若误以为它已经完成所有安全检查并直接跳转，就会绕过镜像 CRC、向量表检查和次数持久化。", "if ((pending_slot != OTA_SLOT_NONE) &&\n    (record->attempt_count < OTA_BOOT_MAX_ATTEMPTS))\n    return pending_slot;\nreturn active_slot;")

    heading(doc,"7 ota storage 中的全部函数",1)
    body(doc,"ota_storage.c 负责真正访问 STM32F103 内部 Flash。A 页和 B 页各占 2 KB，但每页开头只放一条 32 字节记录。浪费一些空间换来的是按 Flash 擦除页进行清晰、可靠的掉电保护。")
    function_section(doc,"OtaStorage_RecordAt","ota_storage.c","OtaStorage_Load OtaStorage_Save","无","它把一个固定 Flash 地址告诉编译器：这里按 OtaBootRecord_t 的格式看。","STM32 Flash 是内存映射的，读取无需 HAL 函数。该函数把 uint32_t 地址转换为 const OtaBootRecord_t 指针。","输入地址，返回只读记录指针；它本身不验证地址和内容。","集中强制转换可以让其他函数更清楚，也避免重复写难读的指针转换。","如果传入错误地址，后续解引用会读取错误数据；const 可以防止普通赋值误写 Flash，但不能代替地址验证。", "return (const OtaBootRecord_t *)address;")
    function_section(doc,"OtaStorage_ProgramRecord","ota_storage.c","OtaStorage_Save","HAL_FLASH_Program","它把 32 字节账本拆成 16 个半字，一个接一个写进 Flash；任何一次失败就立刻退出。","STM32F1 的最小 Flash 编程单位是 16 位 HALFWORD。函数把记录视为 uint16_t 数组，并按地址递增 2 字节编程。","输入目标页地址和记录；返回 HAL 状态。调用前必须已经解锁并擦除目标页。","遵守芯片的最小编程粒度，同时逐次检查状态，避免失败后继续写造成更混乱的半成品。","若按不支持的宽度写入或写入未擦除页，HAL 会失败；若忽略中途错误，调用者可能误以为记录完整。", "for (index = 0U; index < sizeof(OtaBootRecord_t)/sizeof(uint16_t); index++)\n{\n    status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD,\n             address + index*sizeof(uint16_t), halfword[index]);\n    if (status != HAL_OK) return status;\n}")
    function_section(doc,"OtaStorage_Load","ota_storage.c","Bootloader_SelectSlot","OtaStorage_RecordAt OtaRecord_SelectNewest memcpy","它查看两页账本，挑出最新且没坏的一页，再把 32 字节复制到 RAM，供 Bootloader 安心使用。","函数把元数据固定地址映射成记录指针，交给纯逻辑层校验和仲裁。找到记录后用 memcpy 复制到调用者的对齐结构体。","输出参数 record 不能为 NULL；成功返回 true，没有有效记录返回 false。","复制到 RAM 后，后续逻辑使用稳定快照，不必反复直接访问 Flash。memcpy 也避免结构体对齐方式导致的不安全访问。","若直接保存 Flash 指针并在擦写期间继续使用，它指向的内容可能改变；若两页都不校验就随便选，掉电产生的半记录可能被采用。", "newest = OtaRecord_SelectNewest(record_at(A), record_at(B));\nif (newest == NULL) return false;\nmemcpy(record, newest, sizeof(OtaBootRecord_t));\nreturn true;")
    function_section(doc,"OtaStorage_Save","ota_storage.c","Bootloader_ClearPending Bootloader_SelectSlot","OtaRecord_IsValid OtaRecord_SelectNewest HAL Flash 擦写 OtaStorage_ProgramRecord","它绝不覆盖当前最新账本，而是去另一页写新账本。写完以后再从 Flash 读回来验一次。","这是双页日志式提交。函数验证输入记录，确定当前最新页，把另一页设为目标，解锁 Flash，擦除一页，按半字编程，重新锁定，并执行完整记录读回校验。","输入 const 记录指针；返回 HAL_OK 或错误状态。","保留旧页直到新页完整写入，是抵抗擦写过程中掉电的关键。读回验证确认 HAL 返回成功不等于数据一定符合记录规则。","如果先擦除唯一有效页，掉电会让启动状态全部丢失；如果不锁回 Flash，后续错误代码更容易误写；如果不读回校验，部分编程异常可能延迟到下次启动才暴露。", "newest = OtaRecord_SelectNewest(first, second);\ntarget_page = (newest == first) ? B_BASE : A_BASE;\nHAL_FLASH_Unlock();\nerase target_page;\nprogram complete record;\nHAL_FLASH_Lock();\nvalidate record read back from target_page;")
    heading(doc,"7 1 双页掉电保护",2)
    for n,text in enumerate(["假设 A 页是当前最新有效记录。","保存新状态时，只擦除 B 页，A 页保持不动。","如果擦除或写 B 页时掉电，B 页通常因 magic、字段或 CRC 不完整而无效。","下次启动时 OtaRecord_SelectNewest 仍能选回 A 页。","只有 B 页写完并通过验证后，它才凭更大的 generation 成为最新记录。","再下一次保存会反过来写 A 页。"],1): step(doc,n,text)

    heading(doc,"8 ota integrity 中的全部函数",1)
    function_section(doc,"Ota_Crc32","ota_integrity.c","元数据校验 镜像校验","无","它把一段字节逐个算成一个 32 位校验号码。内容只要发生常见误码，号码通常就会变化。","函数实现反射形式的标准 IEEE CRC32，初值和最终异或均为 0xFFFFFFFF，多项式为 0xEDB88320。每个输入字节处理 8 个比特。","输入数据指针和长度；NULL 加非零长度返回 0；返回 32 位 CRC。","使用纯 C 位算法不依赖硬件 CRC 外设，Bootloader、应用和主机测试容易得到一致结果。代价是速度慢于查表或硬件实现，但元数据很小且启动校验可接受。","若固件生成端使用不同多项式、初值、位序或最终异或，双方对同一文件会得到不同 CRC；CRC 也不能防止有意篡改。", "uint32_t crc = 0xFFFFFFFFUL;\nfor each byte\n{\n    crc ^= byte;\n    repeat 8 bits:\n        crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320 : crc >> 1;\n}\nreturn crc ^ 0xFFFFFFFFUL;")
    function_section(doc,"Ota_ImageSizeIsValid","ota_integrity.c","OtaRecord_IsValid","无","它确认固件不是空文件，而且没有大到超过一个 240 KB 槽。","这是镜像长度的边界检查，要求 image_size 大于 0 且小于等于 OTA_SLOT_SIZE。","输入字节数，返回布尔值。","任何按长度读取或校验 Flash 的操作都必须先保证范围，防止跨过槽位边界进入另一个区域。","若允许 0，空镜像可能进入流程；若允许超过槽位，CRC 读取或写入会碰到其他槽位甚至越过 Flash。", "return (image_size > 0U) && (image_size <= OTA_SLOT_SIZE);")
    function_section(doc,"Ota_VectorWordsAreValid","ota_integrity.c","Bootloader_VectorIsValid","Ota_SlotBase","它检查应用开头的两个数字是否说得通：栈顶要在 RAM 里并对齐，入口要带 Thumb 标志并落在这个槽位里。","函数验证 slot 有效；Reset Handler bit0 必须为 1；MSP 必须 4 字节对齐并位于 0x20000000 至 0x20010000；清除 Thumb 位后的入口地址必须处于目标槽范围。","输入槽位、初始 MSP 和 Reset Handler；返回布尔值。","Cortex M 只执行 Thumb 指令，函数地址最低位用作 Thumb 状态标志。栈必须位于 SRAM。入口地址位于正确槽位还能发现应用链接到了错误起始地址。","任何一项错误都可能在跳转后立刻 HardFault。注意当前范围允许 MSP 等于 SRAM_END，这是栈向低地址增长时常见的合法初始栈顶。", "reset_address = reset_handler & ~1UL;\nif ((reset_handler & 1U) == 0U) return false;\nif ((initial_msp & 3U) != 0U) return false;\nif (initial_msp < SRAM_BASE || initial_msp > SRAM_END) return false;\nreturn reset_address >= slot_base && reset_address < slot_base + SLOT_SIZE;")

    heading(doc,"9 程序跳转的底层原理",1)
    p=doc.add_paragraph(); p.alignment=WD_ALIGN_PARAGRAPH.CENTER
    add_picture_with_alt(
        p,
        jump_path,
        Cm(16.3),
        "Bootloader 向应用程序交接 CPU 的六个步骤：检查向量表、关闭中断和 SysTick、清理 NVIC、设置 VTOR、设置 MSP、调用 Reset Handler。",
    )
    p.paragraph_format.space_after=Pt(5)
    heading(doc,"9 1 向量表里的前两个字",2)
    add_table(doc,["偏移","名称","大白话","专业意义"],[["base 加 0","初始 MSP","应用一开始把栈顶放在哪里","装入 Main Stack Pointer 的初始值"],["base 加 4","Reset Handler","应用从哪个函数开始开机","带 Thumb 位的复位异常入口地址"]],[2.5,3.2,5.4,5.2],[True,True,False,False])
    heading(doc,"9 2 清理顺序为什么重要",2)
    body(doc,"关闭全局中断后才开始清理，避免清理到一半突然响应中断。SysTick 的 CTRL、LOAD 和 VAL 清零，防止 Bootloader 的系统节拍继续运行。NVIC 的 ICER 关闭外部中断，ICPR 清除已经挂起但尚未执行的中断。")
    body(doc,"SCB VTOR 写成目标槽位起始地址后，CPU 发生中断时会到应用向量表找处理函数。DSB 保证之前的数据和寄存器写入完成，ISB 刷新指令流水线对执行环境的认识。最后才设置 MSP，因为设置以后不应再依赖 Bootloader 栈上的局部变量。")
    body(doc,"代码随后重新打开中断并调用 Reset Handler。重新打开是因为应用启动和 HAL_Delay 等流程通常依赖中断。不过，切换 MSP 后仍通过普通 C 语句读取局部变量并调用入口，对编译器生成代码存在可移植性风险；更稳妥的通用实现通常把最后几步放进经过验证的汇编跳转桩。")

    heading(doc,"10 典型启动场景",1)
    scenarios=[
        ("首次烧录","元数据 A B 都是空白，Slot A 向量表有效","Load 失败后检查 A，启动 A","这是工厂启动兜底路径，不代表已经生成 active 记录"),
        ("普通启动 A","active A，pending NONE","验证 A 后启动 A","没有版本比较，也不会检查 B"),
        ("第一次试启动 B","active A，pending B，attempt 0，B CRC 正确","先保存 attempt 1，再启动 B","B 如果马上崩溃，这次尝试仍已被记住"),
        ("第二次试启动 B","active A，pending B，attempt 1","先保存 attempt 2，再启动 B","这是允许的最后一次试运行"),
        ("第三次复位","active A，pending B，attempt 2","清除 pending，启动 A","说明 B 两次都没有完成确认"),
        ("B 的 CRC 错误","pending B 但重新计算值不匹配","清除 pending，启动有效的 active","不会浪费试启动次数去运行已知损坏镜像"),
        ("写次数失败","pending 有效但 OtaStorage_Save 返回错误","不启动 pending，回 active","无法记账时不冒险试新程序"),
        ("一个元数据页损坏","另一页仍通过语义和 CRC 校验","选有效页","双副本发挥作用"),
        ("active 也无效","pending 失败且 active 向量表无效","返回 NONE 并停在 Bootloader","当前没有串口恢复或工厂镜像恢复模式"),
        ("Reset Handler 返回","应用异常回到 Bootloader_JumpToSlot","进入永久循环","正常启动入口不应该返回"),
    ]
    add_table(doc,["场景","启动前状态","Bootloader 动作","你应该怎样理解"],scenarios,[3.0,4.8,4.6,3.9])
    heading(doc,"10 1 新应用如何结束 pending",2)
    body(doc,"Bootloader 只负责试启动，不知道新应用的业务是否真的稳定。新应用运行到项目规定的稳定条件后，需要写一条新记录：把自己设为 active，并把 pending 相关字段清零。如果它在确认之前崩溃或复位，Bootloader 会在下次上电继续按 attempt_count 处理。本文不展开应用侧实现。")

    heading(doc,"11 为什么采用这些设计",1)
    add_table(doc,["设计","原因","避免的问题"],[["A B 双槽","升级时不覆盖当前稳定程序","下载失败后没有可回滚程序"],["active 与 pending 分离","新程序先试运行再转正","未经验证的升级直接替换稳定程序"],["尝试次数放在 Flash","复位后仍记得失败次数","坏固件无限重启"],["跳转前先保存次数","崩溃前已经留下尝试证据","永远停留在第一次尝试"],["元数据双页轮换","写新记录时保留旧记录","擦写中掉电导致启动状态全丢"],["记录 CRC 加语义检查","同时检查字节损坏和字段矛盾","仅靠 CRC 接受逻辑错误状态"],["向量表加镜像 CRC","既检查启动骨架也检查完整内容","只看前 8 字节就启动损坏固件"],["选择与跳转分开","决策逻辑可以单测，跳转可二次防护","控制流难审查且错误路径难测试"],["Bootloader 不运行 FreeRTOS","减小依赖和启动复杂度","启动程序过大且受业务模块故障影响"]],[4.2,6.1,6.0])

    heading(doc,"12 Keil 调试方法",1)
    heading(doc,"12 1 推荐断点",2)
    add_table(doc,["断点位置","重点观察","正常现象"],[["main 入口","PC 和启动地址","CPU 已进入 Bootloader 工程"],["OtaStorage_Load 返回后","record 八个字段","看到最新有效元数据的 RAM 快照"],["Bootloader_SelectSlot 的 pending 分支","active_slot pending_slot attempt_count","与 Flash 账本一致"],["OtaStorage_Save 内","newest target_page status","目标页总是最新页的另一页"],["Ota_Crc32 返回处","计算值和 pending_crc32","有效镜像两者相等"],["Bootloader_JumpToSlot 入口","slot base new_msp reset_handler","地址分别落入目标槽和 SRAM"],["SCB VTOR 赋值后","SCB VTOR","等于 Slot A 或 Slot B 起始地址"],["调用 Reset Handler 前","MSP PC PRIMASK","MSP 已切换且中断重新打开"]],[4.6,6.1,5.6])
    heading(doc,"12 2 建议动手实验",2)
    for n,text in enumerate(["先清空元数据页，只保留 Slot A，确认走首次启动路径。","构造 active A pending B attempt 0 的有效记录，确认跳转前次数变为 1。","让 B 在确认前主动复位两次，确认第三次回到 A。","故意修改 B 镜像中的一个字节，确认 pending CRC 失败并回滚。","破坏其中一页 record_crc32，确认仍能选择另一页。","同时让两个槽的向量表无效，确认串口输出 BOOT no valid app 并停机。"],1): step(doc,n,text)
    heading(doc,"12 3 单步跳转时的提醒",2)
    body(doc,"在 __set_MSP 之后继续单步时，调试器的调用栈显示可能暂时混乱，因为 CPU 已经换栈，但调试符号仍横跨两个独立工程。更可靠的观察方式是在应用 Reset Handler 或应用 main 预先设置硬件断点，然后直接运行到该断点。")

    heading(doc,"13 当前代码的边界和风险",1)
    add_table(doc,["级别","现状","影响","改进方向"],[["高","只有 CRC32，没有数字签名验证","能发现偶然损坏，不能证明固件发布者身份","设备内置公钥并验证签名"],["中","pending 的 Reset Handler 只要求落在整个 240 KB 槽内","入口理论上可能指向 pending_size 之外的槽内残留数据","验证入口小于 base 加 pending_size"],["中","两个槽都无效时只停机","现场没有自动恢复通道","增加串口 USB 按键或工厂镜像恢复"],["中","Bootloader_ClearPending 忽略保存结果","清除失败后下次仍可能看到旧 pending","返回状态并明确处理失败策略"],["低","设置 MSP 后仍通过普通 C 调用入口","不同编译器优化下存在可移植性风险","用最小汇编跳转桩完成最后交接"],["低","串口初始化假设复位默认 8 MHz HSI","时钟配置变化后日志波特率会错误","从统一时钟配置计算 BRR 或明确保持默认时钟"],["低","向量表和 CRC 是必要但非充分的运行健康检查","程序可以通过校验后在业务初始化阶段失败","应用确认前加入关键外设和任务健康条件"]],[1.7,5.1,5.0,4.5],[True,False,False,False])
    body(doc,"这些边界不影响你理解当前运行逻辑。它们说明当前实现主要解决意外损坏、升级中断电和新固件启动失败，并没有完成安全启动、现场恢复和跨工具链可移植性的全部工作。")

    heading(doc,"14 源码阅读顺序和术语",1)
    add_table(doc,["顺序","文件","这次只抓住什么"],[["1","ota_port_config.h","芯片容量和五个 Flash 区域地址"],["2","ota_layout.h","槽位枚举和 Ota_SlotBase"],["3","ota_metadata.h","八字段启动记录"],["4","boot_main.c","main SelectSlot JumpToSlot 三条主线"],["5","ota_metadata.c","合法性 新旧比较 CRC 刷新"],["6","ota_storage.c","双页读取 轮换写入 掉电保护"],["7","ota_integrity.c","镜像 CRC 长度和向量表验证"]],[1.7,5.2,9.4],[True,False,False])
    heading(doc,"14 1 术语速查",2)
    add_table(doc,["术语","大白话","专业解释"],[["Slot","放一份应用程序的 Flash 房间","独立链接和存储的镜像区域"],["active","已经转正的稳定程序","确认可作为回滚目标的槽位"],["pending","还在试用期的新程序","待确认启动槽位"],["MSP","应用的主栈顶地址","Main Stack Pointer 初值"],["Reset Handler","应用的开机入口","复位异常处理函数"],["VTOR","中断电话簿放在哪里","Vector Table Offset Register"],["NVIC","管理外设中断的总开关面板","Nested Vectored Interrupt Controller"],["SysTick","CPU 自带的周期闹钟","系统节拍定时器"],["generation","启动记录的更新序号","双副本仲裁代数"],["rollback","试新程序失败后回旧程序","回滚到 active 槽"],["Thumb 位","函数地址最低位的模式标记","Cortex M 入口地址 bit0 必须为 1"],["内存映射","用普通指针就能读 Flash 和寄存器","地址空间统一映射外设和存储器"]],[3.1,6.1,7.1])

    heading(doc,"附录 当前手写源码位置",1)
    paths=[r"portable_ota\stm32\stm32f103_example\bootloader\boot_main.c",r"portable_ota\stm32\stm32f103_example\bootloader\bootloader.h",r"huanjing_HAL\user\OTA\Inc\ota_port_config.h",r"huanjing_HAL\user\OTA\Inc\ota_layout.h",r"huanjing_HAL\user\OTA\Inc\ota_metadata.h",r"huanjing_HAL\user\OTA\Src\ota_metadata.c",r"huanjing_HAL\user\OTA\Inc\ota_storage.h",r"huanjing_HAL\user\OTA\Src\ota_storage.c",r"huanjing_HAL\user\OTA\Inc\ota_integrity.h",r"huanjing_HAL\user\OTA\Src\ota_integrity.c",r"huanjing_HAL\MDK-ARM\Objects\Bootloader\bootloader.sct"]
    for path in paths:
        p=doc.add_paragraph(); p.paragraph_format.left_indent=Cm(0.5); p.paragraph_format.space_after=Pt(2)
        set_font(p.add_run(path),size=8.7,code=True,color="333333")
    body(doc,"启动汇编、CMSIS 和 STM32 HAL 库属于芯片厂商或工具自动生成的基础代码，本手册不逐函数展开。文中只在解释 Reset Handler、MSP、VTOR 和 HAL Flash 接口时说明它们与手写 Bootloader 的边界。")

    for sec in doc.sections:
        page_number(sec.footer.paragraphs[0])
    settings=doc.settings._element
    u=settings.find(qn("w:updateFields"))
    if u is None:
        u=OxmlElement("w:updateFields"); settings.append(u)
    u.set(qn("w:val"),"true")
    doc.core_properties.title="Bootloader 代码逐函数大白话手册"
    doc.core_properties.subject="STM32F103 A B 双槽 Bootloader 运行逻辑与全部手写函数说明"
    doc.core_properties.author=""
    doc.core_properties.keywords="STM32F103 Bootloader OTA A B 回滚 向量表"
    doc.core_properties.comments=""
    doc.save(OUTPUT)
    print(OUTPUT)


if __name__ == "__main__":
    build()
