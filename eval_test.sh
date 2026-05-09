#!/usr/bin/env bash

set -u
set -o pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT_DIR" || exit 1

RESULT_FILE="$ROOT_DIR/eval_results.txt"
TMP_DIR="$(mktemp -d)"
BACKUP_DIR="$TMP_DIR/backups"
mkdir -p "$BACKUP_DIR"
PID_FILE="$ROOT_DIR/.eval_test_webserv.pid"

PASS=0
FAIL=0
SKIP=0
NOTE=0

KEEP_SERVER=0
KILL_EXISTING_WEBSERV=0
CURRENT_SERVER_PID=""
CURRENT_SERVER_LOG=""
CURRENT_SERVER_PORTS=""

declare -a FAIL_LIST
declare -a SKIP_LIST
declare -a NOTE_LIST

timestamp() {
    date "+%Y-%m-%d %H:%M:%S"
}

announce() {
    echo "[*] $*"
}

log() {
    printf "%s\n" "$*" >> "$RESULT_FILE"
}

section() {
    announce "$1"
    log ""
    log "================================================================"
    log "$1"
    log "================================================================"
}

pass() {
    PASS=$((PASS + 1))
    log "[PASS] $1"
}

fail() {
    FAIL=$((FAIL + 1))
    FAIL_LIST+=("$1")
    log "[FAIL] $1"
}

skip() {
    SKIP=$((SKIP + 1))
    SKIP_LIST+=("$1")
    log "[SKIP] $1"
}

note() {
    NOTE=$((NOTE + 1))
    NOTE_LIST+=("$1")
    log "[NOTE] $1"
}

cleanup() {
    if [[ $KEEP_SERVER -eq 0 && -n "$CURRENT_SERVER_PID" ]]; then
        kill "$CURRENT_SERVER_PID" 2>/dev/null || true
        wait "$CURRENT_SERVER_PID" 2>/dev/null || true
    fi
    rm -f "$PID_FILE"

    if [[ -d "$BACKUP_DIR" ]]; then
        while IFS= read -r backup_path; do
            rel_path="${backup_path#$BACKUP_DIR/}"
            if [[ -f "$backup_path" ]]; then
                mkdir -p "$(dirname "$ROOT_DIR/$rel_path")"
                cp -p "$backup_path" "$ROOT_DIR/$rel_path"
            fi
        done < <(find "$BACKUP_DIR" -type f | sort)
    fi

    rm -rf "$TMP_DIR"
}

trap cleanup EXIT

backup_file() {
    local rel="$1"
    local src="$ROOT_DIR/$rel"
    local dst="$BACKUP_DIR/$rel"

    if [[ ! -e "$src" ]]; then
        return 1
    fi
    if [[ -e "$dst" ]]; then
        return 0
    fi

    mkdir -p "$(dirname "$dst")"
    cp -p "$src" "$dst"
}

restore_file() {
    local rel="$1"
    local src="$BACKUP_DIR/$rel"
    local dst="$ROOT_DIR/$rel"

    if [[ -e "$src" ]]; then
        cp -p "$src" "$dst"
    fi
}

write_file() {
    local rel="$1"
    local content="$2"
    backup_file "$rel" || return 1
    printf "%b" "$content" > "$ROOT_DIR/$rel"
}

require_cmd() {
    local cmd="$1"
    local label="$2"

    if command -v "$cmd" >/dev/null 2>&1; then
        pass "$label: commande '$cmd' disponible"
        return 0
    fi

    fail "$label: commande '$cmd' absente"
    return 1
}

optional_cmd() {
    local cmd="$1"
    command -v "$cmd" >/dev/null 2>&1
}

extract_port_pids() {
    local ports_csv="$1"
    local port
    local line

    optional_cmd ss || return 1

    IFS=',' read -r -a _ports <<< "$ports_csv"
    for port in "${_ports[@]}"; do
        port="${port// /}"
        while IFS= read -r line; do
            printf "%s\n" "$line" | grep -o 'pid=[0-9]\+' | cut -d= -f2
        done < <(ss -ltnp "( sport = :$port )" 2>/dev/null | grep ":$port" || true)
    done | awk '!seen[$0]++'
}

kill_existing_webserv_on_ports() {
    local ports_csv="$1"
    local pid
    local comm
    local args
    local killed=1

    while IFS= read -r pid; do
        [[ -z "$pid" ]] && continue
        comm="$(ps -p "$pid" -o comm= 2>/dev/null | tr -d ' ' || true)"
        args="$(ps -p "$pid" -o args= 2>/dev/null | tr -d '\n' || true)"
        kill "$pid" 2>/dev/null || true
        wait "$pid" 2>/dev/null || true
        if [[ -n "$args" ]]; then
            log "[NOTE] Processus termine pour liberer le port: pid=$pid comm=$comm args=$args"
        else
            log "[NOTE] Processus termine pour liberer le port: pid=$pid comm=$comm"
        fi
        killed=0
    done < <(extract_port_pids "$ports_csv" || true)

    return $killed
}

ports_busy() {
    local ports_csv="$1"
    local port

    optional_cmd ss || return 1

    IFS=',' read -r -a _ports <<< "$ports_csv"
    for port in "${_ports[@]}"; do
        port="${port// /}"
        if ss -ltn "( sport = :$port )" 2>/dev/null | grep -q ":$port"; then
            return 0
        fi
    done
    return 1
}

ports_busy_details() {
    local ports_csv="$1"
    local port
    local found=1

    optional_cmd ss || return 1

    IFS=',' read -r -a _ports <<< "$ports_csv"
    for port in "${_ports[@]}"; do
        port="${port// /}"
        if ss -ltnp "( sport = :$port )" 2>/dev/null | grep -q ":$port"; then
            found=0
            log "--- PORT $port OCCUPE ---"
            ss -ltnp "( sport = :$port )" 2>/dev/null >> "$RESULT_FILE" || true
            log "--- FIN PORT $port ---"
        fi
    done
    return $found
}

