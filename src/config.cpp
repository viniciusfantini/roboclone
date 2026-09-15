#include "config.h"
#include <fstream>
#include <map>
#include <cstdlib>
#include <cstdint>

namespace {
std::string caminhoRefs(const std::string& caminhoBase) { return caminhoBase + ".refs"; }
}

bool salvarCalibracao(const Calibracao& c, const std::string& caminhoBase) {
    {
        std::ofstream f(caminhoBase, std::ios::trunc);
        if (!f) return false;
        f << "badge.x=" << c.regiaoBadge.x << "\n";
        f << "badge.y=" << c.regiaoBadge.y << "\n";
        f << "badge.largura=" << c.regiaoBadge.largura << "\n";
        f << "badge.altura=" << c.regiaoBadge.altura << "\n";
        f << "tolerancia=" << c.tolerancia << "\n";
        f << "quantidadeReferencias=" << c.referencias.size() << "\n";
        f << "temReplay=" << (c.temReplay ? 1 : 0) << "\n";
        f << "deslocamentoReplayY=" << c.deslocamentoReplayY << "\n";
    }

    std::ofstream fr(caminhoRefs(caminhoBase), std::ios::trunc | std::ios::binary);
    if (!fr) return false;
    for (const auto& ref : c.referencias) {
        int32_t q = ref.quantidade;
        fr.write(reinterpret_cast<const char*>(&q), sizeof(q));
        fr.write(reinterpret_cast<const char*>(ref.bitmap.data()), (std::streamsize)ref.bitmap.size());
    }
    return (bool)fr;
}

bool carregarCalibracao(Calibracao& c, const std::string& caminhoBase) {
    std::ifstream f(caminhoBase);
    if (!f) return false;

    std::map<std::string, long long> valores;
    std::string linha;
    while (std::getline(f, linha)) {
        auto pos = linha.find('=');
        if (pos == std::string::npos) continue;
        std::string chave = linha.substr(0, pos);
        long long valor = std::atoll(linha.substr(pos + 1).c_str());
        valores[chave] = valor;
    }
    if (valores.empty()) return false;

    c.regiaoBadge.x = (int)valores["badge.x"];
    c.regiaoBadge.y = (int)valores["badge.y"];
    c.regiaoBadge.largura = (int)valores["badge.largura"];
    c.regiaoBadge.altura = (int)valores["badge.altura"];
    c.tolerancia = valores["tolerancia"];
    size_t quantidadeReferencias = (size_t)valores["quantidadeReferencias"];

    c.temReplay = valores["temReplay"] != 0;
    c.deslocamentoReplayY = (int)valores["deslocamentoReplayY"];

    size_t bytesBitmap = (size_t)c.regiaoBadge.largura * c.regiaoBadge.altura * 4;
    if (bytesBitmap == 0 || quantidadeReferencias == 0) return false;

    std::ifstream fr(caminhoRefs(caminhoBase), std::ios::binary);
    if (!fr) return false;

    c.referencias.clear();
    c.referencias.reserve(quantidadeReferencias);
    for (size_t i = 0; i < quantidadeReferencias; ++i) {
        int32_t q = 0;
        fr.read(reinterpret_cast<char*>(&q), sizeof(q));
        if (!fr) return false;
        ReferenciaBadge ref;
        ref.quantidade = q;
        ref.bitmap.resize(bytesBitmap);
        fr.read(reinterpret_cast<char*>(ref.bitmap.data()), (std::streamsize)bytesBitmap);
        if (!fr) return false;
        c.referencias.push_back(std::move(ref));
    }
    return true;
}
