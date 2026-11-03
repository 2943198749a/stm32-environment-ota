from pathlib import Path
from datetime import date

from PIL import Image, ImageDraw, ImageFont
from docx import Document
from docx.enum.section import WD_SECTION
from docx.enum.style import WD_STYLE_TYPE
from docx.enum.table import WD_CELL_VERTICAL_ALIGNMENT, WD_TABLE_ALIGNMENT
from docx.enum.text import WD_ALIGN_PARAGRAPH, WD_BREAK, WD_LINE_SPACING
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Cm, Inches, Pt, RGBColor


ROOT = Path(r"D:\aiguigu\huanjing_freeRTOS_HAL\huanjing_freeRTOS_HAL")
OUTPUT_DIR = ROOT / "huanjing_HAL" / "output"
QA_DIR = OUTPUT_DIR / ".bootloader_guide_qa"
OUTPUT_PATH = OUTPUT_DIR / "Bootloader代码理解指南.docx"

BLACK = "000000"
NAVY = "203864"
BLUE = "4472C4"
LIGHT_BLUE = "D9EAF7"
PALE_BLUE = "EEF4FB"
LIGHT_GRAY = "F2F2F2"
BORDER = "D9D9D9"
WHITE = "FFFFFF"


def set_run_font(run, east_asia="Microsoft YaHei", latin="Aptos", size=None,
                 bold=None, color=BLACK):
    run.font.name = latin
    run._element.get_or_add_rPr().rFonts.set(qn("w:eastAsia"), east_asia)
    run._element.get_or_add_rPr().rFonts.set(qn("w:ascii"), latin)
    run._element.get_or_add_rPr().rFonts.set(qn("w:hAnsi"), latin)
    if size is not None:
        run.font.size = Pt(size)
    if bold is not None:
        run.bold = bold
    run.font.color.rgb = RGBColor.from_string(color)


def shade_cell(cell, fill):
    tc_pr = cell._tc.get_or_add_tcPr()
    shd = tc_pr.find(qn("w:shd"))
    if shd is None:
        shd = OxmlElement("w:shd")
        tc_pr.append(shd)
    shd.set(qn("w:fill"), fill)


def set_cell_margins(cell, top=110, start=120, bottom=110, end=120):
    tc = cell._tc
    tc_pr = tc.get_or_add_tcPr()
    tc_mar = tc_pr.first_child_found_in("w:tcMar")
    if tc_mar is None:
        tc_mar = OxmlElement("w:tcMar")
        tc_pr.append(tc_mar)
    for tag, value in (("top", top), ("start", start), ("bottom", bottom), ("end", end)):
        node = tc_mar.find(qn(f"w:{tag}"))
        if node is None:
            node = OxmlElement(f"w:{tag}")
            tc_mar.append(node)
        node.set(qn("w:w"), str(value))
        node.set(qn("w:type"), "dxa")


def set_table_borders(table, color=BORDER, size="6"):
    tbl_pr = table._tbl.tblPr
    borders = tbl_pr.find(qn("w:tblBorders"))
    if borders is None:
        borders = OxmlElement("w:tblBorders")
        tbl_pr.append(borders)
    for edge in ("top", "left", "bottom", "right", "insideH", "insideV"):
        node = borders.find(qn(f"w:{edge}"))
        if node is None:
            node = OxmlElement(f"w:{edge}")
            borders.append(node)
        node.set(qn("w:val"), "single")
        node.set(qn("w:sz"), size)
        node.set(qn("w:space"), "0")
        node.set(qn("w:color"), color)


def repeat_table_header(row):
    tr_pr = row._tr.get_or_add_trPr()
    tbl_header = OxmlElement("w:tblHeader")
    tbl_header.set(qn("w:val"), "true")
    tr_pr.append(tbl_header)


def keep_with_next(paragraph):
    paragraph.paragraph_format.keep_with_next = True


def set_repeat_table_header(row):
    repeat_table_header(row)


def prevent_row_split(row):
    tr_pr = row._tr.get_or_add_trPr()
    tr_pr.append(OxmlElement("w:cantSplit"))


def add_page_number(paragraph):
    paragraph.alignment = WD_ALIGN_PARAGRAPH.CENTER
    run = paragraph.add_run("第 ")
    set_run_font(run, size=9, color="666666")
    fld_begin = OxmlElement("w:fldChar")
    fld_begin.set(qn("w:fldCharType"), "begin")
    instr = OxmlElement("w:instrText")
    instr.set(qn("xml:space"), "preserve")
    instr.text = " PAGE "
    fld_sep = OxmlElement("w:fldChar")
    fld_sep.set(qn("w:fldCharType"), "separate")
    text = OxmlElement("w:t")
    text.text = "1"
    fld_end = OxmlElement("w:fldChar")
    fld_end.set(qn("w:fldCharType"), "end")
    r = paragraph.add_run()
    r._r.extend([fld_begin, instr, fld_sep, text, fld_end])
    set_run_font(r, size=9, color="666666")
    run = paragraph.add_run(" 页")
    set_run_font(run, size=9, color="666666")


def add_toc(doc):
    entries = [
        ("1  系统职责与边界", "3"),
        ("2  Flash 分区与构建地址", "3"),
        ("3  上电启动流程", "4"),
        ("4  Bootloader 源文件和函数地图", "5"),
        ("5  应用向量表验证", "6"),
        ("6  OTA 元数据和双副本机制", "7"),
        ("7  槽位选择逻辑", "8"),
        ("8  两次试启动和十秒确认", "8"),
        ("9  应用侧下载和校验流程", "9"),
        ("10 跳转到应用的底层过程", "10"),
        ("11 典型运行场景", "11"),
        ("12 Keil 工程关系", "11"),
        ("13 风险和改进建议", "12"),
        ("14 推荐阅读顺序", "12"),
        ("15 术语速查", "13"),
        ("附录 源码位置和当前验证状态", "14"),
    ]
    table = doc.add_table(rows=0, cols=2)
    table.alignment = WD_TABLE_ALIGNMENT.CENTER
    table.autofit = False
    table.allow_autofit = False
    for label, page in entries:
        cells = table.add_row().cells
        set_cell_text(cells[0], label, size=10.2)
        set_cell_text(cells[1], page, size=10.2, align=WD_ALIGN_PARAGRAPH.RIGHT)
        cells[0].width = Cm(14.2)
        cells[1].width = Cm(1.2)
        for cell in cells:
            set_cell_margins(cell, top=70, start=40, bottom=70, end=40)
    return table