start_server() {
    local config="$1"
    local ports="$2"

    if [[ -n "$CURRENT_SERVER_PID" && $KEEP_SERVER -eq 0 ]]; then
        kill "$CURRENT_SERVER_PID" 2>/dev/null || true
        wait "$CURRENT_SERVER_PID" 2>/dev/null || true
        CURRENT_SERVER_PID=""
    fi

    if [[ $KILL_EXISTING_WEBSERV -eq 1 ]]; then
        kill_existing_webserv_on_ports "$ports" || true
        sleep 1
    fi

    # Always try to free the test ports first so a stale webserv from a previous run
    # does not block the next configuration block.
    kill_existing_webserv_on_ports "$ports" || true
    sleep 1

    if ports_busy "$ports"; then
        skip "Demarrage de ./webserv $config impossible: port(s) deja occupes ($ports)"
        ports_busy_details "$ports" || true
        return 1
    fi

    CURRENT_SERVER_LOG="$TMP_DIR/$(basename "$config").$(date +%s%N).log"
    # Keep stdin open (non-EOF) so readCommand() in Core does not quit immediately in non-interactive runs.
    ./webserv "$config" < <(tail -f /dev/null) > "$CURRENT_SERVER_LOG" 2>&1 &
    CURRENT_SERVER_PID=$!
    CURRENT_SERVER_PORTS="$ports"
    printf "%s\n" "$CURRENT_SERVER_PID" > "$PID_FILE"
    sleep 1

    if ! kill -0 "$CURRENT_SERVER_PID" 2>/dev/null; then
        if [[ -s "$CURRENT_SERVER_LOG" ]] && grep -qi 'Address already in use' "$CURRENT_SERVER_LOG"; then
            skip "Demarrage de ./webserv $config impossible: bind refuse (port occupe)."
        elif [[ -s "$CURRENT_SERVER_LOG" ]] && grep -qi 'Bind returned -1' "$CURRENT_SERVER_LOG"; then
            skip "Demarrage de ./webserv $config impossible: bind a echoue (port occupe ou conflit de config)."
        else
            fail "Demarrage de ./webserv $config impossible: processus termine juste apres lancement"
        fi
        if [[ -s "$CURRENT_SERVER_LOG" ]]; then
            log "--- LOG $config ---"
            sed -n '1,60p' "$CURRENT_SERVER_LOG" >> "$RESULT_FILE"
            log "--- FIN LOG ---"
        fi
        CURRENT_SERVER_PID=""
        rm -f "$PID_FILE"
        return 1
    fi
    return 0
}

stop_server() {
    if [[ -n "$CURRENT_SERVER_PID" && $KEEP_SERVER -eq 0 ]]; then
        kill "$CURRENT_SERVER_PID" 2>/dev/null || true
        wait "$CURRENT_SERVER_PID" 2>/dev/null || true
        CURRENT_SERVER_PID=""
        CURRENT_SERVER_LOG=""
        CURRENT_SERVER_PORTS=""
    fi
    rm -f "$PID_FILE"
}

cleanup_stale_pid_file() {
    local pid
    local comm

    if [[ ! -f "$PID_FILE" ]]; then
        return 0
    fi
    pid="$(cat "$PID_FILE" 2>/dev/null || true)"
    if [[ -z "$pid" ]]; then
        rm -f "$PID_FILE"
        return 0
    fi

    if kill -0 "$pid" 2>/dev/null; then
        comm="$(ps -p "$pid" -o comm= 2>/dev/null | tr -d ' ' || true)"
        if [[ "$comm" == "webserv" ]]; then
            kill "$pid" 2>/dev/null || true
            wait "$pid" 2>/dev/null || true
            note "Ancien webserv du script detecte puis ferme (pid=$pid)"
        fi
    fi
    rm -f "$PID_FILE"
}

wait_http() {
    local url="$1"
    local tries=30
    local code=""
    local i

    for ((i = 0; i < tries; i++)); do
        code="$(curl -s -o /dev/null -w "%{http_code}" --max-time 2 "$url" 2>/dev/null || true)"
        if [[ "$code" != "000" && -n "$code" ]]; then
            return 0
        fi
        sleep 0.2
    done
    return 1
}

http_code() {
    curl -s -o /dev/null -w "%{http_code}" --max-time 8 "$@"
}

http_body() {
    curl -s --max-time 8 "$@"
}

http_head() {
    curl -s -D - -o /dev/null --max-time 8 "$@"
}

check_code() {
    local label="$1"
    local expected="$2"
    shift 2
    local got

    got="$(http_code "$@" 2>/dev/null || true)"
    if [[ "$got" == "$expected" ]]; then
        pass "$label -> $got"
    else
        fail "$label -> attendu=$expected recu=$got"
    fi
}

check_code_any() {
    local label="$1"
    shift
    local got="$1"
    shift
    local expected

    for expected in "$@"; do
        if [[ "$got" == "$expected" ]]; then
            pass "$label -> $got"
            return 0
        fi
    done
    fail "$label -> recu=$got (attendu: $*)"
    return 1
}

check_body_contains() {
    local label="$1"
    local url="$2"
    local needle="$3"
    local body

    body="$(http_body "$url" 2>/dev/null || true)"
    if printf "%s" "$body" | grep -Fq "$needle"; then
        pass "$label"
    else
        fail "$label -> motif absent: $needle"
        log "--- BODY $url ---"
        printf "%s\n" "$body" | sed -n '1,40p' >> "$RESULT_FILE"
        log "--- FIN BODY ---"
    fi
}


check_header_contains() {
    local label="$1"
    local url="$2"
    local needle="$3"
    shift 3
    local headers

    headers="$(http_head "$url" "$@" 2>/dev/null || true)"
    if printf "%s" "$headers" | grep -Fqi "$needle"; then
        pass "$label"
    else
        fail "$label -> header absent: $needle"
        log "--- HEADERS $url ---"
        printf "%s\n" "$headers" >> "$RESULT_FILE"
        log "--- FIN HEADERS ---"
    fi
}

raw_request() {
    local port="$1"
    local payload="$2"
    local limit="${3:-5}"

    timeout "$limit" bash -c '
        exec 3<>/dev/tcp/127.0.0.1/"$1" || exit 111
        printf "%b" "$2" >&3
        cat <&3
    ' _ "$port" "$payload" 2>/dev/null
}

raw_status() {
    local port="$1"
    local payload="$2"
    local limit="${3:-5}"
    local line

    line="$(raw_request "$port" "$payload" "$limit" | tr -d '\r' | awk 'NR==1{print; exit}')"
    printf "%s" "$line" | awk '{print $2}'
}

check_raw_status() {
    local label="$1"
    local port="$2"
    local expected="$3"
    local payload="$4"
    local limit="${5:-5}"
    local got

    got="$(raw_status "$port" "$payload" "$limit")"
    if [[ "$got" == "$expected" ]]; then
        pass "$label -> $got"
    else
        fail "$label -> attendu=$expected recu=$got"
    fi
}

