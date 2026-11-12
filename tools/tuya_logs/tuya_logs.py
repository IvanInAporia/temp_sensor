#!/usr/bin/env python3
"""Fetch the sensors' data-point reports from the Tuya cloud and keep them.

Run it every few days -- the free tier keeps only about 7 days of logs:

    py tools\\tuya_logs\\tuya_logs.py

For each device in config.json it fetches every DP report since the last run,
appends them to notes/tuya_logs/<name>_raw.csv, rebuilds <name>_reports.csv
(one row per report, the way the notes list them) and prints the new reports.

Standard library only. One-time setup is in README.md next to this file.
"""

import argparse
import csv
import datetime as dt
import hashlib
import hmac
import json
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent
DEFAULT_CONFIG = HERE / "config.json"
DEFAULT_OUT = REPO / "notes" / "tuya_logs"

# The product's data points (tuya/protocol.h), in the order the summary shows
# them -- the order the notes have used: min, ave, %, damaged -- each with the
# codes the cloud may log it under. Which ones it uses is set by the cloud
# project's control instruction mode (README, step 6):
#
#  - Standard instruction, the default: Tuya's names for the category (wsdcg),
#    so DPs 1 and 2 are va_temperature and va_humidity, and 101-103 are not
#    logged at all.
#  - DP instruction: the identifiers the product was defined with on the
#    platform. DP 101's is spelled batter_min_voltage there.
COLUMNS = [
    ("min mV",  ("battery_min_voltage", "batter_min_voltage")),  # DP 101
    ("avg mV",  ("battery_avg_min_voltage",)),                   # DP 103
    ("batt %",  ("battery_percentage",)),                        # DP 4
    ("state",   ("battery_state",)),                             # DP 3
    ("damaged", ("battery_damaged",)),                           # DP 102
    ("temp C",  ("temp_current", "va_temperature")),             # DP 1, tenths of a degree
    ("rh %",    ("humidity_value", "va_humidity")),              # DP 2
    ("diag",    ("diagnostics",)),                               # DP 104, only when there is something to say
]
ALL_CODES = [code for _, codes in COLUMNS for code in codes]

# The custom DPs, which the standard instruction set leaves out.
CUSTOM_LABELS = ("min mV", "avg mV", "damaged")
DP_MODE_NOTE = ("DPs 101-103 are not visible to this cloud project: set its control "
                "instruction mode to DP Instruction (README, step 6)")

# The free tier keeps this many days. Older rows are gone for good, so a run
# that finds its last row older than this warns that some may have been lost.
RETENTION_DAYS = 7

# Rows closer together than this belong to one report. A report window lasts
# under a minute and windows start at least 5 minutes apart (CYCLE_PERIOD_MS).
REPORT_GAP_MS = 2 * 60 * 1000

DAY_MS = 24 * 60 * 60 * 1000
PAGE_SIZE = 100
MAX_PAGES = 1000  # per day and device: a guard against a paging loop

# Errors about the project, its credentials or the clock rather than about one
# endpoint or device: every endpoint and every device would get the same, so
# they stop the run at the first one.
_IOT_CORE = ("the project's IoT Core subscription is missing or has expired: extend "
             "the free trial at platform.tuya.com, Cloud > Cloud Services > IoT Core")
PROJECT_ERRORS = {
    1004: "sign invalid: check access_secret, and that the PC clock is right",
    1010: "token invalid",
    1011: "token expired",
    1013: "request time invalid: the PC clock is off",
    2009: "unknown access_id here: check it, and that endpoint is the project's data center",
    28841002: _IOT_CORE,
    28841101: _IOT_CORE,
}

# Hints for errors that may be about one endpoint or one device.
ERROR_HINTS = {
    1106: "permission denied: is the device linked to this cloud project "
          "(Devices > Link App Account), and is the project's data center the "
          "one in config.json?",
}


class TuyaError(Exception):
    def __init__(self, code, msg, where):
        self.code = code
        hint = ERROR_HINTS.get(code) or PROJECT_ERRORS.get(code)
        text = f"{where}: Tuya error {code}: {msg}" if code is not None else f"{where}: {msg}"
        super().__init__(text + (f"\n    ({hint})" if hint else ""))


# --- Signing ------------------------------------------------------------------

