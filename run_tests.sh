TOJS="${1:-./tojs}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

PASS=0
FAIL=0
HAS_NODE=1
command -v node >/dev/null 2>&1 || HAS_NODE=0

if [ ! -x "$TOJS" ]; then
    echo "Executavel '$TOJS' nao encontrado. Rode 'make' primeiro."
    exit 2
fi

ok()   { PASS=$((PASS + 1)); printf '  [ OK ] %s\n' "$1"; }
fail() { FAIL=$((FAIL + 1)); printf '  [FAIL] %s\n' "$1"; [ -n "$2" ] && printf '%s\n' "$2" | sed 's/^/         /'; }

expected_output() {
    local f="$1" side="${1%.tojs}.saida"
    if [ -f "$side" ]; then
        tr -d '\r' < "$side"
    else
        tr -d '\r' < "$f" | sed -n 's/^# SAIDA: \{0,1\}//p'
    fi
}

run_valid_dir() {
    local dir="$1" f name js got want
    echo "== $dir =="
    for f in "$dir"/*.tojs; do
        [ -e "$f" ] || continue
        name="$(basename "$f")"
        js="$TMP/${name%.tojs}.js"

        if ! err="$("$TOJS" "$f" "$js" 2>&1 >/dev/null)"; then
            fail "$name (traducao falhou)" "$err"
            continue
        fi
        if [ "$HAS_NODE" -eq 0 ]; then
            ok "$name (traduzido; node ausente, execucao nao verificada)"
            continue
        fi

        got="$(node "$js" 2>&1 | tr -d '\r')"
        want="$(expected_output "$f")"
        if [ "$got" == "$want" ]; then
            ok "$name"
        else
            fail "$name (saida diferente)" "esperado: $(echo "$want" | tr '\n' '|')
obtido:   $(echo "$got" | tr '\n' '|')"
        fi
    done
}

run_invalid_dir() {
    local dir="$1" f name js want err rc
    echo "== $dir =="
    for f in "$dir"/*.tojs; do
        [ -e "$f" ] || continue
        name="$(basename "$f")"
        js="$TMP/${name%.tojs}.js"
        want="$(tr -d '\r' < "$f" | sed -n 's/^# ESPERA: \{0,1\}//p' | head -n 1)"

        err="$("$TOJS" "$f" "$js" 2>&1 >/dev/null)"
        rc=$?

        if [ "$rc" -eq 0 ]; then
            fail "$name (deveria falhar, mas foi aceito)"
        elif [ -e "$js" ]; then
            fail "$name (falhou, mas gerou JS mesmo assim)"
        elif [[ "$err" != *"$want"* ]]; then
            fail "$name (mensagem inesperada)" "esperado conter: $want
obtido: $(echo "$err" | head -n 1)"
        else
            ok "$name"
        fi
    done
}

run_valid_dir   exemplos
run_valid_dir   tests/validos
run_invalid_dir tests/invalidos

echo
echo "Resultado: $PASS passaram, $FAIL falharam."
[ "$FAIL" -eq 0 ]
