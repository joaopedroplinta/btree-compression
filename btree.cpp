/*
 * Arvore B com chaves string (C++)
 * Uso: ./btree <grau_minimo_t>
 *   T=3  -> max 5 chaves/no (pequena)
 *   T=50 -> max 99 chaves/no (grande)
 * Comandos: insert, read, delete, save, load, print, list, quit
 */
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <sstream>
#include <algorithm>

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

struct Node {
    int n;
    bool leaf;
    std::vector<std::string> keys;
    std::vector<Node*> ch;

    Node(int t, bool lf) : n(0), leaf(lf), keys(2*t - 1), ch(2*t, nullptr) {}
};

class BTree {
    int t;
    Node* root;
    long nodes_vis;

    Node* alloc(bool lf) { return new Node(t, lf); }

    void free_rec(Node* x) {
        if (!x) return;
        if (!x->leaf) for (int i = 0; i <= x->n; i++) free_rec(x->ch[i]);
        delete x;
    }

    Node* search_node(Node* x, const std::string& k, int* idx) {
        if (!x) return nullptr;
        nodes_vis++;
        auto it = std::lower_bound(x->keys.begin(), x->keys.begin() + x->n, k);
        int i = (int)(it - x->keys.begin());
        if (i < x->n && *it == k) { if (idx) *idx = i; return x; }
        if (x->leaf) return nullptr;
        return search_node(x->ch[i], k, idx);
    }

    void split_child(Node* x, int i) {
        Node* y = x->ch[i];
        Node* z = alloc(y->leaf);
        z->n = t - 1;
        for (int j = 0; j < t - 1; j++) z->keys[j] = y->keys[j + t];
        if (!y->leaf) for (int j = 0; j < t; j++) z->ch[j] = y->ch[j + t];
        y->n = t - 1;
        for (int j = x->n; j >= i + 1; j--) x->ch[j + 1] = x->ch[j];
        x->ch[i + 1] = z;
        for (int j = x->n - 1; j >= i; j--) x->keys[j + 1] = x->keys[j];
        x->keys[i] = y->keys[t - 1];
        x->n++;
    }

    void insert_nonfull(Node* x, const std::string& k) {
        nodes_vis++;
        auto it = std::lower_bound(x->keys.begin(), x->keys.begin() + x->n, k);
        int i = (int)(it - x->keys.begin());
        if (x->leaf) {
            for (int j = x->n; j > i; j--) x->keys[j] = x->keys[j - 1];
            x->keys[i] = k;
            x->n++;
        } else {
            if (x->ch[i]->n == 2*t - 1) {
                split_child(x, i);
                if (k > x->keys[i]) i++;
            }
            insert_nonfull(x->ch[i], k);
        }
    }

    int find_idx(Node* x, const std::string& k) {
        auto it = std::lower_bound(x->keys.begin(), x->keys.begin() + x->n, k);
        return (int)(it - x->keys.begin());
    }

    std::string get_pred(Node* x) {
        while (!x->leaf) x = x->ch[x->n];
        return x->keys[x->n - 1];
    }

    std::string get_succ(Node* x) {
        while (!x->leaf) x = x->ch[0];
        return x->keys[0];
    }

    void merge_children(Node* x, int i) {
        Node* left  = x->ch[i];
        Node* right = x->ch[i + 1];
        left->keys[t - 1] = x->keys[i];
        for (int j = 0; j < right->n; j++) left->keys[t + j] = right->keys[j];
        if (!left->leaf) for (int j = 0; j <= right->n; j++) left->ch[t + j] = right->ch[j];
        left->n = 2*t - 1;
        for (int j = i + 1; j < x->n; j++) x->keys[j - 1] = x->keys[j];
        for (int j = i + 2; j <= x->n; j++) x->ch[j - 1] = x->ch[j];
        x->n--;
        right->n = 0;
        delete right;
    }

    void borrow_from_prev(Node* x, int i) {
        Node* child = x->ch[i];
        Node* sib   = x->ch[i - 1];
        for (int j = child->n; j > 0; j--) child->keys[j] = child->keys[j - 1];
        if (!child->leaf) for (int j = child->n + 1; j > 0; j--) child->ch[j] = child->ch[j - 1];
        child->keys[0] = x->keys[i - 1];
        if (!sib->leaf) child->ch[0] = sib->ch[sib->n];
        x->keys[i - 1] = sib->keys[sib->n - 1];
        child->n++;
        sib->n--;
    }

    void borrow_from_next(Node* x, int i) {
        Node* child = x->ch[i];
        Node* sib   = x->ch[i + 1];
        child->keys[child->n] = x->keys[i];
        if (!child->leaf) child->ch[child->n + 1] = sib->ch[0];
        x->keys[i] = sib->keys[0];
        for (int j = 1; j < sib->n; j++) sib->keys[j - 1] = sib->keys[j];
        if (!sib->leaf) for (int j = 1; j <= sib->n; j++) sib->ch[j - 1] = sib->ch[j];
        child->n++;
        sib->n--;
    }

