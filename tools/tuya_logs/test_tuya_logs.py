"""Tests for tuya_logs.py. Standard library only:

    py -m unittest discover -s tools\\tuya_logs -v

Nothing here talks to the real cloud. A fake one (FakeTuya) answers the way
the real one did for this product on 2026-11-07, and checks the signature of
every request it gets.
"""

import datetime as dt
import io
import json
import tempfile
import unittest
import urllib.parse
from pathlib import Path

import tuya_logs as tl


def ms(text):
    """A local time as Tuya's 13-digit milliseconds."""
    return int(dt.datetime.strptime(text, "%Y-%m-%d %H:%M:%S").timestamp() * 1000)


class SignTest(unittest.TestCase):
    # Tuya's two worked examples, from
    # https://developer.tuya.com/en/docs/iot/new-singnature?id=Kbw0q34cs2e5g
    # Both sign two custom headers, area_id and call_id.
    CLIENT_ID = "1KAD46OrT9HafiKdsXeg"
    SECRET    = "4OHBOnWOqaEC1mWXOpVL3yV50s0qGSRC"
    T         = "1588925778000"
    NONCE     = "5138cc3a9033d69856923fd07b491173"
    HEADERS   = (("area_id", "29a33e8796834b1efa6"), ("call_id", "8afdb70ab2ed11eb85290242ac130003"))

    EXAMPLES = [
        # url                                              access_token                        sign
        ("/v1.0/token?grant_type=1",                       "",                                 "9E48A3E93B302EEECC803C7241985D0A34EB944F40FB573C7B5C2A82158AF13E"),
        ("/v2.0/apps/schema/users?page_no=1&page_size=50", "3f4eda2bdec17232f67c0b188af3eec1", "AE4481C692AA80B25F3A7E12C3A5FD9BBF6251539DD78E565A1A72A508A88784"),
    ]

    def test_tuya_examples(self):
        for url, token, expected in self.EXAMPLES:
            with self.subTest(url=url):
                self.assertEqual(tl.sign(self.CLIENT_ID, self.SECRET, self.T, "GET", url, token,
                                         nonce=self.NONCE, signed_headers=self.HEADERS), expected)


class PathWithQueryTest(unittest.TestCase):
    TABLE = [
        # params                                                   path + query
        ({},                                                       "/p"),
        ({"grant_type": 1},                                        "/p?grant_type=1"),
        ({"size": 100, "end_time": 2, "codes": "a,b", "start_time": 1},
                                                                   "/p?codes=a,b&end_time=2&size=100&start_time=1"),
        ({"last_row_key": "", "size": 20},                         "/p?size=20"),        # empty left out
        ({"last_row_key": None, "size": 20},                       "/p?size=20"),
        ({"k": "a b&c"},                                           "/p?k=a%20b%26c"),    # escaped, commas kept
    ]

    def test_table(self):
        for params, expected in self.TABLE:
            with self.subTest(params=params):
                self.assertEqual(tl.path_with_query("/p", params), expected)


class ShowTest(unittest.TestCase):
    TABLE = [
        # column     value     shown
        ("temp C",  "213",    "21.3"),
        ("temp C",  "-45",    "-4.5"),
        ("temp C",  "x",      "x"),
        ("damaged", "true",   "on"),
        ("damaged", "false",  "off"),
        ("state",   "middle", "middle"),
        ("batt %",  "76",     "76"),
        ("min mV",  "",       "."),
        ("avg mV",  None,     "."),
    ]

    def test_table(self):
        for label, value, expected in self.TABLE:
            with self.subTest(label=label, value=value):
                self.assertEqual(tl.show(label, value), expected)


class ReportCellsTest(unittest.TestCase):
    # Columns:           min mV  avg mV  batt %  state   damaged  temp C  rh %  diag
    TABLE = [
        # DP instruction mode: the product's identifiers, DP 101's misspelled.
        ({"batter_min_voltage": "391", "battery_avg_min_voltage": "1283", "battery_percentage": "83",
          "battery_state": "high", "battery_damaged": "false", "temp_current": "201", "humidity_value": "58"},
                             ["391", "1283", "83",   "high", "off",   "20.1", "58", "."]),
        # Standard instruction mode: Tuya's names, no 101-104.
        ({"battery_percentage": "83", "battery_state": "high", "va_temperature": "201", "va_humidity": "58"},
                             [".",   ".",    "83",   "high", ".",     "20.1", "58", "."]),
        # The spelling in tuya/protocol.h, should the platform's ever be fixed.
        ({"battery_min_voltage": "400"},
                             ["400", ".",    ".",    ".",    ".",     ".",    ".",  "."]),
        # Both names in one report: the product's wins.
        ({"temp_current": "201", "va_temperature": "199"},
                             [".",   ".",    ".",    ".",    ".",     "20.1", ".",  "."]),
        # DP 104, as the firmware words it, passed through unchanged.
        ({"temp_current": "224", "diagnostics": "r=pin c=108 w=4 u=0 m=02 s=0"},
                             [".",   ".",    ".",    ".",    ".",     "22.4", ".",  "r=pin c=108 w=4 u=0 m=02 s=0"]),
    ]

    def test_table(self):
        for values, expected in self.TABLE:
            with self.subTest(values=values):
                self.assertEqual(tl.report_cells(values), expected)


