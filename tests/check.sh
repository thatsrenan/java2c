#!/usr/bin/env bash
#
# tests/check.sh - confere a saida do analisador lexico (lextest) com o
# resultado esperado e mostra no terminal quantos casos ficaram como
# esperado.
#
# Uso:  bash tests/check.sh [caminho-do-lextest] [diretorio-de-casos]
#       (padrao: ./lextest  examples/lexico)
#
# Expectativas: <diretorio>/esperado.txt, uma linha por caso.
# '#' inicia comentario. Formato:
#
#   NOME  ok         : TOKEN TOKEN ...     # nenhum erro lexico
#   NOME  erro N     : TOKEN TOKEN ...     # exatamente N erros lexicos
#
# NOME e o nome do arquivo sem .java. Depois dos ':' vem a sequencia
# EXATA de tokens que o lexer deve devolver (os descartados por erro
# nao aparecem). Um caso passa se o status, o numero de erros lexicos e
# a sequencia de tokens forem todos iguais ao esperado.
#
# Codigo de saida: 0 se todos passaram, 1 se algum divergiu, 2 se faltou
# o lextest ou o arquivo de expectativas.

LEXTEST=${1:-./lextest}
DIR=${2:-examples/lexico}
MANIFEST="$DIR/esperado.txt"

if [ -t 1 ]; then
    G=$'\033[32m'; R=$'\033[31m'; Y=$'\033[33m'; B=$'\033[1m'; Z=$'\033[0m'
else
    G=; R=; Y=; B=; Z=
fi

[ -x "$LEXTEST" ] || { echo "lextest nao encontrado: $LEXTEST (rode 'make lextest')" >&2; exit 2; }
[ -f "$MANIFEST" ] || { echo "arquivo de expectativas nao encontrado: $MANIFEST" >&2; exit 2; }

errf=$(mktemp)
trap 'rm -f "$errf"' EXIT

total=0
pass=0
failed=()
declare -A listed

norm() { tr -s ' \t' ' ' | sed 's/^ //; s/ $//'; }

echo "${B}Conferindo o lexer contra $MANIFEST${Z}"
echo

while IFS= read -r line || [ -n "$line" ]; do
    line=${line%%#*}
    [[ "$line" =~ ^[[:space:]]*$ ]] && continue

    head=${line%%:*}
    exp_tokens=""
    [[ "$line" == *:* ]] && exp_tokens=$(printf '%s' "${line#*:}" | norm)
    read -r name exp_status exp_nerr <<< "$head"
    [ "$exp_status" = ok ] && exp_nerr=0
    exp_nerr=${exp_nerr:-0}

    listed[$name]=1
    file="$DIR/$name.java"
    total=$((total + 1))

    if [ ! -f "$file" ]; then
        printf "  %s[FALHA]%s %-5s arquivo %s nao existe\n" "$R" "$Z" "$name" "$file"
        failed+=("$name")
        continue
    fi

    out=$("$LEXTEST" "$file" 2>"$errf")
    rc=$?
    got_tokens=$(printf '%s\n' "$out" | awk 'NF { print $2 }' | tr '\n' ' ' | norm)
    got_nerr=$(grep -c '^ERRO LEXICO' "$errf")
    if   [ "$rc" -ge 2 ]; then got_status=falha-de-execucao
    elif [ "$rc" -eq 1 ]; then got_status=erro
    else                       got_status=ok
    fi

    ntok=0
    [ -n "$got_tokens" ] && ntok=$(printf '%s\n' "$got_tokens" | wc -w)

    if [ "$got_status" = "$exp_status" ] && [ "$got_nerr" = "$exp_nerr" ] \
       && [ "$got_tokens" = "$exp_tokens" ]; then
        pass=$((pass + 1))
        if [ "$got_nerr" -eq 0 ]; then
            printf "  %s[ OK ]%s  %-5s sem erros lexicos, %s tokens\n" "$G" "$Z" "$name" "$ntok"
        else
            printf "  %s[ OK ]%s  %-5s %s erro(s) lexico(s) detectado(s), %s tokens\n" "$G" "$Z" "$name" "$got_nerr" "$ntok"
        fi
    else
        failed+=("$name")
        printf "  %s[FALHA]%s %-5s\n" "$R" "$Z" "$name"
        printf "           esperado: %s, %s erro(s) lexico(s)\n" "$exp_status" "$exp_nerr"
        printf "                     tokens: %s\n" "${exp_tokens:-(nenhum)}"
        printf "           obtido:   %s, %s erro(s) lexico(s)\n" "$got_status" "$got_nerr"
        printf "                     tokens: %s\n" "${got_tokens:-(nenhum)}"
        if [ -s "$errf" ]; then
            sed 's/^/           stderr:   /' "$errf" | head -3
        fi
    fi
done < "$MANIFEST"

# Arquivos .java sem expectativa: nao entram na conta, mas avisam.
for f in "$DIR"/*.java; do
    [ -e "$f" ] || continue
    n=$(basename "$f" .java)
    [ -n "${listed[$n]}" ] || printf "  %s[AVISO]%s %-5s sem expectativa em esperado.txt (ignorado)\n" "$Y" "$Z" "$n"
done

echo
if [ "$total" -eq 0 ]; then
    echo "Nenhum caso listado em $MANIFEST."
    exit 2
fi
pct=$((100 * pass / total))
if [ "${#failed[@]}" -eq 0 ]; then
    echo "${G}${B}Resultado: $pass/$total casos como esperado ($pct%).${Z}"
    exit 0
else
    echo "${R}${B}Resultado: $pass/$total casos como esperado ($pct%).${Z}"
    echo "${R}Divergentes: ${failed[*]}${Z}"
    exit 1
fi