    void fill_child(Node* x, int i) {
        if      (i > 0 && x->ch[i-1]->n >= t)      borrow_from_prev(x, i);
        else if (i < x->n && x->ch[i+1]->n >= t)   borrow_from_next(x, i);
        else if (i < x->n)                          merge_children(x, i);
        else                                         merge_children(x, i - 1);
    }

    bool delete_from(Node* x, const std::string& k) {
        nodes_vis++;
        int i = find_idx(x, k);

        if (i < x->n && x->keys[i] == k) {
            if (x->leaf) {
                for (int j = i + 1; j < x->n; j++) x->keys[j - 1] = x->keys[j];
                x->n--;
                return true;
            }
            if (x->ch[i]->n >= t) {
                std::string pred = get_pred(x->ch[i]);
                x->keys[i] = pred;
                return delete_from(x->ch[i], pred);
            }
            if (x->ch[i + 1]->n >= t) {
                std::string succ = get_succ(x->ch[i + 1]);
                x->keys[i] = succ;
                return delete_from(x->ch[i + 1], succ);
            }
            merge_children(x, i);
            return delete_from(x->ch[i], k);
        }

        if (x->leaf) return false;

        bool last = (i == x->n);
        if (x->ch[i]->n < t) fill_child(x, i);
        if (last && i > x->n) return delete_from(x->ch[i - 1], k);
        return delete_from(x->ch[i], k);
    }

    void save_node(Node* x, std::ofstream& f) {
        int lf = x->leaf ? 1 : 0;
        f.write(reinterpret_cast<char*>(&lf),   sizeof(int));
        f.write(reinterpret_cast<char*>(&x->n), sizeof(int));
        for (int i = 0; i < x->n; i++) {
            int len = (int)x->keys[i].size();
            f.write(reinterpret_cast<char*>(&len), sizeof(int));
            f.write(x->keys[i].data(), len);
        }
        if (!x->leaf) for (int i = 0; i <= x->n; i++) save_node(x->ch[i], f);
    }

    Node* load_node(std::ifstream& f) {
        int lf, n;
        if (!f.read(reinterpret_cast<char*>(&lf), sizeof(int))) return nullptr;
        if (!f.read(reinterpret_cast<char*>(&n),  sizeof(int))) return nullptr;
        Node* x = alloc(lf != 0);
        x->n = n;
        for (int i = 0; i < n; i++) {
            int len;
            f.read(reinterpret_cast<char*>(&len), sizeof(int));
            x->keys[i].resize(len);
            f.read(&x->keys[i][0], len);
        }
        if (!x->leaf) for (int i = 0; i <= n; i++) x->ch[i] = load_node(f);
        return x;
    }

    int height_rec(Node* x) {
        if (!x || x->leaf) return 1;
        return 1 + height_rec(x->ch[0]);
    }

    void count_rec(Node* x, long& nodes, long& keys) {
        if (!x) return;
        nodes++;
        keys += x->n;
        if (!x->leaf)
            for (int i = 0; i <= x->n; i++) count_rec(x->ch[i], nodes, keys);
    }

    void print_inorder(Node* x) {
        if (!x) return;
        for (int i = 0; i < x->n; i++) {
            if (!x->leaf) print_inorder(x->ch[i]);
            std::cout << x->keys[i] << '\n';
        }
        if (!x->leaf) print_inorder(x->ch[x->n]);
    }

    void print_tree_rec(Node* x, const std::string& prefix, bool is_root, bool is_last) {
        if (!x) return;
        if (is_root) {
            std::cout << "[";
        } else {
            std::cout << prefix << (is_last ? "\xe2\x94\x94\xe2\x94\x80\xe2\x94\x80 ["   // └──
                                            : "\xe2\x94\x9c\xe2\x94\x80\xe2\x94\x80 ["); // ├──
        }
        for (int i = 0; i < x->n; i++) {
            if (i > 0) std::cout << " | ";
            std::cout << x->keys[i];
        }
        std::cout << "]\n";
        if (!x->leaf) {
            std::string child_prefix = is_root
                ? ""
                : prefix + (is_last ? "    " : "\xe2\x94\x82   "); // │
            for (int i = 0; i <= x->n; i++)
                print_tree_rec(x->ch[i], child_prefix, false, i == x->n);
        }
    }

public:
    BTree(int t_) : t(t_), root(alloc(true)), nodes_vis(0) {}
    ~BTree() { free_rec(root); }

    long get_nodes_visited() const { return nodes_vis; }

