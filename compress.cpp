/*
 * Compressao/descompressao de arquivos usando LZW e Huffman canonico
 * Uso: ./compress <lzw|huffman> <c|d> <entrada> <saida>
 *   c = comprimir  |  d = descomprimir
 *
 * Saida: metricas no stdout (tamanho, taxa, tempo, memoria)
 */
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <array>
#include <unordered_map>
#include <map>
#include <queue>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <cstring>

// ---- Utilitarios ----

static long get_mem_kb() {
    std::ifstream f("/proc/self/status");
    if (!f) return 0;
    std::string line;
    while (std::getline(f, line)) {
        long kb;
        if (sscanf(line.c_str(), "VmRSS: %ld kB", &kb) == 1) return kb;
    }
    return 0;
}

static std::vector<uint8_t> read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) throw std::runtime_error("Nao foi possivel abrir: " + path);
    auto sz = f.tellg();
    f.seekg(0);
    std::vector<uint8_t> buf((size_t)sz);
    if (sz > 0) f.read(reinterpret_cast<char*>(buf.data()), sz);
    return buf;
}

static void write_file(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("Nao foi possivel criar: " + path);
    if (!data.empty()) f.write(reinterpret_cast<const char*>(data.data()), (std::streamsize)data.size());
}

// Helpers para escrita/leitura de inteiros little-endian no vetor
static void push_u16(std::vector<uint8_t>& v, uint16_t x) {
    v.push_back(x & 0xFF);
    v.push_back((x >> 8) & 0xFF);
}
static void push_u64(std::vector<uint8_t>& v, uint64_t x) {
    for (int i = 0; i < 8; i++) v.push_back((x >> (i*8)) & 0xFF);
}
static uint16_t read_u16(const std::vector<uint8_t>& v, size_t pos) {
    return (uint16_t)v[pos] | ((uint16_t)v[pos+1] << 8);
}
static uint64_t read_u64(const std::vector<uint8_t>& v, size_t pos) {
    uint64_t r = 0;
    for (int i = 0; i < 8; i++) r |= ((uint64_t)v[pos+i] << (i*8));
    return r;
}

// ============================================================
// LZW — codigos de 16 bits, dicionario max 65536 entradas
// ============================================================

/*
 * Formato do arquivo LZW:
 *   [4 bytes] magic "LZW\0"
 *   [8 bytes] tamanho original (uint64_t LE)
 *   [8 bytes] numero de codigos (uint64_t LE)
 *   [N*2 bytes] codigos (uint16_t LE cada)
 */

static std::vector<uint8_t> lzw_compress(const std::vector<uint8_t>& in) {
    std::unordered_map<std::string, uint16_t> dict;
    dict.reserve(65536);
    for (int i = 0; i < 256; i++)
        dict[std::string(1, (char)i)] = (uint16_t)i;
    uint32_t next_code = 256;

    std::vector<uint16_t> codes;
    codes.reserve(in.size());
    std::string cur;

    for (uint8_t byte : in) {
        std::string ext = cur + (char)byte;
        if (dict.count(ext)) {
            cur = std::move(ext);
        } else {
            codes.push_back(dict[cur]);
            if (next_code <= 65535) {
                dict[ext] = (uint16_t)next_code++;
            }
            cur = std::string(1, (char)byte);
        }
    }
    if (!cur.empty()) codes.push_back(dict[cur]);

    std::vector<uint8_t> out;
    out.reserve(4 + 8 + 8 + codes.size() * 2);
    out.push_back('L'); out.push_back('Z'); out.push_back('W'); out.push_back('\0');
    push_u64(out, (uint64_t)in.size());
    push_u64(out, (uint64_t)codes.size());
    for (uint16_t c : codes) push_u16(out, c);
    return out;
}

