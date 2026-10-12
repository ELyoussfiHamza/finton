#!/usr/bin/env bash
# Runs every sweep used in the README, three times each, and writes the tables to
# benchmarks/results/. Takes about twenty minutes. Run from the repository root,
# on a quiet machine, after building the release configuration.
#
#   benchmarks/run_all.sh            runs 1 to 3
#   FIRST=2 LAST=3 benchmarks/run_all.sh
set -eu
OUT=benchmarks/results
mkdir -p "$OUT"
MOBILENET=models/mobilenet/model.onnx
FIRST=${FIRST:-1}
LAST=${LAST:-3}

run(){
    NAME=$1; shift
    echo "== $NAME (run $RUN)"
    env "$@" | tee "$OUT/$NAME.run$RUN.txt"
}

for RUN in $(seq "$FIRST" "$LAST"); do
    run mnist_batch         DURATION=8 CLIENTS=32 WORKERS=2 THREADS=1 DELAY=1 tools/sweep.sh batch "1 2 4 8 16 32"
    run mnist_delay         DURATION=8 CLIENTS=32 WORKERS=2 THREADS=1 BATCH=8 tools/sweep.sh delay-ms "0 1 2 5"
    run mnist_load_batch8   DURATION=8 WORKERS=2 THREADS=1 BATCH=8 DELAY=1 tools/sweep.sh clients "1 4 16 64"
    run mnist_load_batch1   DURATION=8 WORKERS=2 THREADS=1 BATCH=1 DELAY=0 tools/sweep.sh clients "1 4 16 64"

    if [ -f "$MOBILENET" ]; then
        run mobilenet_batch_2w1t  MODEL=$MOBILENET DURATION=10 CLIENTS=16 WORKERS=2 THREADS=1 DELAY=5 tools/sweep.sh batch "1 2 4 8"
        run mobilenet_batch_1w4t  MODEL=$MOBILENET DURATION=10 CLIENTS=16 WORKERS=1 THREADS=4 DELAY=5 tools/sweep.sh batch "1 2 4 8"
        run mobilenet_threads     MODEL=$MOBILENET DURATION=10 CLIENTS=16 WORKERS=1 BATCH=8 DELAY=5 tools/sweep.sh threads "1 2 4"
        run mobilenet_workers     MODEL=$MOBILENET DURATION=10 CLIENTS=16 THREADS=1 BATCH=4 DELAY=5 tools/sweep.sh workers "1 2 4"
    else
        echo "Skipping MobileNet : $MOBILENET is missing, see the README for the download command"
    fi
done
