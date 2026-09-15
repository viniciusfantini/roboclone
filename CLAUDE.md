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

**Calibração por detecção automática (15/09/2026)**: dono pediu pra não
precisar ficar apertando ENTER a cada nível de contrato durante a
calibração — só a operação (compra/venda/zerar) já devia bastar, o
programa detecta a mudança sozinho. Implementado (`aguardarMudancaBadge`
em `calibracao.cpp`): fica vigiando o badge (mesmo polling de 50ms usado
em produção), assim que muda espera acomodar (~300ms) e captura,
avançando pro próximo nível sozinho. Só o primeiro passo (confirmar que
está flat) continua pedindo ENTER, porque não há "mudança" pra esperar
se a posição já estiver zerada desde antes.

**Reancorar rápido a posição (15/09/2026)**: dono pediu um jeito de só
reconfirmar/reposicionar o badge (ex. se o Profit reabrir em lugar
diferente na tela) sem precisar refazer a calibração completa (1..N
contratos dos dois lados) — como as referências de bitmap não dependem
de posição de tela, só de aparência, dá pra reaproveitá-las. Implementado
`confirmarOuReancorarPosicao()` em `calibracao.cpp`: pergunta opcional no
início de `debug`/`rodar`, pede só o canto superior esquerdo (reaproveita
o tamanho já calibrado), mostra o que LERIA ali e pede confirmação
sim/não antes de adotar a nova posição, com opção de salvar pra próxima
vez. Pensado pra poder ser feito com o Replay ligado antes do pregão
abrir, mostrando um estado tipo "1C" (não flat) pra conferir de verdade.

**Simplificação: Replay decidido uma vez por sessão, não ao vivo
(15/09/2026)**: dono questionou se a detecção contínua de Replay (por
cor, a cada poll) ainda fazia sentido já que o "reancorar rápido" já
ajusta a posição no início de cada sessão. Resposta: não fazia — Replay
só é usado antes do pregão abrir, nunca liga/desliga no meio de uma
sessão de `rodar` de verdade, então checar isso ao vivo era complexidade
sem necessidade. Pior: achei um bug de interação real — reancorar com o
Replay ligado, sem o programa saber, salvava a posição DESLOCADA como se
fosse a normal, quebrando a leitura quando o Replay desligasse de
verdade pro pregão.

Simplificado: removida a deteccao continua por cor (removida a classe
`LeitorBadge`, `regiaoMarcadorReplay`/`corReplayB,G,R`/
`toleranciaCorReplay` do `Calibracao` e da calibração). Agora
`prepararRegiaoDeLeitura()` (`calibracao.cpp`) pergunta UMA vez, no
início de `debug`/`rodar`: "Replay ligado agora?" + "quer reancorar?" —
e o reancorar, se feito com Replay ligado, desconta o deslocamento
automaticamente ao salvar a posição BASE (resolve o bug de interação).
Compatível com calibrações salvas antes dessa mudança (os campos de cor
removidos só ficam ignorados ao carregar).

**Calibrar inteiro com Replay ligado (15/09/2026)**: dono quer fazer a
construção 1..N inteira (não só a medição do deslocamento) com o Replay
já ligado, pra poder calibrar fora do horário de pregão. Antes, o código
assumia que `regiaoBadge`/as referências eram sempre capturadas SEM
Replay (posição "base" fixa por convenção); com esse pedido, a base pode
ser a posição COM Replay, e operar de verdade (Replay desligado) precisa
DESCONTAR o deslocamento, não somar.

Implementado: novo campo `calibradoComReplayLigado` em `Calibracao`
(true se a construção 1..N foi feita com Replay já ligado). Pergunta
adicionada bem no início de `rodarCalibracao()` (antes dos cliques do
badge). A medição do deslocamento no fim da calibração inverte de
direção conforme essa resposta (liga+mede-descida vs desliga+mede-subida).
`prepararRegiaoDeLeitura()` usa uma formula so' (`ajusteReplay()`) que
cobre os 4 casos (base com/sem Replay × agora com/sem Replay), tanto pra
calcular a regiao ativa da sessao quanto pra normalizar a posicao
reancorada de volta pra convencao da base.

**Pular recalibração de badges se já existe uma (15/09/2026)**: dono
pediu pra `calibrar` perguntar se quer recalibrar as badges (a
construção 1..N) ou manter a que já está salva e pular pro próximo
passo — útil pra só (re)medir o deslocamento do Replay sem repetir a
construção toda. Implementado: `modoCalibrar()` agora tenta
`carregarCalibracao()` antes de chamar `rodarCalibracao()`; se já tinha
referências carregadas, pergunta antes de decidir se refaz os passos de
clicar a região + construir 1..N, ou pula direto pra seção do Replay
(reaproveitando `regiaoBadge`/`referencias`/`tolerancia`/
`calibradoComReplayLigado` já carregados).

