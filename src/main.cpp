// main.cpp -- roboclone: le o indicador de posicao ("Qtd", ex.: "1C"/
// "2V" -- ver imagem/boleta.png) do Profit na conta de ORIGEM (via
// comparacao de bitmap contra referencias POR QUANTIDADE EXATA, sem OCR)
// e manda o comando equivalente (compra/venda/zerar/reduzir) por atalho
// de teclado pra uma segunda janela do Profit, numa conta SIMULADORA.
//
// Uso:
//   roboclone.exe calibrar   -- descobre por clique a regiao do badge e
//                                captura uma referencia por quantidade
//                                exata (vazio, 1..N comprado, 1..N vendido)
//   roboclone.exe debug      -- NAO manda atalho nenhum; escreve o rotulo
//                                lido (C/V/CC/VV/c/v/Z) num bloco de
//                                notas, pra conferir a leitura antes de
//                                confiar nela
//   roboclone.exe rodar      -- roda de verdade (manda atalho de verdade)
//   roboclone.exe testaratalho -- so' testa o envio do atalho numa janela
//                                escolhida, sem calibracao nem leitura de
//                                tela nenhuma (pra depurar se o ALT+C/V/A
//                                esta' chegando de verdade no Profit)
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <thread>
#include <chrono>
#include <iostream>
#include <optional>

#include "config.h"
#include "calibracao.h"
#include "captura_tela.h"
#include "janela_alvo.h"
#include "atalho.h"
#include "sinal.h"
#include "escrever_texto.h"
#include "janela_teste.h"

namespace {

const char* CAMINHO_CALIBRACAO = "calibracao.cfg";
constexpr int INTERVALO_POLL_MS = 10;
constexpr int ESPERA_ACOMODAR_MS = 60;
constexpr int TAMANHO_MAXIMO_POSICAO_PADRAO = 5;

int perguntarDelayMs() {
    std::printf("\nEspacamento minimo entre comandos enviados, em ms (o Profit recusa com\n");
    std::printf("\"Alerta de envio duplicado\" se dois atalhos saem rapido demais -- 200ms\n");
    std::printf("resolveu no b3_money_copy). ENTER pra usar o padrao (%d): ", DELAY_MIN_ENTRE_COPIAS_MS_PADRAO);
    std::fflush(stdout);
    std::string linha;
    std::getline(std::cin, linha);
    if (linha.empty()) return DELAY_MIN_ENTRE_COPIAS_MS_PADRAO;
    int valor = std::atoi(linha.c_str());
    if (valor <= 0) {
        std::printf(">> valor invalido, usando o padrao (%d)\n", DELAY_MIN_ENTRE_COPIAS_MS_PADRAO);
        return DELAY_MIN_ENTRE_COPIAS_MS_PADRAO;
    }
    return valor;
}

int perguntarTamanhoMaximoPosicao() {
    std::printf("\nTamanho maximo de posicao permitido antes de PARAR de mandar atalho\n");
    std::printf("automatico (trava de seguranca contra loop -- ver README). ENTER pra\n");
    std::printf("usar o padrao (%d): ", TAMANHO_MAXIMO_POSICAO_PADRAO);
    std::fflush(stdout);
    std::string linha;
    std::getline(std::cin, linha);
    if (linha.empty()) return TAMANHO_MAXIMO_POSICAO_PADRAO;
    int valor = std::atoi(linha.c_str());
    if (valor <= 0) {
        std::printf(">> valor invalido, usando o padrao (%d)\n", TAMANHO_MAXIMO_POSICAO_PADRAO);
        return TAMANHO_MAXIMO_POSICAO_PADRAO;
    }
    return valor;
}

std::string nomePosicao(int p) {
    char buf[32];
    if (p == 0) return "FLAT";
    std::snprintf(buf, sizeof(buf), "%s %d", p > 0 ? "COMPRADO" : "VENDIDO", std::abs(p));
    return buf;
}

// classifica a captura atual contra TODAS as referencias calibradas
// (vazio + cada quantidade exata dos dois lados) e devolve a mais
// parecida, se estiver dentro da tolerancia.
std::optional<int> classificar(CapturaRegiao& capBadge, const Calibracao& cal) {
    long long menor = -1;
    int quantidade = 0;
    for (const auto& ref : cal.referencias) {
        long long d = capBadge.diferencaPara(ref.bitmap);
        if (menor < 0 || d < menor) { menor = d; quantidade = ref.quantidade; }
    }
    if (menor < 0 || menor > cal.tolerancia) return std::nullopt;
    return quantidade;
}

POINT centroRegiao(const RegiaoTela& r) {
    return POINT{ r.x + r.largura / 2, r.y + r.altura / 2 };
}

// bloqueia ate' detectar uma mudanca de posicao reconhecida (o badge
// mudou E a nova aparencia bate com uma das referencias calibradas).
// Avisa no console (sem travar) se a janela que esta' fisicamente
// naquele pedaco de tela agora nao e' mais a janela de ORIGEM esperada
// (ex.: a janela de destino ficou por cima -- sem essa checagem, leria o
// badge errado, podendo criar um loop lendo as proprias ordens que
// mandou), OU se o badge mudou mas nao bateu com nenhuma referencia
// (ex.: passou do nivel maximo calibrado) -- so' ignora e continua
// esperando, NAO tenta relocalizar sozinho aqui (achado ao vivo,
// 15/09/2026: relocalizar numa busca ampla pode achar por coincidencia
// alguma posicao vizinha parecida com OUTRA referencia -- ex. "FLAT" --
// e reportar uma mudanca de posicao que nao aconteceu de verdade. A
// localizacao automatica ampla so' roda uma vez, no inicio da sessao,
// ver localizarBadge() em calibracao.cpp).
int aguardarProximaPosicao(CapturaRegiao& capBadge, const Calibracao& cal, HWND origemEsperada) {
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(INTERVALO_POLL_MS));