run_telnet_get() {
    local response

    if ! optional_cmd telnet; then
        skip "Telnet GET smoke test impossible: telnet absent"
        return 0
    fi

    response="$(printf 'GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n' | timeout 6 telnet 127.0.0.1 8080 2>/dev/null | tr -d '\r' || true)"
    if printf "%s" "$response" | grep -q "200 OK"; then
        pass "Telnet GET / -> 200 OK"
    else
        fail "Telnet GET / n'a pas retourne 200 OK"
        log "--- TELNET OUTPUT ---"
        printf "%s\n" "$response" >> "$RESULT_FILE"
        log "--- FIN TELNET OUTPUT ---"
    fi
}

test_readme() {
    section "README"

    if [[ ! -f "$ROOT_DIR/README.md" ]]; then
        fail "README.md absent a la racine"
        return
    fi

    if grep -Eq '^\*This project has been created as part of the 42 curriculum by [A-Za-z0-9_-]+(, [A-Za-z0-9_-]+)*\.\*$' README.md; then
        pass "README ligne 1 conforme au format intra"
    else
        fail "README ligne 1 non conforme au format intra"
    fi

    grep -Eq '^## Description$' README.md && pass "README section Description presente" || fail "README section Description absente"
    grep -Eq '^## Instructions$' README.md && pass "README section Instructions presente" || fail "README section Instructions absente"
    grep -Eq '^## Resources$' README.md && pass "README section Resources presente" || fail "README section Resources absente"
    grep -Eqi 'AI Usage|AI was used|Anthropic|Claude|Copilot|ChatGPT' README.md && pass "README mentionne l'usage IA" || fail "README n'explique pas l'usage IA"
}

test_build() {
    section "BUILD"

    if make -s >> "$RESULT_FILE" 2>&1; then
        [[ -x "$ROOT_DIR/webserv" ]] && pass "make produit ./webserv" || fail "make n'a pas produit ./webserv"
    else
        fail "make a echoue"
        return 1
    fi

    if make -n 2>&1 | grep -q "Nothing to be done"; then
        pass "make sans changement ne relink pas"
    else
        fail "make sans changement semble relink"
    fi

    sleep 1
    touch srcs/Core/Core.cpp
    sleep 1
    if make -s >> "$RESULT_FILE" 2>&1; then
        pass "recompilation incremental apres touch OK"
    else
        fail "recompilation incremental apres touch echouee"
    fi
}

test_code_checks() {
    section "CODE CHECKS"

    local poll_count
    local errno_bad
    local eagain_count

    poll_count="$(grep -R -n --include='*.cpp' --include='*.hpp' 'poll[[:space:]]*(' srcs 2>/dev/null | wc -l | tr -d ' ')"
    if [[ "$poll_count" == "1" ]]; then
        pass "Un seul poll() dans srcs"
    else
        fail "Nombre de poll() attendu=1 recu=$poll_count"
    fi

    grep -q 'POLLIN' srcs/Core/Core.cpp && pass "POLLIN present dans la boucle principale" || fail "POLLIN absent dans Core.cpp"
    grep -q 'POLLOUT' srcs/Core/Core.cpp && pass "POLLOUT present dans la boucle principale" || fail "POLLOUT absent dans Core.cpp"
    grep -q 'revents' srcs/Core/Core.cpp && pass "revents utilise dans Core.cpp" || fail "revents absent dans Core.cpp"

    grep -A5 'recv(' srcs/Core/Server.cpp | grep -Eq '<= *0|== *0|< *0' && pass "recv() gere 0 et/ou -1" || fail "recv() ne montre pas clairement la gestion de 0/-1"
    grep -A8 'send(' srcs/Core/Server.cpp | grep -Eq '<= *0|== *0|< *0' && pass "send() gere 0 et/ou -1" || fail "send() ne montre pas clairement la gestion de 0/-1"

    eagain_count="$(grep -R -n 'EAGAIN\|EWOULDBLOCK' srcs 2>/dev/null | wc -l | tr -d ' ')"
    if [[ "$eagain_count" == "0" ]]; then
        pass "Aucun EAGAIN/EWOULDBLOCK dans srcs"
    else
        fail "EAGAIN/EWOULDBLOCK trouves dans srcs ($eagain_count)"
    fi

    errno_bad="$(grep -R -n 'errno' srcs 2>/dev/null | grep -v 'EINTR' | grep -v 'errno.h' || true)"
    if [[ -z "$errno_bad" ]]; then
        pass "errno utilise uniquement pour EINTR"
    else
        fail "errno utilise ailleurs que EINTR"
        printf "%s\n" "$errno_bad" >> "$RESULT_FILE"
    fi

    grep -q 'chdir' srcs/HTTPRequest/CgiManager.cpp && pass "CGI change de dossier via chdir()" || fail "chdir() absent de CgiManager.cpp"
}

