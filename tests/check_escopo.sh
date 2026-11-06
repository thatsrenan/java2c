#!/usr/bin/env bash
#
# tests/check_escopo.sh - confere o parser + tabela de simbolos contra
# examples/escopo/esperado.txt.
#
# Uso:  bash tests/check_escopo.sh [caminho-do-protoparser] [diretorio-de-casos]
#       (padrao: ./protoparser  examples/escopo)
#
# Formato de esperado.txt (o mesmo ja usado pelo projeto):
#
#   NOME  ok                        # programa deve ser ACEITO (exit code 0)
#   NOME  erro : ERRO SEMANTICO     # deve falhar com "ERRO SEMANTICO" em stderr
#   NOME                            # sem expectativa: apenas aviso
#
# '#' inicia comentario. Um caso 'erro' passa se o programa sair com codigo
# diferente de zero, tiver pelo menos uma linha "ERRO SEMANTICO" e nao tiver
# erro de sintaxe (para nao passar "por acidente").
#
# Codigo de saida: 0 se todos passaram, 1 se algum divergiu, 2 se faltou o
# executavel, o manifesto ou algum caso listado.

PARSER=${1:-./protoparser}
DIR=${2:-examples/escopo}
MANIFEST="$DIR/esperado.txt"

if [ -t 1 ]; then
    G=$'\033[32m'; R=$'\033[31m'; Y=$'\033[33m'; B=$'\033[1m'; Z=$'\033[0m'
else
    G=; R=; Y=; B=; Z=
fi

[ -x "$PARSER" ] || { echo "protoparser nao encontrado: $PARSER (rode 'make')" >&2; exit 2; }
[ -f "$MANIFEST" ] || { echo "arquivo de expectativas nao encontrado: $MANIFEST" >&2; exit 2; }

total=0
pass=0
failed=()
declare -A listed

echo "${B}Conferindo parser + tabela de simbolos contra $MANIFEST${Z}"
echo

while IFS= read -r line || [ -n "$line" ]; do
    line=${line%%#*}
    [[ "$line" =~ ^[[:space:]]*$ ]] && continue

    read -r name status _rest <<< "$line"
    [ -z "$name" ] && continue
    status=${status%:}   # aceita tanto "erro" quanto "erro:"
    listed[$name]=1

    if [ -z "$status" ]; then
        printf "  %s[AVISO]%s %-5s sem expectativa definida (ignorado)\n" "$Y" "$Z" "$name"
        continue
    fi

    file="$DIR/$name.java"
    total=$((total + 1))

    if [ ! -f "$file" ]; then
        printf "  %s[FALHA]%s %-5s arquivo %s nao existe\n" "$R" "$Z" "$name" "$file"
        failed+=("$name")
        continue
    fi

    out=$("$PARSER" "$file" 2>&1)
    rc=$?
    has_sem=0
    has_syn=0
    printf '%s\n' "$out" | grep -q '^ERRO SEMANTICO' && has_sem=1
    printf '%s\n' "$out" | grep -q '^ERRO DE SINTAXE' && has_syn=1

    case "$status" in
    ok)
        if [ "$rc" -eq 0 ] && [ "$has_sem" -eq 0 ] && [ "$has_syn" -eq 0 ]; then
            pass=$((pass + 1))
            printf "  %s[ OK ]%s  %-5s aceito\n" "$G" "$Z" "$name"
        else
            failed+=("$name")
            printf "  %s[FALHA]%s %-5s esperado aceito, obtido exit=%d semantico=%d sintatico=%d\n" \
                "$R" "$Z" "$name" "$rc" "$has_sem" "$has_syn"
            printf '%s\n' "$out" | sed -n '1,3p' | sed 's/^/           /'
        fi
        ;;
    erro)
        if [ "$rc" -ne 0 ] && [ "$has_sem" -eq 1 ] && [ "$has_syn" -eq 0 ]; then
            pass=$((pass + 1))
            msg=$(printf '%s\n' "$out" | grep '^ERRO SEMANTICO' | head -1)
            printf "  %s[ OK ]%s  %-5s %s\n" "$G" "$Z" "$name" "$msg"
        else
            failed+=("$name")
            printf "  %s[FALHA]%s %-5s esperado ERRO SEMANTICO, obtido exit=%d semantico=%d sintatico=%d\n" \
                "$R" "$Z" "$name" "$rc" "$has_sem" "$has_syn"
            printf '%s\n' "$out" | sed -n '1,3p' | sed 's/^/           /'
        fi
        ;;
    *)
        printf "  %s[AVISO]%s %-5s expectativa desconhecida: %s (ignorado)\n" \
            "$Y" "$Z" "$name" "$status"
        ;;
    esac
done < "$MANIFEST"

# Arquivos .java sem expectativa: nao entram na conta, mas avisam.
for f in "$DIR"/*.java; do
    [ -e "$f" ] || continue
    n=$(basename "$f" .java)
    [ -n "${listed[$n]}" ] || printf "  %s[AVISO]%s %-5s sem expectativa em esperado.txt (ignorado)\n" "$Y" "$Z" "$n"
done

echo
if [ "$total" -eq 0 ]; then
    echo "Nenhum caso com expectativa em $MANIFEST."
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
