#include "calibracao.h"
#include "entrada.h"
#include "captura_tela.h"
#include <cstdio>
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <windows.h>

namespace {

constexpr int NIVEL_MAXIMO_PADRAO = 5;
constexpr int POLL_CALIBRACAO_MS = 50;
constexpr int ACOMODAR_CALIBRACAO_MS = 300;

RegiaoTela regiaoDeDoisPontos(POINT a, POINT b) {
    RegiaoTela r;
    r.x = std::min(a.x, b.x);
    r.y = std::min(a.y, b.y);
    r.largura = std::max(1L, std::abs(b.x - a.x));
    r.altura = std::max(1L, std::abs(b.y - a.y));
    return r;
}

long long diferencaEntre(const std::vector<BYTE>& a, const std::vector<BYTE>& b) {
    if (a.size() != b.size()) return 0;
    long long soma = 0;
    for (size_t i = 0; i < a.size(); ++i) soma += std::abs((int)a[i] - (int)b[i]);
    return soma;
}

int perguntarNivelMaximo() {
    std::printf("\nAte' quantos contratos calibrar de cada lado? (ex.: 5 calibra 1..5\n");
    std::printf("comprado E 1..5 vendido). ENTER pra usar o padrao (%d): ", NIVEL_MAXIMO_PADRAO);
    std::fflush(stdout);
    std::string linha;
    std::getline(std::cin, linha);
    if (linha.empty()) return NIVEL_MAXIMO_PADRAO;
    int valor = std::atoi(linha.c_str());
    return valor > 0 ? valor : NIVEL_MAXIMO_PADRAO;
}

bool perguntarSimNao(const std::string& pergunta) {
    std::printf("\n%s (s/N): ", pergunta.c_str());
    std::fflush(stdout);
    std::string linha;
    std::getline(std::cin, linha);
    return !linha.empty() && (linha[0] == 's' || linha[0] == 'S');
}

std::string formatarQuantidade(int q) {
    if (q == 0) return "FLAT";
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%s %d", q > 0 ? "COMPRADO" : "VENDIDO", std::abs(q));
    return buf;
}

// mostra a instrucao e fica vigiando o badge sozinho -- assim que
// detectar mudanca (o operador fez a acao pedida), espera acomodar e
// recaptura, sem precisar de ENTER. Devolve false se ESC for apertado
// (cancela a calibracao).
bool aguardarMudancaBadge(CapturaRegiao& cap, const std::string& instrucao) {
    std::printf("\n>> %s\n>>   (deteccao automatica -- so' faca a operacao, sem precisar\n"
                ">>   confirmar aqui; ESC cancela)\n", instrucao.c_str());
    std::fflush(stdout);

    while (true) {
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
            std::printf(">> cancelado.\n");
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(POLL_CALIBRACAO_MS));
        if (!cap.capturar()) continue;
        if (cap.mudouDesdeUltimaCaptura()) break;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(ACOMODAR_CALIBRACAO_MS));
    cap.capturar();
    std::printf(">> mudanca detectada, capturado.\n");
    return true;
}

} // namespace