**Separar "Replay ligado agora" de "modo de operação da sessão"
(15/09/2026)**: dono descreveu o fluxo que queria pro `debug`/`rodar`:
pergunta do Replay → registra/confere a posição do badge → pergunta se
vai CONTINUAR no Replay pra essa sessão ou ir pra janela normal → aplica
o desconto se for pra janela normal → segue com as leituras. Isso expôs
que eu tinha uma pergunta só fazendo dois papéis (interpretar o clique
do reancorar E decidir o modo de operação), quando podem ser respostas
diferentes — ex.: reancorar vendo o Replay (mais conveniente, mostra
"1C" antes do pregão), mas depois operar na janela normal de verdade.

Corrigido em `prepararRegiaoDeLeitura()`: agora são 2 perguntas
distintas — `replayAgora` (só pra interpretar o clique do reancorar,
perguntada antes dele) e `operarComReplay` (perguntada DEPOIS do
reancorar, decide de fato a região ativa da sessão via `ajusteReplay()`).
Se as duas respostas divergirem (reancorou com Replay ligado, mas vai
operar na janela normal), o desconto do deslocamento acontece do jeito
certo mesmo assim, porque a formula `ajusteReplay()` já cobria esse caso
-- só faltava a segunda pergunta existir de verdade.

**Replay pode ligar/desligar NO MEIO da sessão -- vigiar as duas
posições (15/09/2026)**: dono testou o fluxo anterior (pergunta "vai
continuar com Replay ou ir pra janela normal", decide 1x por sessão) e
reportou que não funcionou: "leu a badge corretamente, mas quando tirei
do replay não leu". Causa raiz: ele liga o `debug`/`rodar` ainda no
Replay (antes do pregão) e desliga o Replay **no meio da mesma sessão**
(quando o mercado abre), sem reiniciar o programa -- invalidando a
suposição anterior de que "Replay só é usado antes do pregão, nunca
liga/desliga no meio de uma sessão real" (essa suposição já tinha
motivado remover a detecção contínua por cor uma sessão atrás; agora
ficou provado que a PREMISSA estava errada, não a implementação).

Corrigido sem voltar à detecção por cor (que tinha seu próprio bug):
como já comparamos contra referências exatas por quantidade, dá pra
simplesmente vigiar as DUAS posições possíveis (`cal.regiaoBadge` e
`regiaoAlternativaReplay(cal)`, nova função em `calibracao.cpp`) ao
mesmo tempo, usando qual delas bater com alguma referência -- sem
precisar saber ou perguntar em qual estado o Replay está. Nova classe
`LeitorPosicao` em `main.cpp` substitui a função livre
`aguardarProximaPosicao` + `CapturaRegiao` avulsa, encapsulando 1 ou 2
`CapturaRegiao` (a segunda só se `cal.temReplay`). `prepararRegiaoDeLeitura()`
voltou a ser `void` -- só cuida do reancorar agora, não decide mais "modo
de operação da sessão" (pergunta removida, ficou desnecessária).

