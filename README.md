# roboclone

Lê o indicador de posição ("Qtd", ex.: `1C`/`2V` — ver
`imagem/boleta.png`) do Profit na conta de ORIGEM (a que uma automação
rodando num servidor da B3 opera via ProfitDLL) e replica o comando
equivalente — compra, venda ou zerar — numa segunda janela do Profit,
numa conta SIMULADORA, via atalho de teclado (`ALT+C`/`ALT+V`/`ALT+A`).
Tudo na mesma máquina, nesta primeira versão (ver `CLAUDE.md` para o
contexto completo e as opções de rede descartadas/adiadas).

## Por que não lê pela ProfitDLL nem por UI Automation

- **ProfitDLL**: a conta de origem já está ocupada pela sessão da
  automação que roda no servidor — não dá pra logar de novo nela
  localmente só pra leitura.
- **UI Automation** (acessibilidade do Windows): testado ao vivo em
  11/09/2026 contra o ProfitPro 5.0.4.23 — a grade "Executadas" e os
  painéis de negociação são controles Delphi customizados, sem nenhum
  filho/pattern acessível. Não dá pra ler texto de célula por aí.

A via que sobra é ler por PIXEL, sem OCR de texto.

## Por que ler o badge de posição, e não a grade "Executadas"

Primeira versão lia o LADO (C/V) de cada linha nova na grade Executadas e
inferia abertura/reforço/zeragem contando localmente. Problema apontado
pelo dono (11/09/2026): se uma saída sai como VÁRIAS execuções parciais
em vez de uma execução só do tamanho da posição inteira, esse contador
local dessincroniza (ex.: comprado 2, sai em duas vendas de 1 — a
primeira já seria lida como "zerou", errado).

O indicador de posição do próprio Profit (badge "Qtd", `imagem/boleta.png`)
mostra o estado REAL e atual da posição — só aparece vazio/zerado quando
a posição de fato zerou, não importa quantas execuções levou. É a fonte
da verdade, em vez de inferir.

## Lógica de sinal (`src/sinal.h`)

Pura, sem tela — recebe o estado ANTERIOR e o NOVO (já classificados por
quem lê a tela) e devolve os comandos a mandar:

| estado anterior | estado novo | comando(s) |
|---|---|---|
| flat | comprado | COMPRA |
| flat | vendido | VENDA |
| comprado | comprado (badge mudou, ex. 1C→2C) | COMPRA (reforço) |
| vendido | vendido (badge mudou, ex. 1V→2V) | VENDA (reforço) |
| comprado ou vendido | flat | ZERAR |
| comprado | vendido (virada direta) | ZERAR, depois VENDA |
| vendido | comprado (virada direta) | ZERAR, depois COMPRA |

## Leitura do badge (`src/captura_tela.h`, `src/main.cpp`)

Duas regiões, não uma (mudança de 11/09/2026 — achado ao vivo: 1V → 2V
não disparava reforço na primeira versão):

- **badge inteiro** (número + letra): só serve pra perceber "mudou algo"
  e decidir se **está FLAT**, comparando contra a referência de vazio. Um
  badge vazio tem aparência bem diferente de "tem alguma coisa", então
  funciona bem pra essa distinção binária.
- **só a letra** (C/V, sem o número): decide o **lado** quando não está
  flat. O número muda de forma quando a quantidade muda (1→2→3...), o que
  fazia o badge inteiro "1V" parecer diferente demais de "2V" pra bater
  com a referência — a letra sozinha não muda de forma com a quantidade,
  então reforço (mesmo lado, número mudou) não se confunde com "não
  reconheci".

Cada comparação é **bitmap inteiro** da região (pixel a pixel, soma de
diferenças absolutas) contra as referências capturadas na calibração, não
só cor média — importa porque o badge de comprado e o de vendido podem
ter o MESMO fundo, só mudando a letra.

Não lê o número exato (1, 2, 3...) — só se mudou mantendo o mesmo lado
(reforço) ou não. O "tamanho da posição" que aparece no `debug`/`rodar` é
um **contador interno** (começa em 1 na abertura, +1 a cada reforço
reconhecido, volta a 0 ao zerar) — não vem de ler o número na tela.

## Duas janelas do Profit abertas ao mesmo tempo: origem por POSIÇÃO, destino por IDENTIDADE