def set_update_fields(doc):
    settings = doc.settings._element
    node = settings.find(qn("w:updateFields"))
    if node is None:
        node = OxmlElement("w:updateFields")
        settings.append(node)
    node.set(qn("w:val"), "true")


def set_cell_text(cell, text, *, bold=False, color=BLACK, align=WD_ALIGN_PARAGRAPH.LEFT,
                  size=9.2, code=False):
    cell.text = ""
    p = cell.paragraphs[0]
    p.alignment = align
    p.paragraph_format.space_before = Pt(0)
    p.paragraph_format.space_after = Pt(0)
    p.paragraph_format.line_spacing = 1.12
    run = p.add_run(str(text))
    set_run_font(run, east_asia="Microsoft YaHei", latin="Consolas" if code else "Aptos",
                 size=size, bold=bold, color=color)
    cell.vertical_alignment = WD_CELL_VERTICAL_ALIGNMENT.CENTER
    set_cell_margins(cell)


def add_table(doc, headers, rows, widths=None, aligns=None):
    table = doc.add_table(rows=1, cols=len(headers))
    table.alignment = WD_TABLE_ALIGNMENT.CENTER
    table.autofit = False
    table.allow_autofit = False
    set_table_borders(table)
    for idx, header in enumerate(headers):
        set_cell_text(table.rows[0].cells[idx], header, bold=True, color=WHITE,
                      align=WD_ALIGN_PARAGRAPH.CENTER, size=9.2)
        shade_cell(table.rows[0].cells[idx], NAVY)
        if widths:
            table.rows[0].cells[idx].width = Cm(widths[idx])
    set_repeat_table_header(table.rows[0])
    prevent_row_split(table.rows[0])
    for ridx, values in enumerate(rows):
        cells = table.add_row().cells
        prevent_row_split(table.rows[-1])
        for cidx, value in enumerate(values):
            align = aligns[cidx] if aligns else WD_ALIGN_PARAGRAPH.LEFT
            set_cell_text(cells[cidx], value, align=align,
                          code=(cidx > 0 and isinstance(value, str) and value.startswith("0x")))
            if widths:
                cells[cidx].width = Cm(widths[cidx])
            if ridx % 2 == 1:
                shade_cell(cells[cidx], PALE_BLUE)
    doc.add_paragraph().paragraph_format.space_after = Pt(0)
    return table


def add_body(doc, text, *, bold_lead=None):
    p = doc.add_paragraph()
    p.paragraph_format.first_line_indent = Cm(0.74)
    p.paragraph_format.space_after = Pt(5)
    p.paragraph_format.line_spacing = 1.38
    if bold_lead and text.startswith(bold_lead):
        first = p.add_run(bold_lead)
        set_run_font(first, size=10.5, bold=True)
        rest = p.add_run(text[len(bold_lead):])
        set_run_font(rest, size=10.5)
    else:
        run = p.add_run(text)
        set_run_font(run, size=10.5)
    return p


def add_bullet(doc, text, level=0):
    style = "List Bullet" if level == 0 else "List Bullet 2"
    p = doc.add_paragraph(style=style)
    p.paragraph_format.space_after = Pt(2)
    p.paragraph_format.line_spacing = 1.25
    run = p.add_run(text)
    set_run_font(run, size=10.2)
    return p


def add_numbered(doc, number, text):
    p = doc.add_paragraph()
    p.paragraph_format.left_indent = Cm(0.7)
    p.paragraph_format.first_line_indent = Cm(-0.7)
    p.paragraph_format.space_after = Pt(2)
    p.paragraph_format.line_spacing = 1.25
    run = p.add_run(f"{number}.  {text}")
    set_run_font(run, size=10.2)
    return p


def add_code(doc, code):
    p = doc.add_paragraph(style="Code Block")
    p.paragraph_format.keep_together = True
    p.paragraph_format.space_before = Pt(3)
    p.paragraph_format.space_after = Pt(7)
    p.paragraph_format.left_indent = Cm(0.5)
    p.paragraph_format.right_indent = Cm(0.3)
    p.paragraph_format.line_spacing = 1.05
    p_pr = p._p.get_or_add_pPr()
    shd = OxmlElement("w:shd")
    shd.set(qn("w:fill"), LIGHT_GRAY)
    p_pr.append(shd)
    run = p.add_run(code)
    set_run_font(run, east_asia="Microsoft YaHei", latin="Consolas", size=8.7, color="202020")
    return p


def add_heading(doc, text, level=1):
    p = doc.add_heading(text, level=level)
    keep_with_next(p)
    return p


def load_font(size, bold=False):
    candidates = [
        Path(r"C:\Windows\Fonts\msyhbd.ttc" if bold else r"C:\Windows\Fonts\msyh.ttc"),
        Path(r"C:\Windows\Fonts\simhei.ttf"),
        Path(r"C:\Windows\Fonts\simsun.ttc"),
    ]
    for candidate in candidates:
        if candidate.exists():
            return ImageFont.truetype(str(candidate), size=size)
    return ImageFont.load_default()


def centered_text(draw, box, text, font, fill="#000000", spacing=8):
    x1, y1, x2, y2 = box
    bbox = draw.multiline_textbbox((0, 0), text, font=font, spacing=spacing, align="center")
    w = bbox[2] - bbox[0]
    h = bbox[3] - bbox[1]
    draw.multiline_text(((x1 + x2 - w) / 2, (y1 + y2 - h) / 2), text,
                        font=font, fill=fill, spacing=spacing, align="center")


def arrow(draw, start, end, fill="#4472C4", width=6):
    draw.line([start, end], fill=fill, width=width)
    x, y = end
    draw.polygon([(x, y), (x - 13, y - 18), (x + 13, y - 18)], fill=fill)


