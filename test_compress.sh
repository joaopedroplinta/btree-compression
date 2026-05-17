#!/usr/bin/env bash
# Comprime e descomprime arquivos de arvore usando LZW e Huffman.
# Gera os arquivos .bin automaticamente se ainda nao existirem.
# Tamanhos testados: 100, 10 000, 100 000, 1 000 000 chaves — T=3 e T=50.

set -e
COMPRESS=./compress
BTREE=./btree
TMP=/tmp

# Mesma funcao de geracao de chaves do test_btree.sh (com cache)
gen_keys() {
    local n=$1
    local cache="${TMP}/btree_keys_${n}.txt"
    if [ ! -f "$cache" ]; then
        if [ "$n" -le 10000 ]; then
            python3 - "$n" <<'EOF' > "$cache"
import random, string, sys
random.seed(42)
n = int(sys.argv[1])
keys = set()
while len(keys) < n:
    k = ''.join(random.choices(string.ascii_lowercase + string.digits, k=random.randint(4, 12)))
    keys.add(k)
print('\n'.join(list(keys)[:n]))
EOF
        else
            python3 - "$n" <<'EOF' > "$cache"
import random, sys
random.seed(42)
n = int(sys.argv[1])
keys = [f'k{i:09d}' for i in range(n)]
random.shuffle(keys)
print('\n'.join(keys))
EOF
        fi
    fi
    cat "$cache"
}

# Gera o arquivo de arvore se ainda nao existir
make_tree() {
    local T=$1 N=$2
    local out="${TMP}/tree_T${T}_N${N}.bin"
    if [ ! -f "$out" ]; then
        echo "Gerando arvore T=${T} N=${N}..."
        {
            gen_keys "$N" | sed 's/^/insert /'
            echo "save $out"
            echo "quit"
        } | "$BTREE" "$T" > /dev/null
        echo "  -> $(du -sh "$out" | cut -f1) — $out"
    fi
    echo "$out"
}

compress_test() {
    local src=$1
    local base
    base=$(basename "$src" .bin)

    echo "--------------------------------------"
    echo "Arquivo : $src"
    echo "Tamanho : $(du -sh "$src" | cut -f1) ($(stat -c%s "$src") bytes)"
    echo ""

    for algo in lzw huffman; do
        local comp="${TMP}/${base}.${algo}"
        local decomp="${TMP}/${base}.${algo}.dec"

        echo "--- $algo ---"
        "$COMPRESS" "$algo" c "$src"   "$comp"
        "$COMPRESS" "$algo" d "$comp"  "$decomp"

        if cmp -s "$src" "$decomp"; then
            echo "Integridade: OK (arquivos identicos)"
        else
            echo "ERRO: arquivo descomprimido difere do original!"
        fi
        echo ""
    done
}

echo "=== Teste de Compressao (LZW e Huffman) ==="
echo ""

for T in 3 50; do
    for N in 100 10000 100000 1000000; do
        src=$(make_tree "$T" "$N")
        compress_test "$src"
    done
done

echo "=== Testes de compressao concluidos ==="
