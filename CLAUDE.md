# CLAUDE.md — roboclone

Contexto para qualquer sessão do Claude que trabalhe neste repositório.

## O que é este projeto

Automação genérica de **sinal de compra/venda entre duas máquinas via
internet, com o menor delay possível**: uma máquina gera o sinal (comprou/
vendeu/zerou), a segunda recebe pela rede o mais rápido possível e manda a
ordem de lá. Idioma do projeto: **português (PT-BR)**. O dono é o Vinícius,
trader varejista.

## Contexto / origem

Veio de uma conversa no repo `4q-winfut` (projeto de projeção de níveis do
WINFUT/B3, método "4Q"). Lá existe um dash de trading em C++
(`b3_money_copy`) com uma feature chamada **"Copiar"**: replica ordens numa
**segunda conta do Profit** sem precisar de uma segunda licença da
ProfitDLL (que só loga numa conta por sessão), mandando um atalho de
teclado (`ALT+C` compra / `ALT+V` venda / `ALT+A` zera, via `SendInput`)
pra uma segunda janela do Profit aberta manualmente — como se um humano
estivesse digitando.

Isso funciona, mas só entre duas janelas **na mesma máquina** (é troca de
mensagem do Windows local — `SendInput`/`PostMessage`, não passa pela
rede). Mesmo assim tem uma limitação: `SendInput` só entrega a tecla pra
janela em primeiro plano no momento do envio (testamos `PostMessage` +
`EnumWindows` como alternativa que não depende de foco, e funcionou — mas
continua restrito à mesma máquina).

Esse projeto (`roboclone`) é o próximo passo: **e se as duas contas
rodarem em computadores diferentes**, conectados pela internet, em vez de
duas janelas na mesma máquina? Isso já não é mais troca de mensagem do
Windows — precisa de comunicação de rede de verdade, com o mínimo de
latência possível (é uma decisão de entrada/saída de operação, delay
custa dinheiro).

## Opções de arquitetura discutidas (nenhuma implementada ainda)

1. **VPN mesh (Tailscale ou WireGuard) + socket TCP direto entre as duas
   máquinas — RECOMENDADA.** A VPN resolve NAT sem precisar de servidor
   intermediário (Tailscale é de graça e configura sozinho, sem precisar
   abrir porta no roteador). Por cima da VPN, um socket TCP simples
   mandando uma linha de texto tipo `COMPRA;171970;09:01:00` (mesma
   convenção `;`-separada usada no `4q-winfut`). Latência: só o RTT real
   da internet entre as duas máquinas + overhead mínimo da VPN (poucos
   ms) — o menor delay possível sem hardware dedicado.
2. **WebSocket via relay** (uma VPS pequena, ~$5/mês): as duas máquinas
   conectam PRA FORA no relay (não precisa abrir porta em nenhuma das
   duas), ele repassa a mensagem entre elas. Adiciona um salto extra (a
   latência até a VPS), mas evita configurar VPN nas duas pontas.
3. **Descartado, latência incompatível com o objetivo**: qualquer coisa
   baseada em HTTP polling, webhook, Telegram/e-mail ou serviço de
   terceiros — somam de centenas de ms a segundos.

## Frente de trabalho ativa: "ler o Profit de origem por tela" (mesma máquina)

Decisão de 11/09/2026: antes do problema de rede (opções acima, ainda não
implementadas), surgiu uma necessidade mais imediata e **sem rede
nenhuma**: o dono tem uma automação própria rodando num **servidor da
B3**, que manda ordens via ProfitDLL numa conta real. As execuções dessa
automação só aparecem localmente porque ele tem uma janela do Profit
aberta, logada na MESMA conta (espelho visual, sem API — a DLL já está
ocupada pela automação do servidor). O pedido: ler essa operação
(compra/venda/reforço/alvo) na tela e replicar o comando (compra/venda/
zerar) por atalho de teclado numa SEGUNDA janela do Profit, numa conta
SIMULADORA — mesma técnica do "Copiar" do `b3_money_copy`
(`4q-winfut/b3_money_copy`), só que a ORIGEM agora é lida da tela em vez
de já ser conhecida internamente pelo motor.

Investigado e decidido nesta sessão (ver `roboclone.exe`, pasta `src/`):

- **Sem ProfitDLL local pra ler**: a conta de origem já está com sessão
  ocupada pela automação do servidor.
- **Sem UI Automation**: testado ao vivo (11/09/2026, ProfitPro 5.0.4.23)
  — a grade "Executadas" é um `TGridView` Delphi customizado, zero
  filhos/patterns acessíveis. Só sobra ler por PIXEL.