def make_boot_flow(path):
    img = Image.new("RGB", (1500, 1160), "white")
    draw = ImageDraw.Draw(img)
    title_font = load_font(42, True)
    box_font = load_font(27, False)
    small_font = load_font(23, False)
    draw.text((55, 35), "Bootloader 启动与回滚流程", font=title_font, fill="#000000")

    def box(x1, y1, x2, y2, text, fill="#EEF4FB", outline="#4472C4", font=box_font):
        draw.rounded_rectangle((x1, y1, x2, y2), radius=20, fill=fill, outline=outline, width=4)
        centered_text(draw, (x1, y1, x2, y2), text, font)

    box(500, 105, 1000, 195, "上电或软件复位", fill="#D9EAF7")
    arrow(draw, (750, 195), (750, 245))
    box(500, 245, 1000, 345, "读取两份元数据并选择最新有效记录")
    arrow(draw, (750, 345), (750, 395))
    box(500, 395, 1000, 495, "是否存在 pending 槽", fill="#FFF2CC", outline="#BF9000")

    draw.line([(500, 445), (295, 445), (295, 530)], fill="#4472C4", width=6)
    draw.polygon([(295, 530), (282, 512), (308, 512)], fill="#4472C4")
    draw.text((330, 405), "否", font=small_font, fill="#000000")
    box(80, 530, 510, 630, "验证 active 槽向量表")
    arrow(draw, (295, 630), (295, 700))
    box(80, 700, 510, 800, "启动 active 槽", fill="#E2F0D9", outline="#548235")

    draw.line([(1000, 445), (1205, 445), (1205, 530)], fill="#4472C4", width=6)
    draw.polygon([(1205, 530), (1192, 512), (1218, 512)], fill="#4472C4")
    draw.text((1115, 405), "是", font=small_font, fill="#000000")
    box(990, 530, 1420, 630, "验证向量表和固件 CRC32")
    arrow(draw, (1205, 630), (1205, 700))
    box(990, 700, 1420, 800, "attempt_count 小于 2", fill="#FFF2CC", outline="#BF9000")

    draw.line([(990, 750), (835, 750), (835, 865)], fill="#4472C4", width=6)
    draw.polygon([(835, 865), (822, 847), (848, 847)], fill="#4472C4")
    draw.text((890, 708), "是", font=small_font, fill="#000000")
    box(610, 865, 1060, 965, "先持久化 attempt 加 1")
    arrow(draw, (835, 965), (835, 1015))
    box(610, 1015, 1060, 1110, "启动 pending 槽", fill="#E2F0D9", outline="#548235")

    draw.line([(1420, 750), (1460, 750), (1460, 865), (1225, 865)], fill="#C00000", width=6)
    draw.polygon([(1225, 865), (1243, 852), (1243, 878)], fill="#C00000")
    draw.text((1390, 708), "否", font=small_font, fill="#000000")
    box(1075, 865, 1425, 965, "清除 pending\n回滚 active", fill="#FCE4D6", outline="#C00000", font=small_font)
    draw.line([(1075, 915), (1075, 830), (545, 830), (545, 750), (510, 750)], fill="#C00000", width=6)
    draw.polygon([(510, 750), (528, 737), (528, 763)], fill="#C00000")

    img.save(path, dpi=(180, 180))


def make_flash_map(path):
    img = Image.new("RGB", (1500, 930), "white")
    draw = ImageDraw.Draw(img)
    title_font = load_font(42, True)
    label_font = load_font(28, True)
    text_font = load_font(24, False)
    addr_font = ImageFont.truetype(r"C:\Windows\Fonts\consola.ttf", 24) if Path(r"C:\Windows\Fonts\consola.ttf").exists() else text_font
    draw.text((55, 35), "STM32F103ZET6 Flash 分区", font=title_font, fill="#000000")
    regions = [
        ("Bootloader", "28 KB", "0x08000000", "0x08006FFF", 135, "#D9EAF7"),
        ("元数据副本 A", "2 KB", "0x08007000", "0x080077FF", 90, "#FFF2CC"),
        ("元数据副本 B", "2 KB", "0x08007800", "0x08007FFF", 90, "#FFE699"),
        ("AppSlotA", "240 KB", "0x08008000", "0x08043FFF", 225, "#E2F0D9"),
        ("AppSlotB", "240 KB", "0x08044000", "0x0807FFFF", 225, "#FCE4D6"),
    ]
    x1, x2, y = 420, 1120, 125
    for name, size, start, end, height, fill in regions:
        draw.rectangle((x1, y, x2, y + height), fill=fill, outline="#666666", width=3)
        draw.text((x1 + 25, y + 18), name, font=label_font, fill="#000000")
        draw.text((x2 - 145, y + 20), size, font=text_font, fill="#333333")
        draw.text((70, y + 10), start, font=addr_font, fill="#333333")
        draw.text((1160, y + height - 40), end, font=addr_font, fill="#333333")
        y += height
    draw.text((420, 895), "总容量 512 KB", font=text_font, fill="#333333")
    img.save(path, dpi=(180, 180))


def configure_styles(doc):
    styles = doc.styles
    normal = styles["Normal"]
    normal.font.name = "Aptos"
    normal._element.rPr.rFonts.set(qn("w:eastAsia"), "Microsoft YaHei")
    normal.font.size = Pt(10.5)
    normal.font.color.rgb = RGBColor(0, 0, 0)
    normal.paragraph_format.line_spacing = 1.4
    normal.paragraph_format.space_after = Pt(5)

    title = styles["Title"]
    title.font.name = "Aptos Display"
    title._element.rPr.rFonts.set(qn("w:eastAsia"), "Microsoft YaHei")
    title.font.size = Pt(28)
    title.font.bold = True
    title.font.color.rgb = RGBColor(0, 0, 0)
    title.paragraph_format.space_after = Pt(15)
    title_p_pr = title._element.get_or_add_pPr()
    title_border = title_p_pr.find(qn("w:pBdr"))
    if title_border is not None:
        title_p_pr.remove(title_border)

    for name, size, before, after in (("Heading 1", 17, 14, 7), ("Heading 2", 13, 10, 5), ("Heading 3", 11, 7, 3)):
        style = styles[name]
        style.font.name = "Aptos Display"
        style._element.rPr.rFonts.set(qn("w:eastAsia"), "Microsoft YaHei")
        style.font.size = Pt(size)
        style.font.bold = True
        style.font.color.rgb = RGBColor(0, 0, 0)
        style.paragraph_format.space_before = Pt(before)
        style.paragraph_format.space_after = Pt(after)
        style.paragraph_format.keep_with_next = True

    for name in ("List Bullet", "List Bullet 2", "List Number"):
        styles[name].font.name = "Aptos"
        styles[name]._element.rPr.rFonts.set(qn("w:eastAsia"), "Microsoft YaHei")
        styles[name].font.size = Pt(10.2)

    if "Code Block" not in styles:
        style = styles.add_style("Code Block", WD_STYLE_TYPE.PARAGRAPH)
    else:
        style = styles["Code Block"]
    style.font.name = "Consolas"
    style._element.rPr.rFonts.set(qn("w:eastAsia"), "Microsoft YaHei")
    style.font.size = Pt(8.7)


