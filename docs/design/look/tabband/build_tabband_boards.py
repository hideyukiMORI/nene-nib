"""The four tab band colour options of D16 (2026-09-17; hide chose C).

It reuses the adopted look generator next door (build_boards.py) and varies only the band and the
active tab, so the rest of the artboard stays the adopted look: the palettes and the builders come
from that file by exec, with its write block cut off. Every variant passes its own band and tab
colour, so it keeps drawing the four options after build_boards.py took the chosen values (D16).
The labels name the band token as it was called on the day the options were shown
(`titlebar_tint`, an RGBA tint over Mica); D16 renamed it to `title_bar` and made it opaque.
"""
from pathlib import Path

HERE = Path(__file__).resolve().parent
SOURCE_PATH = HERE.parent / "build_boards.py"
SOURCE = SOURCE_PATH.read_text(encoding="utf-8")
SOURCE = SOURCE[: SOURCE.index('(HERE / "Main.dc.html")')]  # drop the write block; keep palettes and builders
namespace = {"__file__": str(SOURCE_PATH)}
exec(compile(SOURCE, "build_boards.py", "exec"), namespace)
DARK, editor, page = namespace["DARK"], namespace["editor"], namespace["page"]

# 背景に壁紙めいた面を置き、タイトルバーの帯を半透明にして Mica の透けを見せる。
WALLPAPER = ("linear-gradient(135deg, #1b2a4a 0%, #3a2a5a 35%, #5a2a48 60%, #1e1630 100%)")

def variant(name, titlebar, tab_active, note):
    c = dict(DARK, titlebar=titlebar, tab_active=tab_active)
    html = editor(c, mode_vim=True)
    # 窓の地を壁紙にし、本文とステータスの面は元のトークンのまま（帯だけが透ける）。
    html = html.replace(f'background: {DARK["bg"]}; color: {DARK["text"]}; font-family', f'background: {WALLPAPER}; color: {DARK["text"]}; font-family', 1)
    html = html.replace("padding: 12px 0 0 0; font-family: 'JetBrains Mono'", f"padding: 12px 0 0 0; background: {DARK['bg']}; font-family: 'JetBrains Mono'", 1)
    label = (f'<div style="position: absolute; left: 0; right: 0; bottom: -44px; font-family: \'Segoe UI Variable Text\', \'Segoe UI\', \'Noto Sans JP\', system-ui, sans-serif; font-size: 13px; color: #B8A9B3;">{note}</div>')
    inner = html.rstrip()[:-6] + label + "\n</div>"
    (HERE / f"{name}.dc.html").write_text(page(name, inner), encoding="utf-8", newline="\n")

variant("Main", "rgba(255,255,255,0.045)", "#3B1430",
        "A 現状: Mica ＋ 白 4.5%（titlebar_tint）。アクティブなタブは本文より少し明るい #3B1430（tab_active）")
variant("TintedMica", "rgba(30,5,22,0.80)", "#300A24",
        "B Mica ＋ 深い茄子色 80%（titlebar_tint = #1E0516 α0.80）。壁紙が薄く透ける。アクティブなタブは本文色 #300A24 で本文に繋がる")
variant("DeepAubergine", "#1E0516", "#300A24",
        "C 不透明の深い茄子色 #1E0516。Mica は隠れる。色がテーマで決まり、壁紙に左右されない。アクティブなタブは本文色で本文に繋がる")
variant("Black", "#000000", "#300A24",
        "D 不透明の黒 #000000。最も強い対比。ライト系テーマでは別の値にする（トークンなのでテーマごと）")

(HERE / "canvas.json").write_text('''{
  "artboards": [
    { "file": "Main.dc.html", "x": 0, "y": 0, "w": 1280, "h": 860, "title": "A 現状（Mica ＋ 白 4.5%）" },
    { "file": "TintedMica.dc.html", "x": 1400, "y": 0, "w": 1280, "h": 860, "title": "B Mica ＋ 深い茄子色 80%" },
    { "file": "DeepAubergine.dc.html", "x": 0, "y": 1020, "w": 1280, "h": 860, "title": "C 不透明の深い茄子色" },
    { "file": "Black.dc.html", "x": 1400, "y": 1020, "w": 1280, "h": 860, "title": "D 不透明の黒" }
  ],
  "annotations": [
    { "id": "brief", "x": 2800, "y": 0, "w": 520, "text": "タブの帯（タイトルバー）の背景色・案 4 つ（2026-09-17）\\n\\n触るのは Palette の 2 トークンだけ: titlebar_tint（帯に載せる面・RGBA）と tab_active（アクティブなタブの面）。どちらもテーマごとの値なので、:colorscheme で変わる（D13）。\\n\\nA 現状。Mica が最も透ける（ADR 0008 決定 9 の動機）。帯と本文の差が小さい\\nB 深い茄子色 80%。帯は暗いが壁紙が薄く透け、色が壁紙で少し変わる\\nC 不透明の深い茄子色。色がテーマで決まり予測できる。Mica の透けは無くなる（DWM は描いているが隠れる）\\nD 黒。対比が最も強い。ライト系テーマでは黒にしない（テーマごとの値）\\n\\nB〜D はアクティブなタブを本文色にして「タブ＝いま見ている紙」が本文に繋がる形（Windows Terminal / VS Code の作法）。\\n\\n設計リナの推し: C。理由は、施主の要望（本文より深い茄子色）を壁紙に左右されず出せて、テーマの値だけで済むこと。Mica の透けを残したければ B。" }
  ],
  "launch": { "view": "canvas" }
}
''', encoding="utf-8", newline="\n")
print("written", sorted(p.name for p in HERE.glob("*.dc.html")))
