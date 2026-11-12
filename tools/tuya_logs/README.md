# Tuya cloud logs

`tuya_logs.py` fetches each sensor's data-point reports from the Tuya cloud and
keeps them in `notes/tuya_logs/`. Python's standard library is all it needs.

The free tier keeps only **7 days** of logs, so run it at least once a week.
If more than 7 days have passed since the newest row it has stored, it warns
that reports in between may be lost.

## Running it

```
py tools\tuya_logs\tuya_logs.py
```

or double-click `fetch_logs.bat`. For each device it:

- fetches every report since the last run (on a first run, the last 7 days),
- appends the rows to `<name>_raw.csv`, one row per DP value as the cloud logged it,
- rebuilds `<name>_reports.csv`, one row per report (DP rows less than 2 minutes
  apart are one report), which opens straight in Excel,
- prints the new reports and how long ago the last one came in. A board that
  has stopped reporting shows up as a large "h ago".

```
kitchen: 3 new report(s); last report 2026-11-12 09:40, 0.3 h ago
  time               min mV   avg mV   batt %    state  damaged   temp C     rh %     diag
  2026-11-12 09:40        .     1298       94     high      off     22.4       63     r=pin c=17 w=3 u=0 m=02 s=0
```

A `.` is a DP that the report did not include. DP 101 is not sent at 1000 mV or
above, for example, and DP 104 (`diag`) only when there is something to say:
after a reset, or after report windows or sensor reads failed. Its fields are
explained at the top of
[`libs/src/report/diag_record.h`](../../libs/src/report/diag_record.h).

Options: `--show N` prints each device's last N reports, new or not.
`--days N` sets how far back a device's first run goes. `--help` lists the rest.

## One-time setup

You need a cloud project with the sensors linked to it. If you already see the
devices' logs under **Cloud → Development → your project → Devices**, steps 1
and 3 are done.

1. On [platform.tuya.com](https://platform.tuya.com), open **Cloud → Development**.
   A cloud project is not the same as the *product* that defines the sensor's
   DPs; if none is listed, **Create Cloud Project**: Industry *Smart Home*,
   Development Method *Smart Home*, and the data center of the app account's
   region (in the app: **Me → Settings → Account and Security → Region**).
   When it offers API services, keep the defaults (they include IoT Core) and
   authorize them.
2. On the project's **Overview** tab, copy the **Access ID** and **Access Secret**
   (also called Client ID / Client Secret).
3. On the **Devices** tab, link the app account (**Link App Account**, then scan
   the QR code with the app). Copy each sensor's **Device ID**.
4. On the **Service API** tab, check that **IoT Core** is there. Its free trial
   runs out (a month at first, then whatever extension was granted). When it
   has, every request fails with error 28841002, "IoT Core service subscription
   has expired". Extend it under **Cloud → Cloud Services → IoT Core → Extend
   Trial Period**. Tuya approves the request by hand, which can take a day or
   two.
5. Copy `config.example.json` to `config.json` in this folder and fill it in.
   `config.json` holds the secret and is git-ignored. Pick `endpoint` from the
   project's data center:

   | Data center     | endpoint                           |
   |-----------------|------------------------------------|
   | Western America | `https://openapi.tuyaus.com`       |
   | Eastern America | `https://openapi-ueaz.tuyaus.com`  |
   | Central Europe  | `https://openapi.tuyaeu.com`       |
   | Western Europe  | `https://openapi-weaz.tuyaeu.com`  |
   | China           | `https://openapi.tuyacn.com`       |
   | India           | `https://openapi.tuyain.com`       |

   The names under `devices` are yours to choose. They name the CSV files.
6. Set the project's **control instruction mode** to **DP Instruction** for
   this product (Tuya's guide:
   [Change Control Instruction Mode](https://developer.tuya.com/en/docs/iot/change-control-instruction-mode?id=Kcbz8lahbg5st)).
   - In the default *standard instruction* mode, the cloud renames DPs 1 and 2
     to `va_temperature` / `va_humidity` and doesn't log DPs 101–104 at all.
     The script then warns, and the min/avg/damaged/diag columns stay `.`.
   - In DP mode it uses the product's own identifiers. On the platform DP 101's
     is spelled `batter_min_voltage`; the script accepts either spelling.
   - The setting only changes what the cloud API shows. The app and the device
     are unaffected.

   After switching, delete `notes/tuya_logs/*.csv` once. Each run only fetches
   rows newer than the last one stored, so without that it won't go back for
   the older DPs 101–103.

## Endpoints

Tuya has three endpoints for these logs, and a project may only be allowed some
of them. The script tries `/v2.0/cloud/thing/{id}/report-logs`, then
`/v1.0/iot-03/devices/{id}/report-logs`, then the archived
`/v1.0/devices/{id}/logs` (type 7, data point reports). It uses the first one
that answers. If none does, it prints each one's error with a hint.

## Tests

```
py -m unittest discover -s tools\tuya_logs -v
```

Nothing in the tests talks to the real cloud. The signing is checked against
Tuya's two worked examples, and a fake cloud checks the signature of every
request the script makes.
