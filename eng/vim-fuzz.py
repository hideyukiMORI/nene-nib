"""Differential fuzzing of Nib's Vim against the real Vim 9.1 (Issue #954).

A design-seat tool, like eng/verify-window.py: CI and eng/check.ps1 never call it, because it needs
Vim on the machine. From a fixed seed it writes `--count N` random cases (a buffer and a key
sequence), measures each one with the oracle's `measure` (eng/vim-oracle.py: `probe_script` and
`run_vim` under `-u NONE -es`, imported, never changed), replays the measured cases through the
replayer `nib_vim_replay` (tests/unit/VimReplayMain.cpp, the same path as `verify_vim_fixture`), and
compares the five values the fixtures compare: the buffer, `line`, `column`, and the unnamed
register's text and kind.

The key grammar is `WORDS`, the one table of generator units at the top of this file. Every word
leaves Vim in NORMAL mode (an insert ends with `<Esc>`, a VISUAL selection with an operator or
`<Esc>`), because the oracle refuses keys that do not. The table holds only keys Nib handles and the
oracle can observe: no `"+ "*` (the real clipboard), no `q` / `@` (recording does not work inside
`:normal`), no `u` / `<C-r>` (the oracle's `:normal!` is one undo unit), no viewport keys
(`H M L <C-d> <C-u> <C-f> <C-b> <C-e> <C-y> z… gj gk g0 g$ gm gM <PageUp> <PageDown>`), no tabs,
windows, Ex or Ctrl+P, no keys Nib does not take yet (`R s S gv`, block `I A c C > <`)
and no INSERT `<BS>`. The generator does not make VISUAL `<C-a> <C-x> g<C-a> g<C-x>` yet, only
the NORMAL `{n}<C-a>` `{n}<C-x>`.

The buffer has 1 to 12 lines mixing ASCII words, spaces, tabs, bracket and quote pairs, punctuation,
Japanese (kana, kanji, full-width letters), empty lines and trailing spaces. It never ends with a
newline and never holds a CR or a combining character (v1).

Some words depend on what came before them (Issue #963, generator 2): `''` and `` ` ` `` only after
a jump (the oracle's `-es` Vim starts on the last line, the user's Vim on line 1, #962), `n` `N` only
after `/ ? * #`, a read of `"1`..`"9` (`p` `P`) only after an unnamed `dd` or `cc`, and `'a` `` `a ``
only after `ma`. The generator picks each word from the words the state so far allows, and the
shrinker keeps only candidates whose words still hold. `--exclude WORD...` drops words from the
table by name; `--known ISSUE:WORD,WORD` (repeatable) counts a difference whose words (the minimal
ones with `--shrink`) include one of those words as known under that Issue instead of new. A known
name may also be a text object (`aB` `i(`), which matches a `dobj` `cobj` `yobj` `v` word holding it.

A case the oracle refuses (`ValueError`: not NORMAL at the end, or `RuntimeError`: Vim failed or
timed out) is counted as rejected and dropped. `rejected.json` keeps its keys, the word whose prefix
first makes the oracle refuse, and Vim's error lines (a second run of the same probe with `-V1`,
because the `-es` run writes nothing to stderr); `summary.md` counts the refusals by that word. Every difference is written to `<out>/mismatches.json`
with `name` / `text` / `keys` first, so those three keys copy into tests/vim/fixtures.json as they
are, followed by the oracle's and Nib's values. `--shrink` deletes words from the keys and lines from
the buffer while both Vim and the replayer still disagree on one of the same fields, and records the
smallest case under `minimal`. `<out>/summary.md` holds the counts (generated, rejected, matched,
mismatched) and the differences by word kind.

`--seeds A-B` runs the seeds A..B one after another, each into `<out>/seed-N/` as `--seed` would,
and writes the totals and every new difference to `<out>/summary.md`.

`--from-fixtures N` is the replayer's self-check: it measures the first N tests/vim/fixtures.json
cases without settings, viewport or register through the same oracle and expects every one `ok`.

Usage:
    python eng/vim-fuzz.py --count 300 --seed 1 --out D:/NeNeNib/outputs/<date>/vim-fuzz-seed1 --shrink
    python eng/vim-fuzz.py --seeds 1-5 --count 300 --shrink --known 781:~,g~ --out D:/NeNeNib/outputs/<date>/vim-fuzz-seeds
    python eng/vim-fuzz.py --from-fixtures 50 --out D:/NeNeNib/outputs/<date>/vim-fuzz-fixtures

Python standard library only. Nothing here reads the clock or the environment; the order of the
cases is the seed's order (no parallelism).
"""

from __future__ import annotations

import argparse
from collections import Counter
import importlib.util
import json
from pathlib import Path
import random
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_REPLAY = ROOT / "build" / "nib_vim_replay.exe"
FIELDS = ("text", "line", "column", "register", "register_kind")