bool rodarCalibracao(Calibracao& out) {
    std::printf("== Calibracao ==\n");
    std::printf("Antes de comecar: deixe a janela do Profit aberta, mostrando o\n");
    std::printf("painel com o indicador de posicao \"Qtd\" (o badge tipo \"1C\"/\"2V\"\n");
    std::printf("-- ver imagem/boleta.png), do jeito que vai ficar durante o pregao.\n");

    int nivelMaximo = perguntarNivelMaximo();

    std::printf("\nAponte SO' pro badge \"Qtd\" -- nao inclua campos vizinhos que mudam\n");
    std::printf("sozinhos com o preco (ex.: \"Resultado\", \"Res. Aberto\"), senao\n");
    std::printf("qualquer variacao de preco vira uma leitura que nao bate com nada.\n");
    if (nivelMaximo >= 10) {
        std::printf("\n>> ATENCAO: o badge fica um pouco mais LARGO quando a quantidade\n"
                    ">> passa de 1 digito (10 em diante) -- achado ao vivo, 15/09/2026.\n"
                    ">> A regiao que voce vai desenhar agora e' FIXA (nao redimensiona\n"
                    ">> sozinha depois), entao desenhe um pouco mais larga do que o\n"
                    ">> badge aparenta AGORA (provavelmente com 1 digito ou vazio), com\n"
                    ">> folga suficiente pra caber \"%dC\" sem cortar.\n", nivelMaximo);
    }

    POINT b1 = aguardarClique("canto SUPERIOR ESQUERDO do badge de posicao (numero + letra)");
    if (b1.x < 0 && b1.y < 0) return false;
    POINT b2 = aguardarClique("canto INFERIOR DIREITO do badge");
    if (b2.x < 0 && b2.y < 0) return false;
    out.regiaoBadge = regiaoDeDoisPontos(b1, b2);

    CapturaRegiao cap(out.regiaoBadge);
    out.referencias.clear();

    std::printf("\nAgora vamos construir a posicao 1 contrato de cada vez, dos dois\n");
    std::printf("lados. Use a conta SIMULADORA -- isso faz operacao de verdade. A\n");
    std::printf("partir daqui o programa detecta sozinho quando voce faz cada\n");
    std::printf("operacao -- so' o primeiro passo (ficar zerado) precisa de ENTER,\n");
    std::printf("porque nao ha' \"mudanca\" pra esperar se voce ja' estiver flat.\n");

    aguardarEnter("deixe a posicao ZERADA/FLAT agora");
    if (!cap.capturar()) { std::printf(">> falha ao capturar a tela.\n"); return false; }
    out.referencias.push_back({0, cap.pixelsBrutos()});

    for (int i = 1; i <= nivelMaximo; ++i) {
        char instrucao[160];
        std::snprintf(instrucao, sizeof(instrucao),
                      "compre mais 1 contrato a mercado (fique comprado, total %d)", i);
        if (!aguardarMudancaBadge(cap, instrucao)) return false;
        out.referencias.push_back({i, cap.pixelsBrutos()});
    }

    if (!aguardarMudancaBadge(cap, "zere a posicao de teste (fique FLAT de novo)")) return false;

    for (int i = 1; i <= nivelMaximo; ++i) {
        char instrucao[160];
        std::snprintf(instrucao, sizeof(instrucao),
                      "venda mais 1 contrato a mercado (fique vendido, total %d)", i);
        if (!aguardarMudancaBadge(cap, instrucao)) return false;
        out.referencias.push_back({-i, cap.pixelsBrutos()});
    }

    std::printf("\n>> pode zerar a posicao de teste agora, a calibracao ja capturou o que precisava.\n");

    // tolerancia = uma fracao da menor diferenca entre QUAISQUER duas
    // referencias capturadas (as mais parecidas entre si, ex.: 3C vs 4C,
    // sao o par mais critico) -- conservador o bastante pra nao confundir
    // niveis vizinhos, generoso o bastante pra aguentar variacao pequena
    // de rendering entre capturas.
    long long menor = -1;
    int idxA = -1, idxB = -1;
    for (size_t i = 0; i < out.referencias.size(); ++i) {
        for (size_t j = i + 1; j < out.referencias.size(); ++j) {
            long long d = diferencaEntre(out.referencias[i].bitmap, out.referencias[j].bitmap);
            if (menor < 0 || d < menor) { menor = d; idxA = out.referencias[i].quantidade; idxB = out.referencias[j].quantidade; }
        }
    }
    out.tolerancia = menor / 3;

    std::printf("\n== Calibracao concluida ==\n");
    std::printf("regiao badge: x=%d y=%d %dx%d\n", out.regiaoBadge.x, out.regiaoBadge.y,
                out.regiaoBadge.largura, out.regiaoBadge.altura);
    std::printf("referencias capturadas: %zu (vazio + 1..%d comprado + 1..%d vendido)\n",
                out.referencias.size(), nivelMaximo, nivelMaximo);
    std::printf("par mais parecido: quantidade %d vs %d, diferenca=%lld -> tolerancia=%lld\n",
                idxA, idxB, menor, out.tolerancia);

    if (menor < 30) {
        std::printf(">> AVISO: duas referencias ficaram muito parecidas entre si (quantidade %d e\n"
                    ">> %d, diferenca=%lld) -- pode confundir esses dois niveis. Confira se apontou\n"
                    ">> certo pro badge, ou recalibre se a leitura sair errada no modo debug.\n",
                    idxA, idxB, menor);
    }

    // suporte opcional a modo Replay do Profit (achado ao vivo, 15/09/2026:
    // a barra amarela do Replay empurra o badge pra baixo por um
    // deslocamento fixo -- medido em 24px numa maquina, mas aqui e'
    // MEDIDO na tela de quem esta' calibrando, nao chumbado no codigo,
    // porque pode variar com DPI/tema/monitor).
    out.temReplay = perguntarSimNao(
        "Voce as vezes usa o modo REPLAY do Profit nessa janela de origem\n"
        "(a barra amarela com play/pause, que empurra o layout pra baixo)?\n"
        "Se sim, vamos medir o deslocamento agora");

    if (out.temReplay) {
        aguardarEnter("ligue o modo Replay AGORA nessa janela (a barra amarela deve aparecer) e confirme");

        POINT novoTopo = aguardarClique(
            "canto SUPERIOR ESQUERDO do badge de posicao, AGORA com o Replay ligado "
            "(deve estar mais baixo que antes)");
        if (novoTopo.x < 0 && novoTopo.y < 0) { out.temReplay = false; return true; }
        out.deslocamentoReplayY = (int)(novoTopo.y - b1.y);

        std::printf(">> deslocamento medido: %d px\n", out.deslocamentoReplayY);
        std::printf(">> pode desligar o Replay agora.\n");

        if (out.deslocamentoReplayY <= 0) {
            std::printf(">> AVISO: deslocamento veio zero ou negativo -- clique errado? Replay\n"
                        ">> NAO sera' aplicado (desligado pra essa calibracao).\n");
            out.temReplay = false;
        }
    }

    return true;
}

