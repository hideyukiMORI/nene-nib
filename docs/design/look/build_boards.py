"""Generate the three NeNe Nib look artboards (dark / light / Ctrl+P) as .dc.html files."""
from pathlib import Path

HERE = Path(__file__).resolve().parent

DARK = dict(
    bg="#300A24", text="#EEEEEC", muted="#B8A9B3", gutter="#7A6675", line="#3E1A32",
    titlebar="rgba(255,255,255,0.045)", tab_active="#3B1430", tab_hover="rgba(255,255,255,0.06)",
    status="#26071D", accent="#E95420", caret="#E95420", selection="rgba(233,84,32,0.28)",
    keyword="#F0A47A", string="#D7C4E5", comment="#8E7A88", panel="#3B1430", panel_border="#5A2A4C",
    toggle_bg="#4A1E3D", toggle_fg="#EEEEEC", scrim="rgba(20,4,15,0.55)",
)
LIGHT = dict(
    bg="#F4F5F7", text="#1B1F24", muted="#5C6570", gutter="#9AA3AD", line="#E6E8EC",
    titlebar="rgba(0,0,0,0.035)", tab_active="#FFFFFF", tab_hover="rgba(0,0,0,0.05)",
    status="#E9EBEF", accent="#E95420", caret="#E95420", selection="rgba(233,84,32,0.22)",
    keyword="#B23A0F", string="#5E2750", comment="#7A828C", panel="#FFFFFF", panel_border="#D5D9E0",
    toggle_bg="#DFE3E8", toggle_fg="#1B1F24", scrim="rgba(40,44,52,0.35)",
)

ICON_MIN = '<svg width="10" height="10" viewBox="0 0 10 10" fill="none" stroke="currentColor" stroke-width="1"><path d="M0.5 5h9"></path></svg>'
ICON_MAX = '<svg width="10" height="10" viewBox="0 0 10 10" fill="none" stroke="currentColor" stroke-width="1"><rect x="0.5" y="0.5" width="9" height="9"></rect></svg>'
ICON_CLOSE = '<svg width="10" height="10" viewBox="0 0 10 10" fill="none" stroke="currentColor" stroke-width="1.1"><path d="M0.5 0.5l9 9M9.5 0.5l-9 9"></path></svg>'
ICON_X = '<svg width="12" height="12" viewBox="0 0 12 12" fill="none" stroke="currentColor" stroke-width="1.3" stroke-linecap="round"><path d="M2.5 2.5l7 7M9.5 2.5l-7 7"></path></svg>'
ICON_PLUS = '<svg width="14" height="14" viewBox="0 0 14 14" fill="none" stroke="currentColor" stroke-width="1.4" stroke-linecap="round"><path d="M7 2v10M2 7h10"></path></svg>'
ICON_TAB = '<svg width="16" height="16" viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.3" stroke-linejoin="round"><path d="M3 2.5h6l4 4v7H3z"></path><path d="M9 2.5v4h4"></path></svg>'
ICON_STAR = '<svg width="16" height="16" viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.3" stroke-linejoin="round"><path d="M8 1.8l1.9 3.9 4.3.6-3.1 3 .7 4.3L8 11.6l-3.8 2 .7-4.3-3.1-3 4.3-.6z"></path></svg>'
ICON_CLOCK = '<svg width="16" height="16" viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.3" stroke-linecap="round"><circle cx="8" cy="8" r="6.2"></circle><path d="M8 4.5V8l2.5 1.6"></path></svg>'
ICON_FOLDER = '<svg width="16" height="16" viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.3" stroke-linejoin="round"><path d="M1.8 4.2h4.4l1.5 1.6h6.5v7.4H1.8z"></path><path d="M1.8 4.2V2.8h4l1.1 1.4"></path></svg>'
ICON_SEARCH = '<svg width="18" height="18" viewBox="0 0 18 18" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round"><circle cx="7.5" cy="7.5" r="5.2"></circle><path d="M11.5 11.5l4 4"></path></svg>'

