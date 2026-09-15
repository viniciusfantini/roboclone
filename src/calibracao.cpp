#include "calibracao.h"
#include "entrada.h"
#include "captura_tela.h"
#include "janela_alvo.h"
#include <cstdio>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <windows.h>

namespace {

constexpr int NIVEL_MAXIMO_PADRAO = 5;
constexpr int POLL_CALIBRACAO_MS = 50;
constexpr int ACOMODAR_CALIBRACAO_MS = 300;
constexpr int RAIO_BUSCA_AMPLA_PX = 45;   // vizinhanca ampla, pra localizar o badge sozinho

RegiaoTela regiaoDeDoisPontos(POINT a, POINT b) {
    RegiaoTela r;
    r.x = std::min(a.x, b.x);
    r.y = std::min(a.y, b.y);
    r.largura = std::max(1L, std::abs(b.x - a.x));
    r.altura = std::max(1L, std::abs(b.y - a.y));
    return r;
}

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

namespace {

// copia a sub-janela (largura x altura) que comeca em (offX,offY) de
// dentro de um buffer BGRA maior (bufferLargura de largura, top-down),
// pra comparar contra as referencias (que tem esse mesmo tamanho).
std::vector<BYTE> extrairSubImagem(const std::vector<BYTE>& bufferGrande, int bufferLargura,
                                    int offX, int offY, int largura, int altura) {
    std::vector<BYTE> sub((size_t)largura * altura * 4);
    for (int linha = 0; linha < altura; ++linha) {
        const BYTE* origem = bufferGrande.data() + ((size_t)(offY + linha) * bufferLargura + offX) * 4;
        BYTE* destino = sub.data() + (size_t)linha * largura * 4;
        std::memcpy(destino, origem, (size_t)largura * 4);
    }
    return sub;
}

} // namespace

// achado ao vivo, 15/09/2026: a 1a versao capturava a tela (BitBlt) UMA
// VEZ POR POSICAO candidata -- pra um raio de 45px isso e' (2*45+1)^2 =
// 8281 capturas separadas, cada uma com overhead de GDI (GetDC/
// CreateCompatibleDC/CreateCompatibleBitmap), levando dezenas de
// segundos e parecendo travado. Corrigido: captura a area de busca
// INTEIRA de uma vez so' (1 BitBlt) e desliza a janela de comparacao
// dentro desse buffer JA' EM MEMORIA -- so' memcpy + soma de diferencas,
// sem tocar a tela de novo. Termina em poucos milissegundos.
ResultadoBusca buscarBadge(POINT centro, int largura, int altura,
                            const std::vector<ReferenciaBadge>& referencias, int raioPx) {
    ResultadoBusca melhor;

    RegiaoTela areaAmpla;
    areaAmpla.x = centro.x - largura / 2 - raioPx;
    areaAmpla.y = centro.y - altura / 2 - raioPx;
    areaAmpla.largura = largura + 2 * raioPx;
    areaAmpla.altura = altura + 2 * raioPx;

    CapturaRegiao capGrande(areaAmpla);
    if (!capGrande.capturar()) return melhor;
    const std::vector<BYTE>& bufferGrande = capGrande.pixelsBrutos();

    for (int dy = 0; dy <= 2 * raioPx; ++dy) {
        for (int dx = 0; dx <= 2 * raioPx; ++dx) {
            std::vector<BYTE> sub = extrairSubImagem(bufferGrande, areaAmpla.largura, dx, dy, largura, altura);
            for (const auto& ref : referencias) {
                long long d = diferencaEntre(sub, ref.bitmap);
                if (melhor.diferenca < 0 || d < melhor.diferenca) {
                    melhor.diferenca = d;
                    melhor.regiao = RegiaoTela{ areaAmpla.x + dx, areaAmpla.y + dy, largura, altura };
                    melhor.quantidade = ref.quantidade;
                }
            }
        }
    }
    return melhor;
}

bool rodarCalibracao(Calibracao& out) {
    std::printf("== Calibracao ==\n");

    // se ja' existe uma calibracao de badges carregada (chamador tenta
    // carregar o arquivo salvo antes de chamar isso -- ver main.cpp),
    // pergunta se quer refazer a construcao 1..N ou so' reaproveitar.
    bool jaTemBadges = !out.referencias.empty();
    bool recalibrarBadges = true;
    if (jaTemBadges) {
        char pergunta[256];
        std::snprintf(pergunta, sizeof(pergunta),
                      "Ja' existe uma calibracao de badges salva (%zu referencias, regiao "
                      "%dx%d). Quer RECALIBRAR as badges (refazer a construcao 1..N)? "
                      "Respondendo nao, mantem o que ja' esta' calibrado",
                      out.referencias.size(), out.regiaoBadge.largura, out.regiaoBadge.altura);
        recalibrarBadges = perguntarSimNao(pergunta);
    }

    if (!recalibrarBadges) {
        std::printf("\n>> mantendo a calibracao de badges existente (%zu referencias).\n", out.referencias.size());
        return true;
    }

    std::printf("\nAntes de comecar: deixe a janela do Profit aberta, mostrando o\n");
    std::printf("painel com o indicador de posicao \"Qtd\" (o badge tipo \"1C\"/\"2V\"/\"-\"\n");
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

    POINT b1 = aguardarClique("canto SUPERIOR ESQUERDO do badge de posicao (numero + letra, ou o \"-\" quando flat)");
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

    aguardarEnter("deixe a posicao ZERADA/FLAT agora (o badge deve mostrar \"-\")");
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
    std::printf("referencias capturadas: %zu (vazio/\"-\" + 1..%d comprado + 1..%d vendido)\n",
                out.referencias.size(), nivelMaximo, nivelMaximo);
    std::printf("par mais parecido: quantidade %d vs %d, diferenca=%lld -> tolerancia=%lld\n",
                idxA, idxB, menor, out.tolerancia);

    if (menor < 30) {
        std::printf(">> AVISO: duas referencias ficaram muito parecidas entre si (quantidade %d e\n"
                    ">> %d, diferenca=%lld) -- pode confundir esses dois niveis. Confira se apontou\n"
                    ">> certo pro badge, ou recalibre se a leitura sair errada no modo debug.\n",
                    idxA, idxB, menor);
    }

    return true;
}

RegiaoTela localizarBadge(Calibracao& cal, const std::string& caminhoCalibracao, HWND origemEsperada) {
    std::printf("\nLocalizando o badge de posicao automaticamente (so' dentro da janela de "
                "origem escolhida)...\n");

    auto pertenceAOrigem = [&](const RegiaoTela& r) {
        return janelaNoPonto(centroDaRegiao(r)) == origemEsperada;
    };

    // 'achado' so' conta se estiver dentro da tolerancia de bitmap E
    // pertencer fisicamente a' janela de ORIGEM escolhida (achado ao
    // vivo, 15/09/2026: com varias janelas do Profit abertas, uma busca
    // ampla sem checar isso podia achar o badge de OUTRA janela, lendo a
    // conta errada sem avisar).
    POINT centro = centroDaRegiao(cal.regiaoBadge);
    ResultadoBusca achado = buscarBadge(centro, cal.regiaoBadge.largura, cal.regiaoBadge.altura,
                                         cal.referencias, RAIO_BUSCA_AMPLA_PX);
    bool valido = achado.diferenca >= 0 && achado.diferenca <= cal.tolerancia && pertenceAOrigem(achado.regiao);

    if (!valido) {
        if (achado.diferenca >= 0 && achado.diferenca <= cal.tolerancia) {
            std::printf(">> achei algo parecido perto da ultima posicao conhecida, mas pertence a\n"
                        ">> OUTRA janela, nao a de origem escolhida -- ignorado.\n");
        } else {
            std::printf(">> nao achei nada parecido perto da ultima posicao conhecida (num raio de\n"
                        ">> %dpx).\n", RAIO_BUSCA_AMPLA_PX);
        }
        std::printf(">> Aponte aproximadamente onde o badge esta' agora, DENTRO da janela de\n"
                    ">> origem -- nao precisa ser exato, so' perto.\n");
        POINT clique = aguardarClique("perto do badge de posicao (\"Qtd\") na janela de ORIGEM, AGORA");
        if (!(clique.x < 0 && clique.y < 0)) {
            achado = buscarBadge(clique, cal.regiaoBadge.largura, cal.regiaoBadge.altura,
                                  cal.referencias, RAIO_BUSCA_AMPLA_PX);
            valido = achado.diferenca >= 0 && achado.diferenca <= cal.tolerancia && pertenceAOrigem(achado.regiao);
            if (achado.diferenca >= 0 && achado.diferenca <= cal.tolerancia && !valido) {
                std::printf(">> o que achei perto desse clique tambem pertence a OUTRA janela --\n"
                            ">> clique bem em cima do badge, dentro da janela de origem certa.\n");
            }
        }
    }

    if (!valido) {
        std::printf(">> AVISO: continuo sem achar nada reconhecivel na janela de origem -- vou\n"
                    ">> usar a ultima posicao conhecida mesmo assim, mas provavelmente vai dar\n"
                    ">> \"nao bateu com nada\" toda hora. Rode \"calibrar\" de novo se isso continuar.\n");
        return cal.regiaoBadge;
    }

    std::printf(">> achado: %s (diferenca=%lld, tolerancia=%lld) em x=%d y=%d\n",
                formatarQuantidade(achado.quantidade).c_str(), achado.diferenca, cal.tolerancia,
                achado.regiao.x, achado.regiao.y);

    if (achado.regiao.x != cal.regiaoBadge.x || achado.regiao.y != cal.regiaoBadge.y) {
        cal.regiaoBadge = achado.regiao;
        if (perguntarSimNao("Posicao mudou -- salvar essa posicao atualizada pra proxima vez")) {
            if (salvarCalibracao(cal, caminhoCalibracao)) std::printf(">> salvo em %s.\n", caminhoCalibracao.c_str());
            else std::printf(">> falha ao salvar.\n");
        }
    }

    return cal.regiaoBadge;
}