def sign(client_id, secret, t, method, url, access_token="", nonce="",
         body=b"", signed_headers=()):
    """Tuya's request signature, as in
    https://developer.tuya.com/en/docs/iot/new-singnature?id=Kbw0q34cs2e5g

    url is the path with its query, keys sorted. signed_headers are the
    (name, value) pairs listed in a Signature-Headers header, if any. Token
    requests leave access_token empty.
    """
    headers_part = "".join(f"{name}:{value}\n" for name, value in signed_headers)
    string_to_sign = "\n".join([method, hashlib.sha256(body).hexdigest(), headers_part, url])
    message = client_id + access_token + t + nonce + string_to_sign
    return hmac.new(secret.encode(), message.encode(), hashlib.sha256).hexdigest().upper()


def path_with_query(path, params):
    """The path and its query, keys sorted as the signature requires. Empty
    values are left out. Commas stay as they are, so the URL sent is exactly
    the one signed."""
    items = sorted((k, str(v)) for k, v in params.items() if v is not None and str(v) != "")
    if not items:
        return path
    return path + "?" + "&".join(f"{k}={urllib.parse.quote(v, safe=',')}" for k, v in items)


# --- Cloud client ---------------------------------------------------------------

def http_get(url, headers):
    """GET a Tuya OpenAPI URL and return the decoded JSON reply."""
    request = urllib.request.Request(url, headers={**headers, "User-Agent": "temp-sensor-tuya-logs/1.0"})
    try:
        with urllib.request.urlopen(request, timeout=30) as response:
            return json.load(response)
    except urllib.error.HTTPError as e:
        body = e.read()
        try:
            return json.loads(body)
        except ValueError:
            raise TuyaError(e.code, f"HTTP {e.code} {body[:200]!r}", url) from None


class TuyaCloud:
    def __init__(self, endpoint, access_id, access_secret, transport=http_get, clock=time.time):
        self.endpoint = endpoint.rstrip("/")
        self.access_id = access_id
        self.access_secret = access_secret
        self.transport = transport
        self.clock = clock
        self.token = ""

    def get(self, path, params=None):
        url = path_with_query(path, params or {})
        t = str(int(self.clock() * 1000))
        headers = {
            "client_id": self.access_id,
            "sign": sign(self.access_id, self.access_secret, t, "GET", url, self.token),
            "t": t,
            "sign_method": "HMAC-SHA256",
        }
        if self.token:
            headers["access_token"] = self.token
        reply = self.transport(self.endpoint + url, headers)
        if not reply.get("success"):
            raise TuyaError(reply.get("code"), reply.get("msg"), path)
        return reply.get("result") or {}

    def connect(self):
        self.token = ""
        self.token = self.get("/v1.0/token", {"grant_type": 1})["access_token"]


# --- The log endpoints ---------------------------------------------------------
#
# Tuya has three endpoints for a device's DP reports. Which ones a project may
# call depends on its subscriptions and its age, so they are tried in order and
# the first that answers is used for the rest of the run.
#
# Two of them take the codes to fetch, and refuse ("Parameter error") a code
# the project's instruction mode does not show. So the codes are the device's
# own, from its specification.

def device_codes(cloud, device_id):
    """The codes this project shows for the device, in its instruction mode.
    Every name in COLUMNS if the specification cannot be read."""
    try:
        spec = cloud.get(f"/v1.0/iot-03/devices/{device_id}/specification")
    except TuyaError as e:
        if e.code in PROJECT_ERRORS:
            raise
        return ALL_CODES
    return [s["code"] for s in spec.get("status") or [] if s.get("code")] or ALL_CODES


def _report_logs(cloud, path, codes, start_ms, end_ms):
    params = {"codes": ",".join(codes), "start_time": start_ms, "end_time": end_ms, "size": PAGE_SIZE}
    for _ in range(MAX_PAGES):
        result = cloud.get(path, params)
        yield from result.get("logs") or result.get("list") or []
        if not result.get("has_more") or not result.get("last_row_key"):
            return
        params["last_row_key"] = result["last_row_key"]


def _logs_v2(cloud, device_id, codes, start_ms, end_ms):
    return _report_logs(cloud, f"/v2.0/cloud/thing/{device_id}/report-logs", codes, start_ms, end_ms)


def _logs_iot03(cloud, device_id, codes, start_ms, end_ms):
    return _report_logs(cloud, f"/v1.0/iot-03/devices/{device_id}/report-logs", codes, start_ms, end_ms)


