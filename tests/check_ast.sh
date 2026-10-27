#!/usr/bin/env bash
#
# tests/check_ast.sh - confere a AST montada pelo parser com a AST esperada.
#
# Uso:  bash tests/check_ast.sh [caminho-do-protoparser] [diretorio-de-casos]
#       (padrao: ./protoparser  examples/ast)
#
# Para cada NOME.java do diretorio deve existir NOME.ast com a saida
# esperada de "protoparser --ast NOME.java" (a arvore de print_ast com a
# linha de cada no). Um caso passa se o programa e aceito e a saida e
# identica ao .ast; se divergir, mostra o diff.
#
# Para criar/atualizar um .ast depois de conferir a arvore a olho:
#     ./protoparser --ast examples/ast/NOME.java > examples/ast/NOME.ast
#
# Codigo de saida: 0 se todos passaram, 1 se algum divergiu, 2 se faltou
# o protoparser ou nao ha casos.

PARSER=${1:-./protoparser}
DIR=${2:-examples/ast}

if [ -t 1 ]; then
    G=$'\033[32m'; R=$'\033[31m'; Y=$'\033[33m'; B=$'\033[1m'; Z=$'\033[0m'
else
    G=; R=; Y=; B=; Z=
fi

[ -x "$PARSER" ] || { echo "protoparser nao encontrado: $PARSER (rode 'make protoparser')" >&2; exit 2; }

got=$(mktemp)
trap 'rm -f "$got"' EXIT

total=0
pass=0
failed=()

echo "${B}Conferindo a AST contra $DIR/*.ast${Z}"
echo

for f in "$DIR"/*.java; do
    [ -e "$f" ] || continue
    name=$(basename "$f" .java)
    exp="$DIR/$name.ast"
    if [ ! -f "$exp" ]; then
        printf "  %s[AVISO]%s %-5s sem %s (ignorado)\n" "$Y" "$Z" "$name" "$exp"
        continue
    fi
    total=$((total + 1))

    "$PARSER" --ast "$f" > "$got" 2>&1
    rc=$?

    if [ "$rc" -eq 0 ] && diff -q "$exp" "$got" > /dev/null; then
        pass=$((pass + 1))
        printf "  %s[ OK ]%s  %-5s %s linhas de AST\n" "$G" "$Z" "$name" "$(wc -l < "$got" | tr -d ' ')"
    else
        failed+=("$name")
        printf "  %s[FALHA]%s %-5s" "$R" "$Z" "$name"
        [ "$rc" -ne 0 ] && printf " (programa rejeitado, exit code %s)" "$rc"
        printf "\n"
        diff -u --label esperado --label obtido "$exp" "$got" | sed 's/^/           /' | head -40
    fi
done

echo
if [ "$total" -eq 0 ]; then
    echo "Nenhum caso com .ast em $DIR."
    exit 2
fi
pct=$((100 * pass / total))
if [ "${#failed[@]}" -eq 0 ]; then
    echo "${G}${B}Resultado: $pass/$total ASTs como esperado ($pct%).${Z}"
    exit 0
else
    echo "${R}${B}Resultado: $pass/$total ASTs como esperado ($pct%).${Z}"
    echo "${R}Divergentes: ${failed[*]}${Z}"
    exit 1
fi
