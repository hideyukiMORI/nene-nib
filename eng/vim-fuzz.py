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
windows, Ex or Ctrl+P, no keys Nib does not take yet (`R s S gv <C-a> <C-x>`, block `I A c C > <`)
and no INSERT `<BS>`.

The buffer has 1 to 12 lines mixing ASCII words, spaces, tabs, bracket and quote pairs, punctuation,
Japanese (kana, kanji, full-width letters), empty lines and trailing spaces. It never ends with a
newline and never holds a CR or a combining character (v1).

A case the oracle refuses (`ValueError`: not NORMAL at the end, or `RuntimeError`: Vim failed or
timed out) is counted as rejected and dropped. Every difference is written to `<out>/mismatches.json`
with `name` / `text` / `keys` first, so those three keys copy into tests/vim/fixtures.json as they
are, followed by the oracle's and Nib's values. `--shrink` deletes words from the keys and lines from
the buffer while both Vim and the replayer still disagree on one of the same fields, and records the
smallest case under `minimal`. `<out>/summary.md` holds the counts (generated, rejected, matched,
mismatched) and the differences by word kind.

`--from-fixtures N` is the replayer's self-check: it measures the first N tests/vim/fixtures.json
cases without settings, viewport or register through the same oracle and expects every one `ok`.

Usage:
    python eng/vim-fuzz.py --count 300 --seed 1 --out D:/NeNeNib/outputs/<date>/vim-fuzz-seed1 --shrink
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
    ("<BS>", 1, "{n}<BS>"), ("<Home>", 1, "<Home>"), ("<End>", 1, "<End>"),
    ("d", 4, "{r}{n}d{m}"), ("c", 3, "{r}{n}c{m}{i}<Esc>"), ("y", 3, "{r}{n}y{m}"),
    ("dobj", 3, "{r}d{o}"), ("cobj", 2, "{r}c{o}{i}<Esc>"), ("yobj", 2, "{r}y{o}"),
    ("dd", 3, "{r}{n}dd"), ("cc", 2, "{r}{n}cc{i}<Esc>"), ("yy", 3, "{r}{n}yy"),
    ("D", 2, "{r}{n}D"), ("C", 2, "{r}{n}C{i}<Esc>"), ("Y", 2, "{r}{n}Y"),
    ("x", 3, "{r}{n}x"), ("X", 2, "{r}{n}X"), ("r", 2, "{n}r{c}"), ("J", 2, "{n}J"),
    ("gJ", 1, "{n}gJ"), ("~", 2, "{n}~"), ("g~", 1, "g~{m}"), ("gu", 1, "gu{m}"),
    ("gU", 1, "gU{m}"), (">>", 1, "{n}>>"), ("<<", 1, "{n}<<"),
    ("p", 3, "{r}{n}p"), ("P", 3, "{r}{n}P"),
    ("i", 3, "{n}i{i}<Esc>"), ("a", 3, "{n}a{i}<Esc>"), ("I", 2, "{n}I{i}<Esc>"),
    ("A", 2, "{n}A{i}<Esc>"), ("o", 2, "{n}o{i}<Esc>"), ("O", 2, "{n}O{i}<Esc>"),
    (".", 3, "{n}."),
    ("/", 2, "/{p}<CR>"), ("?", 1, "?{p}<CR>"), ("n", 2, "{n}n"), ("N", 1, "{n}N"),
    ("*", 2, "{n}*"), ("#", 1, "{n}#"),
    ("m", 2, "m{k}"), ("'", 1, "'{k}"), ("`", 1, "`{k}"), ("''", 1, "''"), ("``", 1, "``"),
    ("v", 3, "v{w}{V}"), ("V", 2, "V{v}{V}"), ("<C-v>", 2, "<C-v>{v}{X}"),
)
# 鍵の型の中の {m}（オペレータの移動）。回数つきの移動と f/t の 1 文字を含む。
OPERATOR_MOTIONS = ("{n}h", "{n}l", "{n}j", "{n}k", "{n}w", "{n}b", "{n}e", "{n}W", "{n}B", "{n}E",
                    "{n}ge", "0", "^", "$", "g_", "gg", "G", "{n}f{c}", "{n}t{c}", "{n}F{c}",
                    "{n}T{c}", "%", "{n}-", "{n}+", "{n}<Space>", "{n}<BS>")
TEXT_OBJECTS = ("iw", "aw", "iW", "aW", 'i"', 'a"', "i'", "a'", "i(", "a(", "i{", "a{", "i[",
                "a[", "i<", "a<", "ib", "ab", "iB", "aB", "ip", "ap", "is", "as", "it", "at")
VISUAL_MOTIONS = ("{n}h", "{n}l", "{n}j", "{n}k", "{n}w", "{n}b", "{n}e", "{n}W", "{n}E", "0",
                  "^", "$", "{n}f{c}", "{n}t{c}", "G", "gg", "%", "o", "O")
# VISUAL を閉じる鍵。矩形は `> <` が後続（#770）なので外す。
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


def expand_word(rng: random.Random, template: str, text: str) -> str:
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
        "{c}": character, "{i}": insert, "{p}": pattern, "{k}": lambda: rng.choice(MARKS),
        "{r}": lambda: "" if rng.random() < 0.7 else rng.choice(REGISTERS),
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