class GroupReportsTest(unittest.TestCase):
    def test_rows_closer_than_two_minutes_are_one_report(self):
        rows = [
            # time                                code                  value
            (ms("2026-11-06 21:21:05"), "battery_percentage", "76"),
            (ms("2026-11-06 21:21:07"), "battery_damaged",    "false"),
            (ms("2026-11-06 21:22:50"), "battery_percentage", "75"),     # 1:43 later: same report, last wins
            (ms("2026-11-06 21:35:00"), "battery_percentage", "0"),      # a new report
            (ms("2026-11-06 21:37:01"), "battery_damaged",    "true"),   # 2:01 later: a new report
        ]
        self.assertEqual(tl.group_reports(list(reversed(rows))), [
            (ms("2026-11-06 21:21:05"), {"battery_percentage": "75", "battery_damaged": "false"}),
            (ms("2026-11-06 21:35:00"), {"battery_percentage": "0"}),
            (ms("2026-11-06 21:37:01"), {"battery_damaged": "true"}),
        ])


# The product's DPs, as the cloud names them in each control instruction mode,
# as the real project showed them on 2026-11-07. None: not shown in that mode.
DP_CODES = {
    #  dp   DP instruction               standard instruction
    1:   ("temp_current",            "va_temperature"),
    2:   ("humidity_value",          "va_humidity"),
    3:   ("battery_state",           "battery_state"),
    4:   ("battery_percentage",      "battery_percentage"),
    101: ("batter_min_voltage",      None),
    102: ("battery_damaged",         None),
    103: ("battery_avg_min_voltage", None),
}


class FakeTuya:
    """Answers like Tuya's OpenAPI, from a list of DP reports per device.
    Checks every request's signature, and records the paths called.

    As the real one did: the v2 log endpoint refuses a code the instruction
    mode does not show ("Parameter error!"), the iot-03 one skips it, and the
    v1 one takes no codes and returns everything the mode shows.
    """

    def __init__(self, logs, mode="dp", denied=(), expired=False):
        self.logs = logs            # {device_id: [(ms, dp, value)]}
        self.mode = mode            # "dp" or "standard"
        self.denied = set(denied)   # LOG_APIS names, or "spec", that answer "permission deny"
        self.expired = expired      # the project's IoT Core trial has run out
        self.calls = []

    def code(self, dp):
        return DP_CODES[dp][0 if self.mode == "dp" else 1]

    def mode_codes(self):
        return [self.code(dp) for dp in DP_CODES if self.code(dp)]

    def __call__(self, url, headers):
        parts = urllib.parse.urlsplit(url)
        path_and_query = parts.path + (f"?{parts.query}" if parts.query else "")
        expected = tl.sign("id", "secret", headers["t"], "GET", path_and_query, headers.get("access_token", ""))
        if headers["sign"] != expected:
            return {"success": False, "code": 1004, "msg": "sign invalid"}

        query = dict(urllib.parse.parse_qsl(parts.query))
        segments = parts.path.strip("/").split("/")
        self.calls.append(parts.path)

        if parts.path == "/v1.0/token":
            return {"success": True, "result": {"access_token": "tok", "expire_time": 7200}}
        if headers.get("access_token") != "tok":
            return {"success": False, "code": 1010, "msg": "token invalid"}
        if self.expired:
            return {"success": False, "code": 28841002, "msg": "IoT Core service subscription has expired."}

        if segments[-1] == "specification":
            if "spec" in self.denied:
                return {"success": False, "code": 1106, "msg": "permission deny"}
            return {"success": True, "result": {"category": "wsdcg",
                                                "status": [{"code": c} for c in self.mode_codes()]}}

        if segments[:3] == ["v2.0", "cloud", "thing"]:
            api, device = "v2", segments[3]
        elif segments[:3] == ["v1.0", "iot-03", "devices"]:
            api, device = "iot03", segments[3]
        else:
            api, device = "v1", segments[2]
        if api in self.denied:
            return {"success": False, "code": 1106, "msg": "permission deny"}

        wanted = set(self.mode_codes())
        if api != "v1":
            asked = set(query["codes"].split(","))
            if api == "v2" and not asked <= wanted:
                return {"success": False, "code": 40000303, "msg": "Parameter error!"}
            wanted &= asked

        start, end, size = int(query["start_time"]), int(query["end_time"]), int(query["size"])
        matching = [(t, self.code(dp), v) for t, dp, v in sorted(self.logs.get(device, []))
                    if start <= t <= end and self.code(dp) in wanted]
        first = int(query.get("last_row_key") or query.get("start_row_key") or 0)
        page = matching[first:first + size]
        more = first + size < len(matching)
        logs = [{"code": c, "value": v, "event_time": t} for t, c, v in page]
        if api == "v1":
            return {"success": True, "result": {"logs": logs, "has_next": more,
                                                "next_row_key": str(first + size) if more else None}}
        return {"success": True, "result": {"logs": logs, "has_more": more,
                                            "last_row_key": str(first + size)}}