**Clique no CENTRO do badge, nao no canto (15/09/2026)**: dono pediu pra
mudar o ponto de clique usado no reancorar e na medicao do deslocamento
do Replay (com/sem Replay) do canto superior esquerdo pro MEIO do badge,
com a tolerancia de comparacao ja existente cobrindo pequena imprecisao
de clique -- acertar o canto exato de um badge pequeno (a calibracao
dele tem so' 16x13px) e' bem mais impreciso que acertar perto do meio.

Novas funcoes em `calibracao.cpp`: `regiaoDoCentro(centro, largura,
altura)` (converte um clique no centro pro retangulo top-left que
`CapturaRegiao` precisa) e `centroDaRegiao(RegiaoTela)` (o inverso, usado
pra converter a regiao/topo ja conhecidos numa referencia de centro antes
de comparar com um novo clique de centro). So' os cliques de 1 ponto
(reancorar, medir deslocamento do Replay) mudaram -- a definicao inicial
da regiao do badge continua pedindo 2 cantos (precisa dos 2 pra saber o
tamanho).

**Busca local em volta do clique de centro (15/09/2026)**: dono testou o
clique no centro (mudança anterior) e reportou que o `debug` não
conseguia reconhecer "1C" com o clique -- confirmando que mesmo o centro
de um badge de 16x13px é dificil demais de acertar no pixel exato pra
uma comparacao de bitmap pixel-a-pixel. Pediu uma "tolerancia" pro
clique (nao pra comparacao de bitmap em si, que ja tinha tolerancia --
aumentar aquela deixaria niveis vizinhos mais faceis de confundir).

Implementado `buscarMelhorPosicao()` em `calibracao.cpp`: em vez de
confiar cegamente no clique, testa uma vizinhanca de ±6px ao redor dele
(RAIO_BUSCA_CENTRO_PX) -- pra cada posicao candidata, captura e compara
contra TODAS as referencias, ficando com a (posicao, quantidade) de
MENOR diferenca global. Custo: ate' (2*6+1)^2=169 capturas de uma regiao
minuscula, cada uma comparada contra todas as referencias -- poucos
milissegundos no total. Usado nos 3 lugares que pedem clique de 1 ponto:
reancorar (`prepararRegiaoDeLeitura`) e as duas medicoes de deslocamento
do Replay (`rodarCalibracao`). Se nao achar nada dentro da tolerancia
nem na vizinhanca inteira, avisa e mantem o estado anterior (nunca
adota uma posicao ruim silenciosamente).

**Revertido: pergunta fixa em vez de vigiar as duas posições
(15/09/2026)**: dono testou de novo e reportou "não funcionou no debug
quando tirei o replay" -- confirmando um bug real na tentativa anterior
(vigiar as duas posicoes -- com e sem Replay -- ao mesmo tempo, sem
perguntar): quando o Replay liga/desliga, as DUAS regioes mudam no mesmo
instante (`mudouBase` e `mudouAlt` ambos true), mas o codigo so'
verificava UMA delas (`mudouBase ? capBase_ : *capAlt_`), perdendo a
transicao da outra pra sempre (o "mudou" e' consumido no proximo poll
mesmo sem ter sido classificado). Dono pediu explicitamente pra voltar
a perguntar no final, antes de comecar a ler: "esta' na janela com
Replay ou nao" -- decisao FIXA pra sessao inteira, aceitando que trocar
de janela no meio exige reiniciar o debug/rodar.

Revertido: `prepararRegiaoDeLeitura()` volta a devolver `RegiaoTela` (a
regiao ativa, ja' com o ajuste aplicado) em vez de `void`; a classe
`LeitorPosicao` e `regiaoAlternativaReplay()` foram removidas; `main.cpp`
volta a usar `CapturaRegiao` unica + a funcao livre
`aguardarProximaPosicao()`. As melhorias independentes da sessao anterior
(clique no centro do badge + busca local `buscarMelhorPosicao()`)
continuam, nao foram afetadas por esse revert.

**Removido suporte especifico a Replay -- localizacao automatica pelo
"-" (15/09/2026)**: apos o revert pra pergunta fixa nao funcionar
("nao funcionou" de novo, testado ao vivo), o dono propos uma ideia
melhor: esquecer Replay inteiramente. Quando a posicao esta' flat, o
badge mostra um "-" com fundo de cor diferente -- essa referencia
"vazio" ja' existe na calibracao (quantidade=0) e e' tao boa quanto
"1C"/"2V" pra localizar onde o badge esta' fisicamente na tela AGORA,
nao importa se foi o Replay, o zoom ou so' a janela que deslocou tudo.

Redesenho completo (4a tentativa nessa frente, as 3 anteriores -- pedir
Replay ligado agora, vigiar 2 posicoes ao mesmo tempo, pergunta fixa por
sessao -- tinham problema real no uso ao vivo):

- Removido de `Calibracao` (config.h): `temReplay`, `deslocamentoReplayY`,
  `calibradoComReplayLigado`. Toda a secao de medir Replay em
  `rodarCalibracao()` foi removida.
- Nova funcao `buscarBadge(centro, largura, altura, referencias, raioPx)`
  (calibracao.cpp/h): procura numa vizinhanca de raio configuravel a
  posicao de MENOR diferenca contra QUALQUER referencia -- generaliza a
  antiga `buscarMelhorPosicao` (que so' cobria um raio pequeno pra
  cliques).
- Nova funcao `localizarBadge(cal, caminho)`: substitui
  `prepararRegiaoDeLeitura()` por completo. Busca automatica num raio
  AMPLO (45px) ao redor da ultima posicao conhecida, SEM perguntar nada;
  so' pede um clique aproximado (fallback) se a busca ampla nao achar
  nada.
- `aguardarProximaPosicao()` (main.cpp) ganhou auto-relocalizacao: se o
  badge mudar mas nao bater com nada na regiao atual, tenta a mesma
  busca ampla antes de so' avisar -- cobre ligar/desligar o Replay NO
  MEIO de uma sessao ja' em andamento, sem reiniciar (o proprio problema
  que a tentativa anterior, de vigiar 2 posicoes, tentava resolver e
  tinha um bug fazendo).
- Nenhuma pergunta sobre Replay existe mais em lugar nenhum do fluxo.

**Busca automatica travando/lenta -- corrigido pra 1 captura so'
(15/09/2026)**: dono reportou "esta buscando infinitamente" testando a
localizacao automatica. Causa: `buscarBadge()` fazia um `BitBlt` (via
`CapturaRegiao` nova) POR POSICAO candidata -- pra raio 45px isso e'
(2*45+1)^2 = 8281 capturas de tela separadas, cada uma com overhead real
de GDI (`GetDC`/`CreateCompatibleDC`/`CreateCompatibleBitmap`), levando
dezenas de segundos. Nao era um loop infinito de verdade, so' lento
demais pra parecer terminado.

Corrigido: `buscarBadge()` agora captura a area de busca inteira (o raio
todo) numa UNICA `CapturaRegiao`/`BitBlt`, e desliza a janela de
comparacao (`largura x altura`) dentro desse buffer JA' EM MEMORIA via
`extrairSubImagem()` (so' `memcpy` de cada linha) + `diferencaEntre()` --
sem tocar a tela de novo pra cada posicao. Termina em poucos
milissegundos em vez de dezenas de segundos.

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