def generate_case(rng: random.Random) -> tuple[str, list[tuple[str, str]]]:
    """A buffer and its words, each word a (kind, keys) pair."""
    text = generate_text(rng)
    kinds = [kind for kind, _, _ in WORDS]
    weights = [weight for _, weight, _ in WORDS]
    templates = {kind: template for kind, _, template in WORDS}
    words: list[tuple[str, str]] = []
    target = rng.randint(1, WORDS_LIMIT)
    attempts = 0
    while len(words) < target and attempts < 50:
        attempts += 1
        kind = rng.choices(kinds, weights)[0]
        keys = expand_word(rng, templates[kind], text)
        joined = "".join(keys for _, keys in words) + keys
        if unknown_key_name(joined) is None:
            words.append((kind, keys))
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
        return ORACLE.measure(work, {"name": name, "text": text, "keys": keys}), ""
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
               "words": [kind for kind, _ in current]}
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


def fuzz(args: argparse.Namespace) -> int:
    out: Path = args.out
    work = out / "work"
    work.mkdir(parents=True, exist_ok=True)
    rng = random.Random(args.seed)
    cases = []
    for number in range(1, args.count + 1):
        text, words = generate_case(rng)
        cases.append({"name": f"fuzz-{args.seed}-{number:04d}", "text": text, "words": words})
    measured, rejected = [], []
    for case in cases:
        keys = "".join(k for _, k in case["words"])
        answer, reason = measure(work, case["name"], case["text"], keys)
        if answer is None:
            rejected.append({"name": case["name"], "text": case["text"], "keys": keys,
                             "reason": reason})
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
    (out / "mismatches.json").write_text(
        json.dumps(mismatches, ensure_ascii=False, indent=1) + "\n", encoding="utf-8", newline="\n")
    (out / "rejected.json").write_text(
        json.dumps(rejected, ensure_ascii=False, indent=1) + "\n", encoding="utf-8", newline="\n")
    summary = summary_text(args, len(cases), rejected, skipped, matched, mismatches)
    (out / "summary.md").write_text(summary, encoding="utf-8", newline="\n")
    print(f"generated {len(cases)}  rejected {len(rejected)}  skipped {len(skipped)}  "
          f"matched {matched}  mismatched {len(mismatches)}  -> {out / 'summary.md'}")
    return 0


def fenced(value: str) -> str:
    return "`` " + value.replace("\n", "⏎").replace("\t", "→") + " ``"


def summary_text(args, generated, rejected, skipped, matched, mismatches) -> str:
    lines = [f"# Vim の差分 fuzz（seed {args.seed}・{generated} 件）", "",
             f"- Vim: {ORACLE.vim_version()}", f"- replayer: `{args.replay}`",
             f"- shrink: {'on' if args.shrink else 'off'}", "",
             "| 生成 | 拒否 | skip | 一致 | 不一致 |", "|---:|---:|---:|---:|---:|",
             f"| {generated} | {len(rejected)} | {len(skipped)} | {matched} | {len(mismatches)} |",
             ""]
    reasons = Counter(entry["reason"].split(":")[0] for entry in rejected)
    lines.append("拒否の内訳: " + (", ".join(f"{k} {v}" for k, v in sorted(reasons.items()))
                                 or "0"))
    lines.append("")
    fields = Counter(field for entry in mismatches for field in entry["fields"])
    lines.append("不一致の欄（1 件に複数の欄がありうる）: "
                 + (", ".join(f"{k} {v}" for k, v in sorted(fields.items())) or "0"))
    lines.append("")
    kinds = Counter()
    for entry in mismatches:
        words = entry["minimal"]["words"] if "minimal" in entry else entry["words"]
        kinds.update(set(words))
    lines.append("## 鍵の種類ごとの不一致の数（最小化した件はその語・ほかは全部の語）")
    lines.append("")
    lines.extend(f"- `{kind}`: {count}" for kind, count in kinds.most_common())
    if not kinds:
        lines.append("- 0")
    lines.append("")
    lines.append("## 不一致（最小化した件）")
    lines.append("")
    if not mismatches:
        lines.append("不一致は 0。")
    for entry in mismatches:
        if "represented_by" in entry:
            continue
        shown = entry.get("minimal", entry)
        oracle = shown.get("oracle", entry["oracle"])
        nib = shown.get("nib", entry["nib"])
        lines.append(f"### {entry['name']}（欄: {', '.join(sorted(nib)) or '?'}）")
        lines.append("")
        lines.append(f"- text: {fenced(shown['text'])}")
        lines.append(f"- keys: {fenced(shown['keys'])}")
        for field in sorted(nib):
            lines.append(f"- {field}: Vim {fenced(str(oracle[field]))} / Nib {fenced(nib[field])}")
        represented = [e["name"] for e in mismatches if e.get("represented_by") == entry["name"]]
        if represented:
            lines.append(f"- 同じ原因と見た件: {len(represented)}")
        lines.append("")
    if skipped:
        lines.append(f"skip: {len(skipped)}（{skipped[0]['reason']} ほか）")
    return "\n".join(lines) + "\n"


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


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--count", type=int, default=100)
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--replay", type=Path, default=DEFAULT_REPLAY)
    parser.add_argument("--shrink", action="store_true")
    parser.add_argument("--from-fixtures", type=int, metavar="N")
    args = parser.parse_args()
    if not args.replay.is_file():
        parser.error(f"no replayer at {args.replay}; build the nib_vim_replay target first")
    if args.from_fixtures is not None:
        return from_fixtures(args)
    return fuzz(args)


if __name__ == "__main__":
    sys.exit(main())
