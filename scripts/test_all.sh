#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

printf 'Teste de pipes...\n'
"$ROOT_DIR/bin/pipe_pc" 5 >/dev/null

printf 'Teste de threads e semáforos...\n'
mkdir -p "$ROOT_DIR/resultados"
"$ROOT_DIR/bin/semaphore_pc" 10 2 3 200 --quiet \
    --occupancy "$ROOT_DIR/resultados/teste_ocupacao.csv" >/dev/null

printf 'Teste de sinais em modo blocking...\n'
"$ROOT_DIR/bin/receiver" blocking > "$ROOT_DIR/resultados/teste_sinais.log" &
RECEIVER_PID=$!
trap 'kill -TERM "$RECEIVER_PID" 2>/dev/null || true' EXIT
sleep 0.1
"$ROOT_DIR/bin/sender" "$RECEIVER_PID" SIGUSR1 >/dev/null
"$ROOT_DIR/bin/sender" "$RECEIVER_PID" SIGUSR2 >/dev/null
"$ROOT_DIR/bin/sender" "$RECEIVER_PID" SIGTERM >/dev/null
wait "$RECEIVER_PID"
trap - EXIT

grep -q 'SIGUSR1 recebido' "$ROOT_DIR/resultados/teste_sinais.log"
grep -q 'SIGUSR2 recebido' "$ROOT_DIR/resultados/teste_sinais.log"
grep -q 'SIGTERM recebido' "$ROOT_DIR/resultados/teste_sinais.log"

printf 'Todos os testes básicos passaram.\n'