static std::vector<uint8_t> lzw_decompress(const std::vector<uint8_t>& in) {
    if (in.size() < 20 ||
        in[0] != 'L' || in[1] != 'Z' || in[2] != 'W' || in[3] != '\0')
        throw std::runtime_error("Arquivo LZW invalido (magic incorreto)");

    uint64_t orig_size  = read_u64(in, 4);
    uint64_t num_codes  = read_u64(in, 12);
    size_t   data_start = 20;

    if (in.size() < data_start + num_codes * 2)
        throw std::runtime_error("Arquivo LZW truncado");

    if (num_codes == 0) return {};

    // Reconstroi dicionario
    std::vector<std::string> dict;
    dict.reserve(65536);
    for (int i = 0; i < 256; i++) dict.push_back(std::string(1, (char)i));

    std::vector<uint8_t> out;
    out.reserve(orig_size);

    auto get_code = [&](uint64_t i) -> uint16_t {
        return read_u16(in, data_start + i * 2);
    };

    uint16_t first = get_code(0);
    std::string prev = dict[first];
    for (char c : prev) out.push_back((uint8_t)c);

    for (uint64_t i = 1; i < num_codes; i++) {
        uint16_t code = get_code(i);
        std::string entry;

        if ((size_t)code < dict.size()) {
            entry = dict[code];
        } else if ((size_t)code == dict.size()) {
            entry = prev + prev[0];
        } else {
            throw std::runtime_error("Codigo LZW invalido");
        }

        for (char c : entry) out.push_back((uint8_t)c);

        if (dict.size() < 65536)
            dict.push_back(prev + entry[0]);

        prev = std::move(entry);
    }

    return out;
}

// ============================================================
// Huffman canonico
// ============================================================

/*
 * Formato do arquivo Huffman:
 *   [4 bytes]   magic "HUF\0"
 *   [8 bytes]   tamanho original (uint64_t LE)
 *   [1 byte]    bits de padding no ultimo byte de dados
 *   [256 bytes] comprimentos dos codigos canonicos (0 = simbolo ausente)
 *   [N bytes]   dados codificados em bits
 */

struct HuffNode {
    uint8_t  sym;
    uint64_t freq;
    HuffNode* left  = nullptr;
    HuffNode* right = nullptr;
    bool is_leaf() const { return !left && !right; }
};

struct HuffCmp {
    bool operator()(HuffNode* a, HuffNode* b) { return a->freq > b->freq; }
};

static void free_tree(HuffNode* n) {
    if (!n) return;
    free_tree(n->left);
    free_tree(n->right);
    delete n;
}

static void collect_lengths(HuffNode* n, int depth, std::array<uint8_t, 256>& lens) {
    if (!n) return;
    if (n->is_leaf()) { lens[(int)n->sym] = (uint8_t)depth; return; }
    collect_lengths(n->left,  depth + 1, lens);
    collect_lengths(n->right, depth + 1, lens);
}

// Gera codigos canonicos de Huffman a partir dos comprimentos
// Saida: codes[sym] = string de bits (ex: "01101")
static std::map<uint8_t, std::string>
canonical_codes(const std::array<uint8_t, 256>& lens) {
    // Coleta simbolos com comprimento > 0, ordena por (len, sym)
    std::vector<std::pair<uint8_t, uint8_t>> syms; // (length, symbol)
    for (int i = 0; i < 256; i++)
        if (lens[i] > 0) syms.push_back({lens[i], (uint8_t)i});
    std::sort(syms.begin(), syms.end());

    std::map<uint8_t, std::string> codes;
    uint32_t code = 0;
    int prev_len = 0;
    for (auto& [len, sym] : syms) {
        code <<= (len - prev_len);
        std::string bits;
        bits.reserve(len);
        for (int i = len - 1; i >= 0; i--) bits += (char)('0' + ((code >> i) & 1));
        codes[sym] = bits;
        code++;
        prev_len = len;
    }
    return codes;
}

// Escritor de bits (MSB primeiro dentro de cada byte)
class BitWriter {
    std::vector<uint8_t>& out;
    uint8_t cur = 0;
    int pos = 0; // proximo bit dentro do byte atual (0..7)
public:
    explicit BitWriter(std::vector<uint8_t>& v) : out(v) {}