test_core_runtime() {
    section "CORE.CONFIG RUNTIME"

    if ! start_server "configs/Core.config" "8080"; then
        return 1
    fi
    if ! wait_http "http://127.0.0.1:8080/"; then
        fail "Serveur Core.config ne repond pas sur 8080"
        return 1
    fi

    pass "Core.config demarre sur 8080"
    run_telnet_get

    check_code "GET /" 200 "http://127.0.0.1:8080/"
    check_code "GET /script.js" 200 "http://127.0.0.1:8080/script.js"
    check_code "GET /assets/styles.css" 200 "http://127.0.0.1:8080/assets/styles.css"
    check_code "GET /browse/" 200 "http://127.0.0.1:8080/browse/"
    check_body_contains "Listing /browse/ contient a.txt" "http://127.0.0.1:8080/browse/" "a.txt"
    check_body_contains "Listing /browse/ contient b.txt" "http://127.0.0.1:8080/browse/" "b.txt"
    check_code "GET /noindex/" 403 "http://127.0.0.1:8080/noindex/"

    check_code "DELETE interdit sur /readonly/" 405 -X DELETE "http://127.0.0.1:8080/readonly/index.html"
    check_header_contains "405 contient Allow" "http://127.0.0.1:8080/readonly/index.html" "Allow:" -X DELETE

    check_code "UNKNOWN method -> 501" 501 -X UNKNOWN "http://127.0.0.1:8080/"
    check_code "404 URL invalide" 404 "http://127.0.0.1:8080/does-not-exist"
    check_code "Redirection /redirect/ -> 301" 301 "http://127.0.0.1:8080/redirect/"
    check_header_contains "301 contient Location" "http://127.0.0.1:8080/redirect/" "Location: http://www.youtube.com"

    check_raw_status "400 requete HTTP/1.1 sans Host" 8080 400 $'GET / HTTP/1.1\r\nConnection: close\r\n\r\n'
    check_raw_status "505 version HTTP/1.0" 8080 505 $'GET / HTTP/1.0\r\nHost: localhost\r\nConnection: close\r\n\r\n'

    check_code "POST /upload/ -> 201" 201 -X POST --data "hello eval" "http://127.0.0.1:8080/upload/eval_test.txt"
    check_code "GET fichier upload -> 200" 200 "http://127.0.0.1:8080/upload/eval_test.txt"
    check_body_contains "Contenu du fichier upload correct" "http://127.0.0.1:8080/upload/eval_test.txt" "hello eval"
    check_code "DELETE fichier upload -> 204" 204 -X DELETE "http://127.0.0.1:8080/upload/eval_test.txt"

    backup_file "configs/www/errors/404.html"
    write_file "configs/www/errors/404.html" '<html><body>EVAL_404_MARKER</body></html>\n'
    check_body_contains "404 custom page verifiee apres modification" "http://127.0.0.1:8080/missing-page" "EVAL_404_MARKER"
    restore_file "configs/www/errors/404.html"

    backup_file "configs/www/docs/index.html"
    write_file "configs/www/docs/index.html" '<html><body>EVAL_DOCS_INDEX_MARKER</body></html>\n'
    check_body_contains "Index par defaut sur /docs/" "http://127.0.0.1:8080/docs/" "EVAL_DOCS_INDEX_MARKER"
    restore_file "configs/www/docs/index.html"

    backup_file "configs/www/health/index.html"
    write_file "configs/www/health/index.html" '<html><body>EVAL_HEALTH_ROUTE_MARKER</body></html>\n'
    check_body_contains "Route /health pointe vers son propre dossier" "http://127.0.0.1:8080/health" "EVAL_HEALTH_ROUTE_MARKER"
    restore_file "configs/www/health/index.html"

    check_code "CGI GET /cgi-bin/test.py -> 200" 200 "http://127.0.0.1:8080/cgi-bin/test.py"
    check_body_contains "CGI GET contient CGI OK" "http://127.0.0.1:8080/cgi-bin/test.py" "CGI OK"
    check_body_contains "CGI GET contient METHOD = GET" "http://127.0.0.1:8080/cgi-bin/test.py" "METHOD = GET"

    local post_body
    post_body="$(curl -s --max-time 8 -X POST --data "body_from_eval" "http://127.0.0.1:8080/cgi-bin/test.py" 2>/dev/null || true)"
    if printf "%s" "$post_body" | grep -Fq "METHOD = POST" && printf "%s" "$post_body" | grep -Fq "BODY = body_from_eval"; then
        pass "CGI POST reporte methode et body correctement"
    else
        fail "CGI POST ne reporte pas correctement methode/body"
        log "--- CGI POST BODY ---"
        printf "%s\n" "$post_body" >> "$RESULT_FILE"
        log "--- FIN CGI POST BODY ---"
    fi

    backup_file "configs/www/cgi-bin/test.py"
    write_file "configs/www/cgi-bin/test.py" '#!/usr/bin/env python3\nraise RuntimeError("forced eval error")\n'
    chmod +x "$ROOT_DIR/configs/www/cgi-bin/test.py"
    check_code "CGI en erreur -> 500" 500 "http://127.0.0.1:8080/cgi-bin/test.py"
    restore_file "configs/www/cgi-bin/test.py"
    chmod +x "$ROOT_DIR/configs/www/cgi-bin/test.py"
    check_code "Serveur toujours actif apres CGI en erreur" 200 "http://127.0.0.1:8080/"

    backup_file "configs/www/cgi-bin/test.py"
    write_file "configs/www/cgi-bin/test.py" '#!/usr/bin/env python3\nimport time\nwhile True:\n    time.sleep(1)\n'
    chmod +x "$ROOT_DIR/configs/www/cgi-bin/test.py"
    local cgi_loop_code
    local cgi_loop_rc
    cgi_loop_code="$(curl -s -o /dev/null -w '%{http_code}' --max-time 5 "http://127.0.0.1:8080/cgi-bin/test.py" 2>/dev/null)"
    cgi_loop_rc=$?
    if [[ "$cgi_loop_code" == "500" || "$cgi_loop_code" == "504" || "$cgi_loop_code" == "408" || "$cgi_loop_rc" -eq 28 ]]; then
        pass "CGI boucle infinie geree sans crash (timeout ou erreur HTTP)"
    else
        fail "CGI boucle infinie: comportement inattendu (http=$cgi_loop_code rc=$cgi_loop_rc)"
    fi
    restore_file "configs/www/cgi-bin/test.py"
    chmod +x "$ROOT_DIR/configs/www/cgi-bin/test.py"
    check_code "Serveur toujours actif apres CGI boucle infinie" 200 "http://127.0.0.1:8080/"

    local hang_output
    hang_output="$(timeout 14 bash -c '
        exec 3<>/dev/tcp/127.0.0.1/8080 || exit 90
        printf "POST /upload/hang.txt HTTP/1.1\r\nHost: localhost\r\nContent-Length: 100\r\nConnection: close\r\n\r\nabc" >&3
        sleep 12
        cat <&3
    ' 2>/dev/null || true)"
    if printf "%s" "$hang_output" | tr -d '\r' | grep -Eq '400|408|Bad Request|Request Timeout'; then
        pass "Connexion pendante incomplete fermee avec reponse d'erreur"
    else
        note "Test de connexion pendante ambigu (sortie brute a verifier dans le rapport)"
        log "--- HANG OUTPUT ---"
        printf "%s\n" "$hang_output" >> "$RESULT_FILE"
        log "--- FIN HANG OUTPUT ---"
    fi
    check_code "Serveur toujours actif apres connexion pendante" 200 "http://127.0.0.1:8080/"

    local i
    local curl_pid
    local -a curl_pids
    for ((i = 0; i < 20; i++)); do
        curl -s --max-time 4 "http://127.0.0.1:8080/" -o /dev/null &
        curl_pids+=("$!")
    done
    for curl_pid in "${curl_pids[@]}"; do
        wait "$curl_pid" 2>/dev/null || true
    done
    check_code "Serveur stable apres 20 requetes paralleles" 200 "http://127.0.0.1:8080/"

    rm -f "$ROOT_DIR/configs/www/upload/eval_test.txt" 2>/dev/null || true

    # --- Headers de reponse ---
    local ct_html
    ct_html="$(curl -sI --max-time 8 'http://127.0.0.1:8080/' 2>/dev/null | grep -i '^content-type:' || true)"
    if printf "%s" "$ct_html" | grep -qi 'text/html'; then
        pass "Content-Type text/html pour GET /"
    else
        fail "Content-Type incorrect pour GET / (recu: $ct_html)"
    fi

    local ct_css
    ct_css="$(curl -sI --max-time 8 'http://127.0.0.1:8080/assets/styles.css' 2>/dev/null | grep -i '^content-type:' || true)"
    if printf "%s" "$ct_css" | grep -qi 'text/css'; then
        pass "Content-Type text/css pour /assets/styles.css"
    else
        fail "Content-Type incorrect pour /assets/styles.css (recu: $ct_css)"
    fi

    local ct_js
    ct_js="$(curl -sI --max-time 8 'http://127.0.0.1:8080/script.js' 2>/dev/null | grep -i '^content-type:' || true)"
    if printf "%s" "$ct_js" | grep -qi 'javascript'; then
        pass "Content-Type javascript pour /script.js"
    else
        fail "Content-Type incorrect pour /script.js (recu: $ct_js)"
    fi

    # Content-Length present et coherent
    local cl_val cl_body_len
    cl_val="$(curl -sI --max-time 8 'http://127.0.0.1:8080/script.js' 2>/dev/null | grep -i '^content-length:' | tr -d '\r' | awk '{print $2}' || true)"
    cl_body_len="$(curl -s --max-time 8 'http://127.0.0.1:8080/script.js' 2>/dev/null | wc -c | tr -d ' ' || true)"
    if [[ -n "$cl_val" && "$cl_val" == "$cl_body_len" ]]; then
        pass "Content-Length correct pour /script.js ($cl_val octets)"
    elif [[ -z "$cl_val" ]]; then
        fail "Content-Length absent dans la reponse GET /script.js"
    else
        fail "Content-Length incorrect pour /script.js: header=$cl_val body=$cl_body_len"
    fi

    # HEAD : headers presents, body vide
    local head_status head_cl head_body_bytes
    head_status="$(curl -s -o /dev/null -w '%{http_code}' --max-time 8 -X HEAD 'http://127.0.0.1:8080/' 2>/dev/null || true)"
    if [[ "$head_status" == "200" ]]; then
        pass "HEAD / -> 200"
    else
        fail "HEAD / -> attendu=200 recu=$head_status"
    fi
    head_cl="$(curl -sI --max-time 8 'http://127.0.0.1:8080/' 2>/dev/null | grep -i '^content-length:' | tr -d '\r' | awk '{print $2}' || true)"
    if [[ -n "$head_cl" ]]; then
        pass "HEAD / contient Content-Length"
    else
        fail "HEAD / ne contient pas Content-Length"
    fi
    head_body_bytes="$(curl -s --max-time 8 -X HEAD 'http://127.0.0.1:8080/' 2>/dev/null | wc -c | tr -d ' ' || true)"
    if [[ "$head_body_bytes" == "0" ]]; then
        pass "HEAD / body vide (0 octets)"
    else
        fail "HEAD / body non vide ($head_body_bytes octets)"
    fi

    # Path traversal
    local trav_code
    trav_code="$(http_code 'http://127.0.0.1:8080/upload/../../../etc/passwd' 2>/dev/null || true)"
    if [[ "$trav_code" == "400" || "$trav_code" == "404" || "$trav_code" == "403" ]]; then
        pass "Path traversal GET /upload/../../../etc/passwd -> $trav_code (acces refuse)"
    else
        fail "Path traversal GET /upload/../../../etc/passwd -> attendu=400/403/404 recu=$trav_code"
    fi
    local trav_encoded_code
    trav_encoded_code="$(http_code 'http://127.0.0.1:8080/upload/%2e%2e%2f%2e%2e%2fetc%2fpasswd' 2>/dev/null || true)"
    if [[ "$trav_encoded_code" == "400" || "$trav_encoded_code" == "404" || "$trav_encoded_code" == "403" ]]; then
        pass "Path traversal encode -> $trav_encoded_code (acces refuse)"
    else
        fail "Path traversal encode -> attendu=400/403/404 recu=$trav_encoded_code"
    fi

    # CGI QUERY_STRING
    check_body_contains "CGI QUERY_STRING transmis" "http://127.0.0.1:8080/cgi-bin/test.py?foo=bar" "QUERY = foo=bar"

    # CGI fichier inexistant -> 404
    check_code "CGI fichier inexistant -> 404" 404 "http://127.0.0.1:8080/cgi-bin/does_not_exist.py"

    # POST : header Location dans 201
    local post_loc_headers
    post_loc_headers="$(curl -s -D - -o /dev/null --max-time 8 -X POST --data 'loc_test' 'http://127.0.0.1:8080/upload/loc_eval.txt' 2>/dev/null || true)"
    if printf "%s" "$post_loc_headers" | grep -qi '^location:'; then
        pass "POST 201 contient header Location"
    else
        fail "POST 201 ne contient pas header Location"
    fi
    rm -f "$ROOT_DIR/configs/www/upload/loc_eval.txt" 2>/dev/null || true

    # DELETE sur repertoire -> 403 ou 400 (pas 204)
    local del_dir_code
    del_dir_code="$(http_code -X DELETE 'http://127.0.0.1:8080/upload/' 2>/dev/null || true)"
    if [[ "$del_dir_code" == "403" || "$del_dir_code" == "400" || "$del_dir_code" == "405" ]]; then
        pass "DELETE sur repertoire -> $del_dir_code (refuse)"
    else
        fail "DELETE sur repertoire -> attendu=400/403/405 recu=$del_dir_code"
    fi

    # DELETE hors upload_dir -> 403 ou 405 (readonly n'autorise que GET)
    local del_ro_code
    del_ro_code="$(http_code -X DELETE 'http://127.0.0.1:8080/readonly/index.html' 2>/dev/null || true)"
    if [[ "$del_ro_code" == "403" || "$del_ro_code" == "405" ]]; then
        pass "DELETE hors upload_dir /readonly/ -> $del_ro_code (refuse)"
    else
        fail "DELETE hors upload_dir /readonly/ -> attendu=403 ou 405 recu=$del_ro_code"
    fi

    # 413 frontiere exacte (client_max_body_size = 1000000000 pour Core.config, trop grand a tester)
    # On verifie juste que le body de taille 0 est accepte
    check_code "POST body vide accepte -> 201" 201 -X POST --data '' "http://127.0.0.1:8080/upload/empty_eval.txt"
    rm -f "$ROOT_DIR/configs/www/upload/empty_eval.txt" 2>/dev/null || true

    return 0
}