        if (!capBadge.capturar()) continue;
        if (!capBadge.mudouDesdeUltimaCaptura()) continue;

        HWND atual = janelaNoPonto(centroRegiao(capBadge.regiao()));
        if (atual != origemEsperada) {
            std::printf("[aviso] a janela na regiao calibrada da origem NAO e' mais a janela de "
                        "origem esperada (HWND=%p) -- leitura ignorada. Confira se a janela de "
                        "destino nao ficou por cima.\n", (void*)atual);
            continue;
        }

        // deixa o repaint acomodar antes de reler (evita pegar um frame
        // no meio da atualizacao) e reestabelece a baseline pra nao
        // re-disparar no proximo poll com o mesmo conteudo.
        std::this_thread::sleep_for(std::chrono::milliseconds(ESPERA_ACOMODAR_MS));
        capBadge.capturar();

        auto posicao = classificar(capBadge, cal);
        if (posicao.has_value()) return *posicao;

        std::printf("[aviso] badge mudou mas nao bateu com nenhuma referencia dentro da tolerancia "
                    "-- ignorado (pode ser ruido de campo vizinho, ou posicao alem do calibrado)\n");
    }
}

bool carregarCalibracaoOuAvisar(Calibracao& cal) {
    if (carregarCalibracao(cal, CAMINHO_CALIBRACAO)) return true;
    std::fprintf(stderr, "ERRO: nao achei %s -- rode \"roboclone.exe calibrar\" primeiro.\n",
                 CAMINHO_CALIBRACAO);
    return false;
}

int modoCalibrar() {
    Calibracao cal;
    carregarCalibracao(cal, CAMINHO_CALIBRACAO); // ok falhar (ex.: 1a vez) -- cal fica vazia
    if (!rodarCalibracao(cal)) {
        std::printf("Calibracao cancelada, nada foi salvo.\n");
        return 1;
    }
    if (!salvarCalibracao(cal, CAMINHO_CALIBRACAO)) {
        std::fprintf(stderr, "ERRO: nao consegui salvar %s\n", CAMINHO_CALIBRACAO);
        return 1;
    }
    std::printf("Calibracao salva em %s (+ %s.refs)\n", CAMINHO_CALIBRACAO, CAMINHO_CALIBRACAO);
    return 0;
}