A leitura da origem usa um retângulo FIXO de coordenadas de tela (da
calibração) — não sabe de quem é aquele pedaço de tela. O envio pro
destino usa a janela escolhida por clique (identidade, não posição). Isso
cria um risco: se a janela de destino cobrir o mesmo pedaço de tela onde
a origem foi calibrada, o programa leria o badge ERRADO (o do destino),
podendo reagir às próprias ordens que ele mesmo mandou.

Por isso `debug` e `rodar` agora também pedem, logo no início, um clique
pra confirmar qual é a janela de ORIGEM — e a cada leitura conferem se a
janela que está de fato naquele pedaço de tela ainda é essa mesma janela;
se não for, avisa no console e ignora a leitura em vez de agir errado.

**Ainda assim, mantenha as duas janelas do Profit lado a lado, sem
sobrepor uma a outra**, durante a operação — a checagem evita agir errado,
mas não substitui um layout de tela que não deixa uma janela tampar a
outra.

## Envio do atalho (`src/atalho.cpp`)

`PostMessage`/`WM_SYSKEYDOWN`+`WM_SYSKEYUP` direto no HWND da janela
escolhida — não depende da janela estar em foco (achado validado ao vivo
em 15/08/2026 no `b3_money_copy`, ver
`4q-winfut/b3_money_copy/README.md`). A sequência exata de eventos aqui é
uma reconstrução a partir da descrição documentada lá (a ferramenta de
teste original não foi commitada) — **valide ao vivo contra uma conta
SIMULADORA antes de confiar em produção**. Espaçamento mínimo de 200ms
entre comandos copiados (mesmo achado: o Profit recusa com "Alerta de
envio duplicado" se dois atalhos saírem rápido demais).

**Achado 11/09/2026 (teste ao vivo, `rodar` não estava mandando nada)**: o
Profit é um app MDI — a janela que o clique escolhia
(`GetAncestor(GA_ROOT)`) subia até o FRAME externo do aplicativo (ex.
"ProfitPro - 5.0.4.23 - Registrado"), não até o painel MDI específico
(a boleta/DOM) que o Profit considera "ativo" internamente pra processar
o atalho. `src/janela_alvo.cpp` agora sobe pela cadeia de pais só até
achar o filho direto de uma janela de classe `MDIClient` — esse filho é a
janela MDI real, e é isso que devemos mandar pro `PostMessage`. Ainda não
confirmado ao vivo se isso resolve sozinho — testar com
`roboclone.exe testaratalho` (abaixo) antes de assumir que já funciona.

Se mesmo apontando pra janela certa o `PostMessage` continuar sem efeito,
tem um fallback pronto: `enviarAltTeclaComFoco()` (`fc`/`fv`/`fa` no
`testaratalho`) usa `SendInput` de verdade (rouba o foco com
`SetForegroundWindow`, manda a tecla, devolve o foco) — é o mecanismo que
o `b3_money_copy` original validou em produção. Efeito colateral: a
janela alvo pisca por cima na hora do envio.

### `testaratalho` — depurar o envio isolado, sem calibração nem leitura de tela

```
roboclone.exe testaratalho
```

Escolhe a janela por clique (mostra classe + título, útil pra conferir se
pegou o painel certo) e depois aceita comandos no console:

- `c` / `v` / `a` — manda via `PostMessage` (sem foco)
- `fc` / `fv` / `fa` — manda via `SendInput` (com foco, pisca a janela)
- `sair` — termina

Use isso pra descobrir rápido qual dos dois mecanismos realmente funciona
contra o seu Profit antes de rodar o fluxo completo.

## Uso

```
build.bat

roboclone.exe calibrar
roboclone.exe debug
roboclone.exe rodar
```

### `calibrar`

1. Clique no canto superior esquerdo, depois no inferior direito, do
   **badge inteiro** de posição ("Qtd", o retângulo tipo `1C` — ver
   `imagem/boleta.png`).
2. Clique no canto superior esquerdo, depois no inferior direito, **só da
   LETRA** (C ou V) dentro desse badge, sem pegar o número — normalmente
   o caractere mais à direita.
3. Confirma (ENTER) com a posição **zerada/flat**.
4. Compra 1 contrato a mercado (fica comprado), confirma (ENTER).
5. Zera e vende 1 contrato a mercado (fica vendido), confirma (ENTER).

Os passos 3-5 fazem operação de verdade — **use a conta SIMULADORA** pra
calibrar, mesmo que a leitura em produção depois seja de outra conta.

Salva `calibracao.cfg` (as 2 regiões + tolerâncias, texto) e
`calibracao.cfg.refs` (os bitmaps de referência, binário), os dois ao
lado do executável. O programa avisa no console se as referências
saírem parecidas demais entre si (provável clique no lugar errado, ou a
região da letra pegou o número junto).

**Precisa recalibrar se**: a janela do Profit mudar de posição na tela,
for redimensionada, ou o zoom/tema mudar (a região é guardada em
coordenadas absolutas de tela).

### `debug` (usar antes do `rodar`, pra validar a leitura)

Não manda nenhum atalho — em vez de mandar ordem, escreve num bloco de
notas (escolhido por clique, igual à janela de destino) o rótulo do que
foi lido, junto com o horário:

| transição | rótulo escrito |
|---|---|
| abre comprado | `C` |
| abre vendido | `V` |
| reforço comprado | `CC` |
| reforço vendido | `VV` |
| zeragem/alvo | `Z` |
| virada (comprado→vendido ou vice-versa) | `Z` seguido de `C`/`V` |

Rode `roboclone.exe debug`, aponte pro bloco de notas, e vá mandando
ordens manualmente na conta calibrada — cada mudança de posição deve
aparecer como uma linha `HH:MM:SS.mmm ROTULO (tam N)` no bloco de notas
(`N` = tamanho da posição pelo contador interno, não lido da tela — ver
seção acima). No console (`[debug] ...`) também aparece o comando
equivalente (COMPRA/VENDA/ZERAR) que seria mandado no `rodar`. Confira se
bate com o que você mandou antes de confiar no `rodar`.

A escrita usa UI Automation (não simula tecla) — funciona com qualquer
campo de texto que exponha o padrão Value/Text de acessibilidade (bloco
de notas do Windows serve; os painéis do Profit NÃO servem pra isso, só
pra leitura por pixel).

### `rodar`

Carrega a calibração, pede pra clicar na janela de ORIGEM, depois na de
DESTINO, e pergunta o **espaçamento mínimo** em ms (ENTER usa o padrão,
200 — o que resolveu o "Alerta de envio duplicado" no `b3_money_copy`).
Esse espaçamento é esperado **sempre, antes de qualquer envio** — não só
entre ordens em sequência, mas também do instante em que a leitura
detecta a mudança até o envio de verdade (importante ao testar com origem
e destino na MESMA janela do Profit, pra não se confundir com o efeito do
próprio envio). Só depois começa a vigiar o badge calibrado (~10ms de
intervalo) e manda o(s) atalho(s) correspondente(s).

Não pede mais confirmação de "conta simuladora" — permite origem e
destino serem a mesma janela (útil pra testar), só avisa no console
quando isso acontece. **Continua sendo sua responsabilidade não apontar
pra uma conta real** quando origem e destino forem janelas diferentes.

O `testaratalho` também pergunta esse espaçamento antes de começar — só
vale pros comandos `c`/`v`/`a` (via `PostMessage`); `fc`/`fv`/`fa` (via
`SendInput` com foco) não usam esse espaçamento.

## Limitações conhecidas / próximos passos

- Calibração é em coordenadas absolutas de tela — quebra se a janela de
  origem mover. Melhoria futura: capturar relativo ao client area da
  janela (`GetClientRect`/`ClientToScreen`), recalculando a cada poll.
- Não distingue quantidade exata (1C vs 2C vs 3C) — só "mesmo lado,
  mudou" (reforço); o tamanho mostrado no debug é contado por software,
  não lido da tela.
- O clique do **badge inteiro** (passo 1 da calibração) precisa cobrir SÓ
  o badge "Qtd", sem incluir campos vizinhos que mudam sozinhos com o
  preço (ex.: "Resultado", "Res. Aberto") — se incluir, qualquer variação
  desses campos dispara um "reforço" falso (o badge inteiro só é usado
  pra detectar "mudou"/"ficou flat", então qualquer mudança nele conta).
- HWND da janela de destino não é persistido entre execuções — normal,
  precisa clicar de novo toda vez que o Profit de destino reiniciar
  (mesma limitação documentada no `b3_money_copy`).
- Sequência `PostMessage` do atalho ainda não validada ao vivo nesta
  pasta (só a lógica do `b3_money_copy` original foi validada, com
  `SendInput`, não `PostMessage`) — testar contra o simulador antes de
  operar de verdade.
- Se no futuro as duas contas forem para máquinas diferentes, entra a
  parte de rede (VPN/socket) já discutida no `CLAUDE.md` — o envio do
  atalho vira "manda pela rede pro processo que fica na máquina do
  destino e faz o PostMessage lá", sem mudar a lógica de sinal.
