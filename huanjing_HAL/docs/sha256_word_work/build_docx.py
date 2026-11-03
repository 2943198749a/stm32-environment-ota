from pathlib import Path
import re
from docx import Document
from docx.shared import Cm, Pt, RGBColor
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.enum.style import WD_STYLE_TYPE
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.enum.table import WD_TABLE_ALIGNMENT, WD_CELL_VERTICAL_ALIGNMENT

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'SHA256教学代码学习说明.md'
OUTPUT = ROOT / 'SHA256教学代码学习说明.docx'
doc = Document()
sec = doc.sections[0]
sec.page_width, sec.page_height = Cm(21), Cm(29.7)
sec.top_margin = sec.bottom_margin = Cm(1.9)
sec.left_margin = sec.right_margin = Cm(1.9)
sec.footer_distance = Cm(0.8)

def font(style, size, latin='Calibri', east='Microsoft YaHei'):
    style.font.name = latin
    style.font.size = Pt(size)
    style.font.color.rgb = RGBColor(0, 0, 0)
    rf = style.element.get_or_add_rPr().rFonts
    rf.set(qn('w:eastAsia'), east)
    rf.set(qn('w:ascii'), latin)
    rf.set(qn('w:hAnsi'), latin)

font(doc.styles['Normal'], 10.5)
normal = doc.styles['Normal'].paragraph_format
normal.space_after = Pt(6)
normal.line_spacing = 1.2
normal.widow_control = True
for name, size in [('Title',24), ('Heading 1',16), ('Heading 2',12.5)]:
    st = doc.styles[name]
    font(st, size)
    st.font.bold = True
    st.paragraph_format.keep_with_next = True
    st.paragraph_format.space_before = Pt(14 if name != 'Title' else 0)
    st.paragraph_format.space_after = Pt(8)
    st.paragraph_format.line_spacing = 1.15
code_style = doc.styles.add_style('Teaching Code', WD_STYLE_TYPE.PARAGRAPH)
font(code_style, 9, 'Consolas', 'SimSun')
code_style.paragraph_format.space_after = Pt(0)
code_style.paragraph_format.space_before = Pt(0)
code_style.paragraph_format.line_spacing = 1.08
code_style.paragraph_format.left_indent = Cm(0.15)
code_style.paragraph_format.widow_control = False

footer = sec.footer.paragraphs[0]
footer.alignment = WD_ALIGN_PARAGRAPH.CENTER
footer.add_run('第 ')
field = OxmlElement('w:fldSimple')
field.set(qn('w:instr'), 'PAGE')
footer._p.append(field)
footer.add_run(' 页')
for r in footer.runs:
    r.font.size = Pt(9)

def inline(p, text, small=False):
    # Preserve inline code and bold while removing Markdown delimiters.
    for part in re.split(r'(`[^`]+`|\*\*.*?\*\*)', text):
        if not part:
            continue
        is_code = part.startswith('`') and part.endswith('`')
        bold = part.startswith('**') and part.endswith('**')
        content = part[1:-1] if is_code else part[2:-2] if bold else part
        r = p.add_run(content.replace('\\|', '|'))
        if bold:
            r.bold = True
        if is_code:
            r.font.name = 'Consolas'
            r.font.size = Pt(9 if small else 9.5)
            r._element.get_or_add_rPr().rFonts.set(qn('w:eastAsia'), 'Microsoft YaHei')
        elif small:
            r.font.size = Pt(9.5)

def heading_text(t):
    t = t.replace('SHA-256', 'SHA256').replace('`', '')
    t = t.replace('()', '').replace('_', ' ')
    t = re.sub(r'[：，、。？！“”「」《》（）()→～—：]', ' ', t)
    t = re.sub(r'^(\d+)\.\s+', r'\1 ', t)
    return re.sub(r'\s+', ' ', t).strip()

def table_row(line):
    return [c.strip().replace('\\|','|') for c in re.split(r'(?<!\\)\|', line.strip())[1:-1]]

