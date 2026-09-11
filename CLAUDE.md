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

## Estado atual

Repositório criado vazio; este `CLAUDE.md` foi o primeiro commit (via a
sessão do `4q-winfut`, 11/09/2026) — só a definição do problema e as
opções de arquitetura acima, nenhum código ainda. Próximo passo: decidir
entre VPN+TCP direto vs. relay, e prototipar.
