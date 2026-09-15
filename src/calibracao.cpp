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

// converte um clique no MEIO do badge pro retangulo (top-left based) que
// CapturaRegiao precisa, dado o tamanho ja' conhecido. Usado em vez de
// pedir o canto superior esquerdo pros cliques de reancorar/medir Replay
// (achado 15/09/2026: acertar o CANTO exato de um badge pequeno, tipo
// 16x13px, e' bem mais impreciso que acertar perto do meio -- a
// tolerancia de comparacao de bitmap ja' cobre um erro pequeno de clique).
RegiaoTela regiaoDoCentro(POINT centro, int largura, int altura) {
    RegiaoTela r;
    r.x = centro.x - largura / 2;
    r.y = centro.y - altura / 2;
    r.largura = largura;
    r.altura = altura;
    return r;
}

POINT centroDaRegiao(const RegiaoTela& r) {
    return POINT{ r.x + r.largura / 2, r.y + r.altura / 2 };
}

constexpr int RAIO_BUSCA_CENTRO_PX = 6;

struct MelhorPosicao {
    POINT centro{};
    int quantidade = 0;
    long long diferenca = -1;
};

// clicar "o meio" do badge a mao nunca acerta o pixel exato -- em vez de
// confiar cegamente no clique, procura numa vizinhanca pequena ao redor
// dele (RAIO_BUSCA_CENTRO_PX pixels em x e y) qual posicao da' a MENOR
// diferenca de bitmap contra QUALQUER referencia calibrada, e usa essa
// (achado ao vivo, 15/09/2026). Custo: (2*raio+1)^2 capturas de uma
// regiao minuscula, cada uma comparada contra todas as referencias --
// leva poucos milissegundos no total, nada perceptivel.
MelhorPosicao buscarMelhorPosicao(POINT centroClicado, int largura, int altura,
                                   const std::vector<ReferenciaBadge>& referencias) {
    MelhorPosicao melhor;
    for (int dy = -RAIO_BUSCA_CENTRO_PX; dy <= RAIO_BUSCA_CENTRO_PX; ++dy) {
        for (int dx = -RAIO_BUSCA_CENTRO_PX; dx <= RAIO_BUSCA_CENTRO_PX; ++dx) {
            POINT centro{ centroClicado.x + dx, centroClicado.y + dy };
            CapturaRegiao cap(regiaoDoCentro(centro, largura, altura));
            if (!cap.capturar()) continue;
            for (const auto& ref : referencias) {
                long long d = cap.diferencaPara(ref.bitmap);
                if (melhor.diferenca < 0 || d < melhor.diferenca) {
                    melhor.diferenca = d;
                    melhor.centro = centro;
                    melhor.quantidade = ref.quantidade;
                }
            }
        }
    }
    return melhor;
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

// quanto somar ao Y da posicao BASE (a convencao com que 'cal' foi
// calibrado -- ver Calibracao::calibradoComReplayLigado) pra achar a
// posicao de verdade AGORA, dado se o Replay esta' ligado neste momento.
// Cobre os 4 casos (base com/sem Replay x agora com/sem Replay) com uma
// formula so' -- ver config.h pro raciocinio completo.
int ajusteReplay(bool replayAgora, bool calibradoComReplayLigado, int deslocamentoReplayY) {
    return deslocamentoReplayY * ((replayAgora ? 1 : 0) - (calibradoComReplayLigado ? 1 : 0));
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

    // se ja' existe uma calibracao de badges carregada (chamador tenta
    // carregar o arquivo salvo antes de chamar isso -- ver main.cpp),
    // pergunta se quer refazer a construcao 1..N ou so' reaproveitar e
    // pular direto pro proximo passo (ex.: so' medir/remedir o Replay).
    bool jaTemBadges = !out.referencias.empty();
    bool recalibrarBadges = true;
    if (jaTemBadges) {
        char pergunta[256];
        std::snprintf(pergunta, sizeof(pergunta),
                      "Ja' existe uma calibracao de badges salva (%zu referencias, regiao "
                      "%dx%d). Quer RECALIBRAR as badges (refazer a construcao 1..N)? "
                      "Respondendo nao, mantem o que ja' esta' calibrado e pula pro proximo passo",
                      out.referencias.size(), out.regiaoBadge.largura, out.regiaoBadge.altura);
        recalibrarBadges = perguntarSimNao(pergunta);
    }

    POINT topoBadge{ out.regiaoBadge.x, out.regiaoBadge.y };
    bool comReplayAgora = out.calibradoComReplayLigado;
    // (recalculado logo abaixo, depois que largura/altura estiverem
    // certos -- ver uso mais adiante, na secao do Replay)

    if (recalibrarBadges) {
        std::printf("\nAntes de comecar: deixe a janela do Profit aberta, mostrando o\n");
        std::printf("painel com o indicador de posicao \"Qtd\" (o badge tipo \"1C\"/\"2V\"\n");
        std::printf("-- ver imagem/boleta.png), do jeito que vai ficar durante o pregao.\n");

        int nivelMaximo = perguntarNivelMaximo();

        comReplayAgora = perguntarSimNao(
            "Vai fazer essa calibracao (a construcao 1..N abaixo) com o modo\n"
            "REPLAY do Profit JA' LIGADO nessa janela agora? Util pra fazer tudo\n"
            "isso fora do horario de pregao, sem depender do mercado aberto");
        out.calibradoComReplayLigado = comReplayAgora;

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
        topoBadge = b1;

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

        std::printf("\n== Calibracao de badges concluida ==\n");
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
    } else {
        std::printf("\n>> mantendo a calibracao de badges existente (%zu referencias).\n", out.referencias.size());
    }

    // suporte opcional a modo Replay do Profit (achado ao vivo, 15/09/2026:
    // a barra amarela do Replay empurra o badge pra baixo por um
    // deslocamento fixo -- medido em 24px numa maquina, mas aqui e'
    // MEDIDO na tela de quem esta' calibrando, nao chumbado no codigo,
    // porque pode variar com DPI/tema/monitor). A direcao da medicao
    // depende de como a calibracao principal foi feita (pergunta
    // 'comReplayAgora' la' em cima):
    //   - calibrou SEM Replay (comum): liga o Replay agora e mede quanto
    //     desce.
    //   - calibrou COM Replay ligado (pedido do dono, 15/09/2026, pra
    //     poder calibrar tudo fora do horario de pregao): desliga o
    //     Replay agora e mede quanto SOBE -- a base ja' e' a posicao
    //     deslocada, entao operar de verdade (Replay desligado) precisa
    //     DESCONTAR o deslocamento, nao somar (ver ajusteReplay acima).
    POINT centroBadge = centroDaRegiao(RegiaoTela{topoBadge.x, topoBadge.y, out.regiaoBadge.largura, out.regiaoBadge.altura});

    if (comReplayAgora) {
        out.temReplay = true;
        aguardarEnter("desligue o modo Replay AGORA nessa janela (some a barra amarela) e confirme");

        POINT centroClicado = aguardarClique(
            "o MEIO (centro) do badge de posicao, AGORA com o Replay DESLIGADO "
            "(deve estar mais alto que antes -- essa e' a posicao de operar de verdade;\n"
            "nao precisa acertar o pixel exato)");
        if (centroClicado.x < 0 && centroClicado.y < 0) { out.temReplay = false; return true; }
        MelhorPosicao achado = buscarMelhorPosicao(centroClicado, out.regiaoBadge.largura,
                                                    out.regiaoBadge.altura, out.referencias);
        if (achado.diferenca < 0 || achado.diferenca > out.tolerancia) {
            std::printf(">> AVISO: nao achei nada parecido perto desse clique -- clique mais perto\n"
                        ">> do badge. Replay NAO sera' aplicado (desligado pra essa calibracao).\n");
            out.temReplay = false;
            return true;
        }
        out.deslocamentoReplayY = (int)(centroBadge.y - achado.centro.y);

        std::printf(">> deslocamento medido: %d px (a calibracao principal foi feita com Replay\n"
                    ">> ligado -- operar de verdade vai DESCONTAR esse deslocamento sozinho)\n",
                    out.deslocamentoReplayY);
    } else {
        out.temReplay = perguntarSimNao(
            "Voce as vezes usa o modo REPLAY do Profit nessa janela de origem\n"
            "(a barra amarela com play/pause, que empurra o layout pra baixo)?\n"
            "Se sim, vamos medir o deslocamento agora");

        if (out.temReplay) {
            aguardarEnter("ligue o modo Replay AGORA nessa janela (a barra amarela deve aparecer) e confirme");

            POINT centroClicado = aguardarClique(
                "o MEIO (centro) do badge de posicao, AGORA com o Replay ligado "
                "(deve estar mais baixo que antes; nao precisa acertar o pixel exato)");
            if (centroClicado.x < 0 && centroClicado.y < 0) { out.temReplay = false; return true; }
            MelhorPosicao achado = buscarMelhorPosicao(centroClicado, out.regiaoBadge.largura,
                                                        out.regiaoBadge.altura, out.referencias);
            if (achado.diferenca < 0 || achado.diferenca > out.tolerancia) {
                std::printf(">> AVISO: nao achei nada parecido perto desse clique -- clique mais\n"
                            ">> perto do badge. Replay NAO sera' aplicado (desligado pra essa\n"
                            ">> calibracao).\n");
                out.temReplay = false;
                return true;
            }
            out.deslocamentoReplayY = (int)(achado.centro.y - centroBadge.y);

            std::printf(">> deslocamento medido: %d px\n", out.deslocamentoReplayY);
            std::printf(">> pode desligar o Replay agora.\n");
        }
    }

    if (out.temReplay && out.deslocamentoReplayY <= 0) {
        std::printf(">> AVISO: deslocamento veio zero ou negativo -- clique errado? Replay\n"
                    ">> NAO sera' aplicado (desligado pra essa calibracao).\n");
        out.temReplay = false;
    }

    return true;
}

RegiaoTela regiaoAlternativaReplay(const Calibracao& cal) {
    RegiaoTela r = cal.regiaoBadge;
    r.y += cal.calibradoComReplayLigado ? -cal.deslocamentoReplayY : cal.deslocamentoReplayY;
    return r;
}

void prepararRegiaoDeLeitura(Calibracao& cal, const std::string& caminhoCalibracao) {
    // pergunta "Replay esta' ligado AGORA?" so' pra interpretar certo o
    // clique do reancorar abaixo (pode ser conveniente reancorar vendo o
    // Replay, ex. mostrando "1C") -- nao decide mais o modo de operacao
    // da sessao inteira (ver comentario no .h: o dono liga/desliga o
    // Replay NO MEIO da sessao, entao main.cpp vigia as duas posicoes
    // possiveis ao mesmo tempo em vez de fixar uma so' aqui).
    bool replayAgora = cal.temReplay &&
        perguntarSimNao("O modo Replay esta' LIGADO agora nessa janela de origem?");

    bool quer = perguntarSimNao(
        "Confirmar/reancorar rapido a posicao do badge antes de comecar?\n"
        "Util se reabriu o Profit e a janela mudou de lugar -- reaproveita as\n"
        "referencias JA calibradas (nao refaz 1..N contratos), so' atualiza\n"
        "ONDE olhar na tela. Se respondeu que o Replay esta' ligado agora,\n"
        "pode clicar mostrando um estado conhecido (ex. \"1C\") pra conferir");

    if (quer) {
        POINT centroClicado = aguardarClique(
            "o MEIO (centro) do badge de posicao AGORA (o tamanho ja' esta' "
            "calibrado, so' a posicao pode ter mudado -- nao precisa acertar o\n"
            "pixel exato, o programa procura sozinho num raio pequeno)");
        if (centroClicado.x < 0 && centroClicado.y < 0) {
            std::printf(">> cancelado, mantendo a posicao calibrada antes.\n");
        } else {
            MelhorPosicao achado = buscarMelhorPosicao(centroClicado, cal.regiaoBadge.largura,
                                                        cal.regiaoBadge.altura, cal.referencias);
            bool reconhecido = (achado.diferenca >= 0 && achado.diferenca <= cal.tolerancia);
            std::printf(">> perto desse clique, a leitura mais parecida seria: %s "
                        "(diferenca=%lld, tolerancia=%lld, ajuste do clique: %ld,%ld px)\n",
                        reconhecido ? formatarQuantidade(achado.quantidade).c_str() : "NENHUMA (nao bateu com nada)",
                        achado.diferenca, cal.tolerancia,
                        (long)(achado.centro.x - centroClicado.x), (long)(achado.centro.y - centroClicado.y));

            if (!reconhecido) {
                std::printf(">> mantendo a posicao calibrada antes -- clique mais perto do badge, ou\n"
                            ">> rode \"calibrar\" de novo se precisar.\n");
            } else if (perguntarSimNao("Essa leitura bate com o que esta' aparecendo na tela agora")) {
                // a posicao ACHADA (ja' ajustada pela busca) pegou o
                // estado de AGORA (com ou sem Replay, conforme
                // 'replayAgora') -- converte pra posicao BASE (a mesma
                // convencao da calibracao original, ver
                // Calibracao::calibradoComReplayLigado) antes de guardar.
                int ajuste = ajusteReplay(replayAgora, cal.calibradoComReplayLigado, cal.deslocamentoReplayY);
                POINT centroBase{ achado.centro.x, achado.centro.y - ajuste };
                cal.regiaoBadge = regiaoDoCentro(centroBase, cal.regiaoBadge.largura, cal.regiaoBadge.altura);
                std::printf(">> posicao base atualizada pra essa sessao%s.\n",
                            ajuste != 0 ? " (ajustado pro deslocamento do Replay)" : "");

                if (perguntarSimNao("Salvar essa posicao atualizada pra proxima vez")) {
                    if (salvarCalibracao(cal, caminhoCalibracao)) std::printf(">> salvo em %s.\n", caminhoCalibracao.c_str());
                    else std::printf(">> falha ao salvar.\n");
                }
            } else {
                std::printf(">> mantendo a posicao calibrada antes -- rode \"calibrar\" de novo se precisar.\n");
            }
        }
    }

    if (cal.temReplay) {
        std::printf(">> Replay tem deslocamento calibrado (%dpx) -- a leitura vai vigiar as\n"
                    ">> DUAS posicoes possiveis (com e sem Replay) e usar automaticamente\n"
                    ">> qual delas bater com alguma referencia. Pode ligar/desligar o\n"
                    ">> Replay a qualquer momento durante a sessao, sem precisar reiniciar.\n",
                    cal.deslocamentoReplayY);
    }
}