    void write(int bit) {
        cur |= (uint8_t)((bit & 1) << (7 - pos));
        if (++pos == 8) { out.push_back(cur); cur = 0; pos = 0; }
    }

    // Fecha o byte atual e retorna quantos bits de padding foram adicionados
    uint8_t flush() {
        if (pos > 0) { out.push_back(cur); uint8_t pad = (uint8_t)(8 - pos); pos = 0; cur = 0; return pad; }
        return 0;
    }
};

// Leitor de bits (MSB primeiro)
class BitReader {
    const std::vector<uint8_t>& data;
    size_t byte_pos;
    int bit_pos;        // bit dentro do byte atual (7=MSB .. 0=LSB)
    size_t total_bits;
    size_t bits_read = 0;
public:
    BitReader(const std::vector<uint8_t>& d, size_t start, uint8_t padding)
        : data(d), byte_pos(start), bit_pos(7) {
        size_t data_bytes = (start < d.size()) ? (d.size() - start) : 0;
        total_bits = data_bytes * 8 - (size_t)padding;
    }

    // Retorna 0 ou 1, ou -1 se nao ha mais bits
    int read() {
        if (bits_read >= total_bits) return -1;
        int bit = (data[byte_pos] >> bit_pos) & 1;
        bits_read++;
        if (--bit_pos < 0) { bit_pos = 7; byte_pos++; }
        return bit;
    }
};

static std::vector<uint8_t> huffman_compress(const std::vector<uint8_t>& in) {
    std::array<uint8_t, 256> lens = {};

    if (in.empty()) {
        // Arquivo vazio: header sem dados
        std::vector<uint8_t> out;
        out.push_back('H'); out.push_back('U'); out.push_back('F'); out.push_back('\0');
        push_u64(out, 0);
        out.push_back(0); // padding
        out.insert(out.end(), lens.begin(), lens.end());
        return out;
    }

    // Contagem de frequencias
    std::array<uint64_t, 256> freq = {};
    for (uint8_t b : in) freq[b]++;

    // Caso especial: unico simbolo
    int unique = 0; uint8_t only_sym = 0;
    for (int i = 0; i < 256; i++) if (freq[i]) { unique++; only_sym = (uint8_t)i; }

    if (unique == 1) {
        lens[only_sym] = 1;
    } else {
        // Constroi arvore de Huffman
        std::priority_queue<HuffNode*, std::vector<HuffNode*>, HuffCmp> pq;
        for (int i = 0; i < 256; i++) {
            if (freq[i]) {
                auto* n = new HuffNode;
                n->sym = (uint8_t)i; n->freq = freq[i];
                pq.push(n);
            }
        }
        while (pq.size() > 1) {
            auto* a = pq.top(); pq.pop();
            auto* b = pq.top(); pq.pop();
            auto* parent = new HuffNode;
            parent->freq  = a->freq + b->freq;
            parent->left  = a;
            parent->right = b;
            pq.push(parent);
        }
        HuffNode* tree_root = pq.top(); pq.pop();
        collect_lengths(tree_root, 0, lens);
        free_tree(tree_root);
    }

    auto codes = canonical_codes(lens);

    // Monta cabecalho
    std::vector<uint8_t> out;
    out.reserve(4 + 8 + 1 + 256 + in.size());
    out.push_back('H'); out.push_back('U'); out.push_back('F'); out.push_back('\0');
    push_u64(out, (uint64_t)in.size());
    size_t padding_pos = out.size();
    out.push_back(0); // placeholder para padding
    out.insert(out.end(), lens.begin(), lens.end());

    // Codifica dados
    BitWriter bw(out);
    for (uint8_t b : in) {
        const std::string& code = codes[b];
        for (char bit : code) bw.write(bit - '0');
    }
    uint8_t padding = bw.flush();
    out[padding_pos] = padding;

    return out;
}

// Trie binaria para decodificacao Huffman.
// Nós armazenados em vetor; ch[0]/ch[1] são índices (-1 = ausente).
// Folhas têm sym >= 0; nós internos têm sym = -1.
struct TrieNode {
    int ch[2];
    int sym;
    TrieNode() : ch{-1, -1}, sym(-1) {}
};