# 語の表（生成器の単位・Issue #954 の 2）。1 行が 語の種類・重み・鍵の型。型の `{…}` は
# expand_word が埋める: {n} 省略できる回数 / {N} 行の番号 / {P} 割合 / {B} 桁 / {c} 1 文字 /
# {r} レジスタの接頭辞 / {m} オペレータの移動 / {o} テキストオブジェクト / {i} 挿入する本文 /
# {p} 検索の pattern / {k} マークの名前 / {v} VISUAL の中の移動 / {w} 文字単位 VISUAL の中の移動か
# テキストオブジェクト。どの語も NORMAL で終わる。
WORDS = (
    ("h", 3, "{n}h"), ("j", 3, "{n}j"), ("k", 3, "{n}k"), ("l", 3, "{n}l"),
    ("0", 2, "0"), ("^", 2, "^"), ("$", 2, "{n}$"), ("g_", 1, "{n}g_"), ("g^", 1, "g^"),
    ("w", 3, "{n}w"), ("b", 3, "{n}b"), ("e", 3, "{n}e"), ("W", 2, "{n}W"), ("B", 2, "{n}B"),
    ("E", 2, "{n}E"), ("ge", 2, "{n}ge"), ("gE", 1, "{n}gE"),
    ("gg", 1, "gg"), ("G", 1, "G"), ("NG", 1, "{N}G"), ("Ngg", 1, "{N}gg"), ("N%", 1, "{P}%"),
    ("N|", 1, "{B}|"), ("f", 3, "{n}f{c}"), ("F", 2, "{n}F{c}"), ("t", 2, "{n}t{c}"),
    ("T", 2, "{n}T{c}"), (";", 2, "{n};"), (",", 1, "{n},"), ("%", 2, "%"), ("-", 1, "{n}-"),
    ("+", 1, "{n}+"), ("<CR>", 1, "{n}<CR>"), ("<NL>", 1, "{n}<NL>"), ("<Space>", 1, "{n}<Space>"),
    ("<BS>", 1, "{n}<BS>"), ("<Home>", 1, "<Home>"), ("<End>", 1, "<End>"), ("_", 1, "{n}_"),
    ("d", 4, "{r}{n}d{m}"), ("c", 3, "{r}{n}c{m}{i}<Esc>"), ("y", 3, "{r}{n}y{m}"),
    ("dobj", 3, "{r}d{o}"), ("cobj", 2, "{r}c{o}{i}<Esc>"), ("yobj", 2, "{r}y{o}"),
    ("dd", 3, "{r}{n}dd"), ("cc", 2, "{r}{n}cc{i}<Esc>"), ("yy", 3, "{r}{n}yy"),
    ("D", 2, "{r}{n}D"), ("C", 2, "{r}{n}C{i}<Esc>"), ("Y", 2, "{r}{n}Y"),
    ("x", 3, "{r}{n}x"), ("X", 2, "{r}{n}X"), ("r", 2, "{n}r{c}"), ("J", 2, "{n}J"),
    ("gJ", 1, "{n}gJ"), ("~", 2, "{n}~"), ("g~", 1, "g~{m}"), ("gu", 1, "gu{m}"),
    ("gU", 1, "gU{m}"), (">>", 1, "{n}>>"), ("<<", 1, "{n}<<"),
    ("<C-a>", 1, "{n}<C-a>"),
    ("<C-x>", 1, "{n}<C-x>"),
    ("p", 3, "{r}{n}p"), ("P", 3, "{r}{n}P"),
    ("i", 3, "{n}i{i}<Esc>"), ("a", 3, "{n}a{i}<Esc>"), ("I", 2, "{n}I{i}<Esc>"),
    ("A", 2, "{n}A{i}<Esc>"), ("o", 2, "{n}o{i}<Esc>"), ("O", 2, "{n}O{i}<Esc>"),
    # INSERT の Ctrl-w / Ctrl-u / Ctrl-r（#1040）。Ctrl-r は無名レジスタを読む。
    ("i<C-w>", 1, "{n}i{i}<C-w>{i}<Esc>"), ("A<C-u>", 1, "{n}A{i}<C-u>{i}<Esc>"),
    ("a<C-r>", 1, '{n}a{i}<C-r>"{i}<Esc>'),
    (".", 3, "{n}."),
    ("/", 2, "/{p}<CR>"), ("?", 1, "?{p}<CR>"), ("n", 2, "{n}n"), ("N", 1, "{n}N"),
    ("*", 2, "{n}*"), ("#", 1, "{n}#"),
    ("m", 2, "m{k}"), ("'", 1, "'{k}"), ("`", 1, "`{k}"), ("''", 1, "''"), ("``", 1, "``"),
    ("v", 3, "v{w}{V}"), ("V", 2, "V{v}{V}"), ("<C-v>", 2, "<C-v>{v}{X}"),
    # 矩形の `I` `A` `c` `C` `>` `<`（#1042）。矩形の語の後だけに付く 1 行ずつ。
    ("<C-v>I", 1, "<C-v>{v}{n}I{i}<Esc>"), ("<C-v>A", 1, "<C-v>{v}{n}A{i}<Esc>"),
    ("<C-v>c", 1, "<C-v>{v}c{i}<Esc>"), ("<C-v>C", 1, "<C-v>{v}C{i}<Esc>"),
    ("<C-v>>", 1, "<C-v>{v}{n}>"), ("<C-v><", 1, "<C-v>{v}{n}<"),
    # 前の選択の種類は矩形もあり得るので、閉じる鍵は矩形の側（#1039）。
    ("gv", 1, "gv{X}"),
    ("gi", 1, "{n}gi{i}<Esc>"),
    ("<C-o>", 1, "{n}<C-o>"),
    ("<Tab>", 1, "{n}<Tab>"),
)
# `g;` `g,` は語にしない。fuzz も 1 本の `:normal!` なので Vim では全体が 1 つの undo の単位
# （変更の列は 1 件）になり、命令ごとに単位を作る Nib といつも違う。契約 `--vim-marks` の
# verify_change_list が単位を分けた Vim の測定で守る（#1037・#1072 R5-2）。
# 鍵の型の中の {m}（オペレータの移動）。回数つきの移動と f/t の 1 文字を含む。
OPERATOR_MOTIONS = ("{n}h", "{n}l", "{n}j", "{n}k", "{n}w", "{n}b", "{n}e", "{n}W", "{n}B", "{n}E",
                    "{n}ge", "0", "^", "$", "g_", "gg", "G", "{n}f{c}", "{n}t{c}", "{n}F{c}",
                    "{n}T{c}", "%", "{n}-", "{n}+", "{n}<Space>", "{n}<BS>")