CODE_LINES = [
    ('<span style="color:{comment}">// piece table に載る 1 行目。通常モードと Vim モードで同じ本文を編集する</span>', ''),
    ('<span style="color:{keyword}">namespace</span> nenenib::core', ''),
    ('{{', ''),
    ('<span style="color:{keyword}">class</span> TextBuffer <span style="color:{keyword}">final</span>', ''),
    ('{{', ''),
    ('  <span style="color:{keyword}">public</span>:', ''),
    ('    [[nodiscard]] <span style="color:{keyword}">static</span> std::expected&lt;TextBuffer, TextFailure&gt; open(std::string_view <span style="color:{string}">bytes</span>);', ''),
    ('    [[nodiscard]] std::size_t line_count() <span style="color:{keyword}">const</span> <span style="color:{keyword}">noexcept</span>;', ''),
    ('', 'caret'),
    ('  <span style="color:{keyword}">private</span>:', ''),
    ('    TextBuffer(std::string original, std::vector&lt;Piece&gt; pieces);', ''),
    ('}};', ''),
    ('}} <span style="color:{comment}">// namespace nenenib::core</span>', ''),
]


def tab(label, active, c):
    bg = c["tab_active"] if active else "transparent"
    color = c["text"] if active else c["muted"]
    underline = f'border-bottom: 2px solid {c["accent"]};' if active else 'border-bottom: 2px solid transparent;'
    return (
        f'<div style="display: flex; align-items: center; gap: 8px; height: 32px; padding: 0 10px 0 14px; '
        f'border-radius: 6px 6px 0 0; background: {bg}; color: {color}; font-size: 12.5px; {underline} min-width: 120px; max-width: 200px;">'
        f'<span style="white-space: nowrap; overflow: hidden; text-overflow: ellipsis; flex: 1 1 auto;">{label}</span>'
        f'<span style="display: inline-flex; width: 18px; height: 18px; align-items: center; justify-content: center; border-radius: 4px; color: {color}; opacity: 0.8;">{ICON_X}</span>'
        f'</div>'
    )


def editor(c, mode_vim=True, dim=False):
    lines = []
    for number, (html, kind) in enumerate(CODE_LINES, 1):
        html = html.format(**c)
        current = kind == 'caret'
        row_bg = f'background: {c["line"]};' if current else ''
        caret = f'<span style="display: inline-block; width: 2px; height: 20px; background: {c["caret"]}; vertical-align: text-bottom; margin-left: 4px;"></span>' if current else ''
        num_color = c["text"] if current else c["gutter"]
        lines.append(
            f'<div style="display: flex; align-items: stretch; height: 24px; {row_bg}">'
            f'<div style="width: 56px; flex: 0 0 56px; text-align: right; padding-right: 16px; color: {num_color}; font-size: 12px; line-height: 24px; user-select: none;">{number}</div>'
            f'<div style="flex: 1 1 auto; white-space: pre; font-size: 13.5px; line-height: 24px; color: {c["text"]};">{"    " + html if html else "    "}{caret}</div>'
            f'</div>'
        )
    body = "".join(lines)
    tabs = tab("README.md", False, c) + tab("TextBuffer.hpp", True, c) + tab("メモ 2026-09-15.txt", False, c)
    mode_label = "NORMAL" if mode_vim else "通常"
    toggle_left = f'background: {"transparent" if mode_vim else c["accent"]}; color: {c["muted"] if mode_vim else "#FFFFFF"};'
    toggle_right = f'background: {c["accent"] if mode_vim else "transparent"}; color: {"#FFFFFF" if mode_vim else c["muted"]};'
    scrim = f'<div style="position: absolute; inset: 0; background: {c["scrim"]};"></div>' if dim else ''
    return f'''
<div style="position: relative; width: 1280px; height: 800px; overflow: hidden; border-radius: 8px; background: {c["bg"]}; color: {c["text"]}; font-family: 'Segoe UI Variable Text', 'Segoe UI', 'Noto Sans JP', system-ui, sans-serif; box-shadow: 0 24px 64px rgba(0,0,0,0.35);">
  <div style="display: flex; align-items: flex-end; height: 40px; padding: 0 0 0 8px; background: {c["titlebar"]}; backdrop-filter: blur(24px);">
    <div style="display: flex; align-items: flex-end; gap: 2px; flex: 1 1 auto; height: 40px; padding-top: 6px;">
      {tabs}
      <div style="display: inline-flex; width: 32px; height: 32px; align-items: center; justify-content: center; color: {c["muted"]}; border-radius: 6px;">{ICON_PLUS}</div>
    </div>
    <div style="display: flex; align-self: stretch; color: {c["muted"]};">
      <div style="display: inline-flex; width: 46px; align-items: center; justify-content: center;">{ICON_MIN}</div>
      <div style="display: inline-flex; width: 46px; align-items: center; justify-content: center;">{ICON_MAX}</div>
      <div style="display: inline-flex; width: 46px; align-items: center; justify-content: center;">{ICON_CLOSE}</div>
    </div>
  </div>
  <div style="position: absolute; top: 40px; bottom: 28px; left: 0; right: 0; padding: 12px 0 0 0; font-family: 'JetBrains Mono', 'Cascadia Code', Consolas, monospace;">
    {body}
  </div>
  <div style="position: absolute; left: 0; right: 0; bottom: 0; display: flex; align-items: center; height: 28px; padding: 0 12px; gap: 16px; background: {c["status"]}; font-size: 12px; color: {c["muted"]};">
    <div style="display: inline-flex; align-items: center; border-radius: 6px; background: {c["toggle_bg"]}; padding: 2px; gap: 2px;">
      <span style="padding: 2px 10px; border-radius: 4px; font-weight: 600; {toggle_left}">通常</span>
      <span style="padding: 2px 10px; border-radius: 4px; font-weight: 600; {toggle_right}">Vim</span>
    </div>
    <span style="font-weight: 700; letter-spacing: 0.06em; color: {c["text"]};">{mode_label}</span>
    <span style="flex: 1 1 auto;"></span>
    <span>行 9, 桁 1</span>
    <span>UTF-8</span>
    <span>CRLF</span>
    <span>C++</span>
    <span>空白 4</span>
  </div>
  {scrim}
</div>'''