def report(time, min_mv, avg_mv, pct, state, damaged):
    """One report's DPs, the way the module sends them: a second apart."""
    t = ms(time)
    return [(t,        101, str(min_mv)),
            (t + 1000, 103, str(avg_mv)),
            (t + 2000, 4,   str(pct)),
            (t + 3000, 3,   state),
            (t + 4000, 102, damaged),
            (t + 5000, 1,   "213"),
            (t + 6000, 2,   "48")]


class RunTest(unittest.TestCase):
    def setUp(self):
        self.dir = Path(tempfile.mkdtemp())
        self.config = self.dir / "config.json"
        self.config.write_text(json.dumps({
            "endpoint": "https://openapi.example", "access_id": "id", "access_secret": "secret",
            "devices": {"front": "dev1"}}))

    def run_at(self, now, fake, *args):
        out = io.StringIO()
        code = tl.run(["--config", str(self.config), "--out", str(self.dir), *args],
                      transport=fake, clock=lambda: ms(now) / 1000, stdout=out)
        return code, out.getvalue()

    def test_first_run_then_an_incremental_one(self):
        logs = (report("2026-11-06 21:21:05", 391, 697, 76, "high", "false") +
                report("2026-11-06 21:35:10", 386, 687,  0, "low",  "true"))
        fake = FakeTuya({"dev1": logs}, denied={"v2"})   # this project may only call iot-03

        code, out = self.run_at("2026-11-07 08:00:00", fake)

        self.assertEqual(code, 0)
        self.assertIn("front: 2 new report(s); last report 2026-11-06 21:35, 10.4 h ago\n", out)
        self.assertIn(
            "  time               min mV   avg mV   batt %    state  damaged   temp C     rh %     diag\n"
            "  2026-11-06 21:21      391      697       76     high      off     21.3       48        .\n"
            "  2026-11-06 21:35      386      687        0      low       on     21.3       48        .\n", out)
        self.assertNotIn("WARNING", out)
        self.assertEqual((self.dir / "front_reports.csv").read_text(),
            "time,min mV,avg mV,batt %,state,damaged,temp C,rh %,diag\n"
            "2026-11-06 21:21,391,697,76,high,off,21.3,48,.\n"
            "2026-11-06 21:35,386,687,0,low,on,21.3,48,.\n")
        self.assertEqual(len(tl.read_raw(self.dir / "front_raw.csv")), 14)
        # v2 was refused once, then iot-03 was used first for every later day.
        self.assertEqual(sum(p.startswith("/v2.0/") for p in fake.calls), 1)

        # Three days later: one more report. Only it is fetched and printed.
        fake.logs["dev1"] += report("2026-11-09 10:00:00", 402, 1041, 98, "high", "false")
        code, out = self.run_at("2026-11-10 08:00:00", fake)

        self.assertIn("front: 1 new report(s); last report 2026-11-09 10:00, 22.0 h ago\n", out)
        self.assertNotIn("2026-11-06 21:21", out)
        self.assertIn("  2026-11-09 10:00      402     1041       98     high      off     21.3       48        .\n", out)
        self.assertEqual(len(tl.read_raw(self.dir / "front_raw.csv")), 21)   # nothing stored twice
        self.assertEqual((self.dir / "front_reports.csv").read_text().count("\n"), 4)

    def test_standard_instruction_mode(self):
        # The project's default: temperature and humidity under Tuya's names,
        # no DPs 101-103, and a warning that says how to get them.
        fake = FakeTuya({"dev1": report("2026-11-07 10:00:00", 391, 1283, 83, "high", "false")}, mode="standard")
        code, out = self.run_at("2026-11-07 12:00:00", fake)

        self.assertEqual(code, 0)
        self.assertIn("  WARNING: DPs 101-103 are not visible to this cloud project", out)
        self.assertIn("  2026-11-07 10:00        .        .       83     high        .     21.3       48        .\n", out)
        self.assertNotIn("/v1.0/iot-03/devices/dev1/report-logs", fake.calls)   # v2 accepted the codes

    def test_specification_unreadable(self):
        # Without the device's codes, every known name is asked for. v2 refuses
        # the ones the mode does not show; iot-03 answers with the rest.
        fake = FakeTuya({"dev1": report("2026-11-07 10:00:00", 391, 1283, 83, "high", "false")},
                        mode="standard", denied={"spec"})
        code, out = self.run_at("2026-11-07 12:00:00", fake)

        self.assertEqual(code, 0)
        self.assertIn("  2026-11-07 10:00        .        .       83     high        .     21.3       48        .\n", out)
        self.assertNotIn("WARNING", out)   # cannot tell which mode it is

    def test_paging(self):
        # A report every 5 minutes: 288 inside the day asked for, so three
        # pages of 100 from the v2 endpoint.
        start = ms("2026-11-07 00:00:00")
        rows = [(start + i * 5 * 60 * 1000, 4, str(i % 100)) for i in range(400)]
        fake = FakeTuya({"dev1": rows})
        code, out = self.run_at("2026-11-07 23:59:00", fake, "--days", "1")

        self.assertEqual(len(tl.read_raw(self.dir / "front_raw.csv")), 288)
        self.assertIn("front: 288 new report(s)", out)
        self.assertEqual(sum(p.startswith("/v2.0/") for p in fake.calls), 3)

    def test_v1_endpoint_paging(self):
        start = ms("2026-11-07 00:00:00")
        rows = [(start + i * 1000, 4, "50") for i in range(150)]
        fake = FakeTuya({"dev1": rows}, denied={"v2", "iot03"})
        self.run_at("2026-11-07 12:00:00", fake, "--days", "1")

        self.assertEqual(len(tl.read_raw(self.dir / "front_raw.csv")), 150)

    def test_a_gap_longer_than_retention_is_flagged(self):
        fake = FakeTuya({"dev1": report("2026-11-01 10:00:00", 400, 1000, 90, "high", "false")})
        self.run_at("2026-11-01 12:00:00", fake)
        code, out = self.run_at("2026-11-09 12:00:00", fake)

        self.assertIn("WARNING: last stored row is from 2026-11-01 10:00:06", out)

    def test_no_warning_within_retention(self):
        fake = FakeTuya({"dev1": report("2026-11-01 10:00:00", 400, 1000, 90, "high", "false")})
        self.run_at("2026-11-01 12:00:00", fake)
        code, out = self.run_at("2026-11-08 10:00:00", fake)   # just under 7 days

        self.assertNotIn("WARNING", out)

    def test_no_endpoint_answers(self):
        fake = FakeTuya({"dev1": []}, denied={"v2", "iot03", "v1"})
        code, out = self.run_at("2026-11-07 12:00:00", fake)

        self.assertEqual(code, 1)
        self.assertIn("no log endpoint answered", out)
        self.assertIn("is the device linked to this cloud project", out)

    def test_an_expired_iot_core_stops_the_run_at_the_first_request(self):
        # Every endpoint and device would say the same, so nothing else is tried.
        fake = FakeTuya({"dev1": []}, expired=True)
        with self.assertRaisesRegex(tl.TuyaError, "(?s)28841002.*Cloud > Cloud Services > IoT Core"):
            self.run_at("2026-11-07 12:00:00", fake)
        self.assertEqual(fake.calls, ["/v1.0/token", "/v1.0/iot-03/devices/dev1/specification"])

    def test_a_bad_secret_stops_the_run(self):
        self.config.write_text(json.dumps({
            "endpoint": "https://openapi.example", "access_id": "id", "access_secret": "wrong",
            "devices": {"front": "dev1"}}))
        with self.assertRaisesRegex(tl.TuyaError, "(?s)1004.*check access_secret"):
            self.run_at("2026-11-07 12:00:00", FakeTuya({}))


if __name__ == "__main__":
    unittest.main()