TEXT_OBJECTS = ("iw", "aw", "iW", "aW", 'i"', 'a"', "i'", "a'", "i(", "a(", "i{", "a{", "i[",
                "a[", "i<", "a<", "ib", "ab", "iB", "aB", "ip", "ap", "is", "as", "it", "at")
VISUAL_MOTIONS = ("{n}h", "{n}l", "{n}j", "{n}k", "{n}w", "{n}b", "{n}e", "{n}W", "{n}E", "0",
                  "^", "$", "{n}f{c}", "{n}t{c}", "G", "gg", "%", "o", "O")
# VISUAL を閉じる鍵。矩形の `> <` と `I A c C` は WORDS の別の行（#1042）なので外す。
VISUAL_ENDS = ("d", "x", "y", "r{c}", "~", "u", "U", ">", "<", "<Esc>")
BLOCK_ENDS = ("d", "x", "y", "r{c}", "~", "u", "U", "<Esc>")
REGISTERS = tuple('"' + name for name in "abcdefghijklmnopqrstuvwxyz") + (
    '"A', '"B', *('"' + digit for digit in "0123456789"), '"-', '"_', '""')
MARKS = ("a", "b", "c")
INSERT_PIECES = ("foo", "bar", "x", " ", "  ", "\t", "日本", "かな", "ＡＢ", ".", ",", "(", ")",
                 "<CR>", "z9")
# 本文の材料（Issue #954 の 3）。`<b>…</b>` は `it` `at` のため、句点は `is` `as` のため。
TEXT_PIECES = ("alpha", "beta", "gamma", "foo_bar", "x1", "a.b", "word", "Hello", "end.",
               "(paren)", "[sq]", "{br}", "<w>", '"dq s"', "'sq'", "<b>tag</b>", ",", ";", "!",
               "?", ":", "-", "日本語", "かな", "漢字", "ＡＢＣ", "１２", "です。", "、")
# 語の依存（Issue #963）。生成器の版は summary.md に書く（固定 seed の順が変わったら上げる）。
GENERATOR_VERSION = 3
# Vim に置く設定。Nib の `nrformats` は `bin,hex` の固定で、`-u NONE` の Vim の既定 `bin,octal,hex` の
# ままだと `07<C-a>` が 8 進で測られる（ADR 0015 の決定 14）。fixture へ写すときも同じ設定を書く。
ORACLE_SETTINGS = ["set nrformats=bin,hex"]
WORD_NAMES = tuple(kind for kind, _, _ in WORDS)
JUMP_WORDS = frozenset(("G", "gg", "NG", "Ngg", "N%", "/", "?", "*", "#", "%", "n", "N", "'", "`",
                        "''", "``"))
SEARCH_WORDS = frozenset(("/", "?", "*", "#"))
NUMBERED_WRITERS = frozenset(("dd", "cc"))
NUMBERED_READS = tuple('"' + digit for digit in "123456789")
READERS = frozenset(("p", "P"))
OBJECT_CARRIERS = frozenset(("dobj", "cobj", "yobj", "v"))
CHARACTERS = ("a", "z", ".", "(", ")", "e", " ", "日", "x")
COUNT_LIMIT = 4
WORDS_LIMIT = 6
LINES_LIMIT = 12


