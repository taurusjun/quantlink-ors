#!/bin/bash
# Reconnect-stability smoke: start one ORS runtime, repeatedly drop the CTP
# front for a fixed interval, and require a new reconciliation epoch each time.
# Root privileges are required for iptables. No credentials are printed.
set -u

usage() {
  cat <<'EOF'
usage: reconnect_stability.sh --config PATH --front IP --instruments LIST [options]

Required:
  --config PATH           CTP config (secrets are not printed)
  --front IP              CTP front IP to block temporarily
  --instruments LIST      comma-separated instruments for the runtime

Options:
  --runtime PATH          runtime binary (default: ./build/ctp/ors_ctp)
  --aeron-dir PATH        Aeron directory (default: /var/lib/quantlink-fin/aeron)
  --venue-id ID           contract venue id (default: SIMNOW)
  --environment NAME      contract environment (default: SHADOW)
  --exchange NAME         default exchange for the runtime (default: SHFE)
  --stream-base N         stream base (default: 9000)
  --cycles N              disconnect cycles (default: 3)
  --block-seconds N       block duration per cycle (default: 120)
  --ready-timeout N       seconds to wait for runtime READY (default: 600)
  --reconcile-timeout N   seconds to wait after restore (default: 300)
  --log PATH              script/runtime log prefix (default: /tmp/ors-reconnect-stability)
EOF
}

config=""
front=""
instruments=""
runtime="./build/ctp/ors_ctp"
aeron_dir="/var/lib/quantlink-fin/aeron"
venue_id="SIMNOW"
environment="SHADOW"
exchange="SHFE"
stream_base=9000
cycles=3
block_seconds=120
ready_timeout=600
reconcile_timeout=300
log_prefix="/tmp/ors-reconnect-stability"

while [ "$#" -gt 0 ]; do
  case "$1" in
    --config) config="${2:-}"; shift 2 ;;
    --front) front="${2:-}"; shift 2 ;;
    --instruments) instruments="${2:-}"; shift 2 ;;
    --runtime) runtime="${2:-}"; shift 2 ;;
    --aeron-dir) aeron_dir="${2:-}"; shift 2 ;;
    --venue-id) venue_id="${2:-}"; shift 2 ;;
    --environment) environment="${2:-}"; shift 2 ;;
    --exchange) exchange="${2:-}"; shift 2 ;;
    --stream-base) stream_base="${2:-}"; shift 2 ;;
    --cycles) cycles="${2:-}"; shift 2 ;;
    --block-seconds) block_seconds="${2:-}"; shift 2 ;;
    --ready-timeout) ready_timeout="${2:-}"; shift 2 ;;
    --reconcile-timeout) reconcile_timeout="${2:-}"; shift 2 ;;
    --log) log_prefix="${2:-}"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) echo "unknown argument: $1" >&2; usage >&2; exit 2 ;;
  esac
done

if [ -z "$config" ] || [ -z "$front" ] || [ -z "$instruments" ]; then
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

"$runtime" \
  --config "$config" \
  --mode runtime --instruments "$instruments" \
  --aeron-dir "$aeron_dir" \
  --venue-id "$venue_id" --environment "$environment" \
  --exchange "$exchange" --stream-base "$stream_base" \
  >"$runtime_log" 2>&1 &
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
  before=$(grep -c "reconciliation complete" "$runtime_log" || true)
  echo "=== cycle $cycle block $(date) ==="
  iptables -I INPUT -s "$front" -j DROP
  iptables -I OUTPUT -d "$front" -j DROP
  sleep "$block_seconds"
  iptables -D INPUT -s "$front" -j DROP
  iptables -D OUTPUT -d "$front" -j DROP
  echo "=== cycle $cycle restored $(date) ==="

  deadline=$((SECONDS + reconcile_timeout))
  while [ "$SECONDS" -lt "$deadline" ]; do
    now=$(grep -c "reconciliation complete" "$runtime_log" || true)
    if [ "$now" -gt "$before" ]; then
      break
    fi
    sleep 2
  done
  now=$(grep -c "reconciliation complete" "$runtime_log" || true)
  if [ "$now" -le "$before" ]; then
    echo "cycle $cycle: reconciliation timeout" >&2
    failed=1
    break
  fi
  echo "=== cycle $cycle reconciled $(date) ==="
done

echo "=== reconnect stability result failed=$failed $(date) ==="
tail -30 "$runtime_log"
if [ "$failed" -ne 0 ]; then
  exit 1
fi
