#!/usr/bin/env bash
# Comprime e descomprime cada arquivo de arvore salvo usando LZW e Huffman
# Pressupoe que test_btree.sh ja foi executado (gera os .bin em /tmp)
# Uso: ./test_compress.sh

set -e
COMPRESS=./compress
BTREE=./btree
TMP=/tmp

# Gera um arquivo de arvore se ainda nao existir
make_tree() {
    local T=$1 N=$2
    local out="${TMP}/tree_T${T}_N${N}.bin"
    if [ ! -f "$out" ]; then
        echo "Gerando arvore T=${T} N=${N}..."
        python3 -c "
import random, string, sys
random.seed(42)
n = int(sys.argv[1])
keys = set()
while len(keys) < n:
    k = ''.join(random.choices(string.ascii_lowercase + string.digits, k=random.randint(4,12)))
    keys.add(k)
for k in list(keys)[:n]:
    print('insert', k)
print('save $out')
print('quit')
" "$N" | "$BTREE" "$T" > /dev/null
    fi
    echo "$out"
}

compress_test() {
    local src=$1
    local base=$(basename "$src" .bin)

    echo "--------------------------------------"
    echo "Arquivo: $src"
    echo "Tamanho original: $(du -sh "$src" | cut -f1) ($(stat -c%s "$src") bytes)"
    echo ""

    for algo in lzw huffman; do
        local comp="${TMP}/${base}.${algo}"
        local decomp="${TMP}/${base}.${algo}.dec"

        echo "--- $algo ---"
        "$COMPRESS" "$algo" c "$src"    "$comp"
        "$COMPRESS" "$algo" d "$comp"   "$decomp"

        # Verifica integridade
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
    for N in 50 1000 10000; do
        src=$(make_tree $T $N)
        compress_test "$src"
    done
done

echo "=== Testes de compressao concluidos ==="