RegiaoTela prepararRegiaoDeLeitura(Calibracao& cal, const std::string& caminhoCalibracao) {
    // decide UMA vez por sessao se o Replay esta' ligado agora -- nao
    // fica checando isso ao vivo durante a leitura (simplificado em
    // 15/09/2026: o Replay so' e' usado antes do pregao abrir, nunca
    // liga/desliga no meio de uma sessao de verdade).
    bool replayAgora = cal.temReplay &&
        perguntarSimNao("O modo Replay esta' LIGADO agora nessa janela de origem?");

    bool quer = perguntarSimNao(
        "Confirmar/reancorar rapido a posicao do badge antes de comecar?\n"
        "Util se reabriu o Profit e a janela mudou de lugar -- reaproveita as\n"
        "referencias JA calibradas (nao refaz 1..N contratos), so' atualiza\n"
        "ONDE olhar na tela. Se respondeu que o Replay esta' ligado agora,\n"
        "pode clicar mostrando um estado conhecido (ex. \"1C\") pra conferir");

    if (quer) {
        POINT novoTopo = aguardarClique(
            "canto SUPERIOR ESQUERDO do badge de posicao AGORA (o tamanho ja' esta' "
            "calibrado, so' a posicao pode ter mudado)");
        if (novoTopo.x < 0 && novoTopo.y < 0) {
            std::printf(">> cancelado, mantendo a posicao calibrada antes.\n");
        } else {
            RegiaoTela regiaoClicada = cal.regiaoBadge;
            regiaoClicada.x = novoTopo.x;
            regiaoClicada.y = novoTopo.y;

            CapturaRegiao cap(regiaoClicada);
            if (!cap.capturar()) {
                std::printf(">> falha ao capturar -- mantendo a posicao calibrada antes.\n");
            } else {
                long long menor = -1;
                int quantidade = 0;
                for (const auto& ref : cal.referencias) {
                    long long d = cap.diferencaPara(ref.bitmap);
                    if (menor < 0 || d < menor) { menor = d; quantidade = ref.quantidade; }
                }
                bool reconhecido = (menor >= 0 && menor <= cal.tolerancia);
                std::printf(">> com essa posicao, a leitura agora seria: %s (diferenca=%lld, tolerancia=%lld)\n",
                            reconhecido ? formatarQuantidade(quantidade).c_str() : "NENHUMA (nao bateu com nada)",
                            menor, cal.tolerancia);

                if (perguntarSimNao("Essa leitura bate com o que esta' aparecendo na tela agora")) {
                    // se o clique foi feito com o Replay ligado, o clique
                    // pegou a posicao DESLOCADA -- a base (regiaoBadge)
                    // precisa descontar isso pra continuar sendo a
                    // posicao SEM Replay.
                    cal.regiaoBadge.x = novoTopo.x;
                    cal.regiaoBadge.y = replayAgora ? (novoTopo.y - cal.deslocamentoReplayY) : novoTopo.y;
                    std::printf(">> posicao base atualizada pra essa sessao%s.\n",
                                replayAgora ? " (descontando o deslocamento do Replay)" : "");

                    if (perguntarSimNao("Salvar essa posicao atualizada pra proxima vez")) {
                        if (salvarCalibracao(cal, caminhoCalibracao)) std::printf(">> salvo em %s.\n", caminhoCalibracao.c_str());
                        else std::printf(">> falha ao salvar.\n");
                    }
                } else {
                    std::printf(">> mantendo a posicao calibrada antes -- rode \"calibrar\" de novo se precisar.\n");
                }
            }
        }
    }

    RegiaoTela regiaoAtiva = cal.regiaoBadge;
    if (replayAgora) regiaoAtiva.y += cal.deslocamentoReplayY;
    return regiaoAtiva;
}
