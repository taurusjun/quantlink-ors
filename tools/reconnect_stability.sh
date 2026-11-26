#!/bin/bash
# Reconnect-stability smoke: start one ORS runtime, repeatedly drop the CTP
# front, and require a new epoch after each reconnect. Root is required for
# iptables. No credentials are printed.
set -u

usage() {
  cat <<'EOF'
usage: reconnect_stability.sh --config PATH --front IP [options]

Required:
  --config PATH           CTP/runtime config (secrets are not printed)
  --front IP              CTP front IP to block temporarily

Options:
  --runtime PATH          runtime binary (default: ./build/ctp/ors_ctp)
  --cycles N              disconnect cycles (default: 3)
  --block-seconds N       block duration per cycle (default: 120)
  --ready-timeout N       seconds to wait for initial READY (default: 600)
  --reconnect-timeout N   seconds to wait after restore (default: 300)
  --log PATH              script/runtime log prefix (default: /tmp/ors-reconnect-stability)
EOF
}

config=""
front=""
runtime="./build/ctp/ors_ctp"
cycles=3
block_seconds=120
ready_timeout=600
reconnect_timeout=300
log_prefix="/tmp/ors-reconnect-stability"

while [ "$#" -gt 0 ]; do
  case "$1" in
    --config) config="${2:-}"; shift 2 ;;
    --front) front="${2:-}"; shift 2 ;;
    --runtime) runtime="${2:-}"; shift 2 ;;
    --cycles) cycles="${2:-}"; shift 2 ;;
    --block-seconds) block_seconds="${2:-}"; shift 2 ;;
    --ready-timeout) ready_timeout="${2:-}"; shift 2 ;;
    --reconnect-timeout) reconnect_timeout="${2:-}"; shift 2 ;;
    --log) log_prefix="${2:-}"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) echo "unknown argument: $1" >&2; usage >&2; exit 2 ;;
  esac
done

if [ -z "$config" ] || [ -z "$front" ]; then
  usage >&2
  exit 2
fi
if [ "$(id -u)" -ne 0 ]; then
  echo "iptables requires root" >&2
  exit 2
fi
if ! command -v iptables >/dev/null 2>&1; then
  echo "iptables not found" >&2
  exit 2
fi
if [ ! -x "$runtime" ]; then
  echo "runtime not executable: $runtime" >&2
  exit 2
fi

runtime_log="${log_prefix}.runtime.log"
script_log="${log_prefix}.log"
runtime_pid=""
cleanup() {
  if [ -n "$runtime_pid" ] && kill -0 "$runtime_pid" 2>/dev/null; then
    kill -TERM "$runtime_pid" 2>/dev/null || true
    wait "$runtime_pid" 2>/dev/null || true
  fi
  iptables -D INPUT -s "$front" -j DROP 2>/dev/null || true
  iptables -D OUTPUT -d "$front" -j DROP 2>/dev/null || true
}
trap cleanup EXIT INT TERM

exec >>"$script_log" 2>&1
echo "=== reconnect stability start $(date) ==="
echo "runtime=$runtime cycles=$cycles block=${block_seconds}s front=$front"

"$runtime" --config "$config" >"$runtime_log" 2>&1 &
runtime_pid=$!

deadline=$((SECONDS + ready_timeout))
while [ "$SECONDS" -lt "$deadline" ]; do
  if grep -q "runtime READY" "$runtime_log"; then
    echo "runtime READY at $(date)"
    break
  fi
  if grep -q "runtime trader start failed" "$runtime_log"; then
    echo "runtime failed before READY" >&2
    tail -20 "$runtime_log" >&2
    exit 1
  fi
  if ! kill -0 "$runtime_pid" 2>/dev/null; then
    echo "runtime exited before READY" >&2
    tail -20 "$runtime_log" >&2
    exit 1
  fi
  sleep 2
done
if ! grep -q "runtime READY" "$runtime_log"; then
  echo "runtime READY timeout" >&2
  exit 1
fi

failed=0
for cycle in $(seq 1 "$cycles"); do
  before=$(grep -c "runtime reconnected epoch=" "$runtime_log" || true)
  echo "=== cycle $cycle block $(date) ==="
  iptables -I INPUT -s "$front" -j DROP
  iptables -I OUTPUT -d "$front" -j DROP
  sleep "$block_seconds"
  iptables -D INPUT -s "$front" -j DROP
  iptables -D OUTPUT -d "$front" -j DROP
  echo "=== cycle $cycle restored $(date) ==="

  deadline=$((SECONDS + reconnect_timeout))
  while [ "$SECONDS" -lt "$deadline" ]; do
    now=$(grep -c "runtime reconnected epoch=" "$runtime_log" || true)
    if [ "$now" -gt "$before" ]; then
      break
    fi
    sleep 2
  done
  now=$(grep -c "runtime reconnected epoch=" "$runtime_log" || true)
  if [ "$now" -le "$before" ]; then
    echo "cycle $cycle: reconnect timeout" >&2
    failed=1
    break
  fi
  echo "=== cycle $cycle reconnected $(date) ==="
done

echo "=== reconnect stability result failed=$failed $(date) ==="
tail -30 "$runtime_log"
if [ "$failed" -ne 0 ]; then
  exit 1
fi
