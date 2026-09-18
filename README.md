# Trabalho Prático 1 - Sistemas Distribuídos

Implementação em C das três atividades propostas: sinais, pipes anônimos e
produtor-consumidor multithread com semáforos POSIX.

## Requisitos

- Linux ou WSL;
- GCC, `make` e bibliotecas POSIX;
- Python 3 com `matplotlib` para gerar os gráficos.

No Ubuntu/WSL, caso necessário:

```bash
sudo apt update
sudo apt install build-essential python3 python3-matplotlib
```

## Estrutura

```text
TP1_Sistemas_Distribuidos/
├── sinais/                 Emissor e receptor de sinais
├── pipes/                  Produtor-consumidor com processos e pipe
├── semaforos/              Produtor-consumidor com threads e semáforos
├── scripts/                Experimentos e geração de gráficos
├── resultados/             CSVs gerados pelos experimentos
├── graficos/               PNGs gerados a partir dos CSVs
├── relatorio/              Modelo editável do relatório
├── Makefile
└── README.md
```

## Compilação

Na raiz desta pasta:

```bash
make
```

Para remover os executáveis:

```bash
make clean
```

O compilador é chamado com avisos rigorosos (`-Wall -Wextra -Wpedantic`) e
otimização `-O2`.

## Parte 1 - Sinais

Terminal 1:

```bash
./bin/receiver blocking
```

Copie o PID exibido. No terminal 2, substitua `PID` pelo número mostrado:

```bash
./bin/sender PID SIGUSR1
./bin/sender PID SIGUSR2
./bin/sender PID SIGTERM
```

O receptor também aceita `busy`. Os sinais podem ser enviados pelo programa
`kill`:

```bash
kill -SIGUSR1 PID
kill -SIGUSR2 PID
kill -SIGTERM PID
```

Para comparar consumo de CPU, deixe cada modo aberto e observe `%CPU` em
`top` ou `htop`. O modo `busy` realiza espera ativa; o modo `blocking` dorme
até a entrega de um sinal.

## Parte 2 - Pipes

O argumento é a quantidade de números crescentes a produzir:

```bash
./bin/pipe_pc 10
```

O produtor começa em `N0 = 1`, soma um incremento aleatório entre 1 e 100,
escreve mensagens de exatamente 20 bytes no pipe e, ao final, envia zero. O
consumidor testa a primalidade e termina ao receber zero.

## Parte 3 - Threads e semáforos

Uso:

```bash
./bin/semaphore_pc N Np Nc [M] [opções]
```

- `N`: capacidade do buffer;
- `Np`: número de produtoras;
- `Nc`: número de consumidoras;
- `M`: quantidade total de números (padrão: 100000);
- `--quiet`: não imprime cada teste de primalidade, ideal para medir tempo;
- `--occupancy ARQUIVO.csv`: salva a ocupação após cada produção/consumo.

Exemplo curto e visível:

```bash
./bin/semaphore_pc 10 2 4 30 --occupancy resultados/exemplo_ocupacao.csv
```

Exemplo de medição:

```bash
./bin/semaphore_pc 100 4 1 100000 --quiet
```

A última linha contém `RESULT` e campos fáceis de processar por script.

## Experimento completo do enunciado

O comando abaixo executa as 28 combinações (`4` tamanhos de buffer x `7`
pares de threads), 10 vezes cada, usando `M=100000`. Também salva uma série
de ocupação representativa para cada cenário.

```bash
./scripts/run_experiments.sh
```

Para um teste rápido antes do experimento final:

```bash
M=1000 REPS=2 ./scripts/run_experiments.sh
```

Depois, gere os gráficos:

```bash
python3 scripts/plot_results.py
```

Arquivos principais gerados:

- `resultados/tempos.csv`: todas as repetições;
- `resultados/medias.csv`: tempo médio por cenário;
- `resultados/ocupacao/`: ocupação representativa por cenário;
- `graficos/tempos_medios.png`: curvas pedidas no enunciado;
- `graficos/ocupacao_*.png`: um gráfico por cenário.

## Antes da entrega

1. Rode o experimento completo na máquina usada para os testes.
2. Gere os gráficos.
3. Complete nomes, matrícula, URL do repositório e análise no relatório.
4. Escolha gráficos de ocupação legíveis para caber no limite de 5 páginas.
5. Envie o código para um repositório acessível e teste a URL.

