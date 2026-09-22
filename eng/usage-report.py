"""Count the usage of each seat from the Claude Code transcripts (Issue #146).

ADR 0038 decision 5 and ADR 0039 decision 6 ("measure, then say"): the size of a seat is read
from the transcript, not remembered. This script only reads ~/.claude/projects/<dir>/**.jsonl
and prints keys and numbers. It never prints message text and never writes under ~/.claude.

  python eng/usage-report.py [--project <dir>] [--since YYYY-MM-DD] [--until YYYY-MM-DD]
                             [--json <path>]

A seat is one jsonl file. A file whose rows carry isSidechain true or an agentId is a background
seat named agent-<first 8 of the id>; otherwise it is the main line (the design seat), named by
the first 8 characters of the session id (the file name). A turn is one assistant message.id:
the streaming pieces of one id are one turn, output_tokens is the last value of the id and
input / cache_read / cache_creation are the first (they do not change within an id). The model
<synthetic> is not a turn. A line that is not JSON and an assistant line without usage are not
counted; their number is "skipped". --since / --until compare the local date of each row's
timestamp (the day the owner lives in, JST on the owner's machine).

seat_tokens = input + cache_creation + output, the scale of subagent_tokens in the Agent
completion notice (cache_read is left out). max_context is the largest input + cache_read +
cache_creation of a single turn. No cost conversion and no weekly limit estimate.
"""

import argparse
from datetime import date, datetime
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_PROJECT = Path.home() / ".claude" / "projects" / "C--Users-info-WORKS-NeNeNib"
SYNTHETIC = "<synthetic>"
TOTAL_FIELDS = ("turns", "cache_read", "output", "seat_tokens")


def parse_time(stamp):
    return datetime.fromisoformat(stamp.replace("Z", "+00:00"))


def in_range(stamp, since, until, tz):
    if since is None and until is None:
        return True
    if not isinstance(stamp, str):
        return False
    day = parse_time(stamp).astimezone(tz).date()
    return (since is None or day >= since) and (until is None or day <= until)


def read_rows(lines):
    """Yield the JSON objects of a transcript and count the lines that are not one."""
    counter = {"skipped": 0}

    def rows():
        for raw in lines:
            if not raw.strip():
                continue
            try:
                row = json.loads(raw)
            except json.JSONDecodeError:
                counter["skipped"] += 1
                continue
            if isinstance(row, dict):
                yield row
            else:
                counter["skipped"] += 1

    return rows(), counter


def turns_of(rows, since, until, tz, counter):
    """Group the assistant rows in range by message.id. Returns (turns in order, stamps, agent)."""
    turns = {}
    stamps = []
    agent = {"background": False, "id": None}
    for row in rows:
        if row.get("isSidechain") is True or row.get("agentId"):
            agent["background"] = True
            agent["id"] = agent["id"] or row.get("agentId")
        stamp = row.get("timestamp")
        if not in_range(stamp, since, until, tz):
            continue
        if isinstance(stamp, str):
            stamps.append(stamp)
        if row.get("type") != "assistant":
            continue
        message = row.get("message")
        usage = message.get("usage") if isinstance(message, dict) else None
        if not isinstance(usage, dict):
            counter["skipped"] += 1
            continue
        if message.get("model") == SYNTHETIC:
            continue
        key = message.get("id") or f"line-{len(turns)}"
        if key not in turns:
            turns[key] = {
                "model": message.get("model") or "?",
                "input": int(usage.get("input_tokens") or 0),
                "cache_read": int(usage.get("cache_read_input_tokens") or 0),
                "cache_creation": int(usage.get("cache_creation_input_tokens") or 0),
            }
        turns[key]["output"] = int(usage.get("output_tokens") or 0)
    return list(turns.values()), stamps, agent


def seat_name(file_stem, agent):
    if agent["background"]:
        return "agent-" + (agent["id"] or file_stem.removeprefix("agent-"))[:8]
    return file_stem[:8]


def summarize_lines(lines, file_stem, since=None, until=None, tz=None):
    """Summarize one transcript (an iterable of raw lines). None when no row is in range."""
    rows, counter = read_rows(lines)
    values, stamps, agent = turns_of(rows, since, until, tz, counter)
    if not stamps and not values:
        return None
    models = list(dict.fromkeys(t["model"] for t in values))
    stamps.sort(key=parse_time)
    start = stamps[0] if stamps else None
    end = stamps[-1] if stamps else None
    minutes = round((parse_time(end) - parse_time(start)).total_seconds() / 60, 1) if stamps else 0
    return {
        "seat": seat_name(file_stem, agent),
        "kind": "background" if agent["background"] else "main",
        "model": "/".join(models),
        "turns": len(values),
        "max_context": max((t["input"] + t["cache_read"] + t["cache_creation"] for t in values), default=0),
        "cache_read": sum(t["cache_read"] for t in values),
        "cache_creation": sum(t["cache_creation"] for t in values),
        "input": sum(t["input"] for t in values),
        "output": sum(t["output"] for t in values),
        "seat_tokens": sum(t["input"] + t["cache_creation"] + t["output"] for t in values),
        "start": start,
        "end": end,
        "minutes": minutes,
        "skipped": counter["skipped"],
        "byModel": by_model_of(values),
    }


