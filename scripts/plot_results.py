#!/usr/bin/env python3
import csv
from collections import defaultdict
from pathlib import Path

try:
    import matplotlib.pyplot as plt
except ImportError as error:
    raise SystemExit("Instale matplotlib: sudo apt install python3-matplotlib") from error

root = Path(__file__).resolve().parent.parent
results = root / "resultados"
graphs = root / "graficos"
graphs.mkdir(parents=True, exist_ok=True)

curves = defaultdict(dict)
with (results / "medias.csv").open(newline="", encoding="utf-8") as stream:
    for row in csv.DictReader(stream):
        label = f"({row['Np']},{row['Nc']})"
        curves[int(row["N"])][label] = float(row["mean_seconds"])

order = ["(1,1)", "(1,2)", "(1,4)", "(1,8)", "(2,1)", "(4,1)", "(8,1)"]
fig, axis = plt.subplots(figsize=(10, 6))
for size in sorted(curves):
    axis.plot(order, [curves[size][label] for label in order], marker="o", label=f"N={size}")
axis.set_title("Tempo médio por configuração de produtoras/consumidoras")
axis.set_xlabel("(Np, Nc)")
axis.set_ylabel("Tempo médio (s)")
axis.grid(True, alpha=0.3)
axis.legend(title="Buffer")
fig.tight_layout()
fig.savefig(graphs / "tempos_medios.png", dpi=180)
plt.close(fig)

for path in sorted((results / "ocupacao").glob("ocupacao_*.csv")):
    operations, occupancy = [], []
    with path.open(newline="", encoding="utf-8") as stream:
        for row in csv.DictReader(stream):
            operations.append(int(row["operation"]))
            occupancy.append(int(row["occupancy"]))

    # Mantém o gráfico leve sem alterar o CSV completo.
    step = max(1, len(operations) // 5000)
    # Um passo ímpar evita aliasing em N=1, cuja série alterna 1,0,1,0...
    if step > 1 and step % 2 == 0:
        step += 1
    x = operations[::step]
    y = occupancy[::step]
    fig, axis = plt.subplots(figsize=(10, 4.8))
    axis.plot(x, y, linewidth=0.8)
    axis.set_title(path.stem.replace("ocupacao_", "Ocupação: ").replace("_", " "))
    axis.set_xlabel("Operação de produção/consumo")
    axis.set_ylabel("Posições ocupadas")
    axis.set_ylim(bottom=0)
    axis.grid(True, alpha=0.25)
    fig.tight_layout()
    fig.savefig(graphs / f"{path.stem}.png", dpi=160)
    plt.close(fig)

print(f"Gráficos salvos em {graphs}")
