#!/usr/bin/env bash
#
# tests/check_parser.sh - confere o resultado do analisador completo
# (protoparser: lexer + parser) com o resultado esperado e mostra no
# terminal quantos casos ficaram como esperado.
#
# Uso:  bash tests/check_parser.sh [caminho-do-protoparser] [diretorio-de-casos]
#       (padrao: ./protoparser  examples/sintaxe)
#
# Expectativas: <diretorio>/esperado.txt, uma linha por caso.
# '#' inicia comentario. Formato:
#
#   NOME  ok                # programa deve ser aceito (exit code 0)
#   NOME  erro              # programa deve ser rejeitado (exit code 1)
#   NOME  erro : TEXTO      # rejeitado E a saida (stdout+stderr) contem TEXTO
#
# NOME e o nome do arquivo sem .java. Exigir TEXTO evita que um caso passe
# "por acidente" (ex.: rejeitado por erro de sintaxe quando se esperava um
# erro semantico). Linhas so com o NOME (sem status) contam como pendentes.
#
# Codigo de saida: 0 se todos passaram, 1 se algum divergiu, 2 se faltou
# o protoparser ou o arquivo de expectativas.

PARSER=${1:-./protoparser}
DIR=${2:-examples/sintaxe}
MANIFEST="$DIR/esperado.txt"

if [ -t 1 ]; then
    G=$'\033[32m'; R=$'\033[31m'; Y=$'\033[33m'; B=$'\033[1m'; Z=$'\033[0m'
else
    G=; R=; Y=; B=; Z=
fi

[ -x "$PARSER" ] || { echo "protoparser nao encontrado: $PARSER (rode 'make protoparser')" >&2; exit 2; }
[ -f "$MANIFEST" ] || { echo "arquivo de expectativas nao encontrado: $MANIFEST" >&2; exit 2; }

total=0
pass=0
failed=()
declare -A listed

trim() { sed 's/^[[:space:]]*//; s/[[:space:]]*$//'; }

echo "${B}Conferindo o parser contra $MANIFEST${Z}"
echo

while IFS= read -r line || [ -n "$line" ]; do
    line=${line%%#*}
    [[ "$line" =~ ^[[:space:]]*$ ]] && continue

    head=${line%%:*}
    exp_text=""
    [[ "$line" == *:* ]] && exp_text=$(printf '%s' "${line#*:}" | trim)
    read -r name exp_status <<< "$head"

    listed[$name]=1
    [ -n "$exp_status" ] || continue           # pendente: vira [AVISO] abaixo

    file="$DIR/$name.java"
    total=$((total + 1))

    if [ ! -f "$file" ]; then
        printf "  %s[FALHA]%s %-5s arquivo %s nao existe\n" "$R" "$Z" "$name" "$file"
        failed+=("$name")
        continue
    fi

    out=$("$PARSER" "$file" 2>&1)
    rc=$?
    if   [ "$rc" -ge 2 ]; then got_status=falha-de-execucao
    elif [ "$rc" -eq 1 ]; then got_status=erro
    else                       got_status=ok
    fi

    first_err=$(printf '%s\n' "$out" | grep -m1 '^ERRO')

    text_ok=1
    if [ -n "$exp_text" ] && ! printf '%s\n' "$out" | grep -qF -- "$exp_text"; then
        text_ok=0
    fi

    if [ "$got_status" = "$exp_status" ] && [ "$text_ok" -eq 1 ]; then
        pass=$((pass + 1))
        if [ "$got_status" = ok ]; then
            printf "  %s[ OK ]%s  %-5s aceito\n" "$G" "$Z" "$name"
        else
            printf "  %s[ OK ]%s  %-5s rejeitado: %s\n" "$G" "$Z" "$name" "$first_err"
        fi
    else
        failed+=("$name")
        printf "  %s[FALHA]%s %-5s\n" "$R" "$Z" "$name"
        printf "           esperado: %s%s\n" "$exp_status" "${exp_text:+ (saida contendo \"$exp_text\")}"
        printf "           obtido:   %s\n" "$got_status"
        if [ -n "$first_err" ]; then
            printf "           erro:     %s\n" "$first_err"
        fi
    fi
done < "$MANIFEST"

# Arquivos .java sem expectativa (ou pendentes): nao entram na conta, mas avisam.
for f in "$DIR"/*.java; do
    [ -e "$f" ] || continue
    n=$(basename "$f" .java)
    if [ -z "${listed[$n]}" ]; then
        printf "  %s[AVISO]%s %-5s sem expectativa em esperado.txt (ignorado)\n" "$Y" "$Z" "$n"
    elif ! grep -qE "^$n[[:space:]]+(ok|erro)" "$MANIFEST"; then
        printf "  %s[AVISO]%s %-5s expectativa pendente em esperado.txt (ignorado)\n" "$Y" "$Z" "$n"
    fi
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