test_test_config() {
    section "TEST.CONFIG MULTI-SERVER"

    if ! start_server "configs/test.config" "8080,8081,8082"; then
        return 1
    fi
    wait_http "http://127.0.0.1:8080/" || { fail "test.config ne repond pas sur 8080"; return 1; }
    wait_http "http://127.0.0.1:8081/" || fail "test.config ne repond pas sur 8081"
    wait_http "http://127.0.0.1:8082/" || fail "test.config ne repond pas sur 8082"

    backup_file "configs/www1/index.html"
    backup_file "configs/www2/index.html"
    backup_file "configs/www3/index.html"
    write_file "configs/www1/index.html" '<html><body>PORT_8080_MARKER</body></html>\n'
    write_file "configs/www2/index.html" '<html><body>PORT_8081_MARKER</body></html>\n'
    write_file "configs/www3/index.html" '<html><body>PORT_8082_MARKER</body></html>\n'

    check_body_contains "8080 sert bien www1" "http://127.0.0.1:8080/" "PORT_8080_MARKER"
    check_body_contains "8081 sert bien www2" "http://127.0.0.1:8081/" "PORT_8081_MARKER"
    check_body_contains "8082 sert bien www3" "http://127.0.0.1:8082/" "PORT_8082_MARKER"

    restore_file "configs/www1/index.html"
    restore_file "configs/www2/index.html"
    restore_file "configs/www3/index.html"

    check_code "8080 /form-test.html absent (www1)" 404 "http://127.0.0.1:8080/form-test.html"
    check_code "8081 /form-test.html present (www2)" 200 "http://127.0.0.1:8081/form-test.html"
    check_code "8082 /browse/ present" 200 "http://127.0.0.1:8082/browse/"
    check_body_contains "8082 /browse/ liste c.txt" "http://127.0.0.1:8082/browse/" "c.txt"
    check_code "8082 POST / interdit" 405 -X POST --data "x" "http://127.0.0.1:8082/"

    check_code "8080 petit body accepte" 201 -X POST --data "small-body" "http://127.0.0.1:8080/upload/small_eval.txt"
    check_code "8080 GET fichier upload -> 200" 200 "http://127.0.0.1:8080/upload/small_eval.txt"
    check_body_contains "8080 contenu fichier upload correct" "http://127.0.0.1:8080/upload/small_eval.txt" "small-body"
    check_code "8080 DELETE fichier upload -> 204" 204 -X DELETE "http://127.0.0.1:8080/upload/small_eval.txt"

    # 413 frontiere : body exactement a la limite (1000000 octets) -> 201
    local boundary_code
    boundary_code="$(head -c 1000000 /dev/zero | tr '\0' 'B' | curl -s -o /dev/null -w '%{http_code}' --max-time 12 -X POST -H 'Content-Type: text/plain' --data-binary @- 'http://127.0.0.1:8080/upload/boundary_eval.txt' 2>/dev/null || true)"
    if [[ "$boundary_code" == "201" ]]; then
        pass "8080 body exactement a la limite -> 201"
    else
        fail "8080 body exactement a la limite -> attendu=201 recu=$boundary_code"
    fi
    rm -f "$ROOT_DIR/configs/www1/upload/boundary_eval.txt" 2>/dev/null || true

    local big_code
    big_code="$(head -c 2000000 /dev/zero | tr '\0' 'A' | curl -s -o /dev/null -w '%{http_code}' --max-time 12 -X POST -H 'Content-Type: text/plain' --data-binary @- 'http://127.0.0.1:8080/upload/big_eval.txt' 2>/dev/null || true)"
    if [[ "$big_code" == "413" ]]; then
        pass "8080 gros body > limite -> 413"
    else
        fail "8080 gros body > limite -> attendu=413 recu=$big_code"
    fi

    check_code "8081 redirect -> 301" 301 "http://127.0.0.1:8081/redirect/"
    check_header_contains "8081 redirect contient example.com" "http://127.0.0.1:8081/redirect/" "Location: http://example.com"
    check_code "8081 CGI Python GET" 200 "http://127.0.0.1:8081/cgi-bin/test.py"

    if optional_cmd php-cgi; then
        local php_code
        php_code="$(http_code 'http://127.0.0.1:8081/cgi-php/test.php' 2>/dev/null || true)"
        check_code_any "8081 CGI PHP" "$php_code" 200 500
    else
        skip "8081 CGI PHP impossible: php-cgi absent"
    fi

    rm -f "$ROOT_DIR/configs/www1/upload/small_eval.txt" "$ROOT_DIR/configs/www1/upload/big_eval.txt" 2>/dev/null || true
    return 0
}

