"""Generate the editing-state artboards (ordinary / NORMAL / VISUAL / Ex line A / Ex line B)."""
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "design-look"))
import build_boards as look  # noqa: E402  (the adopted look: colors, icons, tab, page)

C = dict(look.DARK)
C["on_accent"] = "#FFFFFF"
C["current_line"] = C["line"]
C["search"] = "rgba(240,164,122,0.35)"
C["ime"] = "#D7C4E5"

TEXT = [
    "// piece table に載る本文。通常モードと Vim モードで同じ本文を編集する",
    "namespace nenenib::core",
    "{",
    "class TextBuffer final",
    "{",
    "  public:",
    "    [[nodiscard]] static std::expected<TextBuffer, TextFailure> open(std::string_view bytes);",
    "    [[nodiscard]] std::size_t line_count() const noexcept;",
    "    [[nodiscard]] std::string_view line(LineNumber number) const noexcept;",
    "  private:",
    "    TextBuffer(std::string original, std::vector<Piece> pieces);",
    "};",
    "} // namespace nenenib::core",
]


def esc(s):
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def span(text, style=""):
    return f'<span style="{style}">{esc(text)}</span>'


def render_line(number, parts, current=False):
    row_bg = f'background: {C["current_line"]};' if current else ''
    num_color = C["text"] if current else C["gutter"]
    return (
        f'<div style="display: flex; align-items: stretch; height: 24px; {row_bg}">'
        f'<div style="width: 56px; flex: 0 0 56px; text-align: right; padding-right: 16px; color: {num_color}; font-size: 12px; line-height: 24px; user-select: none;">{number}</div>'
        f'<div style="flex: 1 1 auto; white-space: pre; font-size: 13.5px; line-height: 24px; color: {C["text"]};">{parts}</div>'
        f'</div>'
    )


BAR = f'<span style="display: inline-block; width: 2px; height: 20px; background: {C["accent"]}; vertical-align: text-bottom;"></span>'


def block(ch):
    return f'<span style="background: {C["accent"]}; color: {C["on_accent"]}; border-radius: 1px;">{esc(ch)}</span>'


def sel(text):
    return f'<span style="background: {C["selection"]};">{esc(text)}</span>'


def hit(text, current=False):
    border = f'outline: 1px solid {C["accent"]};' if current else ''
    return f'<span style="background: {C["search"]}; {border} border-radius: 2px;">{esc(text)}</span>'


def ime(text):
    return f'<span style="border-bottom: 2px solid {C["ime"]}; color: {C["ime"]};">{esc(text)}</span>'


def ime_active(text):
    return f'<span style="border-bottom: 2px solid {C["accent"]}; background: rgba(215,196,229,0.18);">{esc(text)}</span>'


def body(lines_html):
    return f'<div style="position: absolute; top: 40px; bottom: 28px; left: 0; right: 0; padding: 12px 0 0 0; font-family: \'JetBrains Mono\', \'Cascadia Code\', Consolas, monospace;">{"".join(lines_html)}</div>'


def frame(mode_vim, mode_label, inner_body, status_left_extra="", extra="", status_override=None):
    toggle_left = f'background: {"transparent" if mode_vim else C["accent"]}; color: {C["muted"] if mode_vim else C["on_accent"]};'
    toggle_right = f'background: {C["accent"] if mode_vim else "transparent"}; color: {C["on_accent"] if mode_vim else C["muted"]};'
    tabs = look.tab("README.md", False, C) + look.tab("TextBuffer.hpp", True, C) + look.tab("メモ 2026-09-15.txt", False, C)
    status = status_override or f'''
  <div style="position: absolute; left: 0; right: 0; bottom: 0; display: flex; align-items: center; height: 28px; padding: 0 12px; gap: 16px; background: {C["status"]}; font-size: 12px; color: {C["muted"]};">
    <div style="display: inline-flex; align-items: center; border-radius: 6px; background: {C["toggle_bg"]}; padding: 2px; gap: 2px;">
      <span style="padding: 2px 10px; border-radius: 4px; font-weight: 600; {toggle_left}">通常</span>
      <span style="padding: 2px 10px; border-radius: 4px; font-weight: 600; {toggle_right}">Vim</span>
    </div>
    <span style="font-weight: 700; letter-spacing: 0.06em; color: {C["text"]};">{mode_label}</span>
    {status_left_extra}
    <span style="flex: 1 1 auto;"></span>
    <span>行 7, 桁 44</span>
    <span>UTF-8</span>
    <span>CRLF</span>
    <span>C++</span>
    <span>空白 4</span>
  </div>'''
    return f'''
<div style="position: relative; width: 1280px; height: 800px; overflow: hidden; border-radius: 8px; background: {C["bg"]}; color: {C["text"]}; font-family: 'Segoe UI Variable Text', 'Segoe UI', 'Noto Sans JP', system-ui, sans-serif; box-shadow: 0 24px 64px rgba(0,0,0,0.35);">
  <div style="display: flex; align-items: flex-end; height: 40px; padding: 0 0 0 8px; background: {C["titlebar"]}; backdrop-filter: blur(24px);">
    <div style="display: flex; align-items: flex-end; gap: 2px; flex: 1 1 auto; height: 40px; padding-top: 8px;">
      {tabs}
      <div style="display: inline-flex; width: 32px; height: 32px; align-items: center; justify-content: center; color: {C["muted"]}; border-radius: 6px;">{look.ICON_PLUS}</div>
    </div>
    <div style="display: flex; align-self: stretch; color: {C["muted"]};">
      <div style="display: inline-flex; width: 46px; align-items: center; justify-content: center;">{look.ICON_MIN}</div>
      <div style="display: inline-flex; width: 46px; align-items: center; justify-content: center;">{look.ICON_MAX}</div>
      <div style="display: inline-flex; width: 46px; align-items: center; justify-content: center;">{look.ICON_CLOSE}</div>
    </div>
  </div>
  {inner_body}
  {status}
  {extra}
</div>'''


