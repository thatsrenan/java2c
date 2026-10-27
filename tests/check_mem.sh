#!/usr/bin/env bash
#
# tests/check_mem.sh - roda o protoparser compilado com AddressSanitizer +
# UBSan sobre todos os exemplos (validos E invalidos) e acusa vazamento,
# double free ou acesso invalido na montagem/liberacao da AST.
#
# Os casos invalidos importam: neles o parser para no meio e quem libera as
# subarvores ja montadas sao os %destructor do parser.y.
#
# Uso:  bash tests/check_mem.sh [caminho-do-protoparser-san] [diretorio...]
#       (padrao: ./protoparser-san  examples/*/ e examples/*.java)
#
# Aceito ou rejeitado nao importa aqui (exit 0 ou 1); falha e so quando o
# sanitizer reclama. Codigo de saida: 0 se nenhum arquivo deu problema,
# 1 caso contrario, 2 se faltou o executavel.

PARSER=${1:-./protoparser-san}
shift
DIRS=("$@")
[ "${#DIRS[@]}" -eq 0 ] && DIRS=(examples examples/*/)

if [ -t 1 ]; then
    G=$'\033[32m'; R=$'\033[31m'; B=$'\033[1m'; Z=$'\033[0m'
else
    G=; R=; B=; Z=
fi

[ -x "$PARSER" ] || { echo "executavel nao encontrado: $PARSER (rode 'make protoparser-san')" >&2; exit 2; }

# exit code proprio do sanitizer, para nao confundir com "programa rejeitado"
export ASAN_OPTIONS=exitcode=99:detect_leaks=1
export UBSAN_OPTIONS=halt_on_error=1:exitcode=99:print_stacktrace=1

errf=$(mktemp)
trap 'rm -f "$errf"' EXIT

total=0
failed=()

echo "${B}Conferindo memoria da AST (sanitizers) em: ${DIRS[*]}${Z}"
echo

for d in "${DIRS[@]}"; do
    for f in "${d%/}"/*.java; do
        [ -e "$f" ] || continue
        total=$((total + 1))
        "$PARSER" "$f" > /dev/null 2> "$errf"
        rc=$?
        if [ "$rc" -gt 1 ] || grep -qE 'Sanitizer|runtime error' "$errf"; then
            failed+=("$f")
            printf "  %s[FALHA]%s %s (exit code %s)\n" "$R" "$Z" "$f" "$rc"
            grep -m3 -E 'ERROR|SUMMARY|runtime error' "$errf" | sed 's/^/           /'
        fi
    done
done

echo
if [ "$total" -eq 0 ]; then
    echo "Nenhum .java encontrado."
    exit 2
fi
if [ "${#failed[@]}" -eq 0 ]; then
    echo "${G}${B}Resultado: $total/$total arquivos sem erros de memoria (100%).${Z}"
    exit 0
else
    echo "${R}${B}Resultado: $((total - ${#failed[@]}))/$total arquivos sem erros de memoria.${Z}"
    exit 1
fi