int modoDebug() {
    Calibracao cal;
    if (!carregarCalibracaoOuAvisar(cal)) return 1;

    HWND origem = escolherJanelaPorClique("Profit da conta de ORIGEM (a que sera' lida)");
    if (!origem) {
        std::printf("Nenhuma janela escolhida, saindo.\n");
        return 1;
    }

    RegiaoTela regiaoLeitura = localizarBadge(cal, CAMINHO_CALIBRACAO, origem);

    HWND blocoDeNotas = escolherJanelaPorClique("bloco de notas (vai receber o rotulo lido: C/V/CC/VV/c/v/Z)");
    if (!blocoDeNotas) {
        std::printf("Nenhuma janela escolhida, saindo.\n");
        return 1;
    }

    CapturaRegiao capBadge(regiaoLeitura);
    if (!capBadge.capturar()) {
        std::fprintf(stderr, "ERRO: falha ao capturar a regiao calibrada. Recalibre.\n");
        return 1;
    }
    int posicaoAnterior = classificar(capBadge, cal).value_or(0);
    std::printf("posicao inicial: %s\n", nomePosicao(posicaoAnterior).c_str());

    std::printf("\nModo DEBUG -- NAO manda nenhum atalho, so' escreve no bloco de notas.\n");
    std::printf("Pode operar manualmente na conta de origem agora. CTRL+C pra sair.\n");

    while (true) {
        int nova = aguardarProximaPosicao(capBadge, cal, origem);

        for (const Evento& evento : transicao(posicaoAnterior, nova)) {
            const char* rotulo = rotuloDebug(evento);

            SYSTEMTIME st;
            GetLocalTime(&st);
            char linha[80];
            std::snprintf(linha, sizeof(linha), "%02d:%02d:%02d.%03d %s",
                          st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, rotulo);

            std::printf("[debug] %s -> %s : rotulo %s, comando %s\n",
                        nomePosicao(posicaoAnterior).c_str(), nomePosicao(nova).c_str(), rotulo,
                        nomeComando(evento.comando));

            if (!escreverLinha(blocoDeNotas, linha)) {
                std::fprintf(stderr, "[debug] falha ao escrever no bloco de notas (a janela ainda esta aberta?)\n");
            }
        }
        posicaoAnterior = nova;
    }
}

