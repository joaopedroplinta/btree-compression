#!/usr/bin/env bash
# Testa a arvore B com dois graus (T=3 e T=50) em tres tamanhos
# Uso: ./test_btree.sh

set -e
BTREE=./btree

# Gera N chaves aleatorias distintas
gen_keys() {
    python3 -c "
import random, string, sys
random.seed(42)
n = int(sys.argv[1])
keys = set()
while len(keys) < n:
    k = ''.join(random.choices(string.ascii_lowercase + string.digits, k=random.randint(4,12)))
    keys.add(k)
for k in list(keys)[:n]:
    print(k)
" "$1"
}

run_test() {
    local T=$1
    local N=$2
    local savefile="/tmp/tree_T${T}_N${N}.bin"

    echo "============================================"
    echo "T=${T}  N=${N} chaves"
    echo "============================================"

    {
        # Insercoes
        gen_keys $N | while read -r k; do echo "insert $k"; done

        # Algumas buscas
        gen_keys $N | head -10 | while read -r k; do echo "read $k"; done
        echo "read chave_inexistente_xyz"

        # Algumas remocoes
        gen_keys $N | tail -5 | while read -r k; do echo "delete $k"; done

        echo "save $savefile"
        echo "quit"
    } | "$BTREE" "$T"

    echo ""
    echo "Arquivo salvo: $(du -sh "$savefile" 2>/dev/null | cut -f1) — $savefile"
}

echo "=== Teste da Arvore B ==="
echo ""

for T in 3 50; do
    for N in 50 1000 10000; do
        run_test $T $N
        echo ""
    done
done

echo "=== Testes concluidos ==="
