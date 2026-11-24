#!/usr/bin/env bash
# Generate contract 0.2.0 C++ SBE headers. Generated files stay in build/.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CONTRACT="${CONTRACT_HOME:-/opt/quantlink-venue-contract}"
OUT="${CONTRACT_GENERATED_DIR:-$ROOT/build/generated/contract}"
SBE_JAR="${SBE_JAR:-$ROOT/.tools/sbe-all-1.35.6.jar}"

while [ "$#" -gt 0 ]; do
  case "$1" in
    --contract) CONTRACT="$2"; shift 2 ;;
    --version) shift 2 ;;
    --output) OUT="$2"; shift 2 ;;
    *) echo "unknown argument: $1" >&2; exit 2 ;;
  esac
done

SCHEMA="$CONTRACT/abi/sbe/quantlink-fin-sbe.xml"
if [ ! -f "$SCHEMA" ]; then
  echo "missing contract schema: $SCHEMA" >&2
  exit 1
fi
if [ ! -f "$SBE_JAR" ]; then
  echo "missing SBE jar: $SBE_JAR" >&2
  echo "set SBE_JAR or download sbe-all-1.35.6.jar" >&2
  exit 1
fi

rm -rf "$OUT"
mkdir -p "$OUT"
java -Dsbe.target.language=Cpp -Dsbe.output.dir="$OUT" \
  -jar "$SBE_JAR" "$SCHEMA"
echo "contract C++ SBE generated at $OUT"