def by_model_of(values):
    result = {}
    for turn in values:
        entry = result.setdefault(turn["model"], dict.fromkeys(TOTAL_FIELDS, 0))
        entry["turns"] += 1
        entry["cache_read"] += turn["cache_read"]
        entry["output"] += turn["output"]
        entry["seat_tokens"] += turn["input"] + turn["cache_creation"] + turn["output"]
    return result


def build_report(project, seats, since=None, until=None):
    """Order the seats by start time and add the per-model totals (seats lose their byModel)."""
    seats = sorted(seats, key=lambda s: (s["start"] is None, parse_time(s["start"]) if s["start"] else 0, s["seat"]))
    by_model = {}
    for seat in seats:
        for model, entry in seat.pop("byModel").items():
            total = by_model.setdefault(model, dict.fromkeys(TOTAL_FIELDS, 0))
            for field, value in entry.items():
                total[field] += value
    return {
        "project": str(project),
        "since": since.isoformat() if since else None,
        "until": until.isoformat() if until else None,
        "skipped": sum(s["skipped"] for s in seats),
        "seats": seats,
        "byModel": by_model,
    }


def collect(project, since=None, until=None, tz=None):
    seats = []
    for path in sorted(Path(project).rglob("*.jsonl")):
        with path.open(encoding="utf-8", errors="replace") as handle:
            seat = summarize_lines(handle, path.stem, since, until, tz)
        if seat is not None:
            seats.append(seat)
    return build_report(project, seats, since, until)


def local_minute(stamp, tz):
    return parse_time(stamp).astimezone(tz).strftime("%m-%d %H:%M") if stamp else "-"


def seat_row(s, tz):
    numbers = [f"{s[field]:,}" for field in ("turns", "max_context", "cache_read", "cache_creation",
                                               "input", "output", "seat_tokens")]
    return (s["seat"], s["kind"], s["model"], *numbers, local_minute(s["start"], tz),
            local_minute(s["end"], tz), f"{s['minutes']}")


def render(report, tz=None):
    head = ("seat", "kind", "model", "turns", "max_context", "cache_read", "cache_creation",
            "input", "output", "seat_tokens", "start", "end", "minutes")
    rows = [head] + [seat_row(s, tz) for s in report["seats"]]
    model_rows = [("model", *TOTAL_FIELDS)]
    for model, total in sorted(report["byModel"].items()):
        model_rows.append((model, *(f"{total[field]:,}" for field in TOTAL_FIELDS)))
    summary = (f"Usage: seats {len(report['seats'])} / skipped {report['skipped']}"
               f" / since {report['since'] or '-'} / until {report['until'] or '-'}")
    return "\n".join([format_rows(rows, 3), "", format_rows(model_rows, 1), "", summary])


def format_rows(rows, text_columns):
    widths = [max(len(row[i]) for row in rows) for i in range(len(rows[0]))]
    lines = []
    for row in rows:
        cells = [c.ljust(w) if i < text_columns else c.rjust(w) for i, (c, w) in enumerate(zip(row, widths))]
        lines.append("  ".join(cells).rstrip())
    return "\n".join(lines)


def parse_day(text):
    try:
        return date.fromisoformat(text)
    except ValueError as error:
        raise argparse.ArgumentTypeError(f"not YYYY-MM-DD: {text}") from error


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--project", type=Path, default=DEFAULT_PROJECT)
    parser.add_argument("--since", type=parse_day)
    parser.add_argument("--until", type=parse_day)
    parser.add_argument("--json", type=Path)
    args = parser.parse_args(argv)
    if not args.project.is_dir():
        print(f"Usage: error project not found: {args.project}", file=sys.stderr)
        return 2
    tz = datetime.now().astimezone().tzinfo
    report = collect(args.project, args.since, args.until, tz)
    print(render(report, tz))
    target = args.json or ROOT / "out" / "usage" / f"{(args.since or date.today()).isoformat()}.json"
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"Usage: wrote {target}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
