#!/usr/bin/env bash
# Performance smoke test: launch time, RSS, and playback FPS under Xvfb.
# Uses a synthetic local MPEG-TS HTTP stream (no HDHomeRun required).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${ROOT}/build/tv-cpp"

# Thresholds (Xvfb + software decode; tune for real GPU desktops).
MAX_LAUNCH_MS="${PERF_MAX_LAUNCH_MS:-2500}"
MAX_IDLE_RSS_KB="${PERF_MAX_IDLE_RSS_KB:-204800}"       # 200 MiB
MAX_PLAY_RSS_KB="${PERF_MAX_PLAY_RSS_KB:-460800}"       # 450 MiB
MIN_MEAN_FPS="${PERF_MIN_MEAN_FPS:-25}"
MAX_DROP_DELTA="${PERF_MAX_DROP_DELTA:-30}"
SAMPLE_SECS="${PERF_SAMPLE_SECS:-10}"

die() { echo "perf_smoke: ERROR: $*" >&2; exit 1; }
have() { command -v "$1" >/dev/null 2>&1; }

have ffmpeg || die "ffmpeg is required (sudo apt install ffmpeg)"
have xvfb-run || die "xvfb-run is required (sudo apt install xvfb)"
[[ -x "$BIN" ]] || die "binary not found: $BIN (build with: cmake -B build && cmake --build build)"

TMP="$(mktemp -d "${TMPDIR:-/tmp}/tv-cpp-perf.XXXXXX")"
cleanup() {
  if [[ -n "${TV_PID:-}" ]] && kill -0 "$TV_PID" 2>/dev/null; then
    kill "$TV_PID" 2>/dev/null || true
  fi
  if [[ -n "${XVFB_PID:-}" ]] && kill -0 "$XVFB_PID" 2>/dev/null; then
    kill "$XVFB_PID" 2>/dev/null || true
    wait "$XVFB_PID" 2>/dev/null || true
  fi
  if [[ -n "${FFMPEG_PID:-}" ]] && kill -0 "$FFMPEG_PID" 2>/dev/null; then
    kill "$FFMPEG_PID" 2>/dev/null || true
    wait "$FFMPEG_PID" 2>/dev/null || true
  fi
  rm -rf "$TMP"
}
trap cleanup EXIT

# Pick a free TCP port.
PORT="$(python3 - <<'PY'
import socket
s = socket.socket()
s.bind(("127.0.0.1", 0))
print(s.getsockname()[1])
s.close()
PY
)"
STREAM_URL="http://127.0.0.1:${PORT}/test.ts"

echo "perf_smoke: starting ffmpeg MPEG-TS on ${STREAM_URL}"
# Infinite test pattern; -listen 1 waits for the first HTTP client (mpv).
ffmpeg -hide_banner -loglevel error -nostdin \
  -re -f lavfi -i "testsrc2=size=1280x720:rate=30" \
  -f lavfi -i "sine=frequency=440:sample_rate=48000" \
  -c:v libx264 -preset ultrafast -tune zerolatency -pix_fmt yuv420p -g 30 \
  -c:a aac -b:a 64k \
  -f mpegts -listen 1 "${STREAM_URL}" \
  >"${TMP}/ffmpeg.log" 2>&1 &
FFMPEG_PID=$!
sleep 0.5
kill -0 "$FFMPEG_PID" 2>/dev/null || die "ffmpeg exited early; see ${TMP}/ffmpeg.log"

export XDG_CONFIG_HOME="${TMP}/config"
export XDG_STATE_HOME="${TMP}/state"
mkdir -p "${XDG_CONFIG_HOME}/tv-cpp" "${XDG_STATE_HOME}/tv-cpp"
cat >"${XDG_CONFIG_HOME}/tv-cpp/channels.json" <<EOF
{
  "channels": [
    {
      "id": "perf0001",
      "name": "Perf Stream",
      "url": "${STREAM_URL}",
      "favorite": false
    }
  ]
}
EOF
printf '%s\n' '{ "osc_enabled": false }' >"${XDG_CONFIG_HOME}/tv-cpp/settings.json"

APP_LOG="${TMP}/app.log"
echo "perf_smoke: launching tv-cpp under Xvfb"
set +e
TV_CPP_PERF=1 \
TV_CPP_PERF_CHANNEL_URL="${STREAM_URL}" \
GDK_BACKEND=x11 \
xvfb-run -a -s "-screen 0 1280x720x24" \
  "$BIN" >"${APP_LOG}" 2>&1 &
XVFB_PID=$!
set -e

find_tv_pid() {
  # Prefer the real binary PID (not the xvfb-run wrapper).
  pgrep -n -f "^${BIN}$" 2>/dev/null || pgrep -n -f "${BIN}" 2>/dev/null || true
}

# Wait for PERF_READY (launch complete / mpv attached).
LAUNCH_MS=""
TV_PID=""
for _ in $(seq 1 100); do
  if ! kill -0 "$XVFB_PID" 2>/dev/null; then
    echo "---- app log ----" >&2
    cat "${APP_LOG}" >&2 || true
    die "tv-cpp exited before PERF_READY"
  fi
  TV_PID="$(find_tv_pid)"
  if grep -q '^PERF_READY ' "${APP_LOG}" 2>/dev/null; then
    LAUNCH_MS="$(grep '^PERF_READY ' "${APP_LOG}" | tail -1 | sed -n 's/.*t_ms=\([0-9][0-9]*\).*/\1/p')"
    break
  fi
  sleep 0.1