- **Sem OCR de texto**: em vez de ler a grade "Executadas", a leitura usa
  o indicador de posição do próprio Profit (badge "Qtd", ex. `1C`/`2V` —
  ver `imagem/boleta.png`), comparando o bitmap inteiro contra 3
  referências calibradas (vazio/comprado/vendido). Mudança de desenho em
  11/09/2026: a 1ª versão lia o LADO de cada linha nova na grade e
  inferia a posição contando localmente, mas o dono apontou que isso
  dessincroniza se uma saída virar várias execuções parciais em vez de
  uma só do tamanho da posição inteira — o badge de posição é a fonte da
  verdade (só fica vazio quando a posição REALMENTE zerou), sem esse
  risco.
- **Linguagem: C++** — pedido explícito do dono foi "o que funciona mais
  rápido com menos delay", e o app reaproveita a técnica de envio de
  atalho já validada ao vivo no `b3_money_copy` (`PostMessage`/
  `WM_SYSKEYDOWN`, não depende de foco — ver o README de lá).
- **Escopo desta primeira versão: mesma máquina**, as duas janelas do
  Profit (origem real + destino simulador) — a parte de rede (VPN/socket,
  opções no topo deste arquivo) fica pra quando/se as contas forem para
  máquinas diferentes.

Detalhes de uso, lógica de sinal e limitações conhecidas: ver
`README.md` nesta pasta. O envio de atalho via `PostMessage`
(reconstruído a partir da documentação do `b3_money_copy`, não copiado de
código já testado) ainda **não foi validado ao vivo** contra um Profit de
verdade.

**Teste ao vivo de 11/09/2026** (modo `debug`, dono testando na conta
simuladora): achado que 1V→2V (reforço) não disparava nada. Causa:
comparar o badge de posição INTEIRO contra a referência de "1V" falhava
porque o dígito muda de forma (1→2). Corrigido separando em 2 regiões —
badge inteiro só pra decidir FLAT vs não-flat, e uma região estreita só
da LETRA (C/V, sem o número) pra decidir o lado, que não muda de forma
com a quantidade. Calibração agora pede 4 cliques (badge inteiro +
letra) em vez de 2. Debug também passou a mostrar o tamanho da posição
(contador interno, não lido da tela) e o comando equivalente que seria
mandado no `rodar`.

**Teste ao vivo de 11/09/2026 (2º achado, `rodar` não mandava ALT+C/V/A
nenhum)**: suspeita forte é que o picker de janela por clique
(`GetAncestor(GA_ROOT)`) subia até o FRAME externo do Profit (app MDI),
não até o painel MDI real que processa o atalho. Corrigido em
`src/janela_alvo.cpp` pra subir só até o filho direto de uma janela
`MDIClient`. Ainda não confirmado ao vivo se resolve sozinho — adicionado
`roboclone.exe testaratalho` (testa o envio isolado, sem calibração) e um
2º mecanismo de envio (`enviarAltTeclaComFoco`, `SendInput` com
`SetForegroundWindow` — o que o `b3_money_copy` original validou em
produção) como fallback caso o `PostMessage` continue sem efeito mesmo na
janela certa.

**Confirmado ao vivo (11/09/2026, via `testaratalho`)**: com a correção
do picker MDI, os DOIS mecanismos de envio funcionaram (`PostMessage`
sem foco E `SendInput` com foco) — dono confirmou preferência pelo sem
foco (`enviarAltTeclaComEspacamento`, o que o `rodar` já usa).

Ajustes finos depois da confirmação: tirada a exigência de digitar
`SIMULADOR` antes de `rodar` (o dono está testando com origem e destino
na MESMA janela do Profit por enquanto — só avisa no console quando
detecta isso, não bloqueia mais); o espaçamento mínimo entre comandos
agora é perguntado ao operador (`rodar`/`testaratalho`, ENTER = 200ms
padrão) e é esperado sempre antes de qualquer envio (não só entre ordens
em sequência), pra cobrir o caminho leitura→envio inteiro mesmo quando
origem e destino são a mesma janela.

**Teste ao vivo de 15/09/2026 (loop de posição até 27 contratos)**: dono
testou `rodar` com origem e destino na MESMA janela — o contador de
tamanho subiu até 27 sozinho (contra 3 reais na conta), porque cada
atalho mandado executa ordem de verdade ali, que muda a posição, que é
lida de novo como sinal novo, que manda outro atalho. **Não é bug de
calibração, é consequência matemática de origem == destino** — só um
teste de ponta a ponta com duas janelas diferentes valida o ciclo
completo de verdade. Duas correções ainda saíram desse teste:
1. A janelinha de teste (botões) nasceu "always on top" em posição
   padrão do Windows — podia cair em cima do pedaço de tela calibrado da
   origem e travar a leitura pra sempre (explica os avisos finais de
   "janela não é mais a esperada" no log). Corrigido: sem topmost, nasce
   no canto inferior direito.