def build_document():
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    QA_DIR.mkdir(parents=True, exist_ok=True)
    flow_path = QA_DIR / "boot-flow.png"
    flash_path = QA_DIR / "flash-map.png"
    make_boot_flow(flow_path)
    make_flash_map(flash_path)

    doc = Document()
    configure_styles(doc)
    section = doc.sections[0]
    section.page_width = Cm(21)
    section.page_height = Cm(29.7)
    section.top_margin = Cm(2.2)
    section.bottom_margin = Cm(2.0)
    section.left_margin = Cm(2.25)
    section.right_margin = Cm(2.25)
    section.header_distance = Cm(1.0)
    section.footer_distance = Cm(0.9)

    # Title page
    for _ in range(4):
        doc.add_paragraph()
    p = doc.add_paragraph(style="Title")
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    run = p.add_run("Bootloader 代码理解指南")
    set_run_font(run, east_asia="Microsoft YaHei", latin="Aptos Display", size=28, bold=True)
    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.paragraph_format.space_after = Pt(20)
    run = p.add_run("STM32F103 A B 双分区 OTA 启动 回滚与应用确认")
    set_run_font(run, size=14, color="333333")
    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    run = p.add_run("面向 huanjing freeRTOS HAL 工程的源码阅读手册")
    set_run_font(run, size=11, color="666666")
    for _ in range(8):
        doc.add_paragraph()
    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    run = p.add_run("整理日期 2026 年 9 月 4 日")
    set_run_font(run, size=10, color="666666")
    doc.add_page_break()

    # Opening summary and TOC
    add_heading(doc, "阅读说明", 1)
    add_body(doc, "这份文档解释工程中的 STM32F103 Bootloader 及其配套 OTA 逻辑。读者不需要先通读全部源码，可以先建立分区、元数据、试启动和回滚四个概念，再回到具体函数。")
    add_body(doc, "核心结论是：应用程序负责把新固件写入另一槽并创建 pending 记录；Bootloader 只负责读取记录、验证镜像、记录试启动次数、选择槽位和跳转；新应用稳定运行后再把自己确认为 active。")
    add_body(doc, "本文以 portable_ota 中的 Bootloader 公共源码为准，同时说明 huanjing_HAL 应用侧如何下载和确认固件。文中地址、容量、版本宏和构建结果均对应当前工程配置。")
    add_heading(doc, "目录", 1)
    add_toc(doc)
    doc.add_page_break()

    add_heading(doc, "1 系统职责与边界", 1)
    add_body(doc, "这套 OTA 系统采用 A B 双应用槽。设备始终保留一个已经确认的稳定槽位，当前应用只能擦写另一个槽位。Bootloader 不连接 WiFi，不接收云端数据，也不初始化 LCD 和传感器，因此启动代码保持很小。")
    add_table(doc,
              ["模块", "主要职责", "明确不负责"],
              [
                  ["Bootloader", "读取元数据，校验 pending 镜像，选择槽位，记录试启动次数，回滚并跳转", "联网下载，LCD，传感器，FreeRTOS 业务"],
                  ["当前应用", "接收固件，擦除另一槽，写入 Flash，校验 SHA 256，创建 pending，确认运行", "覆盖正在运行的槽位"],
                  ["ESP32 网关", "连接云端并把 OTA 数据通过 USART2 发送给 STM32 应用", "决定 STM32 最终启动槽位"],
                  ["元数据区", "保存 active pending 镜像长度 CRC 和试启动次数", "保存完整固件"],
              ], widths=[2.8, 8.2, 5.3], aligns=[WD_ALIGN_PARAGRAPH.CENTER, WD_ALIGN_PARAGRAPH.LEFT, WD_ALIGN_PARAGRAPH.LEFT])
    add_heading(doc, "1 1 最重要的状态", 2)
    add_bullet(doc, "active 表示已经被应用确认、可长期启动的稳定槽位。")
    add_bullet(doc, "pending 表示刚下载完成、正在等待试运行和确认的新槽位。")
    add_bullet(doc, "attempt_count 表示 pending 槽已经被 Bootloader 启动过多少次。")
    add_bullet(doc, "generation 表示元数据记录的新旧顺序，与应用版本字符串无关。")

    add_heading(doc, "2 Flash 分区与构建地址", 1)
    add_body(doc, "STM32F103ZET6 提供 512 KB 内部 Flash。工程为 Bootloader 预留 28 KB，为两份元数据各预留一个 2 KB 页，其余空间均分成两个 240 KB 应用槽。所有地址由 ota_port_config.h 定义，并由三个 Keil 工程共同遵守。")
    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.paragraph_format.keep_together = True
    p.add_run().add_picture(str(flash_path), width=Cm(15.8))
    cap = doc.add_paragraph("图 1  STM32F103ZET6 Flash 分区")
    cap.alignment = WD_ALIGN_PARAGRAPH.CENTER
    set_run_font(cap.runs[0], size=9, color="666666")
    add_table(doc,
              ["区域", "起始地址", "结束地址", "大小", "用途"],
              [
                  ["Bootloader", "0x08000000", "0x08006FFF", "28 KB", "启动选择和跳转"],
                  ["元数据副本 A", "0x08007000", "0x080077FF", "2 KB", "第一份启动记录"],
                  ["元数据副本 B", "0x08007800", "0x08007FFF", "2 KB", "第二份启动记录"],
                  ["AppSlotA", "0x08008000", "0x08043FFF", "240 KB", "A 槽应用镜像"],
                  ["AppSlotB", "0x08044000", "0x0807FFFF", "240 KB", "B 槽应用镜像"],
              ], widths=[3.1, 3.2, 3.2, 2.1, 4.7],
              aligns=[WD_ALIGN_PARAGRAPH.CENTER] * 4 + [WD_ALIGN_PARAGRAPH.LEFT])
    add_body(doc, "SRAM 范围是 0x20000000 到 0x20010000。向量表验证允许初始 MSP 等于 SRAM 顶端 0x20010000，因为 Cortex M 栈向低地址增长。")

    add_heading(doc, "3 上电启动流程", 1)
    add_body(doc, "CPU 从 0x08000000 进入 Bootloader。Bootloader 先读取两份元数据，再根据 active pending 和 attempt_count 选择槽位。若存在 pending，它还会重新计算固件 CRC32。只有启动决策持久化成功后才会跳转。")
    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.paragraph_format.keep_together = True
    p.add_run().add_picture(str(flow_path), width=Cm(14.8))
    cap = doc.add_paragraph("图 2  Bootloader 启动与回滚流程")
    cap.alignment = WD_ALIGN_PARAGRAPH.CENTER
    set_run_font(cap.runs[0], size=9, color="666666")
    add_heading(doc, "3 1 主入口", 2)
    add_code(doc, "int main(void)\n{\n    OtaSlot_t slot;\n\n    Bootloader_DebugInit();\n    slot = Bootloader_SelectSlot();\n    Bootloader_JumpToSlot(slot);\n\n    while (1) {}\n}")
    add_body(doc, "main 函数只有初始化调试串口、选择槽位和跳转三个阶段。Bootloader_DebugInit 直接使用复位后的 8 MHz HSI 配置 USART1 TX 引脚 PA9，以 115200 波特率输出 BOOT slot A、BOOT slot B 或 BOOT no valid app。")

    add_heading(doc, "4 Bootloader 源文件和函数地图", 1)
    add_table(doc,
              ["函数", "位置", "作用"],
              [
                  ["Bootloader DebugInit", "boot_main.c 18", "使用寄存器初始化 USART1 发送功能"],
                  ["Bootloader DebugText", "boot_main.c 33", "轮询发送固定文本，不引入 printf"],
                  ["Bootloader VectorIsValid", "boot_main.c 51", "读取槽位首部 MSP 和 Reset Handler 并检查"],
                  ["Bootloader PendingImageIsValid", "boot_main.c 68", "检查 pending 记录、向量表和整镜像 CRC32"],
                  ["Bootloader ClearPending", "boot_main.c 95", "清除失效或超时 pending，保留 active"],
                  ["Bootloader SelectSlot", "boot_main.c 105", "完成启动选择和试启动次数持久化"],
                  ["Bootloader JumpToSlot", "boot_main.c 146", "清理中断状态并跳转应用 Reset Handler"],
              ], widths=[5.3, 3.7, 7.3], aligns=[WD_ALIGN_PARAGRAPH.LEFT, WD_ALIGN_PARAGRAPH.CENTER, WD_ALIGN_PARAGRAPH.LEFT])
    add_body(doc, "bootloader.h 只对外暴露 Bootloader_SelectSlot 和 Bootloader_JumpToSlot。元数据格式、Flash 读写和校验算法分别放在 ota_metadata、ota_storage 和 ota_integrity 模块中。")

    add_heading(doc, "5 应用向量表验证", 1)
    add_body(doc, "Cortex M 应用镜像的前两个 32 位字分别是初始主栈指针 MSP 和 Reset Handler。Bootloader 不能仅凭 Flash 非空判断应用有效，必须检查这两个值是否符合目标芯片和目标槽位。")
    add_code(doc, "msp           = *(const uint32_t *)base;\nreset_handler = *(const uint32_t *)(base + sizeof(uint32_t));\nreturn Ota_VectorWordsAreValid(slot, msp, reset_handler);")
    add_table(doc,
              ["检查项", "当前规则", "防止的问题"],
              [
                  ["槽位", "必须是 A 或 B", "空槽位编号和错误参数"],
                  ["Thumb 位", "Reset Handler 最低位必须为 1", "跳到非 Thumb 地址"],
                  ["MSP 对齐", "MSP 必须按 4 字节对齐", "无效栈指针"],
                  ["MSP 范围", "必须位于 STM32 SRAM 范围", "把 Flash 数据当作栈"],
                  ["入口范围", "Reset Handler 必须位于目标槽", "A 固件错误放入 B 槽等链接地址错误"],
              ], widths=[3.0, 6.2, 7.1], aligns=[WD_ALIGN_PARAGRAPH.CENTER, WD_ALIGN_PARAGRAPH.LEFT, WD_ALIGN_PARAGRAPH.LEFT])
    add_body(doc, "这套检查可以识别全 0xFF Flash、不完整镜像、错误链接地址和受损向量表，但它不能证明固件来自可信发布者。")

    add_heading(doc, "6 OTA 元数据和双副本机制", 1)
    add_body(doc, "OtaBootRecord_t 只有 32 字节，却分别占用两个独立的 2 KB Flash 页。保存时总是擦除并写入当前最新记录的另一页，因此掉电不会同时破坏新旧两份启动信息。")
    add_table(doc,
              ["字段", "含义", "典型值"],
              [
                  ["magic", "固定标识 OTA1，用于识别记录", "0x4F544131"],
                  ["generation", "每次保存新记录时递增，用于判断新旧", "1 2 3"],
                  ["active_slot", "已经确认的稳定槽位", "A 或 B"],
                  ["pending_slot", "等待试启动的新槽位", "NONE A 或 B"],
                  ["pending_size", "待启动镜像的字节数", "0 或实际长度"],
                  ["pending_crc32", "Flash 中待启动镜像的 CRC32", "无 pending 时为 0"],
                  ["attempt_count", "pending 已经试启动的次数", "0 1 2"],
                  ["record_crc32", "前面所有字段的 CRC32", "写入顺序中的最后字段"],
              ], widths=[3.4, 8.0, 4.9], aligns=[WD_ALIGN_PARAGRAPH.LEFT, WD_ALIGN_PARAGRAPH.LEFT, WD_ALIGN_PARAGRAPH.CENTER])
    add_heading(doc, "6 1 记录选择", 2)
    add_numbered(doc, 1, "分别校验 A 页和 B 页的魔数、槽位组合、长度和记录 CRC32。")
    add_numbered(doc, 2, "如果只有一页有效，直接选择它。")
    add_numbered(doc, 3, "如果两页都有效，选择 generation 更新的一页。")
    add_numbered(doc, 4, "generation 使用有符号差值比较，因此从 0xFFFFFFFF 回绕到 0 时仍可判断新旧。")
    add_heading(doc, "6 2 掉电安全顺序", 2)
    add_code(doc, "旧记录在 A 页有效\n    -> 擦除 B 页\n    -> 把新记录写入 B 页，record_crc32 最后写入\n    -> 读回并校验 B 页\n    -> 下一次保存再轮换到 A 页")
    add_body(doc, "若写 B 页过程中掉电，B 页通常因为记录 CRC 不完整而无效，A 页旧记录仍可使用。当前实现每次只擦除一个元数据页，不会先删除唯一有效的回滚信息。")

    add_heading(doc, "7 槽位选择逻辑", 1)
    add_heading(doc, "7 1 首次启动", 2)
    add_body(doc, "新板通过 ST Link 烧录 Bootloader 和 AppSlotA 后，元数据区通常还是空白。OtaStorage_Load 返回 false 时，Bootloader 只尝试验证并启动 A 槽。若 A 也无效，则返回 NONE。")
    add_code(doc, "if (!OtaStorage_Load(&record))\n{\n    return Bootloader_VectorIsValid(OTA_SLOT_A)\n               ? OTA_SLOT_A\n               : OTA_SLOT_NONE;\n}")
    add_heading(doc, "7 2 正常启动", 2)
    add_body(doc, "当 pending_slot 为 NONE 时，Bootloader 验证 active_slot 的向量表并启动 active。它不会根据 v1.2.0 或 v1.1.0 这样的版本字符串比较新旧。")
    add_heading(doc, "7 3 试启动", 2)
    add_body(doc, "存在 pending 时，Bootloader 检查记录、向量表和 pending_size 范围内的 CRC32。如果镜像有效且尝试次数少于 2，它先生成下一代元数据，把 attempt_count 加 1，再启动 pending。")
    add_code(doc, "next_record = record;\nnext_record.generation++;\nOtaRecord_SetAttemptCount(&next_record, record.attempt_count + 1U);\nif (OtaStorage_Save(&next_record) != HAL_OK)\n{\n    return active_slot;\n}\nreturn pending_slot;")
    add_heading(doc, "7 4 回滚", 2)
    add_body(doc, "pending 镜像无效或 attempt_count 已达到 2 时，Bootloader 创建一条新记录：active_slot 保持不变，pending_slot 设为 NONE，长度、CRC 和次数全部清零。随后启动原 active 槽。")

    add_heading(doc, "8 两次试启动和十秒确认", 1)
    add_body(doc, "最大尝试次数 OTA_BOOT_MAX_ATTEMPTS 为 2。每次跳转 pending 之前次数都会加一，因此新镜像可以真正运行两次；第三次进入 Bootloader 时才会因为次数已经达到 2 而回滚。")
    add_table(doc,
              ["启动阶段", "启动前次数", "Bootloader 动作", "记录结果"],
              [
                  ["第一次试启动", "0", "先写 1，再启动 pending", "attempt_count 1"],
                  ["第二次试启动", "1", "先写 2，再启动 pending", "attempt_count 2"],
                  ["下一次启动", "2", "清除 pending，启动 active", "回滚完成"],
              ], widths=[3.7, 3.0, 6.2, 3.4], aligns=[WD_ALIGN_PARAGRAPH.CENTER] * 4)
    add_heading(doc, "8 1 为什么必须先写次数", 2)
    add_body(doc, "如果先跳转再记录次数，新应用可能在执行第一条业务代码前就崩溃，Bootloader 将永远看不到失败。当前顺序先把尝试写入 Flash，写入失败时也拒绝启动 pending，因此不会形成无限重启循环。")
    add_heading(doc, "8 2 应用确认", 2)
    add_body(doc, "应用侧 OTA 任务记录启动时间。当前槽位属于 pending 且运行超过 10 秒后，OtaApp_ConfirmRunning 会把当前槽位写成 active，并清除 pending。若应用在 10 秒内崩溃、看门狗复位或掉电，它来不及确认，下一次仍按试启动流程处理。")
    add_code(doc, "if (!confirmed && RtosOta_ConfirmWhenStable(boot_time))\n{\n    confirmed = true;\n}")

    add_heading(doc, "9 应用侧下载和校验流程", 1)
    add_body(doc, "Bootloader 不接收固件。运行中的 AppSlotA 或 AppSlotB 通过 USART2 接收 ESP32 网关传来的固件，并调用 ota_app 模块把数据写入另一槽。只有 OtaApp_Finalize 完成全部检查后才会创建 pending 记录。")
    add_numbered(doc, 1, "OtaApp_Begin 检查固件大小、SHA 256 文本、运行槽和目标槽，并擦除目标槽所需页面。")
    add_numbered(doc, 2, "OtaApp_Write 要求 offset 连续，按 STM32F1 的 16 位半字写入 Flash，并在每段写入后读回比较。")
    add_numbered(doc, 3, "每段数据同时进入增量 SHA 256 上下文。")
    add_numbered(doc, 4, "OtaApp_Finalize 检查总长度和 SHA 256，再检查向量表。")
    add_numbered(doc, 5, "应用从 Flash 重新计算 CRC32，创建 active 仍为当前槽、pending 为目标槽的新记录。")
    add_numbered(doc, 6, "系统复位，Bootloader 在下一次启动时重新验证 CRC32 并试启动。")
    add_heading(doc, "9 1 三层校验", 2)
    add_table(doc,
              ["校验", "对象", "执行阶段", "作用"],
              [
                  ["record_crc32", "OTA 元数据", "每次加载记录", "发现启动决策数据损坏"],
                  ["SHA 256", "下载的固件", "应用 Finalize", "确认接收内容与云端声明一致"],
                  ["pending_crc32", "Flash 中待启动镜像", "每次 Bootloader 试启动", "快速发现传输或 Flash 偶然错误"],
              ], widths=[3.3, 3.8, 4.0, 5.2], aligns=[WD_ALIGN_PARAGRAPH.CENTER, WD_ALIGN_PARAGRAPH.CENTER, WD_ALIGN_PARAGRAPH.CENTER, WD_ALIGN_PARAGRAPH.LEFT])
    add_body(doc, "SHA 256 和 CRC32 在这里都用于完整性判断。当前代码没有数字签名验证，因此不能仅凭这些值证明固件来自可信发布者。")

    add_heading(doc, "10 跳转到应用的底层过程", 1)
    add_body(doc, "Bootloader_JumpToSlot 在跳转前再次验证向量表，然后把 CPU 从 Bootloader 的运行环境切换到应用环境。执行顺序不能随意调整。")
    add_table(doc,
              ["顺序", "操作", "原因"],
              [
                  ["1", "读取新 MSP 和 Reset Handler", "获取应用栈顶与入口"],
                  ["2", "关闭全局中断", "防止清理期间进入旧中断"],
                  ["3", "停止 SysTick", "避免 Bootloader 节拍继续运行"],
                  ["4", "关闭并清除两组 NVIC", "移除旧应用或 Bootloader 的中断状态"],
                  ["5", "SCB VTOR 设为槽位首地址", "让中断向量指向新应用"],
                  ["6", "执行 DSB 和 ISB", "确保向量表修改立即生效"],
                  ["7", "设置 MSP", "切换到应用栈"],
                  ["8", "重新使能中断并调用 Reset Handler", "按接近硬件复位的状态进入应用"],
              ], widths=[1.7, 6.6, 8.0], aligns=[WD_ALIGN_PARAGRAPH.CENTER, WD_ALIGN_PARAGRAPH.LEFT, WD_ALIGN_PARAGRAPH.LEFT])
    add_code(doc, "__disable_irq();\nSysTick->CTRL = 0U;\nNVIC->ICER[0] = NVIC->ICER[1] = 0xFFFFFFFFUL;\nNVIC->ICPR[0] = NVIC->ICPR[1] = 0xFFFFFFFFUL;\nSCB->VTOR = base;\n__DSB();\n__ISB();\n__set_MSP(new_msp);\n__enable_irq();\n((BootloaderEntry_t)reset_handler)();")
    add_body(doc, "应用的 system_stm32f1xx.c 当前没有启用 USER_VECT_TAB_ADDRESS，因此 SystemInit 不会把 Bootloader 已设置的 VTOR 改回 0x08000000。")

    add_heading(doc, "11 典型运行场景", 1)
    add_table(doc,
              ["场景", "元数据或条件", "结果"],
              [
                  ["正常启动 A", "active A，pending NONE", "验证 A 向量表并启动 A"],
                  ["A 下载 B 成功", "active A，pending B，attempt 0", "复位后第一次试启动 B"],
                  ["B 稳定运行", "B 运行超过 10 秒并确认", "active 改为 B，pending 清除"],
                  ["B 连续启动失败", "attempt 依次变为 1 和 2", "下一次回滚 A"],
                  ["下载中途断电", "尚未写 pending 记录", "继续启动旧 active，不采用残缺槽"],
                  ["元数据一页损坏", "另一页记录仍有效", "加载有效副本并正常启动"],
                  ["两槽均无效", "无可用向量表", "打印 no valid app 并停机"],
              ], widths=[4.0, 6.2, 6.1], aligns=[WD_ALIGN_PARAGRAPH.CENTER, WD_ALIGN_PARAGRAPH.LEFT, WD_ALIGN_PARAGRAPH.LEFT])

    add_heading(doc, "12 Keil 工程关系", 1)
    add_body(doc, "当前仓库使用三个独立 Keil 工程。这样可以避免 Bootloader 错误链接应用 main.c，也可以避免 A B 应用错误链接 boot_main.c。三个工程的 IROM1 地址必须与 ota_port_config.h 保持一致。")
    add_table(doc,
              ["工程", "IROM1 起始地址", "容量", "编译宏", "当前输出"],
              [
                  ["Bootloader", "0x08000000", "0x7000", "STM32F103xE", "bootloader"],
                  ["AppSlotA", "0x08008000", "0x3C000", "OTA SLOT A，v1.2.0", "app_slot_a"],
                  ["AppSlotB", "0x08044000", "0x3C000", "OTA SLOT B，v1.1.0", "app_slot_b"],
              ], widths=[3.1, 3.3, 2.4, 4.6, 2.9], aligns=[WD_ALIGN_PARAGRAPH.CENTER] * 5)
    add_body(doc, "当前 map 文件显示 Bootloader ROM 约 2.71 KB，A B 应用 ROM 均约 81.09 KB。应用槽容量为 240 KB，因此现有镜像没有接近槽位上限。")
    add_heading(doc, "12 1 两套 OTA 源码", 2)
    add_body(doc, "Bootloader 从 portable_ota stm32 ota_core 编译元数据、存储和校验代码；A B 应用从 huanjing_HAL user OTA 编译本地副本。当前两套相关文件内容一致，test_ota_source_sync.ps1 会检查同步状态。修改公共 OTA 逻辑时必须同步两处。")

    add_heading(doc, "13 风险和改进建议", 1)
    add_table(doc,
              ["优先级", "发现", "影响", "建议"],
              [
                  ["高", "没有固件数字签名", "攻击者若能同时替换固件和 SHA 256，设备仍会接受", "在发布链加入私钥签名，在设备端内置公钥并验证签名"],
                  ["中", "pending 入口只检查整个槽位范围", "Reset Handler 理论上可指向镜像长度之外的槽内残留数据", "要求入口小于 slot_base 加 pending_size"],
                  ["中", "十秒确认只依赖 OTA 任务和系统节拍", "传感器 LCD 或通信故障时仍可能确认", "把关键任务心跳和必要自检纳入确认条件"],
                  ["中", "两槽都无效时没有恢复通道", "设备只能打印错误并停机", "增加按键串口 USB 或工厂镜像恢复模式"],
                  ["低", "镜像长度注释与实现分散", "公共函数注释说要求半字对齐，但偶数检查在应用层", "统一 Ota_ImageSizeIsValid 的契约或修正注释"],
                  ["低", "切换 MSP 后仍由普通 C 代码调用入口", "编译器或工具链变化时存在栈访问风险", "将设置 MSP 和跳转封装为经过验证的最小汇编例程"],
                  ["低", "OTA 公共源码存在两份", "长期修改可能发生漂移", "保留同步测试，或让三个工程直接引用同一份公共源码"],
              ], widths=[1.8, 4.1, 5.2, 5.2], aligns=[WD_ALIGN_PARAGRAPH.CENTER, WD_ALIGN_PARAGRAPH.LEFT, WD_ALIGN_PARAGRAPH.LEFT, WD_ALIGN_PARAGRAPH.LEFT])
    add_body(doc, "这些项目不代表当前系统不能工作。高优先级项主要影响固件来源的安全性；中优先级项影响异常镜像、局部功能故障和现场恢复；低优先级项主要影响可移植性与长期维护。")

    add_heading(doc, "14 推荐阅读顺序", 1)
    add_body(doc, "第一次阅读时，先抓住地址和状态，再看 Flash 写入细节。下面的顺序可以减少在函数之间来回跳转。")
    add_table(doc,
              ["顺序", "文件", "先理解什么"],
              [
                  ["1", "ota_port_config.h", "Flash SRAM 容量以及 A B 起始地址"],
                  ["2", "ota_layout.h", "槽位枚举和地址计算"],
                  ["3", "ota_metadata.h", "active pending attempt generation 字段"],
                  ["4", "boot_main.c", "选择槽位和跳转的主流程"],
                  ["5", "ota_metadata.c", "记录合法性和新旧副本选择"],
                  ["6", "ota_storage.c", "Flash 页轮换和掉电保护"],
                  ["7", "ota_integrity.c", "CRC32 镜像长度和向量表验证"],
                  ["8", "ota_app.c", "应用写入另一槽并创建 pending"],
                  ["9", "freertos.c", "新应用运行 10 秒后的确认"],
                  ["10", "ota_uart.c", "ESP32 和 STM32 的升级传输协议"],
              ], widths=[1.7, 5.5, 9.1], aligns=[WD_ALIGN_PARAGRAPH.CENTER, WD_ALIGN_PARAGRAPH.LEFT, WD_ALIGN_PARAGRAPH.LEFT])
    add_heading(doc, "14 1 建议调试观察点", 2)
    add_bullet(doc, "在 Bootloader_SelectSlot 入口观察 record 的八个字段。")
    add_bullet(doc, "在 OtaStorage_Save 观察 newest 和 target_page，确认每次写入另一页。")
    add_bullet(doc, "在第一次和第二次试启动前观察 attempt_count 是否分别变为 1 和 2。")
    add_bullet(doc, "在 Bootloader_JumpToSlot 观察 base、new_msp、reset_handler 和 SCB VTOR。")
    add_bullet(doc, "在 OtaApp_ConfirmRunning 前后比较 active_slot 和 pending_slot。")

    add_heading(doc, "15 术语速查", 1)
    add_table(doc,
              ["术语", "含义"],
              [
                  ["A B 槽", "两块互不重叠的应用 Flash 区域，一块运行时只更新另一块"],
                  ["active", "已确认可稳定启动的槽位"],
                  ["pending", "已下载但尚未确认的新槽位"],
                  ["MSP", "Main Stack Pointer，应用启动时使用的主栈指针"],
                  ["Reset Handler", "应用向量表中的复位入口函数"],
                  ["VTOR", "Vector Table Offset Register，中断向量表基地址寄存器"],
                  ["CRC32", "适合发现偶然数据损坏的校验值，不提供身份认证"],
                  ["SHA 256", "较强的内容摘要；没有可信签名时仍不能独立证明发布者身份"],
                  ["generation", "用于选择最新元数据副本的单调递增序号"],
                  ["rollback", "新镜像连续试启动失败后重新启动旧 active 槽"],
              ], widths=[4.1, 12.2], aligns=[WD_ALIGN_PARAGRAPH.CENTER, WD_ALIGN_PARAGRAPH.LEFT])

    add_heading(doc, "附录 源码位置", 1)
    source_paths = [
        r"portable_ota\stm32\stm32f103_example\bootloader\boot_main.c",
        r"portable_ota\stm32\stm32f103_example\bootloader\bootloader.h",
        r"portable_ota\stm32\ota_core\Inc\ota_port_config.h",
        r"portable_ota\stm32\ota_core\Inc\ota_layout.h",
        r"portable_ota\stm32\ota_core\Inc\ota_metadata.h",
        r"portable_ota\stm32\ota_core\Src\ota_metadata.c",
        r"portable_ota\stm32\ota_core\Src\ota_storage.c",
        r"portable_ota\stm32\ota_core\Src\ota_integrity.c",
        r"huanjing_HAL\user\OTA\Src\ota_app.c",
        r"huanjing_HAL\Core\Src\freertos.c",
        r"huanjing_HAL\MDK-ARM\Bootloader.uvprojx",
        r"huanjing_HAL\MDK-ARM\AppSlotA.uvprojx",
        r"huanjing_HAL\MDK-ARM\AppSlotB.uvprojx",
    ]
    for source in source_paths:
        p = doc.add_paragraph()
        p.paragraph_format.left_indent = Cm(0.5)
        p.paragraph_format.space_after = Pt(2)
        run = p.add_run(source)
        set_run_font(run, east_asia="Microsoft YaHei", latin="Consolas", size=8.8, color="333333")

    add_heading(doc, "附录 当前验证状态", 1)
    add_body(doc, "工程现有 PowerShell 自动检查在 2026 年 9 月 4 日全部通过，包括 Keil 目标地址、CPU 配置、OTA 源码同步、OTA 串口契约和任务结构检查。现有 Keil 构建日志记录 Bootloader、AppSlotA 和 AppSlotB 均为 0 个错误、0 个警告。实机升级仍应覆盖下载中断电、两次启动失败、确认成功和 A B 双向升级。")

    # Footer page numbers and document settings
    for sec in doc.sections:
        add_page_number(sec.footer.paragraphs[0])
    set_update_fields(doc)
    doc.core_properties.title = "Bootloader 代码理解指南"
    doc.core_properties.subject = "STM32F103 A B 双分区 OTA 启动 回滚与应用确认"
    doc.core_properties.author = ""
    doc.core_properties.keywords = "STM32F103, Bootloader, OTA, A/B, FreeRTOS"
    doc.core_properties.comments = ""
    doc.save(OUTPUT_PATH)
    print(OUTPUT_PATH)


if __name__ == "__main__":
    build_document()
