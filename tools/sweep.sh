#!/usr/bin/env bash
# Runs the load generator against finton once per value of one setting, and prints a table.
#
#   tools/sweep.sh batch "1 2 4 8 16 32"
#   tools/sweep.sh delay-ms "0 1 5 20"
#   tools/sweep.sh workers "1 2 4"
#   tools/sweep.sh threads "1 2 4"
#   tools/sweep.sh clients "1 4 16 64"
#
# The other settings stay fixed. Change them with environment variables :
#   BATCH=8 DELAY=1 WORKERS=2 THREADS=1 CLIENTS=32 DURATION=5 MODEL=models/mnist/model.onnx tools/sweep.sh batch "1 8"
set -u

SETTING=${1:?"which setting to sweep : batch, delay-ms, workers, threads or clients"}
VALUES=${2:?"the values to try, in quotes"}

BATCH=${BATCH:-8}
DELAY=${DELAY:-1}
WORKERS=${WORKERS:-2}
THREADS=${THREADS:-1}
CLIENTS=${CLIENTS:-32}
SECONDS_PER_RUN=${DURATION:-5}
MODEL=${MODEL:-models/mnist/model.onnx}
PORT=${PORT:-3077}
BIN=${BIN:-build-release}

if [ ! -x "$BIN/finton" ] || [ ! -x "$BIN/loadgen" ]; then
    echo "Build the release configuration first : cmake --build $BIN" >&2
    exit 1
fi

LOG=$(mktemp)
trap 'rm -f "$LOG"; [ -n "${SERVER:-}" ] && kill "$SERVER" 2>/dev/null' EXIT

echo "model $MODEL | fixed : batch $BATCH, delay $DELAY ms, workers $WORKERS, threads $THREADS, clients $CLIENTS | $SECONDS_PER_RUN s per run"
printf '%10s %12s %9s %9s %9s %9s %10s %9s\n' "$SETTING" "requests/s" "mean ms" "p50 ms" "p95 ms" "p99 ms" "avg batch" "rejected"

for VALUE in $VALUES; do
    B=$BATCH; D=$DELAY; W=$WORKERS; T=$THREADS; C=$CLIENTS
    case $SETTING in
        batch) B=$VALUE ;;
        delay-ms) D=$VALUE ;;
        workers) W=$VALUE ;;
        threads) T=$VALUE ;;
        clients) C=$VALUE ;;
        *) echo "unknown setting $SETTING" >&2; exit 1 ;;
    esac
    HTTP=$(( C + 8 ))
    QUEUE=$(( C + 8 ))

    "$BIN/finton" --port "$PORT" --model "$MODEL" --batch "$B" --delay-ms "$D" --workers "$W" --threads "$T" \
                  --http-threads "$HTTP" --queue "$QUEUE" > "$LOG" 2>&1 &
    SERVER=$!
    for _ in $(seq 100); do
        grep -q "listening" "$LOG" && break
        kill -0 "$SERVER" 2>/dev/null || break
        sleep 0.1
    done
    if ! grep -q "listening" "$LOG"; then
        echo "finton did not start :" >&2; cat "$LOG" >&2; exit 1
    fi
    FLOATS=$(grep -o '[0-9]* floats per request' "$LOG" | cut -d' ' -f1)

    LINE=$("$BIN/loadgen" --port "$PORT" --clients "$C" --seconds "$SECONDS_PER_RUN" --floats "$FLOATS" --csv)
    kill -INT "$SERVER"; wait "$SERVER" 2>/dev/null; SERVER=
    AVG=$(grep -o 'average batch size [0-9.]*' "$LOG" | cut -d' ' -f4)

    IFS=, read -r _ _ _ REJECTED _ RATE MEAN P50 P95 P99 _ <<< "$LINE"
    printf '%10s %12s %9s %9s %9s %9s %10s %9s\n' "$VALUE" "$RATE" "$MEAN" "$P50" "$P95" "$P99" "${AVG:-?}" "$REJECTED"
done
