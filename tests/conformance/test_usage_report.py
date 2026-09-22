"""eng/usage-report.py の数え方だけを jsonl の断片で回す（Issue #146）。

実機の transcript（~/.claude）は読まない。断片は一時ディレクトリに書いて collect へ渡す。
鍵の形は probe-146 が実物から拾ったもの（type / isSidechain / agentId / timestamp / message.id /
message.model / message.usage）。本文の鍵（content）は断片に入れない。
"""

from datetime import date, timedelta, timezone
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("usage_report", ROOT / "eng/usage-report.py")
USAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(USAGE)

JST = timezone(timedelta(hours=9))


def assistant(message_id, output, stamp="2026-09-22T03:00:00.000Z", model="claude-opus-5", **row):
    usage = {"input_tokens": 3, "cache_read_input_tokens": 1000, "cache_creation_input_tokens": 200,
             "output_tokens": output}
    line = {"type": "assistant", "timestamp": stamp,
            "message": {"id": message_id, "model": model, "usage": usage}}
    line.update(row)
    return json.dumps(line)


def run(files, since=None, until=None):
    with tempfile.TemporaryDirectory() as directory:
        for name, lines in files.items():
            path = Path(directory) / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("\n".join(lines) + "\n", encoding="utf-8")
        return USAGE.collect(Path(directory), since, until, JST)


class Turns(unittest.TestCase):
    def test_streaming_pieces_of_one_id_are_one_turn_with_the_last_output(self):
        report = run({"86c7704f-aaaa.jsonl": [assistant("msg_1", 8), assistant("msg_1", 5),
                                              assistant("msg_1", 364)]})
        seat = report["seats"][0]
        self.assertEqual(seat["turns"], 1)
        self.assertEqual(seat["output"], 364)
        self.assertEqual(seat["cache_read"], 1000)
        self.assertEqual(seat["max_context"], 1203)
        self.assertEqual(seat["seat_tokens"], 3 + 200 + 364)

    def test_the_synthetic_model_is_not_a_turn(self):
        report = run({"86c7704f-aaaa.jsonl": [assistant("msg_1", 10),
                                              assistant("msg_2", 0, model="<synthetic>")]})
        self.assertEqual(report["seats"][0]["turns"], 1)
        self.assertEqual(list(report["byModel"]), ["claude-opus-5"])


class Seats(unittest.TestCase):
    def test_a_sidechain_file_is_a_background_seat_and_the_main_line_is_named_by_the_session(self):
        report = run({
            "86c7704f-0ecf-4314.jsonl": [assistant("msg_1", 10, isSidechain=False)],
            "86c7704f-0ecf-4314/subagents/agent-ac78e56b67c3e2523.jsonl":
                [assistant("msg_2", 20, stamp="2026-09-22T04:00:00.000Z", isSidechain=True,
                           agentId="ac78e56b67c3e2523")],
        })
        kinds = [(s["seat"], s["kind"]) for s in report["seats"]]
        self.assertEqual(kinds, [("86c7704f", "main"), ("agent-ac78e56b", "background")])


class Skipped(unittest.TestCase):
    def test_a_broken_line_and_an_assistant_line_without_usage_are_counted_as_skipped(self):
        no_usage = json.dumps({"type": "assistant", "timestamp": "2026-09-22T03:00:00.000Z",
                               "message": {"id": "msg_9", "model": "claude-opus-5"}})
        report = run({"86c7704f-aaaa.jsonl": [assistant("msg_1", 10), "{not json", no_usage]})
        self.assertEqual(report["seats"][0]["turns"], 1)
        self.assertEqual(report["skipped"], 2)


class Since(unittest.TestCase):
    def test_since_compares_the_local_date(self):
        # 2026-09-21T16:31Z is 2026-09-22 01:31 in JST: in range. 2026-09-21T14:00Z is 23:00 on 09-21.
        report = run({"dc0aed1f-aaaa.jsonl": [assistant("msg_1", 10, stamp="2026-09-21T14:00:00.000Z"),
                                              assistant("msg_2", 20, stamp="2026-09-21T16:31:00.000Z")]},
                     since=date(2026, 9, 22))
        seat = report["seats"][0]
        self.assertEqual((seat["turns"], seat["output"]), (1, 20))
        self.assertEqual(seat["start"], "2026-09-21T16:31:00.000Z")
        self.assertEqual(report["since"], "2026-09-22")

    def test_a_file_with_nothing_in_range_is_not_a_seat(self):
        report = run({"dc0aed1f-aaaa.jsonl": [assistant("msg_1", 10, stamp="2026-09-20T03:00:00.000Z")]},
                     since=date(2026, 9, 22))
        self.assertEqual(report["seats"], [])


if __name__ == "__main__":
    unittest.main()
