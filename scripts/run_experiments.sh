#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROGRAM="$ROOT_DIR/bin/semaphore_pc"
RESULTS_DIR="$ROOT_DIR/resultados"
OCCUPANCY_DIR="$RESULTS_DIR/ocupacao"
M="${M:-100000}"
REPS="${REPS:-10}"

mkdir -p "$OCCUPANCY_DIR"
make -C "$ROOT_DIR" all >/dev/null

printf 'N,Np,Nc,repetition,M,seconds,primes\n' > "$RESULTS_DIR/tempos.csv"

for N in 1 10 100 1000; do
    for pair in 1:1 1:2 1:4 1:8 2:1 4:1 8:1; do
        IFS=: read -r NP NC <<< "$pair"
        for REP in $(seq 1 "$REPS"); do
            EXTRA=()
            if [[ "$REP" -eq 1 ]]; then
                EXTRA=(--occupancy "$OCCUPANCY_DIR/ocupacao_N${N}_Np${NP}_Nc${NC}.csv")
            fi
            LINE="$($PROGRAM "$N" "$NP" "$NC" "$M" --quiet "${EXTRA[@]}")"
            SECONDS_VALUE="$(sed -n 's/.*seconds=\([^ ]*\).*/\1/p' <<< "$LINE")"
            PRIMES="$(sed -n 's/.*primes=\([^ ]*\).*/\1/p' <<< "$LINE")"
            if [[ -z "$SECONDS_VALUE" || -z "$PRIMES" ]]; then
                printf 'Saída inesperada: %s\n' "$LINE" >&2
                exit 1
            fi
            printf '%s,%s,%s,%s,%s,%s,%s\n' \
                "$N" "$NP" "$NC" "$REP" "$M" "$SECONDS_VALUE" "$PRIMES" \
                >> "$RESULTS_DIR/tempos.csv"
            printf 'N=%s Np=%s Nc=%s repetição=%s/%s tempo=%ss\n' \
                "$N" "$NP" "$NC" "$REP" "$REPS" "$SECONDS_VALUE"
        done
    done
done

python3 "$ROOT_DIR/scripts/summarize_results.py"
printf '\nConcluído. Execute: python3 scripts/plot_results.py\n'

