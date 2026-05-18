#!/bin/bash
# End-to-end test of tierscaped with masim. Saves all artifacts in eval/exp-<timestamp>/.
set -u
cd "$(dirname "$0")/.."
ROOT="$(pwd)"
TS=$(date +%Y%m%d_%H%M%S)
EXP_DIR="$ROOT/masim_mod/eval/exp-${TS}"
mkdir -p "$EXP_DIR"

SUMMARY="$EXP_DIR/run.summary"
TS_LOG="$EXP_DIR/tierscaped.log"
MASIM_LOG="$EXP_DIR/masim.log"
NUMA_LOG="$EXP_DIR/numastat.log"

echo "=== Tierscaped end-to-end test ===" | tee "$SUMMARY"
echo "Run ID: $TS" | tee -a "$SUMMARY"
echo "Exp dir: $EXP_DIR" | tee -a "$SUMMARY"
date | tee -a "$SUMMARY"
echo "" | tee -a "$SUMMARY"

echo "[1/4] Starting masim on node 0 (4 GB stairs, 100s)..." | tee -a "$SUMMARY"
numactl --membind=0 "$ROOT/masim_mod/masim" "$ROOT/masim_mod/configs/stairs_4gb_100s" \
    >"$MASIM_LOG" 2>&1 &
MASIM_PID=$!
echo "masim PID: $MASIM_PID" | tee -a "$SUMMARY"
sleep 4

if ! kill -0 $MASIM_PID 2>/dev/null; then
    echo "ERROR: masim died early. log:" | tee -a "$SUMMARY"
    cat "$MASIM_LOG" | tee -a "$SUMMARY"
    exit 1
fi

echo "" | tee -a "$SUMMARY"
echo "[2/4] Pre-tierscaped numastat:" | tee -a "$SUMMARY"
numastat -p $MASIM_PID 2>&1 | tee -a "$SUMMARY"

# Record T+0 snapshot (before daemon starts)
echo "" >> "$NUMA_LOG"
echo "=== T+0s ===" >> "$NUMA_LOG"
numastat -p $MASIM_PID >> "$NUMA_LOG" 2>&1

# Copy config for reproducibility
TOML_CFG="$ROOT/src/test_config.toml"
cp "$TOML_CFG" "$EXP_DIR/config.toml"

echo "" | tee -a "$SUMMARY"
echo "[3/4] Launching tierscaped (window=10s, hot_pct=75 -> demote 75%)..." | tee -a "$SUMMARY"
DUMP_FILE="$EXP_DIR/samples.dump"
"$ROOT/src/build/tierscaped" -f -v -c "$TOML_CFG" \
    -p $MASIM_PID --window 10 --hot-pct 75 \
    --dump-file "$DUMP_FILE" \
    >"$TS_LOG" 2>&1 &
TS_PID=$!
echo "tierscaped PID: $TS_PID" | tee -a "$SUMMARY"

# Snapshot numastat every 10s for 100 seconds
for i in $(seq 1 10); do
    sleep 10
    if ! kill -0 $MASIM_PID 2>/dev/null; then break; fi
    echo "" >> "$NUMA_LOG"
    echo "=== T+$((i*10))s ===" >> "$NUMA_LOG"
    numastat -p $MASIM_PID >> "$NUMA_LOG" 2>&1
done

echo "" | tee -a "$SUMMARY"
echo "[4/4] Final numastat:" | tee -a "$SUMMARY"
if kill -0 $MASIM_PID 2>/dev/null; then
    numastat -p $MASIM_PID 2>&1 | tee -a "$SUMMARY"
fi

# Shut things down
echo "" | tee -a "$SUMMARY"
echo "Stopping daemon and workload..." | tee -a "$SUMMARY"
kill -TERM $TS_PID 2>/dev/null
sleep 1
kill -TERM $MASIM_PID 2>/dev/null
sleep 1
kill -KILL $TS_PID 2>/dev/null
kill -KILL $MASIM_PID 2>/dev/null

echo "" | tee -a "$SUMMARY"
echo "=== Tierscaped log tail ===" | tee -a "$SUMMARY"
tail -30 "$TS_LOG" | tee -a "$SUMMARY"

echo "" | tee -a "$SUMMARY"
echo "Artifacts saved in $EXP_DIR:" | tee -a "$SUMMARY"
ls -la "$EXP_DIR"/ | tee -a "$SUMMARY"

echo "" | tee -a "$SUMMARY"
echo "Done. Plot with: python3 masim_mod/plotting/plot_migration.py --eval-dir $EXP_DIR"