int modoRodar() {
    Calibracao cal;
    if (!carregarCalibracaoOuAvisar(cal)) return 1;

    HWND origem = escolherJanelaPorClique("Profit da conta de ORIGEM (a que sera' lida)");
    if (!origem) {
        std::printf("Nenhuma janela escolhida, saindo.\n");
        return 1;
    }

    RegiaoTela regiaoLeitura = localizarBadge(cal, CAMINHO_CALIBRACAO, origem);

    HWND destino = escolherJanelaPorClique("Profit da conta SIMULADORA (destino dos atalhos)");
    if (!destino) {
        std::printf("Nenhuma janela escolhida, saindo.\n");
        return 1;
    }

    if (origem == destino) {
        std::printf(">> aviso: origem e destino sao a MESMA janela (ok pra teste, mas em uso real\n"
                    ">> isso reage ao proprio envio -- confira antes de deixar rodando sozinho).\n");
    }

    definirEspacamentoMinimoMs(perguntarDelayMs());
    std::printf("espacamento minimo entre comandos: %dms\n", espacamentoMinimoAtualMs());

    int tamanhoMaximo = perguntarTamanhoMaximoPosicao();
    std::printf("tamanho maximo de posicao antes de parar o envio automatico: %d\n", tamanhoMaximo);

    CapturaRegiao capBadge(regiaoLeitura);
    if (!capBadge.capturar()) {
        std::fprintf(stderr, "ERRO: falha ao capturar a regiao calibrada. Recalibre.\n");
        return 1;
    }
    int posicaoAnterior = classificar(capBadge, cal).value_or(0);
    std::printf("posicao inicial: %s\n", nomePosicao(posicaoAnterior).c_str());
    std::printf("\nRodando. Feche a janela de teste (ou CTRL+C aqui) pra sair.\n");

    // leitura + envio automatico rodam numa thread separada -- a thread
    // principal fica livre pra bombear mensagens da janela de teste
    // (Compra/Venda/Zerar), que manda o atalho manualmente a qualquer
    // momento, sem interromper a leitura.
    std::thread threadLeitura([&capBadge, &cal, origem, destino, posicaoAnterior, tamanhoMaximo]() mutable {
        while (true) {
            int nova = aguardarProximaPosicao(capBadge, cal, origem);

            for (const Evento& evento : transicao(posicaoAnterior, nova)) {
                char tecla = (evento.comando == Comando::Compra) ? 'C' : (evento.comando == Comando::Venda) ? 'V' : 'A';
                std::printf("[sinal] %s -> %s : comando %s (ALT+%c)\n",
                            nomePosicao(posicaoAnterior).c_str(), nomePosicao(nova).c_str(),
                            nomeComando(evento.comando), tecla);

                if (std::abs(nova) > tamanhoMaximo) {
                    std::printf("\n!!! TRAVA DE SEGURANCA !!! tamanho (%d) passou do maximo (%d) --\n"
                                "envio automatico PARADO (provavel loop ou leitura errada). Os\n"
                                "botoes da janela de teste continuam funcionando pra voce zerar\n"
                                "manualmente. Reinicie o \"rodar\" depois de conferir.\n\n",
                                std::abs(nova), tamanhoMaximo);
                    return;
                }

                enviarAltTeclaComEspacamento(destino, tecla);
            }
            posicaoAnterior = nova;
        }
    });
    threadLeitura.detach();

    rodarJanelaTeste(destino);
    return 0;
}

int modoTestarAtalho() {
    HWND alvo = escolherJanelaPorClique("janela de teste (a que deve receber o ALT+C/V/A)");
    if (!alvo) {
        std::printf("Nenhuma janela escolhida, saindo.\n");
        return 1;
    }

    definirEspacamentoMinimoMs(perguntarDelayMs());
    std::printf("espacamento minimo entre comandos (so' vale pro c/v/a, nao pro fc/fv/fa): %dms\n",
                espacamentoMinimoAtualMs());

    std::printf("\nDigite um comando e ENTER:\n");
    std::printf("  c / v / a      -> manda via PostMessage (sem foco)\n");
    std::printf("  fc / fv / fa   -> manda via SendInput (rouba o foco e devolve)\n");
    std::printf("  sair           -> termina\n");

    std::string linha;
    while (true) {
        std::printf("> ");
        std::fflush(stdout);
        if (!std::getline(std::cin, linha)) break;
        if (linha == "sair") break;

        if (linha == "c") enviarAltTeclaComEspacamento(alvo, 'C');
        else if (linha == "v") enviarAltTeclaComEspacamento(alvo, 'V');
        else if (linha == "a") enviarAltTeclaComEspacamento(alvo, 'A');
        else if (linha == "fc") enviarAltTeclaComFoco(alvo, 'C');
        else if (linha == "fv") enviarAltTeclaComFoco(alvo, 'V');
        else if (linha == "fa") enviarAltTeclaComFoco(alvo, 'A');
        else std::printf("nao entendi \"%s\" -- use c/v/a, fc/fv/fa ou sair\n", linha.c_str());
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("uso: %s calibrar|debug|rodar|testaratalho\n", argv[0]);
        return 1;
    }
    if (std::strcmp(argv[1], "calibrar") == 0) return modoCalibrar();
    if (std::strcmp(argv[1], "debug") == 0) return modoDebug();
    if (std::strcmp(argv[1], "rodar") == 0) return modoRodar();
    if (std::strcmp(argv[1], "testaratalho") == 0) return modoTestarAtalho();

    std::printf("comando desconhecido: %s\nuso: %s calibrar|debug|rodar|testaratalho\n", argv[1], argv[0]);
    return 1;
}