done
[[ -n "$LAUNCH_MS" ]] || die "timed out waiting for PERF_READY (see ${APP_LOG})"
TV_PID="$(find_tv_pid)"
[[ -n "$TV_PID" ]] || die "could not find tv-cpp PID"

rss_kb() {
  local pid="$1"
  awk '/VmRSS:/ {print $2; exit}' "/proc/${pid}/status" 2>/dev/null || echo 0
}

sleep 0.5
IDLE_RSS_KB="$(rss_kb "$TV_PID")"
echo "perf_smoke: launch_ms=${LAUNCH_MS} idle_rss_kb=${IDLE_RSS_KB} tv_pid=${TV_PID}"

# Collect PERF_MPV samples during playback.
echo "perf_smoke: sampling FPS for ${SAMPLE_SECS}s"
sleep "${SAMPLE_SECS}"
TV_PID="$(find_tv_pid)"
PLAY_RSS_KB="$(rss_kb "$TV_PID")"

MPV_COUNT="$(grep -c '^PERF_MPV ' "${APP_LOG}" || true)"
if [[ "${MPV_COUNT}" -eq 0 ]]; then
  echo "---- app log (tail) ----" >&2
  tail -50 "${APP_LOG}" >&2 || true
  die "no PERF_MPV samples collected"
fi

STATS="$(grep '^PERF_MPV ' "${APP_LOG}" | python3 -c '
import sys
fps_vals = []
drops = []
for line in sys.stdin:
    fps = None
    drop = None
    for part in line.split():
        if part.startswith("fps="):
            try:
                fps = float(part.split("=", 1)[1])
            except ValueError:
                pass
        elif part.startswith("drops="):
            try:
                drop = int(float(part.split("=", 1)[1]))
            except ValueError:
                pass
    if fps is not None:
        fps_vals.append(fps)
    if drop is not None:
        drops.append(drop)
if not fps_vals:
    print("0 0 0")
    raise SystemExit(0)
mean = sum(fps_vals) / len(fps_vals)
d0 = drops[0] if drops else 0
d1 = drops[-1] if drops else 0
print(f"{mean:.3f} {d0} {d1}")
')"
MEAN_FPS="$(echo "$STATS" | awk '{print $1}')"
DROP_START="$(echo "$STATS" | awk '{print $2}')"
DROP_END="$(echo "$STATS" | awk '{print $3}')"
DROP_DELTA=$((DROP_END - DROP_START))
if [[ "$DROP_DELTA" -lt 0 ]]; then
  DROP_DELTA=0
fi

echo "perf_smoke: mean_fps=${MEAN_FPS} drop_delta=${DROP_DELTA} play_rss_kb=${PLAY_RSS_KB}"
echo "perf_smoke: samples=${MPV_COUNT} thresholds: launch<=${MAX_LAUNCH_MS}ms idle_rss<=${MAX_IDLE_RSS_KB}KB play_rss<=${MAX_PLAY_RSS_KB}KB fps>=${MIN_MEAN_FPS} drops<=${MAX_DROP_DELTA}"

FAIL=0
python3 - <<PY
launch = int("${LAUNCH_MS}")
idle = int("${IDLE_RSS_KB}")
play = int("${PLAY_RSS_KB}")
fps = float("${MEAN_FPS}")
drops = int("${DROP_DELTA}")
fail = 0
def check(ok, msg):
    global fail
    print(("PASS" if ok else "FAIL") + ": " + msg)
    if not ok:
        fail = 1
check(launch <= int("${MAX_LAUNCH_MS}"), f"launch {launch} ms <= ${MAX_LAUNCH_MS} ms")
check(idle <= int("${MAX_IDLE_RSS_KB}"), f"idle RSS {idle} KB <= ${MAX_IDLE_RSS_KB} KB")
check(play <= int("${MAX_PLAY_RSS_KB}"), f"playback RSS {play} KB <= ${MAX_PLAY_RSS_KB} KB")
check(fps >= float("${MIN_MEAN_FPS}"), f"mean fps {fps} >= ${MIN_MEAN_FPS}")
check(drops <= int("${MAX_DROP_DELTA}"), f"frame-drop delta {drops} <= ${MAX_DROP_DELTA}")
raise SystemExit(fail)
PY
FAIL=$?

# Tear down app cleanly before trap.
if [[ -n "${TV_PID:-}" ]] && kill -0 "$TV_PID" 2>/dev/null; then
  kill "$TV_PID" 2>/dev/null || true
  TV_PID=""
fi
if [[ -n "${XVFB_PID:-}" ]] && kill -0 "$XVFB_PID" 2>/dev/null; then
  kill "$XVFB_PID" 2>/dev/null || true
  wait "$XVFB_PID" 2>/dev/null || true
  XVFB_PID=""
fi

exit "$FAIL"