def add_table(rows):
    rows = [r for r in rows if not all(re.fullmatch(r':?-+:?', c.strip()) for c in r)]
    n = len(rows[0])
    tab = doc.add_table(rows=0, cols=n)
    tab.alignment = WD_TABLE_ALIGNMENT.CENTER
    tab.autofit = False
    weights = []
    for j in range(n):
        lengths = [sum(2 if ord(x)>127 else 1 for x in row[j].replace('`','')) for row in rows]
        weights.append(max(9, min(42, sum(lengths)/len(lengths) + 6)))
    widths = [17.2 * w / sum(weights) for w in weights]
    for col, width in zip(tab.columns, widths):
        col.width = Cm(width)
    borders = OxmlElement('w:tblBorders')
    for edge in ['top','left','bottom','right','insideH','insideV']:
        el = OxmlElement('w:'+edge)
        for k,v in [('val','single'),('sz','5'),('color','D9D9D9')]:
            el.set(qn('w:'+k), v)
        borders.append(el)
    tab._tbl.tblPr.append(borders)
    for index, row in enumerate(rows):
        rr = tab.add_row()
        pr = rr._tr.get_or_add_trPr()
        pr.append(OxmlElement('w:cantSplit'))
        if index == 0:
            pr.append(OxmlElement('w:tblHeader'))
        for j, (cell, value) in enumerate(zip(rr.cells, row)):
            cell.width = Cm(widths[j])
            cell.vertical_alignment = WD_CELL_VERTICAL_ALIGNMENT.CENTER
            cp = cell._tc.get_or_add_tcPr()
            mar = OxmlElement('w:tcMar')
            for side in ['top','bottom','left','right']:
                e = OxmlElement('w:'+side)
                e.set(qn('w:w'),'85')
                e.set(qn('w:type'),'dxa')
                mar.append(e)
            cp.append(mar)
            shd = OxmlElement('w:shd')
            shd.set(qn('w:fill'), 'DCE6F1' if index == 0 else 'FFFFFF')
            cp.append(shd)
            p = cell.paragraphs[0]
            p.paragraph_format.space_after = Pt(0)
            p.paragraph_format.line_spacing = 1.12
            inline(p,value,True)
            if index == 0:
                for run in p.runs: run.bold = True
            elif re.fullmatch(r'\d+', value):
                p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p = doc.add_paragraph()
    p.paragraph_format.space_after = Pt(2)
    p.paragraph_format.space_before = Pt(0)
    p.paragraph_format.line_spacing = Pt(3)
    p.add_run().font.size = Pt(3)

lines = SOURCE.read_text(encoding='utf-8').splitlines()
i=0
code_lines=[]
while i < len(lines):
    line = lines[i]
    if not line.strip() or line.strip() == '---':
        i+=1; continue
    if line.startswith('```'):
        block=[]; i+=1
        while i < len(lines) and not lines[i].startswith('```'):
            block.append(lines[i]); i+=1
        code_lines.extend(block)
        for j, val in enumerate(block):
            p = doc.add_paragraph(style='Teaching Code')
            p.add_run(val)
            p.paragraph_format.keep_with_next = (len(block) <= 12 and j < len(block)-1)
            if j == 0: p.paragraph_format.space_before = Pt(5)
            if j == len(block)-1: p.paragraph_format.space_after = Pt(7)
        i+=1; continue
    if line.startswith('|'):
        rows=[]
        while i<len(lines) and lines[i].startswith('|'):
            rows.append(table_row(lines[i])); i+=1
        add_table(rows); continue
    match = re.match(r'^(#{1,3})\s+(.*)', line)
    if match:
        depth, title = len(match[1]), match[2]
        p = doc.add_paragraph(heading_text(title), style='Title' if depth==1 else f'Heading {depth-1}')
        if title.startswith('附录 A'):
            p.paragraph_format.page_break_before = True
        i+=1; continue
    p=doc.add_paragraph()
    if line.startswith('- '):
        p.paragraph_format.left_indent = Cm(.4)
        p.paragraph_format.first_line_indent = Cm(-.3)
        inline(p, '• '+line[2:])
    else:
        inline(p,line)
    i+=1

doc.core_properties.title='SHA256 教学代码学习说明'
doc.core_properties.subject='C 语言位运算 字节序 SHA256 分组计算与 OTA 应用'
doc.core_properties.author=''
doc.core_properties.keywords='SHA256, STM32, OTA, C语言, 学习说明'
doc.save(OUTPUT)
reopened=Document(OUTPUT)
actual_code=[p.text for p in reopened.paragraphs if p.style.name=='Teaching Code']
assert actual_code==code_lines
prose=re.sub(r'```[^\n]*\n.*?\n```', '', SOURCE.read_text(encoding='utf-8'), flags=re.S).splitlines()
expected_tables=sum(line.startswith('|') and (idx==0 or not prose[idx-1].startswith('|')) for idx,line in enumerate(prose))
assert len(reopened.tables)==expected_tables, (len(reopened.tables), expected_tables)
print(OUTPUT)
print(f'{len(reopened.paragraphs)} paragraphs, {len(reopened.tables)} tables, {len(code_lines)} code lines preserved')