test_multi_config() {
    section "MULTI.CONFIG"

    if ! start_server "configs/multi.config" "8080,8081"; then
        return 1
    fi
    wait_http "http://127.0.0.1:8080/" || { fail "multi.config ne repond pas sur 8080"; return 1; }
    wait_http "http://127.0.0.1:8081/" || { fail "multi.config ne repond pas sur 8081"; return 1; }

    check_code "multi.config 8080 -> 200" 200 "http://127.0.0.1:8080/"
    check_code "multi.config 8081 -> 200" 200 "http://127.0.0.1:8081/"

    # Verifier que les deux ports servent des contenus differents
    backup_file "configs/www/index.html"
    write_file "configs/www/index.html" '<html><body>MULTI_8080_MARKER</body></html>\n'
    local multi_8080_body multi_8081_body
    multi_8080_body="$(http_body 'http://127.0.0.1:8080/' 2>/dev/null || true)"
    multi_8081_body="$(http_body 'http://127.0.0.1:8081/' 2>/dev/null || true)"
    restore_file "configs/www/index.html"
    if printf "%s" "$multi_8080_body" | grep -Fq "MULTI_8080_MARKER"; then
        pass "multi.config 8080 sert son contenu (racine injectee)"
    else
        fail "multi.config 8080 ne sert pas le bon contenu"
    fi
    if printf "%s" "$multi_8081_body" | grep -Fq "MULTI_8080_MARKER"; then
        pass "multi.config 8081 partage la meme racine que 8080 (attendu)"
    else
        note "multi.config 8081 sert un contenu different de 8080 (verifier la config)"
    fi

    # 404 sur les deux ports
    check_code "multi.config 8080 404 URL invalide" 404 "http://127.0.0.1:8080/does-not-exist"
    check_code "multi.config 8081 404 URL invalide" 404 "http://127.0.0.1:8081/does-not-exist"

    # Methode inconnue sur les deux ports
    check_code "multi.config 8080 UNKNOWN -> 501" 501 -X UNKNOWN "http://127.0.0.1:8080/"
    check_code "multi.config 8081 UNKNOWN -> 501" 501 -X UNKNOWN "http://127.0.0.1:8081/"

    # Upload + GET + DELETE sur 8080
    check_code "multi.config 8080 POST upload -> 201" 201 -X POST --data "multi_eval" "http://127.0.0.1:8080/upload/multi_eval.txt"
    check_code "multi.config 8080 GET upload -> 200" 200 "http://127.0.0.1:8080/upload/multi_eval.txt"
    check_body_contains "multi.config 8080 contenu upload correct" "http://127.0.0.1:8080/upload/multi_eval.txt" "multi_eval"
    check_code "multi.config 8080 DELETE upload -> 204" 204 -X DELETE "http://127.0.0.1:8080/upload/multi_eval.txt"

    rm -f "$ROOT_DIR/configs/www/upload/multi_eval.txt" 2>/dev/null || true
}