def plain(lines, caret_line=None, caret_col=None, caret="bar"):
    out = []
    for i, text in enumerate(lines, 1):
        if i == caret_line:
            before, at, after = text[:caret_col], text[caret_col:caret_col + 1] or " ", text[caret_col + 1:]
            if caret == "bar":
                parts = span(before) + BAR + span(at + after)
            else:
                parts = span(before) + block(at) + span(after)
            out.append(render_line(i, parts, current=True))
        else:
            out.append(render_line(i, span(text)))
    return out


# 1. 通常モード: Shift+矢印の選択（3 行にまたがる）とバーのキャレット、IME の変換中（別の行）
ordinary = []
for i, text in enumerate(TEXT, 1):
    if i == 7:
        ordinary.append(render_line(i, span(text[:41]) + sel(text[41:])))
    elif i == 8:
        ordinary.append(render_line(i, sel(text)))
    elif i == 9:
        ordinary.append(render_line(i, sel(text[:36]) + BAR + span(text[36:]), current=True))
    elif i == 1:
        ordinary.append(render_line(i, span("// ") + ime("piece table に載る本文。") + ime_active("つうじょう") + ime("モードと Vim モードで同じ本文を編集する")))
    else:
        ordinary.append(render_line(i, span(text)))

# 2. Vim NORMAL: ブロックのキャレット、/line の検索の当たり（現在の当たりは枠）
normal = []
for i, text in enumerate(TEXT, 1):
    if i == 8:
        j = text.index("line_count")
        normal.append(render_line(i, span(text[:j]) + hit("line", current=True) + span(text[j + 4:j + 21]) + block(text[j + 21]) + span(text[j + 22:]), current=True))
    elif i == 9:
        j = text.index("line(")
        normal.append(render_line(i, span(text[:j]) + hit("line") + span(text[j + 4:])))
    elif i == 7:
        normal.append(render_line(i, span(text)))
    else:
        normal.append(render_line(i, span(text)))
normal_status_extra = f'<span style="color: {C["muted"]};">/line</span><span style="color: {C["muted"]};">2 / 2</span>'

# 3. Vim VISUAL: 行選択（V）で 3 行。モードは VISUAL LINE
visual = []
for i, text in enumerate(TEXT, 1):
    if i in (7, 8, 9):
        visual.append(render_line(i, sel(text) if i != 9 else sel(text[:36]) + block(text[36]) + sel(text[37:]), current=(i == 9)))
    else:
        visual.append(render_line(i, span(text)))
visual_status_extra = f'<span style="color: {C["muted"]};">3 行</span>'

# 4. Ex 行 A: ステータスバーの左側が : の入力に置き換わる
insert_lines = plain(TEXT, caret_line=7, caret_col=44, caret="bar")
ex_a_status = f'''
  <div style="position: absolute; left: 0; right: 0; bottom: 0; display: flex; align-items: center; height: 28px; padding: 0 12px; gap: 12px; background: {C["status"]}; font-size: 13px; color: {C["text"]}; font-family: 'JetBrains Mono', 'Cascadia Code', Consolas, monospace;">
    <span style="color: {C["accent"]}; font-weight: 700;">:</span>
    <span>colorscheme dra</span>{BAR}
    <span style="flex: 1 1 auto;"></span>
    <span style="font-family: 'Segoe UI Variable Text', 'Segoe UI', 'Noto Sans JP', system-ui, sans-serif; font-size: 11.5px; color: {C["muted"]};">Tab 補完　Esc 戻る</span>
  </div>'''
