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

Pura, sem tela — recebe a posição ANTERIOR e a NOVA (inteiros com sinal:
positivo = comprado N, negativo = vendido N, 0 = flat; já classificados
por quem lê a tela) e devolve os comandos a mandar, um por unidade de
diferença:

| posição anterior | posição nova | comando(s) |
|---|---|---|
| 0 (flat) | +N (comprado) | COMPRA × N |
| 0 (flat) | −N (vendido) | VENDA × N |
| +A (comprado) | +B, B>A (reforço) | COMPRA × (B−A) |
| +A (comprado) | +B, B<A (**redução parcial**) | VENDA × (A−B) |
| −A (vendido) | −B, B>A (reforço) | VENDA × (B−A) |
| −A (vendido) | −B, B<A (**redução parcial**) | COMPRA × (A−B) |
| qualquer não-zero | 0 | ZERAR |
| comprado | vendido (virada direta) | ZERAR, depois COMPRA/VENDA × N |

A distinção reforço vs. redução parcial existe pra decidir o RÓTULO
(debug) e pra nunca precisar de "zerar" numa saída que só reduziu — a
TECLA enviada já é naturalmente a certa nos dois casos (reduzir um
comprado manda venda, igual abrir um vendido; é só uma ordem a mercado
normal, o Profit resolve sozinho).

## Leitura do badge (`src/captura_tela.h`, `src/config.h`, `src/main.cpp`)

Uma referência de bitmap **por quantidade exata**: vazio (0), comprado
1..N, vendido 1..N (mudança de 15/09/2026 — antes eram só 3 referências
genéricas — vazio/compra/venda —, o que causava dois problemas: 1V→2V às
vezes não disparava reforço porque o dígito muda de forma; e uma REDUÇÃO
parcial, ex. 7C→6C, ficava indistinguível de reforço porque o lado/letra
não muda quando a posição só diminui sem zerar).

Cada leitura compara o bitmap capturado contra TODAS as referências
calibradas (pixel a pixel, soma de diferenças absolutas) e escolhe a mais
parecida, desde que dentro da tolerância — a quantidade (com sinal) da
referência vencedora é a posição lida. Uma posição além do que foi
calibrado (ex. 8 contratos, se só calibrou até 7) não bate com nenhuma
referência e é ignorada com aviso — calibre até um nível confortavelmente
acima do que você espera usar.

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

0. **Se já existir uma calibração de badges salva**: pergunta se quer
   RECALIBRAR (refazer a construção 1..N) ou manter a que já está salva.
   Respondendo não, o resto dos passos é pulado.
1. Responde até quantos contratos calibrar de cada lado (ENTER usa o
   padrão, 5 — ou seja, reconhece de 1 a 5 comprado e de 1 a 5 vendido).
2. Clique no canto superior esquerdo, depois no inferior direito, do
   **badge inteiro** de posição ("Qtd", o retângulo que mostra `1C` ou o
   `-` quando está zerado — ver `imagem/boleta.png`). Aponte SÓ pro
   badge — não inclua campos vizinhos que mudam sozinhos com o preço
   (ex. "Resultado", "Res. Aberto"), senão qualquer variação de preço
   vira leitura falsa. **Se o nível máximo for 10 ou mais**: o badge
   fica um pouco mais LARGO com 2 dígitos (achado ao vivo, 15/09/2026) —
   como a região é fixa (não redimensiona sozinha depois), desenhe-a com
   folga mesmo que agora o badge esteja mostrando só 1 dígito ou vazio,
   senão o "10" pode ficar cortado quando a posição passar de 9.
3. Confirma (ENTER) com a posição **zerada/flat** (o badge deve mostrar
   `-`) — esse é o único passo que pede ENTER, porque é o ponto de
   partida (sem "mudança" nenhuma pra esperar se você já estiver flat).
4. Compra 1 contrato a mercado — **não precisa confirmar nada**: o
   programa fica vigiando o badge e detecta sozinho quando você fizer a
   operação, espera acomodar (~300ms) e já captura, pedindo a próxima
   automaticamente. Repete até o nível máximo escolhido.