2. Adicionada trava de segurança: `rodar` agora pergunta um **tamanho
   máximo de posição** (padrão 5) — se passar disso, o envio automático
   para (a leitura continua, só não manda mais atalho sozinho), evitando
   um loop como esse rodar sem limite de novo.

**Teste ao vivo de 15/09/2026 (2º loop, dessa vez com duas contas/janelas
DIFERENTES de verdade)**: o dono corrigiu uma suposição errada da sessão
anterior — o loop de 27 contratos NÃO foi origem reagindo ao próprio
envio (origem e destino eram janelas/contas separadas). Causa real,
inferida pelo padrão de timestamps no log (várias detecções em sequência,
no ritmo de tick de preço): a região do badge calibrada provavelmente
incluía campos vizinhos que mudam sozinhos com o preço (ex. "Resultado",
"Res. Aberto", que ficam coladinhos do "Qtd" na boleta) — qualquer tick
de preço registrava como "mudou, ainda parece o mesmo lado" e disparava
reforço falso.

Isso expôs um problema de design mais fundo, que o dono pediu pra
resolver: calibrar só 3 referências genéricas (vazio/compra/venda,
comparando só a LETRA) não sabia distinguir REFORÇO de REDUÇÃO PARCIAL
(ex. 7C→6C continua "comprado", a letra não muda) — uma redução seria
lida errado como reforço, mandando a tecla ERRADA (compra em vez de
venda). Redesenhado (15/09/2026): calibração agora captura uma referência
de bitmap **por quantidade exata** (vazio, 1..N comprado, 1..N vendido —
pergunta o N, padrão 5), comparando o BADGE INTEIRO contra todas (não
precisa mais da região separada da letra). `sinal.h` mudou de estado
enum (Flat/Comprado/Vendido) pra um INTEIRO com sinal (posição real), e
`transicao()` agora distingue reforço de redução parcial (redução manda
a tecla oposta ao lado atual, sem zerar — o Profit resolve sozinho,
igual abrir do lado oposto resolveria). Rótulos de debug novos: `c`/`v`
minúsculo pra redução parcial (maiúsculo continua abertura/reforço).
Leituras fora do que foi calibrado (ex. 8 contratos com calibração até 7)
agora ficam so' como aviso ignorado, nunca mais viram reforço/redução por
engano.

**Modo Replay do Profit (15/09/2026)**: o dono relatou que abrir o Replay
(barra amarela de reprodução de pregão passado) empurra o badge de
posição pra baixo, atrapalhando a calibração. Ele mandou 2 imagens
(com/sem Replay) pra medir o deslocamento — medi via análise de pixel
(PowerShell + System.Drawing): **24px pra baixo**, consistente em 4
colunas testadas. **Incidente**: rodei um `rm` sem necessidade nenhuma
durante a investigação e apaguei as 2 imagens (`imagem/com replay.png` e
`imagem/sem replay.png`) antes de commitar — não recuperável (arquivos
novos, nunca chegaram a entrar no git). Erro meu, sem justificativa; a
medição em si (24px) já tinha sido extraída e não foi perdida.

Implementado (não chumbando 24px no código, já que pode variar com
DPI/tema): passo opcional na calibração pra medir o deslocamento na tela
de quem está calibrando — aponta a área amarela do Replay + o novo topo
do badge com o Replay ligado. `debug`/`rodar` agora detectam sozinhos se
o Replay está ligado (comparando a cor de uma área calibrada) e trocam a
região de leitura automaticamente, sem precisar recalibrar toda vez que
ligar/desligar o Replay.

**Badge mais largo com 2 dígitos (15/09/2026)**: dono relatou que passando
de 10 contratos o badge de posição fica um pouco mais largo (cabe o 2º
dígito). Como a região capturada é FIXA (definida no clique da
calibração, não redimensiona sozinha depois), isso podia cortar o "10"
se a região fosse desenhada justa no tamanho de 1 dígito. Corrigido só
por orientação (sem mudança de lógica): `calibrar` agora pergunta o
nível máximo ANTES de pedir os cliques do badge, e avisa explicitamente
pra desenhar a região com folga quando o nível for ≥ 10.

## Estado atual

Protótipo funcional (15/09/2026): `roboclone.exe`
(`calibrar`/`debug`/`rodar`/`testaratalho`), ver seção acima e
`README.md`. O envio de atalho (ambos os mecanismos) já foi validado ao
vivo contra o Profit de verdade, com duas janelas/contas separadas
inclusive. A leitura por quantidade exata (redesenho de 15/09) e a
detecção automática de Replay ainda **não foram validadas ao vivo** —
falta recalibrar com o novo fluxo (pede N níveis por lado + passo
opcional de Replay) e confirmar no `debug` que reforço/redução/zeragem/
Replay saem certos antes de confiar no `rodar` de novo. Depois, se/quando
precisar, decidir entre VPN+TCP direto vs. relay pra levar isso pra duas
máquinas.