def _logs_v1(cloud, device_id, codes, start_ms, end_ms):
    # type 7 is "data point report"; the archived v1.0 device log. It takes
    # no codes and returns every DP the instruction mode shows.
    params = {"type": 7, "start_time": start_ms, "end_time": end_ms, "size": PAGE_SIZE}
    for _ in range(MAX_PAGES):
        result = cloud.get(f"/v1.0/devices/{device_id}/logs", params)
        yield from result.get("logs") or []
        if not result.get("has_next") or not result.get("next_row_key"):
            return
        params["start_row_key"] = result["next_row_key"]


LOG_APIS = {"v2": _logs_v2, "iot03": _logs_iot03, "v1": _logs_v1}


def fetch_logs(cloud, device_id, codes, start_ms, end_ms, api_order):
    """All reports of the given codes for one device between two times, a day
    per request.

    api_order is a list of LOG_APIS names. It is reordered in place so the one
    that answered comes first for the next call.
    """
    logs = []
    day_start = start_ms
    while day_start < end_ms:
        day_end = min(day_start + DAY_MS, end_ms)
        errors = []
        for name in list(api_order):
            try:
                logs.extend(LOG_APIS[name](cloud, device_id, codes, day_start, day_end))
            except TuyaError as e:
                if e.code in PROJECT_ERRORS:
                    raise
                errors.append(f"  [{name}] {e}")
                continue
            api_order.remove(name)
            api_order.insert(0, name)
            break
        else:
            raise TuyaError(None, "no log endpoint answered:\n" + "\n".join(errors), device_id)
        day_start = day_end
    return logs


# --- Local storage ----------------------------------------------------------------

RAW_HEADER = ["event_time_ms", "local_time", "code", "value"]


def local_time(ms, seconds=True):
    return dt.datetime.fromtimestamp(ms / 1000).strftime("%Y-%m-%d %H:%M:%S" if seconds else "%Y-%m-%d %H:%M")


def read_raw(path):
    if not path.exists():
        return []
    with path.open(newline="", encoding="utf-8") as f:
        return [(int(r["event_time_ms"]), r["code"], r["value"]) for r in csv.DictReader(f)]


