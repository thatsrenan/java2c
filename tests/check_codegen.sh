#!/usr/bin/env bash
#
# tests/check_codegen.sh - conferência ponta a ponta da geração de C.
#
# Para cada NOME.java em examples/codegen e cada modo (bruto e limpo):
#   1. 'protoparser --c' ou '--c-clean' gera NOME.c / NOME_clean.c;
#   2. gcc (C11, -Wall -Wextra -Werror) compila sem warnings/erros;
#   3. o binário é executado e stdout é comparado com NOME.out — ou com
#      NOME.clean.out no modo limpo, quando a formatação difere (ex.: `%g`
#      imprime 2 onde o helper Java imprime 2.0);
#   4. o status de saída é comparado com NOME.exit (padrão: 0).
#
# Uso: bash tests/check_codegen.sh [protoparser] [diretorio-de-casos]
# Saída: 0 se todos passaram, 1 se algum falhou, 2 se faltou algo.

PARSER=${1:-./protoparser}
DIR=${2:-examples/codegen}
BUILD=${BUILD_DIR:-build/codegen}
CC=${CC:-gcc}
CFLAGS=${CFLAGS:--std=c11 -Wall -Wextra -Werror -pedantic -fwrapv}

if [ -t 1 ]; then
    G=$'\033[32m'; R=$'\033[31m'; Y=$'\033[33m'; B=$'\033[1m'; Z=$'\033[0m'
else
    G=; R=; Y=; B=; Z=
fi

[ -x "$PARSER" ] || { echo "protoparser nao encontrado: $PARSER" >&2; exit 2; }
command -v "$CC" > /dev/null 2>&1 || { echo "compilador C nao encontrado: $CC" >&2; exit 2; }
mkdir -p "$BUILD" || exit 2

got=$(mktemp)
errf=$(mktemp)
trap 'rm -f "$got" "$errf"' EXIT

total=0
pass=0
failed=()

echo "${B}Conferindo geracao de C em $DIR/*.java (modos --c e --c-clean)${Z}"
echo

for f in "$DIR"/*.java; do
    [ -e "$f" ] || continue
    name=$(basename "$f" .java)
    expected="$DIR/$name.out"
    if [ ! -f "$expected" ]; then
        printf "  %s[AVISO]%s %-5s sem %s (ignorado)\n" "$Y" "$Z" "$name" "$expected"
        continue
    fi

    # O modo limpo usa printf("%g") para reais, então números inteiros exatos
    # não recebem ".0". Cada caso pode ter um arquivo de saída próprio.
    clean_expected="$DIR/$name.clean.out"

    wanted_exit=0
    if [ -f "$DIR/$name.exit" ]; then
        wanted_exit=$(tr -d '[:space:]' < "$DIR/$name.exit")
    fi

    for mode in raw clean; do
        total=$((total + 1))
        case "$mode" in
            raw)   flag=--c;        base=$name ;;
            clean) flag=--c-clean;  base=${name}_clean ;;
        esac
        cfile="$BUILD/$base.c"
        binary="$BUILD/$base"
        id="$name/$mode"
        case "$mode" in
            clean) [ -f "$clean_expected" ] && expected="$clean_expected" ;;
            raw)   expected="$DIR/$name.out" ;;
        esac

        "$PARSER" "$flag" -o "$cfile" "$f" > "$got" 2> "$errf"
        rc=$?
        if [ "$rc" -ne 0 ]; then
            failed+=("$id (geracao)")
            printf "  %s[FALHA]%s %-12s geracao de C (exit %s)\n" "$R" "$Z" "$id" "$rc"
            sed 's/^/           /' "$errf" | head -10
            continue
        fi

        # -Werror transforma qualquer warning em falha de compilacao; -fwrapv
        # reproduz o wrap-around de int do Java em operadores C com sinal.
        $CC $CFLAGS "$cfile" -o "$binary" > "$got" 2> "$errf"
        rc=$?
        if [ "$rc" -ne 0 ] || [ -s "$errf" ]; then
            failed+=("$id (compilacao)")
            printf "  %s[FALHA]%s %-12s compilacao do C (exit %s)\n" "$R" "$Z" "$id" "$rc"
            sed 's/^/           /' "$errf" | head -20
            continue
        fi

        "$binary" > "$got" 2> "$errf"
        rc=$?
        if [ "$rc" -ne "$wanted_exit" ]; then
            failed+=("$id (exit)")
            printf "  %s[FALHA]%s %-12s esperava exit %s, obteve %s\n" \
                "$R" "$Z" "$id" "$wanted_exit" "$rc"
            sed 's/^/           /' "$errf" | head -10
            continue
        fi

        if ! diff -u --label esperado --label obtido "$expected" "$got" > "$errf"; then
            failed+=("$id (saida)")
            printf "  %s[FALHA]%s %-12s saida divergente\n" "$R" "$Z" "$id"
            sed 's/^/           /' "$errf" | head -30
            continue
        fi

        pass=$((pass + 1))
        printf "  %s[ OK ]%s  %-12s %s\n" "$G" "$Z" "$id" \
            "$(wc -c < "$got" | tr -d ' ') bytes de stdout"
    done
done

echo
if [ "$total" -eq 0 ]; then
    echo "Nenhum caso com .out em $DIR."
    exit 2
fi
if [ "${#failed[@]}" -eq 0 ]; then
    echo "${G}${B}Resultado: $pass/$pass combinacoes geraram C valido e produziram a saida esperada.${Z}"
    exit 0
fi
echo "${R}${B}Resultado: $pass/$pass combinacoes OK.${Z}"
echo "${R}Falhas: ${failed[*]}${Z}"
exit 1
