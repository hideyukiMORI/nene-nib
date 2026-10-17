"""Generate the wrapped-display fixtures with the pinned Vim, without opening a window.

The original oracle owns the key notation, defaults and executable. This companion measures
the additional display inputs (width, wrapped rows) that its nowrap fixtures do not describe.
Existing fixtures and their measurement code remain unchanged (ADR 0114 / QLT-001).
A viewport with "leftcol" is the nowrap form (ADR 0145): the window is "columns" cells wide,
starts scrolled to "leftcol", and the snapshot records winsaveview().leftcol.
"""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "tests/vim/wrap-fixtures.json"
HEADER = ROOT / "tests/vim/VimWrapFixtures.hpp"
SPEC = importlib.util.spec_from_file_location("vim_oracle", ROOT / "eng/vim-oracle.py")
oracle = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(oracle)


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def provenance() -> str:
    return (f"// wrap-fixtures.json: sha256 {digest(SOURCE)}\n\n"
            f"// measurement: sha256 {digest(Path(__file__))}\n\n"
            f"// shared oracle: sha256 {digest(ROOT / 'eng/vim-oracle.py')}\n\n")


def script(fixture: dict, keys: str) -> str:
    view = fixture["viewport"]
    snapshot = ("{'text': join(getline(1, '$'), \"\\n\"), 'line': line('.'), "
                "'column': col('.'), 'first': line('w0'), 'scroll': &scroll, "
                "'width': winwidth(0), 'height': winheight(0), "
                "'skipcol': winsaveview().skipcol, 'leftcol': winsaveview().leftcol, "
                "'register': getreg('\"'), "
                "'register_kind': getregtype('\"')}")
    unwrapped = "leftcol" in view
    # nowrap は Nib の最小の横の追従に近い sidescroll=1（ADR 0145 の決定 10）。
    wrapping = ("set nowrap sidescroll=1 sidescrolloff=0" if unwrapped
                else "set wrap nolinebreak nobreakindent")
    commands = [*oracle.DEFAULT_SETTINGS,
                f"set columns={view['columns']} lines={view['visible_lines'] + 4}",
                f"{wrapping} nonumber norelativenumber foldcolumn=0",
                "set noshowcmd noruler scrolloff=0 scrolljump=1",
                f"resize {view['visible_lines']}",
                f"call cursor({view['line']}, {view['column']})",
                "call winrestview({'lnum': %d, 'col': %d, 'topline': %d, 'leftcol': %d})"
                % (view['line'], view['column'] - 1, view['first_visible'],
                   view.get('leftcol', 0)),
                "redraw!", f"let before = {snapshot}",
                'execute "normal! " . "%s"' % oracle.vim_keys(oracle.normal_keys(keys)),
                "redraw!", f"let after = {snapshot}",
                "call writefile([json_encode({'before': before, 'after': after})], 'out.json')",
                "qa!"]
    return "\n".join(commands) + "\n"


def run(work: Path, fixture: dict, keys: str) -> dict:
    work.mkdir(parents=True, exist_ok=False)
    (work / "input.txt").write_bytes(fixture['text'].encode('utf-8'))
    (work / "probe.vim").write_text(script(fixture, keys), encoding="utf-8", newline="\n")
    command = [str(oracle.VIM), "-u", "NONE", "-i", "NONE", "-N", "-n", "--not-a-term",
               "-S", "probe.vim", "input.txt"]
    completed = subprocess.run(command, cwd=work, capture_output=True,
                               timeout=oracle.ORACLE_TIMEOUT_SECONDS,
                               creationflags=subprocess.CREATE_NO_WINDOW)
    (work / "stdout.bin").write_bytes(completed.stdout)
    (work / "stderr.bin").write_bytes(completed.stderr)
    (work / "command.json").write_text(json.dumps({"command": command,
                                                  "exit": completed.returncode}, indent=2),
                                       encoding="utf-8")
    if completed.returncode != 0:
        raise RuntimeError(f"{fixture['name']}: Vim exited {completed.returncode}")
    result = json.loads((work / "out.json").read_bytes())
    before, view = result["before"], fixture["viewport"]
    for result_key, input_key in (("width", "columns"), ("height", "visible_lines"),
                                  ("line", "line"), ("column", "column"),
                                  ("first", "first_visible")):
        if before[result_key] != view[input_key]:
            raise RuntimeError(f"{fixture['name']}: invalid initial {input_key}: {before}")
    if before["skipcol"] != 0:
        raise RuntimeError(f"{fixture['name']}: initial line is partly hidden")
    if before["leftcol"] != view.get("leftcol", 0):
        raise RuntimeError(f"{fixture['name']}: invalid initial leftcol: {before}")
    return result["after"]