5. Zera a posição de teste — detectado automaticamente, igual acima.
6. Vende 1 contrato a mercado — detectado automaticamente, repete até o
   nível máximo.

Do passo 3 em diante faz operação de verdade — **use a conta SIMULADORA**
pra calibrar, mesmo que a leitura em produção depois seja de outra conta.
Ajuste o nível pro tamanho de posição que você realmente espera usar
(calibrar até 10 não custa muito mais que até 5, já que agora é só ir
fazendo as operações em sequência, sem parar pra confirmar cada uma).
ESC a qualquer momento cancela a calibração.

Salva `calibracao.cfg` (região + tolerância + contagem, texto) e
`calibracao.cfg.refs` (os bitmaps de referência, binário), os dois ao
lado do executável. O programa avisa no console qual par de quantidades
ficou mais parecido entre si — se a diferença for pequena demais, dois
níveis vizinhos (ex. 3C e 4C) podem se confundir; recalibre apontando
mais preciso pro badge.

**Precisa recalibrar (a construção completa 1..N dos dois lados) se**: a
janela do Profit for redimensionada, ou o zoom/tema mudar (o formato do
badge muda, as referências capturadas deixam de bater). Só **mudar de
lugar** na tela (mesmo tamanho/zoom/tema) não precisa recalibrar nada —
ver a seção seguinte.

### Localização automática do badge (sem perguntar sobre Replay)

**Mudança de design de 15/09/2026** (substitui três tentativas
anteriores — pergunta única, vigiar duas posições, pergunta fixa por
sessão — todas com algum problema real no uso ao vivo): em vez de
perguntar/lembrar se o Replay está ligado, medir deslocamento, ou pedir
pra confirmar/reancorar a posição, o programa **localiza o badge
sozinho**, sempre, sem perguntar nada.

Ideia: a referência "vazio" (o `-` que aparece quando a posição está
flat) é só mais uma referência normal, tão boa quanto "1C"/"2V" pra
achar onde o badge está fisicamente na tela AGORA — não importa se foi o
Replay, o zoom, ou só a janela que deslocou tudo.

`debug` e `rodar`, logo depois de carregar a calibração, chamam
`localizarBadge()`:

1. Procura numa vizinhança AMPLA (±45px em x e y) ao redor da última
   posição conhecida (a salva em `calibracao.cfg`) qual ponto dá a
   MENOR diferença de bitmap contra QUALQUER referência calibrada.
2. Se achar algo dentro da tolerância, usa essa posição — mostra no
   console o que achou (ex. "FLAT" ou "COMPRADO 2") e, se a posição
   mudou desde a última vez, pergunta se quer salvar a atualização.
3. Se não achar nada (a janela mudou de monitor, por exemplo, ou saiu
   muito da vizinhança de 45px), pede **um clique aproximado** — não
   precisa ser exato, só perto de onde o badge está agora — e tenta a
   mesma busca ampla a partir desse ponto.

**Também acontece em tempo real, durante a leitura** (`aguardarProximaPosicao`
em `main.cpp`): se o badge mudar mas não bater com nenhuma referência na
posição atual, o programa tenta se relocalizar sozinho na mesma
vizinhança ampla antes de só avisar e continuar — então ligar/desligar o
Replay **no meio** de uma sessão de `debug`/`rodar` já em andamento
também é coberto, sem precisar reiniciar nada.

Nenhuma pergunta sobre Replay existe mais em lugar nenhum do programa.