def palette_row(icon, name, path, hint, c, active=False):
    bg = f'background: {c["selection"]};' if active else ''
    return (
        f'<div style="display: flex; align-items: center; gap: 12px; height: 40px; padding: 0 16px; border-radius: 6px; {bg}">'
        f'<span style="display: inline-flex; width: 20px; color: {c["accent"] if active else c["muted"]};">{icon}</span>'
        f'<span style="font-size: 13.5px; color: {c["text"]}; white-space: nowrap;">{name}</span>'
        f'<span style="font-size: 12px; color: {c["muted"]}; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; flex: 1 1 auto;">{path}</span>'
        f'<span style="font-size: 11.5px; color: {c["muted"]};">{hint}</span>'
        f'</div>'
    )


def ctrlp(c):
    rows = "".join([
        palette_row(ICON_TAB, "TextBuffer.hpp", "src/core", "開いているタブ", c, active=True),
        palette_row(ICON_STAR, "SPECIFICATION.md", "NeNeNib", "ブックマーク", c),
        palette_row(ICON_CLOCK, "メモ 2026-09-15.txt", "Documents/nib", "3 分前", c),
        palette_row(ICON_CLOCK, "0007-first-slice-frameless-window-direct2d-line.md", "docs/adr", "昨日", c),
        palette_row(ICON_FOLDER, "DisplayText.cpp", "src/core", "同じフォルダ", c),
        palette_row(ICON_FOLDER, "Palette.cpp", "src/core", "同じフォルダ", c),
    ])
    return f'''
  <div style="position: absolute; left: 320px; top: 96px; width: 640px; border-radius: 10px; background: {c["panel"]}; border: 1px solid {c["panel_border"]}; box-shadow: 0 20px 60px rgba(0,0,0,0.45); overflow: hidden; font-family: 'Segoe UI Variable Text', 'Segoe UI', 'Noto Sans JP', system-ui, sans-serif;">
    <div style="display: flex; align-items: center; gap: 12px; height: 52px; padding: 0 16px; border-bottom: 1px solid {c["panel_border"]}; color: {c["muted"]};">
      <span style="display: inline-flex;">{ICON_SEARCH}</span>
      <span style="font-size: 15px; color: {c["text"]};">text<span style="display: inline-block; width: 2px; height: 18px; background: {c["caret"]}; vertical-align: text-bottom; margin-left: 1px;"></span></span>
      <span style="flex: 1 1 auto;"></span>
      <span style="font-size: 11.5px;">★ ブックマーク　◷ 履歴　/ フォルダ</span>
    </div>
    <div style="display: flex; flex-direction: column; gap: 2px; padding: 8px;">
      {rows}
    </div>
    <div style="display: flex; align-items: center; gap: 16px; height: 34px; padding: 0 16px; border-top: 1px solid {c["panel_border"]}; font-size: 11.5px; color: {c["muted"]};">
      <span>↑↓ 移動</span><span>Enter 開く</span><span>Ctrl+Enter 新しいタブ</span><span>Esc 閉じる</span>
    </div>
  </div>'''