def append_raw(path, rows):
    new_file = not path.exists()
    with path.open("a", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        if new_file:
            writer.writerow(RAW_HEADER)
        for ms, code, value in rows:
            writer.writerow([ms, local_time(ms), code, value])


def to_rows(logs):
    """Tuya's log entries as (event_time_ms, code, value), values as text."""
    rows = []
    for log in logs:
        value = log.get("value")
        if isinstance(value, bool):
            value = "true" if value else "false"
        rows.append((int(log["event_time"]), str(log.get("code", "")), "" if value is None else str(value)))
    return rows


def group_reports(rows):
    """Rows, sorted by time, grouped into reports: [(start_ms, {code: value})].
    A row more than REPORT_GAP_MS after the one before starts a new report.
    Within a report the last value of each code wins."""
    reports = []
    last_ms = None
    for ms, code, value in sorted(rows):
        if last_ms is None or ms - last_ms > REPORT_GAP_MS:
            reports.append((ms, {}))
        reports[-1][1][code] = value
        last_ms = ms
    return reports


def show(label, value):
    """A column's value as the notes write it. Missing reads as '.'."""
    if value is None or value == "":
        return "."
    if label == "temp C":
        try:
            return f"{int(value) / 10:.1f}"
        except ValueError:
            return value
    if label == "damaged":
        return {"true": "on", "false": "off"}.get(value.lower(), value)
    return value


def report_cells(values):
    """One report's columns, each from whichever of its codes the report has."""
    cells = []
    for label, codes in COLUMNS:
        value = next((values[c] for c in codes if c in values), None)
        cells.append(show(label, value))
    return cells


def write_reports(path, reports):
    with path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(["time"] + [label for label, _ in COLUMNS])
        for ms, values in reports:
            writer.writerow([local_time(ms, seconds=False)] + report_cells(values))


def format_table(reports):
    widths = [16] + [max(len(label), 7) for label, _ in COLUMNS]
    def line(cells):
        return "  " + "  ".join(c.rjust(w) if i else c.ljust(w) for i, (c, w) in enumerate(zip(cells, widths)))
    out = [line(["time"] + [label for label, _ in COLUMNS])]
    for ms, values in reports:
        out.append(line([local_time(ms, seconds=False)] + report_cells(values)))
    return "\n".join(out)


# --- The run ----------------------------------------------------------------------

def load_config(path):
    if not path.exists():
        sys.exit(f"No config at {path}.\n"
                 f"Copy {HERE / 'config.example.json'} to it and fill it in (README.md says where from).")
    config = json.loads(path.read_text(encoding="utf-8"))
    for key in ("endpoint", "access_id", "access_secret", "devices"):
        if not config.get(key):
            sys.exit(f"{path}: '{key}' is missing or empty.")
    return config


def update_device(cloud, name, device_id, out_dir, now_ms, first_days, api_order):
    """Fetch what is new for one device and store it. Returns (all reports,
    the reports that are new, a list of warnings)."""
    raw_path = out_dir / f"{name}_raw.csv"
    rows = read_raw(raw_path)
    known = set(rows)
    warnings = []

    if rows:
        last_ms = max(ms for ms, _, _ in rows)
        start_ms = last_ms + 1
        if now_ms - last_ms > RETENTION_DAYS * DAY_MS:
            warnings.append(f"last stored row is from {local_time(last_ms)}, more than {RETENTION_DAYS} days ago: "
                            f"reports in between may be gone from the cloud")
    else:
        start_ms = int(now_ms - first_days * DAY_MS)   # --days may be fractional; Tuya wants 13 digits

    codes = device_codes(cloud, device_id)
    custom = [c for label, cs in COLUMNS if label in CUSTOM_LABELS for c in cs]
    if codes is not ALL_CODES and not set(custom) & set(codes):
        warnings.append(DP_MODE_NOTE)

    fetched = to_rows(fetch_logs(cloud, device_id, codes, start_ms, now_ms, api_order))
    new_rows = sorted(r for r in set(fetched) if r not in known)
    if new_rows:
        append_raw(raw_path, new_rows)

    reports = group_reports(rows + new_rows)
    write_reports(out_dir / f"{name}_reports.csv", reports)

    # New rows all come after the stored ones, so the new reports are the one
    # holding the first new row -- it may have begun before the last run --
    # and everything after it.
    new_reports = []
    if new_rows:
        first_new_ms = new_rows[0][0]
        start_ms = max(ms for ms, _ in reports if ms <= first_new_ms)
        new_reports = [r for r in reports if r[0] >= start_ms]
    return reports, new_reports, warnings


def run(argv=None, transport=http_get, clock=time.time, stdout=sys.stdout):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--config", type=Path, default=DEFAULT_CONFIG, help="default: %(default)s")
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT, help="default: %(default)s")
    parser.add_argument("--days", type=float, default=RETENTION_DAYS,
                        help="how far back the first run of a device goes (default: %(default)s)")
    parser.add_argument("--show", type=int, default=None, metavar="N",
                        help="print the last N reports of each device, new or not (default: the new ones)")
    args = parser.parse_args(argv)

    config = load_config(args.config)
    args.out.mkdir(parents=True, exist_ok=True)

    cloud = TuyaCloud(config["endpoint"], config["access_id"], config["access_secret"], transport, clock)
    cloud.connect()

    now_ms = int(clock() * 1000)
    api_order = list(LOG_APIS)
    failed = False

    for name, device_id in config["devices"].items():
        try:
            reports, new_reports, warnings = update_device(cloud, name, device_id, args.out, now_ms, args.days, api_order)
        except TuyaError as e:
            if e.code in PROJECT_ERRORS:
                raise
            print(f"\n{name}: {e}", file=stdout)
            failed = True
            continue

        shown = reports[-args.show:] if args.show else new_reports
        last = (f"last report {local_time(reports[-1][0], seconds=False)}, "
                f"{(now_ms - reports[-1][0]) / 3_600_000:.1f} h ago") if reports else "no reports stored"
        print(f"\n{name}: {len(new_reports)} new report(s); {last}", file=stdout)
        for warning in warnings:
            print(f"  WARNING: {warning}", file=stdout)
        if shown:
            print(format_table(shown), file=stdout)

    print(f"\nStored in {args.out}", file=stdout)
    return 1 if failed else 0


def main():
    try:
        sys.exit(run())
    except TuyaError as e:
        sys.exit(f"Error: {e}")
    except urllib.error.URLError as e:
        sys.exit(f"Could not reach the Tuya cloud: {e.reason}")


if __name__ == "__main__":
    main()