static std::vector<TrieNode> build_decode_trie(
        const std::map<uint8_t, std::string>& codes) {
    std::vector<TrieNode> trie(1); // índice 0 = raiz
    for (auto& [sym, bits] : codes) {
        int node = 0;
        for (char c : bits) {
            int b = c - '0';
            if (trie[node].ch[b] == -1) {
                trie[node].ch[b] = (int)trie.size();
                trie.emplace_back();
            }
            node = trie[node].ch[b];
        }
        trie[node].sym = (int)(uint8_t)sym;
    }
    return trie;
}

static std::vector<uint8_t> huffman_decompress(const std::vector<uint8_t>& in) {
    const size_t HEADER = 4 + 8 + 1 + 256; // magic + orig_size + padding + lens
    if (in.size() < HEADER ||
        in[0] != 'H' || in[1] != 'U' || in[2] != 'F' || in[3] != '\0')
        throw std::runtime_error("Arquivo Huffman invalido (magic incorreto)");

    uint64_t orig_size = read_u64(in, 4);
    uint8_t  padding   = in[12];

    std::array<uint8_t, 256> lens = {};
    std::copy(in.begin() + 13, in.begin() + 13 + 256, lens.begin());

    if (orig_size == 0) return {};

    auto codes = canonical_codes(lens);
    auto trie  = build_decode_trie(codes);

    std::vector<uint8_t> out;
    out.reserve(orig_size);

    BitReader br(in, HEADER, padding);
    int node = 0;
    while (out.size() < orig_size) {
        int bit = br.read();
        if (bit < 0) break;
        node = trie[node].ch[bit];
        if (node == -1) throw std::runtime_error("Trie Huffman: caminho invalido");
        if (trie[node].sym >= 0) {
            out.push_back((uint8_t)trie[node].sym);
            node = 0;
        }
    }

    return out;
}

// ============================================================
// Main
// ============================================================

int main(int argc, char* argv[]) {
    if (argc != 5) {
        std::cerr << "Uso: " << argv[0]
                  << " <lzw|huffman> <c|d> <entrada> <saida>\n"
                  << "  c = comprimir  |  d = descomprimir\n";
        return 1;
    }

    std::string algo  = argv[1];
    std::string mode  = argv[2];
    std::string ipath = argv[3];
    std::string opath = argv[4];

    if (algo != "lzw" && algo != "huffman") {
        std::cerr << "Algoritmo invalido. Use 'lzw' ou 'huffman'.\n";
        return 1;
    }
    if (mode != "c" && mode != "d") {
        std::cerr << "Modo invalido. Use 'c' (comprimir) ou 'd' (descomprimir).\n";
        return 1;
    }

    try {
        auto t0 = std::chrono::steady_clock::now();

        std::vector<uint8_t> input  = read_file(ipath);
        std::vector<uint8_t> output;

        if (algo == "lzw") {
            output = (mode == "c") ? lzw_compress(input) : lzw_decompress(input);
        } else {
            output = (mode == "c") ? huffman_compress(input) : huffman_decompress(input);
        }

        write_file(opath, output);

        auto t1 = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(t1 - t0).count();
        long mem_after = get_mem_kb();

        std::string modo_str = (mode == "c") ? "compressao" : "descompressao";
        std::cout << "Algoritmo    : " << algo << " (" << modo_str << ")\n"
                  << "Entrada      : " << input.size()  << " bytes\n"
                  << "Saida        : " << output.size() << " bytes\n";

        if (mode == "c" && input.size() > 0) {
            double ratio = (double)input.size() / (double)output.size();
            double saved = 100.0 * (1.0 - (double)output.size() / (double)input.size());
            std::cout << "Taxa compres.: " << ratio << "x  ("
                      << saved << "% menor)\n";
        }

        std::cout << "Tempo        : " << elapsed    << " s\n"
                  << "Memoria      : " << mem_after  << " KB\n";

    } catch (const std::exception& e) {
        std::cerr << "Erro: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