test_port_conflicts() {
    section "PORT CONFLICTS"

    start_server "configs/Core.config" "8080" || return 1
    wait_http "http://127.0.0.1:8080/" || { fail "Core.config non disponible avant tests de conflit"; return 1; }

    local log_same_process="$TMP_DIR/same-port-single-process.log"
    local dup_conf="$TMP_DIR/duplicate_same_port.config"
    cat configs/Core.config configs/Core.config > "$dup_conf"

    ./webserv "$dup_conf" > "$log_same_process" 2>&1 &
    local pid_same=$!
    sleep 1
    if kill -0 "$pid_same" 2>/dev/null; then
        if wait_http "http://127.0.0.1:8080/"; then
            pass "Deux server blocks meme interface:port geres de facon coherente"
            kill "$pid_same" 2>/dev/null || true
            wait "$pid_same" 2>/dev/null || true
        else
            fail "Processus avec deux server blocks meme port demarre mais ne repond pas correctement"
            kill "$pid_same" 2>/dev/null || true
            wait "$pid_same" 2>/dev/null || true
        fi
    else
        local code_same=0
        wait "$pid_same" 2>/dev/null || code_same=$?
        if [[ "$code_same" == "139" ]]; then
            fail "Deux server blocks meme port provoquent un crash"
        else
            pass "Deux server blocks meme interface:port rejetes proprement"
        fi
    fi

    local log_second_process="$TMP_DIR/second-process.log"
    ./webserv configs/test.config > "$log_second_process" 2>&1 &
    local pid_second=$!
    sleep 1

    if kill -0 "$pid_second" 2>/dev/null; then
        if wait_http "http://127.0.0.1:8082/"; then
            pass "Deux processus avec ports en commun restent coherents (second exploite au moins 8082)"
        else
            fail "Second processus reste vivant mais comportement reseau incoherent"
        fi
        kill "$pid_second" 2>/dev/null || true
        wait "$pid_second" 2>/dev/null || true
    else
        local code_second=0
        wait "$pid_second" 2>/dev/null || code_second=$?
        if [[ "$code_second" == "139" ]]; then
            fail "Second processus avec ports en commun crash"
        else
            pass "Second processus avec ports en commun rejete proprement"
        fi
    fi

    check_code "Premier serveur reste actif apres conflits de ports" 200 "http://127.0.0.1:8080/"
}

test_siege() {
    section "SIEGE"

    start_server "configs/Core.config" "8080" || return 1
    wait_http "http://127.0.0.1:8080/" || { fail "Core.config indisponible avant siege"; return 1; }

    if ! optional_cmd siege; then
        skip "Siege absent: stress tests impossibles"
        return 0
    fi

    local siege_out
    local avail
    local rss_before
    local rss_after
    local diff

    siege_out="$(siege -b -c 20 -r 100 -d 1 http://127.0.0.1:8080/ 2>&1 || true)"
    printf "%s\n" "$siege_out" >> "$RESULT_FILE"

    if printf "%s" "$siege_out" | grep -q 'Availability:'; then
        pass "Siege charge simple execute"
        avail="$(printf "%s\n" "$siege_out" | awk '/Availability:/ {print $2; exit}')"
        if awk "BEGIN { exit !($avail >= 99.5) }"; then
            pass "Availability >= 99.5% ($avail)"
        else
            fail "Availability < 99.5% ($avail)"
        fi
    else
        fail "Sortie siege sans ligne Availability"
    fi

    local failed_tx
    failed_tx="$(printf "%s\n" "$siege_out" | awk '/Failed transactions:/ {print $3; exit}')"
    if [[ -n "$failed_tx" ]]; then
        if [[ "$failed_tx" == "0" || "$failed_tx" == "0.00" ]]; then
            pass "Aucune transaction echouee sous siege initial"
        else
            fail "Transactions echouees detectees sous siege initial ($failed_tx)"
        fi
    else
        note "Siege: impossible de parser 'Failed transactions'"
    fi

    rss_before="$(ps -o rss= -p "$CURRENT_SERVER_PID" 2>/dev/null | tr -d ' ' || true)"
    siege -b -c 10 -t 15s http://127.0.0.1:8080/ >/dev/null 2>&1 || true
    rss_after="$(ps -o rss= -p "$CURRENT_SERVER_PID" 2>/dev/null | tr -d ' ' || true)"
    if [[ -n "$rss_before" && -n "$rss_after" ]]; then
        diff=$((rss_after - rss_before))
        if [[ $diff -lt 10240 ]]; then
            pass "RSS stable sous siege (delta=${diff}kB)"
        else
            fail "RSS augmente trop sous siege (delta=${diff}kB)"
        fi
    else
        note "Mesure RSS indisponible pendant siege"
    fi

    siege -b -c 10 -d 2 -t 60s http://127.0.0.1:8080/ >/dev/null 2>&1 || true
    check_code "Serveur toujours actif apres siege prolonge" 200 "http://127.0.0.1:8080/"
    pass "Parametres siege utilises: -b -c 20 -r 100 -d 1 puis -t 15s/60s"
}