    bool insert(const std::string& k) {
        nodes_vis = 0;
        int idx;
        if (search_node(root, k, &idx)) {
            std::cout << "Chave '" << k << "' ja existe.\n";
            nodes_vis = 0;
            return false;
        }
        nodes_vis = 0;
        Node* r = root;
        if (r->n == 2*t - 1) {
            Node* s = alloc(false);
            root = s;
            s->ch[0] = r;
            split_child(s, 0);
            insert_nonfull(s, k);
        } else {
            insert_nonfull(r, k);
        }
        return true;
    }

    bool read(const std::string& k) {
        nodes_vis = 0;
        int idx;
        Node* found = search_node(root, k, &idx);
        if (found) {
            std::cout << "Encontrado: '" << found->keys[idx] << "'\n";
            return true;
        }
        std::cout << "Chave '" << k << "' nao encontrada.\n";
        return false;
    }

    bool remove(const std::string& k) {
        nodes_vis = 0;
        if (!root || root->n == 0) { std::cout << "Arvore vazia.\n"; return false; }
        if (!delete_from(root, k)) {
            std::cout << "Chave '" << k << "' nao encontrada.\n";
            return false;
        }
        if (root->n == 0 && !root->leaf) {
            Node* old = root;
            root = old->ch[0];
            old->ch[0] = nullptr;
            old->n = 0;
            delete old;
        }
        return true;
    }

    void save(const std::string& path) {
        std::ofstream f(path, std::ios::binary);
        if (!f) { std::cerr << "Erro ao abrir '" << path << "'\n"; return; }
        f.write(reinterpret_cast<const char*>(&t), sizeof(int));
        save_node(root, f);
        std::cout << "Arvore salva em '" << path << "'.\n";
    }

    void load(const std::string& path) {
        std::ifstream f(path, std::ios::binary);
        if (!f) { std::cerr << "Erro ao abrir '" << path << "'\n"; return; }
        int saved_t;
        f.read(reinterpret_cast<char*>(&saved_t), sizeof(int));
        free_rec(root);
        root = nullptr;
        if (saved_t != t) {
            std::cout << "Aviso: arquivo usa T=" << saved_t << ", ajustando.\n";
            t = saved_t;
        }
        root = load_node(f);
        std::cout << "Arvore carregada de '" << path << "' (T=" << t << ").\n";
    }

    void stats() {
        long nodes = 0, keys = 0;
        count_rec(root, nodes, keys);
        int h = height_rec(root);
        int max_per_node = 2*t - 1;
        double fill = (nodes > 0)
            ? 100.0 * (double)keys / (nodes * max_per_node)
            : 0.0;
        std::cout << "Altura       : " << h          << '\n'
                  << "Total nos    : " << nodes       << '\n'
                  << "Total chaves : " << keys        << '\n'
                  << "Max chaves/no: " << max_per_node << " (T=" << t << ")\n"
                  << "Fill factor  : " << fill        << "%\n";
    }

    void print() { print_tree_rec(root, "", true, true); }
    void list()  { print_inorder(root); }
};

int main(int argc, char* argv[]) {
    int t = 3;
    if (argc > 1) t = std::max(2, std::stoi(argv[1]));

    std::cout << "Arvore B | T=" << t
              << " | max chaves/no=" << 2*t - 1
              << " | min chaves/no=" << t - 1 << '\n';

    BTree bt(t);
    std::string line, cmd, key, path;

    while (true) {
        std::cout << "> " << std::flush;
        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;

        std::istringstream ss(line);
        ss >> cmd;

        bool timed = true;
        auto t0 = std::chrono::steady_clock::now();

        if (cmd == "insert" && (ss >> key)) {
            bt.insert(key);
        } else if (cmd == "read" && (ss >> key)) {
            bt.read(key);
        } else if (cmd == "delete" && (ss >> key)) {
            bt.remove(key);
        } else if (cmd == "save" && (ss >> path)) {
            bt.save(path); timed = false;
        } else if (cmd == "load" && (ss >> path)) {
            bt.load(path); timed = false;
        } else if (cmd == "stats") {
            bt.stats(); timed = false;
        } else if (cmd == "print") {
            bt.print(); timed = false;
        } else if (cmd == "list") {
            bt.list(); timed = false;
        } else if (cmd == "quit" || cmd == "exit") {
            break;
        } else {
            std::cout << "Comandos: insert <chave> | read <chave> | delete <chave>"
                         " | save <arq> | load <arq> | stats | print | list | quit\n";
            timed = false;
        }

        if (timed) {
            auto t1 = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double>(t1 - t0).count();
            long mem_after = get_mem_kb();
            std::cout << "[nos visitados: " << bt.get_nodes_visited()
                      << " | tempo: " << elapsed << " s"
                      << " | memoria: " << mem_after << " KB]\n";
        }
    }
    return 0;
}