def load_oracle():
    """eng/vim-oracle.py as a module (its file name has a hyphen)."""
    spec = importlib.util.spec_from_file_location("vim_oracle", ROOT / "eng" / "vim-oracle.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


ORACLE = load_oracle()
UNKNOWN_NAME = re.compile(r"<[A-Za-z][A-Za-z0-9-]*>")


def unknown_key_name(keys: str) -> str | None:
    """A `<name>` the key table does not hold (the replayer would skip the case)."""
    index = 0
    while index < len(keys):
        name = next((name for name in ORACLE.KEY_NAMES if keys.startswith(name, index)), None)
        if name is not None:
            index += len(name)
            continue
        match = UNKNOWN_NAME.match(keys, index)
        if match is not None:
            return match.group(0)
        index += 1
    return None


def generate_text(rng: random.Random) -> str:
    lines = []
    for _ in range(rng.randint(1, LINES_LIMIT)):
        if rng.random() < 0.12:
            lines.append("")
            continue
        indent = rng.choice(("", "", "", "  ", "\t", "    "))
        pieces = [rng.choice(TEXT_PIECES) for _ in range(rng.randint(1, 6))]
        separators = [rng.choice((" ", " ", " ", "  ", "\t", "")) for _ in pieces]
        line = indent + "".join(piece + separator for piece, separator in zip(pieces, separators))
        line = line.rstrip(" \t") + rng.choice(("", "", "", " ", "  "))
        lines.append(line)
    while lines and lines[-1] == "":
        lines.pop()
    return "\n".join(lines) if lines else "alpha"


def patterns_of(text: str) -> list[str]:
    """Literal search patterns: ASCII words and Japanese runs that are in the buffer."""
    return sorted(set(re.findall(r"[A-Za-z0-9]+|[\u3040-\u30ff\u4e00-\u9fff]+", text)))


def expand_word(rng: random.Random, template: str, text: str, marks: tuple[str, ...] = MARKS,
                registers: tuple[str, ...] = REGISTERS) -> str:
    def count() -> str:
        return "" if rng.random() < 0.6 else str(rng.randint(1, COUNT_LIMIT))

    def character() -> str:
        own = [c for c in text if c not in "\n<\\"]
        return rng.choice(own) if own and rng.random() < 0.7 else rng.choice(CHARACTERS)

    def insert() -> str:
        return "".join(rng.choice(INSERT_PIECES) for _ in range(rng.randint(0, 3)))

    def pattern() -> str:
        found = patterns_of(text)
        if not found:
            return "zz"
        word = rng.choice(found)
        start = rng.randint(0, max(0, len(word) - 1))
        return word[start:start + rng.randint(1, len(word) - start)]

    fill = {
        "{n}": count, "{N}": lambda: str(rng.randint(1, LINES_LIMIT)),
        "{P}": lambda: str(rng.randint(1, 100)), "{B}": lambda: str(rng.randint(1, 20)),
        "{c}": character, "{i}": insert, "{p}": pattern, "{k}": lambda: rng.choice(marks),
        "{r}": lambda: "" if rng.random() < 0.7 else rng.choice(registers),
        "{m}": lambda: expand_word(rng, rng.choice(OPERATOR_MOTIONS), text),
        "{o}": lambda: rng.choice(TEXT_OBJECTS),
        "{v}": lambda: "".join(expand_word(rng, rng.choice(VISUAL_MOTIONS), text)
                               for _ in range(rng.randint(1, 3))),
        "{w}": lambda: "".join(expand_word(rng, rng.choice(VISUAL_MOTIONS), text)
                               if rng.random() < 0.7 else rng.choice(TEXT_OBJECTS)
                               for _ in range(rng.randint(1, 3))),
        "{V}": lambda: expand_word(rng, rng.choice(VISUAL_ENDS), text),
        "{X}": lambda: expand_word(rng, rng.choice(BLOCK_ENDS), text),
    }
    result = []
    index = 0
    while index < len(template):
        slot = template[index:index + 3]
        if slot in fill:
            result.append(fill[slot]())
            index += 3
        else:
            result.append(template[index])
            index += 1
    return "".join(result)


def initial_state() -> dict:
    """What the words so far did: jumped, searched, wrote "1 (unnamed dd/cc), set which marks."""
    return {"jumped": False, "searched": False, "numbered": False, "marks": frozenset()}


def available(kind: str, state: dict) -> bool:
    """Whether the state allows the word kind at all (Issue #963)."""
    # `Ctrl-o` `Ctrl-i` も跳びの後だけ（-es の Vim は開いた最終行をジャンプの列に持つ・#1037）。
    if kind in ("''", "``", "<C-o>", "<Tab>"):
        return state["jumped"]
    if kind in ("n", "N"):
        return state["searched"]
    if kind in ("'", "`"):
        return bool(state["marks"])
    return True


def word_holds(state: dict, kind: str, keys: str) -> bool:
    """Whether a generated word's dependencies hold in the state (the shrinker re-checks them)."""
    if not available(kind, state):
        return False
    if kind in ("'", "`") and keys[1:2] not in state["marks"]:
        return False
    return not (kind in READERS and keys.startswith(NUMBERED_READS) and not state["numbered"])


def advanced(state: dict, kind: str, keys: str) -> dict:
    return {"jumped": state["jumped"] or kind in JUMP_WORDS,
            "searched": state["searched"] or kind in SEARCH_WORDS,
            "numbered": state["numbered"] or (kind in NUMBERED_WRITERS and not keys.startswith('"')),
            "marks": state["marks"] | ({keys[1]} if kind == "m" else set())}


def dependencies_hold(words: list[tuple[str, str]]) -> bool:
    state = initial_state()
    for kind, keys in words:
        if not word_holds(state, kind, keys):
            return False
        state = advanced(state, kind, keys)
    return True


def generate_case(rng: random.Random,
                  excluded: frozenset[str]) -> tuple[str, list[tuple[str, str]]]:
    """A buffer and its words, each word a (kind, keys) pair the words before it allow."""
    text = generate_text(rng)
    pool = [word for word in WORDS if word[0] not in excluded]
    unnumbered = tuple(name for name in REGISTERS if name not in NUMBERED_READS)
    words: list[tuple[str, str]] = []
    state = initial_state()
    target = rng.randint(1, WORDS_LIMIT)
    attempts = 0
    while len(words) < target and attempts < 50:
        attempts += 1
        choices = [word for word in pool if available(word[0], state)]
        if not choices:
            break
        kind, _, template = rng.choices(choices, [weight for _, weight, _ in choices])[0]
        marks = tuple(sorted(state["marks"])) if kind in ("'", "`") else MARKS
        registers = unnumbered if kind in READERS and not state["numbered"] else REGISTERS
        keys = expand_word(rng, template, text, marks, registers)
        joined = "".join(keys for _, keys in words) + keys
        if unknown_key_name(joined) is None:
            words.append((kind, keys))
            state = advanced(state, kind, keys)
    return text, words


def escape(value: str) -> str:
    return (value.replace("\\", "\\\\").replace("\n", "\\n").replace("\t", "\\t")
            .replace("\r", "\\r"))


def unescape(value: str) -> str:
    out = []
    index = 0
    while index < len(value):
        character = value[index]
        if character == "\\" and index + 1 < len(value):
            index += 1
            out.append({"n": "\n", "t": "\t", "r": "\r"}.get(value[index], value[index]))
        else:
            out.append(character)
        index += 1
    return "".join(out)


def replay_line(measured: dict) -> str:
    values = (measured["name"], measured["text"], measured["keys"], measured["expected"],
              str(measured["line"]), str(measured["column"]), measured["register"],
              measured["register_kind"])
    return "\t".join(escape(value) for value in values)


def replay(replayer: Path, work: Path, measured: list[dict]) -> dict[str, dict]:
    """name -> {"status": ok|mismatch|skip, "fields": {field: (expected, actual)}, "reason"}."""
    source = work / "replay-input.tsv"
    source.write_bytes(("".join(replay_line(m) + "\n" for m in measured)).encode("utf-8"))
    finished = subprocess.run([str(replayer), "--fixtures", str(source)], capture_output=True,
                              check=False)
    if finished.returncode != 0:
        raise RuntimeError(f"nib_vim_replay failed ({finished.returncode}): "
                           f"{finished.stderr.decode('utf-8', errors='replace')[-2000:]}")
    results: dict[str, dict] = {}
    for line in finished.stdout.decode("utf-8").split("\n"):
        if not line:
            continue
        status, _, rest = line.partition(" ")
        name, _, detail = rest.partition(" ")
        entry = results.setdefault(name, {"status": status, "fields": {}, "reason": ""})
        if status == "mismatch":
            entry["status"] = "mismatch"
            match = re.fullmatch(r"(\S+) expected=(.*) actual=(.*)", detail)
            if match is None:
                raise RuntimeError(f"unreadable replayer line: {line!r}")
            entry["fields"][match.group(1)] = (unescape(match.group(2)), unescape(match.group(3)))
        elif status == "skip":
            entry["reason"] = detail
    missing = [m["name"] for m in measured if m["name"] not in results]
    if missing:
        raise RuntimeError(f"nib_vim_replay wrote nothing for {missing[:5]}")
    return results


def measure(work: Path, name: str, text: str, keys: str) -> tuple[dict | None, str]:
    """The oracle's answer, or None and the reason it refused."""
    try:
        return ORACLE.measure(work, {"name": name, "text": text, "keys": keys,
                                     "settings": ORACLE_SETTINGS}), ""
    except ValueError as error:
        return None, f"ValueError: {error}"
    except RuntimeError as error:
        return None, f"RuntimeError: {str(error).splitlines()[0] if str(error) else ''}"


def still_differs(replayer: Path, work: Path, text: str, keys: str, fields: set[str]) -> dict | None:
    if not keys:
        return None
    measured, _ = measure(work, "shrink", text, keys)
    if measured is None:
        return None
    result = replay(replayer, work, [measured])["shrink"]
    if result["status"] == "mismatch" and fields & set(result["fields"]):
        return {"measured": measured, "result": result}
    return None


def shrink(replayer: Path, work: Path, text: str, words: list[tuple[str, str]],
           fields: set[str]) -> dict:
    """Deletes words, then lines, while Vim and Nib still disagree on one of the same fields."""
    current = list(words)
    lines = text.split("\n")
    last = None
    changed = True
    while changed:
        changed = False
        for index in reversed(range(len(current))):
            if len(current) == 1:
                break
            candidate = current[:index] + current[index + 1:]
            if not dependencies_hold(candidate):
                continue
            found = still_differs(replayer, work, "\n".join(lines),
                                  "".join(k for _, k in candidate), fields)
            if found is not None:
                current, last, changed = candidate, found, True
        for index in reversed(range(len(lines))):
            if len(lines) == 1:
                break
            candidate = lines[:index] + lines[index + 1:]
            joined = "\n".join(candidate)
            if joined.endswith("\n") or not joined:
                continue
            found = still_differs(replayer, work, joined, "".join(k for _, k in current), fields)
            if found is not None:
                lines, last, changed = candidate, found, True
    keys = "".join(k for _, k in current)
    if last is None:
        last = still_differs(replayer, work, "\n".join(lines), keys, fields)
    minimal = {"name": "", "text": "\n".join(lines), "keys": keys,
               "words": [kind for kind, _ in current], "word_keys": [k for _, k in current]}
    if last is not None:
        minimal["oracle"] = oracle_values(last["measured"])
        minimal["nib"] = {field: actual for field, (_, actual) in last["result"]["fields"].items()}
    return minimal


def oracle_values(measured: dict) -> dict:
    return {"text": measured["expected"], "line": measured["line"], "column": measured["column"],
            "register": measured["register"], "register_kind": measured["register_kind"]}


def covered_by(kinds: list[str], representative: list[str]) -> bool:
    """Whether a representative's minimal words appear in order among a case's words."""
    position = 0
    for kind in kinds:
        if position < len(representative) and kind == representative[position]:
            position += 1
    return position == len(representative)


def vim_errors(work: Path, text: str, keys: str) -> str:
    """Vim's error lines for a refused case: the oracle's probe again under `-V1` (Issue #963).

    The oracle's `-es` run writes nothing to stderr, so this second run (same probe, same buffer)
    exists only to show which E-number refused; it never feeds a measurement.
    """
    diagnose = work / "diagnose"
    diagnose.mkdir(parents=True, exist_ok=True)
    (diagnose / "input.txt").write_bytes(text.encode("utf-8"))
    (diagnose / "probe.vim").write_text(ORACLE.probe_script([], keys), encoding="utf-8")
    command = [str(ORACLE.VIM), "-u", "NONE", "-i", "NONE", "-N", "-n", "-es", "-V1",
               "--cmd", "lang mes C", "-S", "probe.vim", "input.txt"]
    try:
        finished = subprocess.run(command, cwd=diagnose, capture_output=True,
                                  timeout=ORACLE.ORACLE_TIMEOUT_SECONDS)
    except subprocess.TimeoutExpired:
        return "timed out"
    lines = finished.stderr.decode("utf-8", errors="replace").splitlines()
    return "\n".join(line.strip() for line in lines if re.match(r"\s*E\d+:", line))


def rejecting_word(work: Path, text: str, words: list[tuple[str, str]]) -> str:
    """The word whose prefix first makes the oracle refuse (the last word when only all of it does)."""
    for size in range(1, len(words)):
        answer, _ = measure(work, "diagnose", text, "".join(k for _, k in words[:size]))
        if answer is None:
            return words[size - 1][0]
    return words[-1][0] if words else "?"


def object_name(name: str) -> bool:
    return name in TEXT_OBJECTS and name not in WORD_NAMES


def known_issue(words: list[tuple[str, str]], known: list[tuple[str, tuple[str, ...]]]) -> str | None:
    """The first `--known` Issue one of whose names the words hold, or None for a new difference."""
    for issue, names in known:
        for name in names:
            if object_name(name):
                hit = any(kind in OBJECT_CARRIERS and name in keys for kind, keys in words)
            else:
                hit = any(kind == name for kind, _ in words)
            if hit:
                return issue
    return None


def judged_words(entry: dict, by_name: dict, mismatches: list[dict]) -> list[tuple[str, str]]:
    """The words the known check reads: the minimal ones, the representative's, or all of them."""
    if "minimal" in entry:
        return list(zip(entry["minimal"]["words"], entry["minimal"]["word_keys"]))
    if "represented_by" in entry:
        representative = next(e for e in mismatches if e["name"] == entry["represented_by"])
        return judged_words(representative, by_name, mismatches)
    return list(by_name[entry["name"]]["words"])


def fuzz(args: argparse.Namespace, seed: int, out: Path) -> dict:
    work = out / "work"
    work.mkdir(parents=True, exist_ok=True)
    rng = random.Random(seed)
    excluded = frozenset(args.exclude)
    cases = []
    for number in range(1, args.count + 1):
        text, words = generate_case(rng, excluded)
        cases.append({"name": f"fuzz-{seed}-{number:04d}", "text": text, "words": words})
    measured, rejected = [], []
    for case in cases:
        keys = "".join(k for _, k in case["words"])
        answer, reason = measure(work, case["name"], case["text"], keys)
        if answer is None:
            rejected.append({"name": case["name"], "text": case["text"], "keys": keys,
                             "words": [kind for kind, _ in case["words"]],
                             "word": rejecting_word(work, case["text"], case["words"]),
                             "reason": reason,
                             "stderr": vim_errors(work, case["text"], keys)})
        else:
            measured.append(answer)
    results = replay(args.replay, work, measured) if measured else {}
    by_name = {case["name"]: case for case in cases}
    mismatches, skipped, matched = [], [], 0
    for answer in measured:
        result = results[answer["name"]]
        if result["status"] == "ok":
            matched += 1
            continue
        if result["status"] == "skip":
            skipped.append({"name": answer["name"], "reason": result["reason"]})
            continue
        case = by_name[answer["name"]]
        mismatches.append({
            "name": answer["name"], "text": answer["text"], "keys": answer["keys"],
            "words": [kind for kind, _ in case["words"]],
            "fields": sorted(result["fields"]), "oracle": oracle_values(answer),
            "nib": {field: actual for field, (_, actual) in result["fields"].items()},
        })
    if args.shrink:
        representatives: list[dict] = []
        for entry in mismatches:
            grouped = len(mismatches) >= 50 and next(
                (rep for rep in representatives
                 if set(rep["fields"]) & set(entry["fields"])
                 and covered_by(entry["words"], rep["minimal"]["words"])), None)
            if grouped:
                entry["represented_by"] = grouped["name"]
                continue
            case = by_name[entry["name"]]
            entry["minimal"] = shrink(args.replay, work, case["text"], case["words"],
                                      set(entry["fields"]))
            entry["minimal"]["name"] = entry["name"] + "-min"
            representatives.append(entry)
    for entry in mismatches:
        entry["known"] = known_issue(judged_words(entry, by_name, mismatches), args.known)
    (out / "mismatches.json").write_text(
        json.dumps(mismatches, ensure_ascii=False, indent=1) + "\n", encoding="utf-8", newline="\n")
    (out / "rejected.json").write_text(
        json.dumps(rejected, ensure_ascii=False, indent=1) + "\n", encoding="utf-8", newline="\n")
    tally = {"seed": seed, "generated": len(cases), "rejected": len(rejected),
             "skipped": len(skipped), "matched": matched, "mismatched": len(mismatches),
             "known": sum(1 for e in mismatches if e["known"] is not None),
             "new": sum(1 for e in mismatches if e["known"] is None)}
    summary = summary_text(args, tally, rejected, skipped, mismatches)
    (out / "summary.md").write_text(summary, encoding="utf-8", newline="\n")
    print(f"seed {seed}: generated {len(cases)}  rejected {len(rejected)}  skipped {len(skipped)}  "
          f"matched {matched}  mismatched {len(mismatches)} (known {tally['known']}  "
          f"new {tally['new']})  -> {out / 'summary.md'}")
    return {"tally": tally, "mismatches": mismatches}


def fenced(value: str) -> str:
    return "`` " + value.replace("\n", "⏎").replace("\t", "→") + " ``"


def settings_lines(args: argparse.Namespace) -> list[str]:
    known = "; ".join(f"#{issue}: {' '.join(fenced(name) for name in names)}"
                      for issue, names in args.known)
    return [f"- generator={GENERATOR_VERSION}（語の依存・Issue #963）",
            f"- Vim: {ORACLE.vim_version()}", f"- replayer: `{args.replay}`",
            f"- shrink: {'on' if args.shrink else 'off'}",
            "- exclude: " + (" ".join(fenced(name) for name in args.exclude) or "なし"),
            "- known: " + (known or "なし"), ""]


def mismatch_lines(entry: dict, mismatches: list[dict], heading: str) -> list[str]:
    shown = entry.get("minimal", entry)
    oracle = shown.get("oracle", entry["oracle"])
    nib = shown.get("nib", entry["nib"])
    lines = [f"{heading} {entry['name']}（欄: {', '.join(sorted(nib)) or '?'}）", "",
             f"- text: {fenced(shown['text'])}", f"- keys: {fenced(shown['keys'])}"]
    for field in sorted(nib):
        lines.append(f"- {field}: Vim {fenced(str(oracle[field]))} / Nib {fenced(nib[field])}")
    represented = [e["name"] for e in mismatches if e.get("represented_by") == entry["name"]]
    if represented:
        lines.append(f"- 同じ原因と見た件: {len(represented)}")
    lines.append("")
    return lines


def summary_text(args, tally, rejected, skipped, mismatches) -> str:
    lines = [f"# Vim の差分 fuzz（seed {tally['seed']}・{tally['generated']} 件）", "",
             *settings_lines(args),
             "| 生成 | 拒否 | skip | 一致 | 不一致 | 既知 | 新規 |",
             "|---:|---:|---:|---:|---:|---:|---:|",
             f"| {tally['generated']} | {tally['rejected']} | {tally['skipped']} | "
             f"{tally['matched']} | {tally['mismatched']} | {tally['known']} | {tally['new']} |", ""]
    by_issue = Counter(entry["known"] for entry in mismatches if entry["known"] is not None)
    lines.append("## 既知（Issue ごと）")
    lines.append("")
    lines.extend(f"- #{issue}: {count}" for issue, count in sorted(by_issue.items()))
    if not by_issue:
        lines.append("- 0")
    lines.append("")
    reasons = Counter(entry["reason"].split(":")[0] for entry in rejected)
    lines.append("## 拒否の内訳")
    lines.append("")
    lines.append("理由: " + (", ".join(f"{k} {v}" for k, v in sorted(reasons.items())) or "0"))
    lines.append("")
    lines.append("拒否を出した語（前から測って最初に拒まれた語）:")
    lines.append("")
    culprits = Counter(entry["word"] for entry in rejected)
    lines.extend(f"- `{kind}`: {count}" for kind, count in culprits.most_common())
    if not culprits:
        lines.append("- 0")
    lines.append("")
    errors = Counter((entry["stderr"].split(":")[0] if entry["stderr"] else "(なし)")
                     for entry in rejected)
    lines.append("Vim のエラー番号: " + (", ".join(f"{k} {v}" for k, v in sorted(errors.items()))
                                        or "0"))
    lines.append("")
    fields = Counter(field for entry in mismatches for field in entry["fields"])
    lines.append("不一致の欄（1 件に複数の欄がありうる）: "
                 + (", ".join(f"{k} {v}" for k, v in sorted(fields.items())) or "0"))
    lines.append("")
    kinds = Counter()
    for entry in mismatches:
        if entry["known"] is None:
            words = entry["minimal"]["words"] if "minimal" in entry else entry["words"]
            kinds.update(set(words))
    lines.append("## 新規の不一致の鍵の種類（最小化した件はその語・ほかは全部の語）")
    lines.append("")
    lines.extend(f"- `{kind}`: {count}" for kind, count in kinds.most_common())
    if not kinds:
        lines.append("- 0")
    lines.append("")
    lines.append("## 新規の不一致（最小化した件）")
    lines.append("")
    new = [e for e in mismatches if e["known"] is None and "represented_by" not in e]
    if not new:
        lines.append("新規の不一致は 0。")
        lines.append("")
    for entry in new:
        lines.extend(mismatch_lines(entry, mismatches, "###"))
    lines.append("## 既知の不一致")
    lines.append("")
    known = [e for e in mismatches if e["known"] is not None]
    lines.extend(f"- {e['name']} → #{e['known']}: {fenced(e.get('minimal', e)['keys'])}"
                 for e in known)
    if not known:
        lines.append("- 0")
    lines.append("")
    if skipped:
        lines.append(f"skip: {len(skipped)}（{skipped[0]['reason']} ほか）")
    return "\n".join(lines) + "\n"


def fuzz_seeds(args: argparse.Namespace) -> int:
    """`--seeds A-B`: each seed as `--seed` would into `<out>/seed-N/`, then the totals."""
    runs = [fuzz(args, seed, args.out / f"seed-{seed}") for seed in args.seeds]
    columns = ("generated", "rejected", "skipped", "matched", "mismatched", "known", "new")
    lines = [f"# Vim の差分 fuzz（seeds {args.seeds[0]}〜{args.seeds[-1]}・各 {args.count} 件）", "",
             *settings_lines(args),
             "| seed | 生成 | 拒否 | skip | 一致 | 不一致 | 既知 | 新規 |",
             "|---:|---:|---:|---:|---:|---:|---:|---:|"]
    for run in runs:
        tally = run["tally"]
        lines.append(f"| {tally['seed']} | " + " | ".join(str(tally[c]) for c in columns) + " |")
    lines.append("| 計 | " + " | ".join(str(sum(r["tally"][c] for r in runs)) for c in columns)
                 + " |")
    lines.append("")
    by_issue = Counter(e["known"] for r in runs for e in r["mismatches"] if e["known"] is not None)
    lines.append("既知（Issue ごと）: " + (", ".join(f"#{k} {v}" for k, v in sorted(by_issue.items()))
                                         or "0"))
    lines.append("")
    lines.append("## 新規の不一致（全 seed・最小化した件）")
    lines.append("")
    count = 0
    for run in runs:
        for entry in run["mismatches"]:
            if entry["known"] is None and "represented_by" not in entry:
                lines.extend(mismatch_lines(entry, run["mismatches"], "###"))
                count += 1
    if count == 0:
        lines.append("新規の不一致は 0。")
        lines.append("")
    args.out.mkdir(parents=True, exist_ok=True)
    (args.out / "summary.md").write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")
    print(f"seeds {args.seeds[0]}-{args.seeds[-1]} -> {args.out / 'summary.md'}")
    return 0


def from_fixtures(args: argparse.Namespace) -> int:
    """The replayer's self-check on the first N plain fixtures (no settings/viewport/register)."""
    work = args.out / "work"
    work.mkdir(parents=True, exist_ok=True)
    fixtures = json.loads((ROOT / "tests" / "vim" / "fixtures.json").read_text(encoding="utf-8"))
    plain = [f for f in fixtures if not set(f) - {"name", "text", "keys"}][:args.from_fixtures]
    measured = [ORACLE.measure(work, fixture) for fixture in plain]
    results = replay(args.replay, work, measured)
    counts = Counter(result["status"] for result in results.values())
    for name, result in results.items():
        if result["status"] != "ok":
            print(f"{result['status']} {name} {result['fields'] or result['reason']}")
    print(f"fixtures {len(plain)}: " + ", ".join(f"{k} {v}" for k, v in sorted(counts.items())))
    return 0 if counts.get("ok", 0) == len(plain) else 1


def word_name(value: str) -> str:
    if value not in WORD_NAMES:
        raise argparse.ArgumentTypeError(f"{value!r} is not a word of the table "
                                         f"({' '.join(WORD_NAMES)})")
    return value


def known_names(value: str) -> list[str]:
    """`a,b,c` into names; a name is the longest word (or text object) that a comma or the end
    follows, so the words `,` and `;` read as they are (`;,,` is `;` and `,`)."""
    candidates = sorted(set(WORD_NAMES) | set(TEXT_OBJECTS), key=len, reverse=True)
    names, index = [], 0
    while index < len(value):
        name = next((c for c in candidates if value.startswith(c, index)
                     and (index + len(c) == len(value) or value[index + len(c)] == ",")), None)
        if name is None:
            raise argparse.ArgumentTypeError(f"no word of the table at {value[index:]!r}")
        names.append(name)
        index += len(name) + 1
    return names


def known_spec(value: str) -> tuple[str, tuple[str, ...]]:
    issue, separator, rest = value.partition(":")
    if not separator or not issue.isdigit() or not rest:
        raise argparse.ArgumentTypeError(f"{value!r} is not ISSUE:WORD,WORD")
    return issue, tuple(known_names(rest))


def seed_range(value: str) -> list[int]:
    match = re.fullmatch(r"(\d+)(?:-(\d+))?", value)
    if match is None or int(match.group(2) or match.group(1)) < int(match.group(1)):
        raise argparse.ArgumentTypeError(f"{value!r} is not A-B")
    return list(range(int(match.group(1)), int(match.group(2) or match.group(1)) + 1))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--count", type=int, default=100)
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--seeds", type=seed_range, metavar="A-B")
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--replay", type=Path, default=DEFAULT_REPLAY)
    parser.add_argument("--shrink", action="store_true")
    parser.add_argument("--exclude", type=word_name, nargs="+", action="extend", default=[],
                        metavar="WORD")
    parser.add_argument("--known", type=known_spec, action="append", default=[],
                        metavar="ISSUE:WORD,WORD")
    parser.add_argument("--from-fixtures", type=int, metavar="N")
    args = parser.parse_args()
    if not args.replay.is_file():
        parser.error(f"no replayer at {args.replay}; build the nib_vim_replay target first")
    if args.from_fixtures is not None:
        return from_fixtures(args)
    if args.seeds is not None:
        return fuzz_seeds(args)
    fuzz(args, args.seed, args.out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
