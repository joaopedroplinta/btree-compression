#!/usr/bin/env bash
# Testa a arvore B com T=3 e T=50 em quatro tamanhos:
#   100, 10 000, 100 000, 1 000 000 chaves
#
# Nota: "bilhoes de chaves" exigiria >1 TB de RAM para strings em memoria
# principal; 1 000 000 e o maior caso pratico viavel em hardware convencional.

set -e
BTREE=./btree
TMP=/tmp

# Gera (ou recupera do cache) N chaves unicas embaralhadas.
# N <= 10000: strings aleatorias de comprimento variavel
# N >  10000: chaves sequenciais embaralhadas (evita overhead de deduplicacao)
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

run_test() {
    local T=$1
    local N=$2
    local savefile="${TMP}/tree_T${T}_N${N}.bin"
    local logfile="${TMP}/tree_T${T}_N${N}.log"

    echo "============================================"
    echo "T=${T}  N=${N} chaves"
    echo "============================================"

    {
        # Insercoes (sed e muito mais rapido que um while-loop bash para N grande)
        gen_keys "$N" | sed 's/^/insert /'

        # 20 buscas com chave existente + 1 inexistente
        gen_keys "$N" | head -20 | sed 's/^/read /'
        echo "read chave_inexistente_xyz"

        # 10 remocoes
        gen_keys "$N" | tail -10 | sed 's/^/delete /'

        echo "stats"
        echo "save $savefile"
        echo "quit"
    } | "$BTREE" "$T" > "$logfile" 2>&1

    # Para N grande, exibir todas as linhas inunda o terminal; mostra apenas o final
    local total
    total=$(wc -l < "$logfile")
    local shown=50
    if [ "$total" -gt "$shown" ]; then
        echo "[... ${total} linhas de saida; exibindo as ultimas ${shown} ...]"
        tail -"$shown" "$logfile"
    else
        cat "$logfile"
    fi

    echo ""
    echo "Arquivo salvo : $(du -sh "$savefile" 2>/dev/null | cut -f1) — $savefile"
    echo "Log completo  : $logfile"
}

echo "=== Teste da Arvore B ==="
echo ""

for T in 3 50; do
    for N in 100 10000 100000 1000000; do
        run_test "$T" "$N"
        echo ""
    done
done

echo "=== Testes concluidos ==="
echo ""
echo "Escala maxima testada : 1 000 000 chaves."
echo "Para bilhoes de chaves seria necessario armazenamento em disco (arvore B"
echo "persistente), o que ultrapassa o escopo deste trabalho (arvore em RAM)."