**Desempenho da busca (achado ao vivo, 15/09/2026)**: a primeira versão
capturava a tela (BitBlt) uma vez POR POSIÇÃO candidata — pra um raio de
45px isso é 8281 capturas separadas, cada uma com overhead de sistema,
levando dezenas de segundos e parecendo travado ("buscando
infinitamente"). Corrigido: captura a área de busca inteira **uma vez
só** e desliza a janela de comparação dentro desse buffer já em memória
(só `memcpy` + soma de diferenças, sem tocar a tela de novo) — termina
em poucos milissegundos.

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
| **redução parcial** de comprado (ex. 7C→6C, manda venda) | `v` (minúsculo) |
| **redução parcial** de vendido (ex. 7V→6V, manda compra) | `c` (minúsculo) |
| zeragem total | `Z` |
| virada (comprado→vendido ou vice-versa) | `Z` seguido de `C`/`V` (× N se abrir mais de 1) |

Rode `roboclone.exe debug`, aponte pro bloco de notas, e vá mandando
ordens manualmente na conta calibrada — cada mudança de posição deve
aparecer como uma linha `HH:MM:SS.mmm ROTULO` no bloco de notas. No
console (`[debug] ...`) também aparece a posição antes/depois (ex.
"COMPRADO 7 -> COMPRADO 6") e o comando equivalente (COMPRA/VENDA/ZERAR)
que seria mandado no `rodar`. Confira se bate com o que você mandou antes
de confiar no `rodar`.

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

**Janela de teste durante o `rodar`**: assim que a leitura começa, abre
uma janelinha com 3 botões — **Compra (ALT+C)**, **Venda (ALT+V)**,
**Zerar (ALT+A)** — que mandam o atalho na hora pra janela de DESTINO
escolhida, com o mesmo espaçamento configurado, sem parar a leitura
automática (rodam em threads separadas). Útil pra confirmar a qualquer
momento que o atalho ainda está chegando na janela certa. Fechar essa
janela encerra o `rodar` (junto com CTRL+C no console). Não é "always on
top" (achado ao vivo, 15/09/2026: sendo topmost ela podia cair em cima do
pedaço de tela calibrado da origem e travar a leitura pra sempre) e nasce
no canto inferior direito da tela — se mesmo assim cobrir alguma janela
do Profit, clique na janela do Profit pra trazê-la de volta pra cima.

## Trava de segurança: tamanho máximo de posição

**Testar com origem e destino na MESMA janela cria um loop de verdade**:
cada atalho mandado executa uma ordem real ali, que muda a posição, que é
lida de novo como um novo sinal, que manda outro atalho — sem limite.
Isso não é bug de calibração, é matemática — só acontece porque as duas
janelas são a mesma. Pra validar o ciclo completo (leitura + envio)
de verdade, use duas janelas: uma só de origem (só você opera nela
manualmente) e outra só de destino (só recebe atalho).

Como rede de segurança pra esse (e qualquer outro) cenário de loop, o
`rodar` pergunta um **tamanho máximo de posição** (ENTER usa o padrão, 5)
— se o contador interno passar disso, o envio automático **para**
imediatamente (a leitura continua rodando, mas não manda mais atalho
sozinho) e avisa bem visível no console. Os botões da janela de teste
continuam funcionando normalmente pra você zerar manualmente.

## Limitações conhecidas / próximos passos

- A busca automática (ver "Localização automática do badge" acima) tem
  um raio de ±45px — se a janela mudar de posição mais do que isso (ex.
  trocar de monitor), o `debug`/`rodar` pedem um clique aproximado pra
  recomeçar a busca a partir dali; a auto-relocalização em tempo real
  durante a leitura usa o mesmo raio e não pede clique (só ignora com
  aviso se não achar nada).
- Calibração é em coordenadas absolutas de tela como ponto de partida da
  busca — se a janela de origem mudar de posição, a localização
  automática (ver acima) resolve sozinha na maioria dos casos, dentro do
  raio de busca. Melhoria futura: capturar relativo ao client area da
  janela (`GetClientRect`/`ClientToScreen`), recalculando a cada poll,
  pra não precisar nem da busca ampla ocasional.
- Posição além do nível máximo calibrado (ex. 8 contratos, calibrado só
  até 7) não bate com nenhuma referência — fica ignorada com aviso no
  console em vez de agir errado. Recalibrar com um nível maior resolve.
- O clique do **badge** (passo 1 da calibração) precisa cobrir SÓ o badge
  "Qtd", sem incluir campos vizinhos que mudam sozinhos com o preço (ex.:
  "Resultado", "Res. Aberto") — se incluir, qualquer variação de preço
  vira uma leitura que não bate com nenhuma referência exata (fica só
  como aviso ignorado agora, em vez de virar reforço falso como na
  versão anterior — mas ainda assim é ruído a menos).
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