def page(title, inner):
    return f'''<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <script src="./support.js"></script>
</head>
<body>
<x-dc>
<helmet>
  <link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=JetBrains+Mono:wght@400;600&family=Noto+Sans+JP:wght@400;600;700&display=swap">
  <style>
    body {{ margin: 0; background: transparent; }}
    a {{ color: #E95420; }} a:hover {{ color: #C7431A; }}
  </style>
</helmet>
<div style="width: 1280px; height: 800px;">
{inner}
</div>
</x-dc>
</body>
</html>
'''


(HERE / "Main.dc.html").write_text(page("Dark", editor(DARK, mode_vim=True)), encoding="utf-8", newline="\n")
(HERE / "Light.dc.html").write_text(page("Light", editor(LIGHT, mode_vim=False)), encoding="utf-8", newline="\n")
ctrl = editor(DARK, mode_vim=True, dim=True)
ctrl = ctrl.rstrip()[:-6] + ctrlp(DARK) + "\n</div>"
(HERE / "CtrlP.dc.html").write_text(page("CtrlP", ctrl), encoding="utf-8", newline="\n")
(HERE / "canvas.json").write_text('''{
  "artboards": [
    { "file": "Main.dc.html", "x": 0, "y": 0, "w": 1280, "h": 800, "title": "ダーク（茄子色・Vim モード）" },
    { "file": "Light.dc.html", "x": 1400, "y": 0, "w": 1280, "h": 800, "title": "ライト（通常モード）" },
    { "file": "CtrlP.dc.html", "x": 0, "y": 960, "w": 1280, "h": 800, "title": "Ctrl+P 統合検索（ダーク）" }
  ],
  "annotations": [
    { "id": "brief", "x": 1400, "y": 960, "w": 520, "text": "NeNe Nib の見た目・初案（2026-09-15）\\n\\n・枠なし窓。タイトルバーにタブを横並び（Windows Terminal 型）、右端に最小化・最大化・閉じる\\n・タイトルバーは Mica（半透明の淡い面で表現）。アクティブなタブは下線が Ubuntu 橙\\n・ダークは施主決定 D11 の茄子色 #300A24 / 文字 #EEEEEC。ライトは #F4F5F7 / #1B1F24\\n・ステータスバー左: 通常 | Vim のトグル（選択側が橙）と現在モード（NORMAL）。右: 行と桁・文字コード・改行・言語・空白\\n・本文は等幅（JetBrains Mono の代わりに実装では Cascadia Code / Consolas）。行番号は右寄せ 56px、現在行は薄い面\\n・Ctrl+P: 上に検索欄と絞り込みの記号（★ ◷ /）、行は種別のアイコン・名前・場所・補足。選択行は橙の薄い面\\n\\n決めてほしいこと: ①橙のアクセントで良いか（別候補: 茄子色を明るくした #77216F） ②タブの角丸と下線の形 ③ステータスバーのトグルの見せ方" }
  ],
  "launch": { "view": "canvas" }
}
''', encoding="utf-8", newline="\n")
print("boards written")