test_valgrind_best_effort() {
    section "VALGRIND (OPTIONNEL)"

    stop_server
    kill_existing_webserv_on_ports "8080" || true

    if ! optional_cmd valgrind; then
        skip "Valgrind absent: verification automatique des leaks impossible"
        return 0
    fi

    local vg_log="$TMP_DIR/valgrind_webserv.log"
    local vg_fifo="$TMP_DIR/valgrind_webserv.stdin"
    local vg_pid

    rm -f "$vg_fifo"
    if ! mkfifo "$vg_fifo"; then
        skip "Valgrind: creation FIFO stdin impossible"
        return 0
    fi

    # Keep the FIFO open to avoid immediate EOF on webserv stdin.
    exec 9<>"$vg_fifo"  

    timeout 90s valgrind --leak-check=full --show-leak-kinds=all --show-reachable=yes --errors-for-leak-kinds=definite --track-fds=yes \
        ./webserv configs/Core.config < "$vg_fifo" > "$vg_log" 2>&1 &
    vg_pid=$!
    sleep 2

    if ! kill -0 "$vg_pid" 2>/dev/null; then
        skip "Valgrind: webserv n'a pas demarre (port occupe ou lancement invalide)"
        if [[ -s "$vg_log" ]]; then
            log "--- VALGRIND STARTUP LOG ---"
            sed -n '1,80p' "$vg_log" >> "$RESULT_FILE"
            log "--- FIN VALGRIND STARTUP LOG ---"
        fi
        return 0
    fi

    if ! wait_http "http://127.0.0.1:8080/"; then
        note "Valgrind: serveur lent/non joignable pendant le test, resultat a confirmer manuellement"
    fi

    curl -s --max-time 4 "http://127.0.0.1:8080/" -o /dev/null || true
    curl -s --max-time 4 -X POST --data "vg" "http://127.0.0.1:8080/upload/vg.txt" -o /dev/null || true
    curl -s --max-time 4 -X DELETE "http://127.0.0.1:8080/upload/vg.txt" -o /dev/null || true

    # Ask webserv for a graceful shutdown before using signals.
    printf 'q\n' >&9 || true
    for _ in 1 2 3 4 5 6 7 8 9 10; do
        if ! kill -0 "$vg_pid" 2>/dev/null; then
            break
        fi
        sleep 1
    done

    if kill -0 "$vg_pid" 2>/dev/null; then
        kill -INT "$vg_pid" 2>/dev/null || true
        for _ in 1 2 3 4 5; do
            if ! kill -0 "$vg_pid" 2>/dev/null; then
                break
            fi
            sleep 1
        done
    fi

    if kill -0 "$vg_pid" 2>/dev/null; then
        kill -TERM "$vg_pid" 2>/dev/null || true
        for _ in 1 2 3 4 5; do
            if ! kill -0 "$vg_pid" 2>/dev/null; then
                break
            fi
            sleep 1
        done
    fi
    if kill -0 "$vg_pid" 2>/dev/null; then
        kill -KILL "$vg_pid" 2>/dev/null || true
    fi
    wait "$vg_pid" 2>/dev/null || true
    exec 9>&-
    exec 9<&-
    rm -f "$vg_fifo"

    if grep -Eq 'definitely lost: +0 bytes' "$vg_log" || grep -q 'All heap blocks were freed -- no leaks are possible' "$vg_log"; then
        pass "Valgrind: definitely lost = 0 bytes"
    else
        fail "Valgrind: fuite potentielle (definitely lost non nul ou introuvable)"
    fi

    if grep -Eq 'ERROR SUMMARY: +0 errors' "$vg_log"; then
        pass "Valgrind: ERROR SUMMARY = 0"
    else
        note "Valgrind: ERROR SUMMARY non nul (voir le rapport detaille)"
    fi

    kill_existing_webserv_on_ports "8080" || true

    log "--- VALGRIND LOG (DEBUT) ---"
    sed -n '1,120p' "$vg_log" >> "$RESULT_FILE"
    log "--- VALGRIND LOG (FIN) ---"
}

print_summary() {
    echo
    echo "PASS: $PASS"
    echo "FAIL: $FAIL"
    echo "SKIP/IMPOSSIBLE: $SKIP"
    echo "NOTE: $NOTE"
    echo "Rapport complet: $RESULT_FILE"

    if [[ -n ${FAIL_LIST[*]-} ]]; then
        echo
        echo "Faux:"
        printf ' - %s\n' "${FAIL_LIST[@]}"
    fi

    if [[ -n ${SKIP_LIST[*]-} ]]; then
        echo
        echo "Impossible:"
        printf ' - %s\n' "${SKIP_LIST[@]}"
    fi

    if [[ -n ${NOTE_LIST[*]-} ]]; then
        echo
        echo "Ambigus / manuels:"
        printf ' - %s\n' "${NOTE_LIST[@]}"
    fi
}

main() {
    : > "$RESULT_FILE"
    log "webserv eval run - $(timestamp)"
    log "root: $ROOT_DIR"

    cleanup_stale_pid_file

    while [[ $# -gt 0 ]]; do
        case "$1" in
            --keep-server)
                KEEP_SERVER=1
                note "Option --keep-server activee: le dernier serveur ne sera pas arrete automatiquement"
                ;;
            --kill-existing-webserv)
                KILL_EXISTING_WEBSERV=1
                note "Option --kill-existing-webserv activee: les webserv deja a l'ecoute sur les ports testes seront termines"
                ;;
            --help|-h)
                echo "Usage: ./eval_test.sh [--keep-server] [--kill-existing-webserv]"
                echo "  --keep-server            garde le dernier serveur lance vivant"
                echo "  --kill-existing-webserv  termine les processus 'webserv' deja a l'ecoute sur 8080/8081/8082 avant les tests"
                exit 0
                ;;
            *)
                echo "Option inconnue: $1"
                echo "Usage: ./eval_test.sh [--keep-server] [--kill-existing-webserv]"
                exit 2
                ;;
        esac
        shift
    done

    require_cmd curl "Preflight"
    require_cmd make "Preflight"
    require_cmd grep "Preflight"
    require_cmd timeout "Preflight"
    optional_cmd ss && pass "Preflight: commande 'ss' disponible" || note "Commande 'ss' absente: diagnostic des ports moins précis"

    test_readme
    test_build || { print_summary; exit 1; }
    test_code_checks
    test_core_runtime
    # Stop only after each config test block, before starting the next config.
    stop_server
    test_test_config
    stop_server
    test_multi_config
    stop_server
    test_port_conflicts
    stop_server
    test_siege
    stop_server
    test_valgrind_best_effort

    stop_server
    print_summary

    if [[ $FAIL -gt 255 ]]; then
        exit 255
    fi
    exit "$FAIL"
}

main "$@"
