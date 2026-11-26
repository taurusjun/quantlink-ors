#!/usr/bin/env bash
# Generate the C++ SBE headers consumed by ORS. The contract schema remains the
# only source; generated files are build artifacts, not committed.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CONTRACT_HOME="${CONTRACT_HOME:-/Users/wangjun/PG/quantlink-new/quantlink-venue-contract}"
SCHEMA="${SCHEMA:-$CONTRACT_HOME/abi/sbe/quantlink-fin-sbe.xml}"
OUT="${1:-$ROOT/build/generated/contract}"
SBE_JAR="${SBE_JAR:-/Users/wangjun/PG/quantlink-fin/.tools/sbe-all-1.35.6.jar}"
JAVA_BIN="${JAVA_BIN:-java}"

if [ ! -f "$SBE_JAR" ]; then
  echo "missing SBE jar: $SBE_JAR" >&2
  exit 1
fi

rm -rf "$OUT"
mkdir -p "$OUT"
"$JAVA_BIN" -Dsbe.target.language=Cpp -Dsbe.output.dir="$OUT" -jar "$SBE_JAR" "$SCHEMA"
echo "C++ contract codegen OK: $OUT"
