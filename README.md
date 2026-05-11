# Trabalho 2 — Estruturas de Dados Avançadas

**Integrantes:** <!-- coloque seu nome completo aqui -->  
**Curso:** Bacharelado em Ciência da Computação — IFPR Campus Pinhais  
**Disciplina:** Estruturas de Dados Avançadas  
**Entrega:** 10/junho/2026

---

## Descrição dos arquivos

| Arquivo | Descrição |
|---|---|
| `btree.cpp` | Programa principal da Árvore B com chaves string |
| `compress.cpp` | Programa de compressão/descompressão (LZW e Huffman) |
| `Makefile` | Compilação dos dois programas |
| `test_btree.sh` | Script de testes de desempenho da Árvore B |
| `test_compress.sh` | Script de testes dos algoritmos de compressão |

---

## Compilação

```bash
make
```

Gera os executáveis `btree` e `compress`. Requer g++ com suporte a C++17.

```bash
make clean   # remove binários e arquivos temporários
```

---

## Programa 1 — Árvore B (`btree`)

### Uso

```bash
./btree <grau_minimo_T>
```

O parâmetro T define o tamanho dos nós:

| T | Mín. chaves/nó | Máx. chaves/nó |
|---|---|---|
| 3 | 2 | 5 |
| 50 | 49 | 99 |

### Comandos interativos

```
insert <chave>     insere uma nova chave na árvore
read   <chave>     busca e exibe a chave
delete <chave>     remove a chave (se existir)
save   <arquivo>   salva a árvore em arquivo binário
load   <arquivo>   carrega a árvore a partir de um arquivo
print              exibe a estrutura hierárquica da árvore no terminal
list               lista todas as chaves em ordem crescente
quit               encerra o programa
```

### Métricas exibidas após cada operação

Após cada `insert`, `read` ou `delete`, o programa imprime:

```
[nos visitados: N | tempo: X s | memoria: Y KB]
```

### Exemplos

```bash
# Árvore pequena (T=3)
./btree 3
> insert banana
> insert abacaxi
> insert manga
> read banana
> delete abacaxi
> save arvore.bin
> quit

# Árvore grande (T=50)
./btree 50
> insert banana
> save arvore50.bin
> quit

# Carregar árvore salva
./btree 3
> load arvore.bin
> read banana
> quit
```

### Formato do arquivo salvo

Binário em pré-ordem:
- `int` — grau mínimo T
- Para cada nó: `int leaf` + `int n` + n × (`int len` + `len bytes` da chave)
- Filhos recursivamente (se não for folha)

---

## Programa 2 — Compressão (`compress`)

### Uso

```bash
./compress <algoritmo> <modo> <entrada> <saida>
```

| Parâmetro | Valores |
|---|---|
| algoritmo | `lzw` ou `huffman` |
| modo | `c` (comprimir) ou `d` (descomprimir) |

### Exemplos

```bash
# Comprimir
./compress lzw     c arvore.bin arvore.lzw
./compress huffman c arvore.bin arvore.huf

# Descomprimir
./compress lzw     d arvore.lzw arvore_dec.bin
./compress huffman d arvore.huf arvore_dec.bin
```

### Saída de métricas

```
Algoritmo    : lzw (compressao)
Entrada      : 67497 bytes
Saida        : 40972 bytes
Taxa compres.: 1.64x  (39.3% menor)
Tempo        : 0.006 s
Memoria      : 4804 KB
```

### Detalhes dos algoritmos

**LZW**
- Dicionário com até 65 536 entradas, códigos de 16 bits (little-endian)
- Ao atingir o limite o dicionário é congelado (sem reset)
- Cabeçalho: magic `LZW\0` + tamanho original (8 B) + número de códigos (8 B)

**Huffman Canônico**
- Frequências contadas byte a byte; árvore construída com fila de prioridade
- Códigos canônicos gerados a partir dos comprimentos → decompressor determinístico
- Cabeçalho: magic `HUF\0` + tamanho original (8 B) + padding (1 B) + 256 comprimentos (1 B cada)
- Bits escritos MSB-primeiro dentro de cada byte

---

## Scripts de teste

### Teste da Árvore B

```bash
./test_btree.sh
```

Executa inserções, buscas e remoções com **T=3** e **T=50** para 50, 1 000 e 10 000 chaves. Imprime as métricas de cada operação.

### Teste de compressão

```bash
./test_compress.sh
```

Para cada combinação de T e tamanho, comprime e descomprime o arquivo da árvore com ambos os algoritmos e verifica a integridade (`cmp`).

### Teste manual rápido

```bash
# Gera 1000 insercoes, salva e comprime
python3 -c "
import random, string
random.seed(1)
keys = set()
while len(keys) < 1000:
    k = ''.join(random.choices(string.ascii_lowercase, k=8))
    keys.add(k)
for k in keys: print('insert', k)
print('save /tmp/arvore.bin')
print('quit')
" | ./btree 3

./compress lzw     c /tmp/arvore.bin /tmp/arvore.lzw
./compress huffman c /tmp/arvore.bin /tmp/arvore.huf
```

---

## Observações

- Chaves duplicadas são rejeitadas com aviso.
- O arquivo salvo inclui o grau T; ao carregar um arquivo com T diferente do atual, o programa ajusta T automaticamente.
- Para árvores muito grandes (milhões de chaves), recomenda-se usar T=50 para reduzir a profundidade e o número de nós visitados por operação.
