# Trabalho Prático 1 - Sistemas Distribuídos

**Integrante(s):** [Giovanna Laura e Thainá Martins]  
**Matrícula(s):** [20213001559 20223002792]  
**Professora:** Michelle Hanne  
**Código-fonte:** [URL DO REPOSITÓRIO]

> Este texto é um modelo. Substitua os campos entre colchetes, insira os
> resultados obtidos na sua máquina e ajuste o conteúdo para o limite de cinco
> páginas antes da entrega.

## 1. Introdução

O trabalho explora mecanismos de comunicação entre processos e sincronização
entre threads. Foram implementadas três soluções em C/POSIX: comunicação por
sinais, troca de mensagens por pipe anônimo e produtor-consumidor multithread
com buffer compartilhado e semáforos. Os programas foram compilados com GCC em
[SISTEMA/VERSÃO E HARDWARE].

## 2. Sinais

Foram desenvolvidos um emissor e um receptor. O emissor recebe PID e sinal pela
linha de comando, valida os argumentos, consulta a existência/permissão do
processo com `kill(pid, 0)` e envia o sinal com `kill(pid, sinal)`. O receptor
instala handlers por `sigaction` para `SIGUSR1`, `SIGUSR2` e `SIGTERM`. Os dois
primeiros registram eventos diferentes e o último solicita o encerramento.

O código do handler altera apenas variáveis do tipo `volatile sig_atomic_t`.
A impressão ocorre no fluxo principal, pois funções de E/S como `printf` não
são seguras em contexto assíncrono de sinal. No modo bloqueante, os sinais são
bloqueados durante a configuração e `sigsuspend` realiza atomicamente a troca
da máscara e a espera, evitando a condição de corrida existente entre testar
uma condição e chamar `pause`.

Nos testes, foram enviados os três sinais tanto pelo emissor quanto pelo
comando `kill`. Em espera ativa, o laço permaneceu executável e apresentou
[PREENCHER]% de CPU. Em espera bloqueante, o processo dormiu até receber um
sinal e apresentou aproximadamente [PREENCHER]% de CPU. Assim, o bloqueio é
mais eficiente quando não há trabalho útil durante a espera.

## 3. Produtor-consumidor com pipe

O programa cria o pipe antes do `fork`, de modo que pai e filho herdam seus
descritores. O pai atua como produtor, fecha a ponta de leitura e gera a
sequência `Ni = Ni-1 + delta`, com `N0 = 1` e `delta` pseudoaleatório no
intervalo de 1 a 100. O filho fecha a ponta de escrita, recebe os números,
testa a primalidade e imprime o resultado.

Cada número é convertido em uma mensagem de 20 bytes. Funções auxiliares de
leitura e escrita tratam interrupções e transferências parciais, garantindo que
uma mensagem completa atravesse o pipe. Após [EXEMPLOS TESTADOS] valores, o
produtor envia zero, fecha sua ponta e espera o filho com `waitpid`. O consumidor
interpreta zero como marcador de término.

## 4. Produtor-consumidor com threads e semáforos

Foi usado um buffer circular de tamanho `N`, com índices independentes de
entrada e saída. Três semáforos implementam a coordenação:

| Semáforo | Valor inicial | Função |
|---|---:|---|
| `empty_slots` | `N` | conta posições livres e bloqueia produtoras no buffer cheio |
| `full_slots` | `0` | conta posições ocupadas e bloqueia consumidoras no buffer vazio |
| `mutex` | `1` | serializa buffer, índices, ocupação e log |

Um contador atômico distribui exatamente `M=100000` tarefas entre as `Np`
produtoras. Depois que elas terminam, a thread principal insere um zero para
cada consumidora. Como o buffer é FIFO, todos os dados anteriores são retirados
antes dos marcadores, e nenhuma consumidora permanece bloqueada. A primalidade
é testada fora da região crítica, reduzindo a contenção. A ocupação é registrada
após cada produção e consumo normal, totalizando `2M` amostras por cenário.

## 5. Resultados e análise

Cada configuração foi executada 10 vezes nesta máquina de teste. A Tabela 1
resume o menor tempo médio observado para cada tamanho de buffer; os 28 valores
e respectivos desvios estão em `resultados/medias.csv`. A Figura 1 corresponde
a `graficos/tempos_medios.png`.

| N | Melhor (Np,Nc) | Tempo médio (s) | Desvio padrão (s) |
|---:|---:|---:|---:|
| 1 | (1,1) | 1,383945 | 0,250589 |
| 10 | (1,1) | 0,165048 | 0,034507 |
| 100 | (1,1) | 0,058734 | 0,007757 |
| 1000 | (1,2) | 0,046430 | 0,031753 |

**Figura 1 - Tempo médio em função de `(Np,Nc)` para cada capacidade `N`.**

![Tempo médio](../graficos/tempos_medios.png)

Os dados mostram que mais threads não implicaram menor tempo. Com `N=1`, todas
as configurações ficaram entre 1,38 s e 1,81 s porque o buffer força forte
alternância e bloqueio. A passagem para `N=10` reduziu muito o tempo em `(1,1)`,
e `N=100` trouxe novo ganho. Entre `N=100` e `N=1000`, o ganho se estabilizou:
em `(1,1)`, as médias foram 0,0587 s e 0,0666 s. O melhor valor global apareceu
em `N=1000`, `(1,2)`, com 0,0464 s, mas com desvio relativamente alto.

Configurações com quatro ou oito threads ficaram próximas de 0,4-0,5 s para
buffers maiores, acima das configurações mais simples. Nesta máquina e para
esta carga, o custo de contenção nos semáforos, escalonamento e trocas de
contexto superou o benefício do paralelismo adicional. Também se observa que,
quando `Nc=1`, acrescentar produtoras não acelera o teste de primalidade, pois
ele continua concentrado em uma única consumidora.

**Figura 2 - Ocupação representativa do buffer para `N=1000`, `(1,2)`.**

![Ocupação](../graficos/ocupacao_N1000_Np1_Nc2.png)

Nos gráficos de ocupação, `N=1` alterna necessariamente entre zero e um. Com
buffers maiores, aparecem rajadas de enchimento e esvaziamento. Cenários com
mais produtoras tendem a pressionar a ocupação para cima; cenários com mais
consumidoras retiram itens com maior concorrência e tendem a manter mais espaço
livre. A série de `N=1000`, `(1,2)`, por exemplo, chega perto da capacidade no
início e depois passa a maior parte da execução em níveis menores, mostrando
que a relação entre taxas de produção e consumo varia durante a execução.

## 6. Conclusão

As implementações demonstraram a diferença entre comunicação assíncrona por
sinais, fluxo unidirecional entre processos por pipe e coordenação de memória
compartilhada entre threads. Os semáforos contadores impediram acesso quando o
buffer estava cheio ou vazio, enquanto o semáforo binário protegeu o estado do
buffer contra condições de corrida. Os resultados mostraram que aumentar o
buffer de 1 para 100 reduziu o custo de sincronização, mas que o crescimento do
número de threads não gerou aceleração automática. O melhor equilíbrio medido
foi um buffer grande com uma produtora e duas consumidoras; entretanto, como os
tempos dependem do hardware e do escalonador, o script permite repetir todo o
estudo de forma reproduzível.