def measured(work: Path, fixture: dict) -> dict:
    result = run(work / "normal", fixture, fixture["keys"])
    settled = run(work / "escape", fixture, fixture["keys"] + "<Esc>")
    if settled != result:
        raise RuntimeError(f"{fixture['name']}: keys did not finish in NORMAL")
    return {"input": fixture, "result": result}


def row(record: dict) -> str:
    fixture, result = record["input"], record["result"]
    view = fixture["viewport"]
    if result["skipcol"] % view["columns"] != 0:
        raise RuntimeError(f"{fixture['name']}: fractional hidden screen line")
    if "leftcol" in view:
        viewport = ("VimViewportFixture{%d, %d, %d, %d, %d, %d, 0, 0, %d, %d, %d}" %
                    (view['visible_lines'], view['first_visible'], view['line'], view['column'],
                     result['first'], result['scroll'], view['columns'], view['leftcol'],
                     result['leftcol']))
    else:
        viewport = ("VimViewportFixture{%d, %d, %d, %d, %d, %d, %d, %d}" %
                    (view['visible_lines'], view['first_visible'], view['line'], view['column'],
                     result['first'], result['scroll'], view['columns'],
                     result['skipcol'] // view['columns']))
    values = [oracle.literal(fixture[key]) for key in ('name', 'text', 'keys')]
    values += [oracle.literal(result['text']), str(result['line']), str(result['column']),
               oracle.literal(result['register']), oracle.literal(result['register_kind']), viewport]
    return "    {" + ", ".join(values) + "},\n"


def header(records: list[dict], version: str) -> str:
    generated = ("// Generated by python eng/vim-wrap-oracle.py --regenerate (ADR 0114).\n"
                 f"// oracle: {oracle.VIM} - {version}\n\n" + provenance() +
                 '#pragma once\n\n#include "VimFixture.hpp"\n\n#include <array>\n\n'
                 'namespace nenenib::tests\n{\n' +
                 f'constexpr std::array<VimFixture, {len(records)}> vim_wrap_fixtures{{{{\n' +
                 ''.join(row(record) for record in records) +
                 '}};\n} // namespace nenenib::tests\n')
    formatted = subprocess.run(["clang-format", "--style=file", f"--assume-filename={HEADER}"],
                               input=generated, capture_output=True, cwd=ROOT, check=True,
                               text=True, encoding="utf-8")
    return formatted.stdout


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--regenerate", action="store_true")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if not args.regenerate:
        content = HEADER.read_text(encoding="utf-8")
        if provenance() not in content:
            raise RuntimeError("wrapped fixtures have stale input or measurement provenance")
        print("Wrapped Vim fixtures: provenance matches")
        return 0
    if args.output is None or args.output.resolve().drive.lower() != "d:":
        raise ValueError("--output must name a new evidence directory on D:")
    args.output.mkdir(parents=True, exist_ok=False)
    fixtures = json.loads(SOURCE.read_bytes())
    names = [fixture['name'] for fixture in fixtures]
    if len(set(names)) != len(names):
        raise ValueError("fixture names must be unique")
    if any('/' in name or '\\' in name or '..' in name for name in names):
        raise ValueError("fixture names must be simple directory names")
    if any(fixture['text'].endswith('\n') for fixture in fixtures):
        raise ValueError("fixture text must not end with a newline")
    version = subprocess.run([str(oracle.VIM), "--version"], capture_output=True, check=True,
                             text=True, creationflags=subprocess.CREATE_NO_WINDOW).stdout.splitlines()[0]
    records = [measured(args.output / fixture['name'], fixture) for fixture in fixtures]
    (args.output / "results.json").write_text(json.dumps(records, ensure_ascii=False, indent=2),
                                             encoding="utf-8", newline="\n")
    HEADER.write_text(header(records, version), encoding="utf-8", newline="\n")
    print(f"Wrapped Vim fixtures: {len(records)} measured twice; {HEADER}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