ex_a_extra = f'''
  <div style="position: absolute; left: 12px; bottom: 34px; width: 360px; border-radius: 8px; background: {C["panel"]}; border: 1px solid {C["panel_border"]}; box-shadow: 0 12px 32px rgba(0,0,0,0.4); padding: 6px; font-size: 12.5px;">
    <div style="display: flex; align-items: center; height: 30px; padding: 0 10px; border-radius: 5px; background: {C["selection"]}; color: {C["text"]};">dracula<span style="flex: 1 1 auto;"></span><span style="color: {C["muted"]}; font-size: 11.5px;">dark</span></div>
  </div>'''

# 5. Ex 行 B: ステータスバーの上に 1 行が現れる（ステータスバーは残る）
ex_b_extra = f'''
  <div style="position: absolute; left: 0; right: 0; bottom: 28px; display: flex; align-items: center; height: 32px; padding: 0 12px; gap: 12px; background: {C["panel"]}; border-top: 1px solid {C["panel_border"]}; font-size: 13.5px; color: {C["text"]}; font-family: 'JetBrains Mono', 'Cascadia Code', Consolas, monospace;">
    <span style="color: {C["accent"]}; font-weight: 700;">:</span>
    <span>colorscheme dra</span>{BAR}
    <span style="flex: 1 1 auto;"></span>
    <span style="font-family: 'Segoe UI Variable Text', 'Segoe UI', 'Noto Sans JP', system-ui, sans-serif; font-size: 11.5px; color: {C["muted"]};">Tab 補完　Esc 戻る</span>
  </div>
  <div style="position: absolute; left: 12px; bottom: 66px; width: 360px; border-radius: 8px; background: {C["panel"]}; border: 1px solid {C["panel_border"]}; box-shadow: 0 12px 32px rgba(0,0,0,0.4); padding: 6px; font-size: 12.5px;">
    <div style="display: flex; align-items: center; height: 30px; padding: 0 10px; border-radius: 5px; background: {C["selection"]}; color: {C["text"]};">dracula<span style="flex: 1 1 auto;"></span><span style="color: {C["muted"]}; font-size: 11.5px;">dark</span></div>
  </div>'''

boards = {
    "Main.dc.html": frame(False, "通常", body(ordinary)),
    "Normal.dc.html": frame(True, "NORMAL", body(normal), status_left_extra=normal_status_extra),
    "Visual.dc.html": frame(True, "VISUAL LINE", body(visual), status_left_extra=visual_status_extra),
    "ExLineA.dc.html": frame(True, "NORMAL", body(insert_lines), status_override=ex_a_status, extra=ex_a_extra),
    "ExLineB.dc.html": frame(True, "NORMAL", body(insert_lines), extra=ex_b_extra),
}
for name, inner in boards.items():
    (HERE / name).write_text(look.page(name, inner), encoding="utf-8", newline="\n")

(HERE / "canvas.json").write_text('''{
  "artboards": [
    { "file": "Main.dc.html", "x": 0, "y": 0, "w": 1280, "h": 800, "title": "通常モード: 選択（Shift+矢印）・バーのキャレット・IME の変換中" },
    { "file": "Normal.dc.html", "x": 1400, "y": 0, "w": 1280, "h": 800, "title": "Vim NORMAL: ブロックのキャレット・/ の検索の当たり" },
    { "file": "Visual.dc.html", "x": 0, "y": 960, "w": 1280, "h": 800, "title": "Vim VISUAL LINE: 行選択" },
    { "file": "ExLineA.dc.html", "x": 0, "y": 1920, "w": 1280, "h": 800, "title": "案 A: : の行はステータスバーの中（左側が置き換わる）" },
    { "file": "ExLineB.dc.html", "x": 1400, "y": 1920, "w": 1280, "h": 800, "title": "案 B: : の行はステータスバーの上に 1 行現れる" }
  ],
  "annotations": [
    { "id": "brief", "x": 1400, "y": 960, "w": 560, "text": "編集中の状態の見た目（2026-09-15・編集の縦切りの前に決める）\\n\\n・通常モード: キャレットは 2 DIP のバー（橙）。選択は橙 28% の面で行をまたぐ。IME の変換中は注目文節を橙の下線と淡い面、他の文節は淡い紫の下線\\n・Vim NORMAL: キャレットは 1 文字のブロック（橙の面に白い字）。/ の検索の当たりは淡い橙の面、現在の当たりは橙の枠。ステータスに /line と 2 / 2\\n・Vim VISUAL LINE: 行全体を選択の面。ステータスに行数\\n・INSERT はバーのキャレットに戻る（通常モードと同じ）\\n\\n決めてほしいこと: ① : のコマンドラインの置き場。案 A（ステータスバーの中・Vim の流儀に近い）か 案 B（上に 1 行・ステータスは残る） ② ブロックのキャレットの色（橙の面か、文字色の反転か） ③ 検索の当たりの色（淡い橙か、黄色系か）" }
  ],
  "launch": { "view": "canvas" }
}
''', encoding="utf-8", newline="\n")
print("edit boards written")
