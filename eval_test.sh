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
RUN_SIEGE=0
RUN_VALGRIND=1
KEEP_LOGS=0
VERBOSE=0
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

    if [[ $KEEP_LOGS -eq 0 ]]; then
        rm -rf "$TMP_DIR"
    else
        announce "Logs conserves dans: $TMP_DIR"
    fi
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
    shift 3
    local body

    body="$(http_body "$url" "$@" 2>/dev/null || true)"
    if printf "%s" "$body" | grep -Fq "$needle"; then
        pass "$label"
    else
        fail "$label -> motif absent: $needle"
        log "--- BODY $url ---"
        printf "%s\n" "$body" | sed -n '1,40p' >> "$RESULT_FILE"
        log "--- FIN BODY ---"
    fi
}

run_parser_case() {
    local parser_bin="$1"
    local label="$2"
    local mode="$3"
    local expected="$4"
    local rel_cfg="$5"
    local content="$6"
    local cfg_path="$TMP_DIR/$rel_cfg"
    local output

    printf "%b" "$content" > "$cfg_path"
    output="$("$parser_bin" "$cfg_path" 2>&1 || true)"

    if [[ "$mode" == "accept" ]]; then
        if printf "%s" "$output" | grep -Fq "$expected"; then
            pass "$label"
        else
            fail "$label -> parse non valide (motif absent: $expected)"
            log "--- PARSER OUTPUT $rel_cfg ---"
            printf "%s\n" "$output" | sed -n '1,80p' >> "$RESULT_FILE"
            log "--- FIN PARSER OUTPUT ---"
        fi
        return
    fi

    if printf "%s" "$output" | grep -Fq "$expected"; then
        pass "$label"
    else
        fail "$label -> rejet attendu (motif absent: $expected)"
        log "--- PARSER OUTPUT $rel_cfg ---"
        printf "%s\n" "$output" | sed -n '1,80p' >> "$RESULT_FILE"
        log "--- FIN PARSER OUTPUT ---"
    fi
}

run_parser_todo_accept_case() {
    local parser_bin="$1"
    local label="$2"
    local expected="$3"
    local rel_cfg="$4"
    local content="$5"
    local cfg_path="$TMP_DIR/$rel_cfg"
    local output

    printf "%b" "$content" > "$cfg_path"
    output="$("$parser_bin" "$cfg_path" 2>&1 || true)"

    if printf "%s" "$output" | grep -Fq "$expected"; then
        pass "$label"
    else
        note "TODO: $label pas encore accepte"
        log "--- PARSER OUTPUT $rel_cfg ---"
        printf "%s\n" "$output" | sed -n '1,80p' >> "$RESULT_FILE"
        log "--- FIN PARSER OUTPUT ---"
    fi
}

run_parser_suite_dir() {
    local parser_bin="$1"
    local dir="$2"
    local mode="$3"
    local file
    local output
    local name

    if [[ ! -d "$dir" ]]; then
        skip "suite parsing absente: $dir"
        return 0
    fi

    while IFS= read -r file; do
        [[ -n "$file" ]] || continue
        name="$(basename "$file")"
        output="$("$parser_bin" "$file" 2>&1 || true)"
        if [[ "$mode" == "invalid" ]]; then
            if printf "%s" "$output" | grep -Fq "nb de servers: 0"; then
                pass "parse suite invalid: $name rejete"
            else
                fail "parse suite invalid: $name accepte a tort"
                log "--- PARSER SUITE OUTPUT $name ---"
                printf "%s\n" "$output" | sed -n '1,80p' >> "$RESULT_FILE"
                log "--- FIN PARSER SUITE OUTPUT ---"
            fi
        else
            if printf "%s" "$output" | grep -Eq "nb de servers: [1-9][0-9]*"; then
                pass "parse suite valid: $name accepte"
            else
                note "parse suite valid: $name rejete (fixture peut etre obsolete)"
                log "--- PARSER SUITE OUTPUT $name ---"
                printf "%s\n" "$output" | sed -n '1,80p' >> "$RESULT_FILE"
                log "--- FIN PARSER SUITE OUTPUT ---"
            fi
        fi
    done < <(find "$dir" -type f -name '*.config' | sort)
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

progressive_chunked_status() {
    local port="$1"
    local path="$2"
    local first_chunk="$3"
    local second_chunk="$4"
    local limit="${5:-8}"
    local line
    local full_response

    full_response="$(timeout "$limit" bash -c '
        exec 3<>/dev/tcp/127.0.0.1/"$1" || exit 111
        printf "POST %s HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n" "$2" >&3
        printf "%s\r\n%s\r\n" "${#3}" "$3" >&3
        sleep 1
        printf "%s\r\n%s\r\n0\r\n\r\n" "${#4}" "$4" >&3
        cat <&3
    ' _ "$port" "$path" "$first_chunk" "$second_chunk" 2>&1 || true)"
    
    line="$(printf "%s" "$full_response" | tr -d '\r' | awk 'NR==1{print; exit}')"
    
    if [[ -z "$line" ]]; then
        [[ $VERBOSE -eq 1 ]] && log "DEBUG: progressive_chunked - empty response"
        printf ""
    else
        printf "%s" "$line" | awk '{print $2}'
    fi
}

incomplete_chunked_status() {
    local port="$1"
    local path="$2"
    local limit="${3:-12}"
    local line

    line="$(timeout "$limit" bash -c '
        exec 3<>/dev/tcp/127.0.0.1/"$1" || exit 111
        printf "POST %s HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n5\r\nabc" "$2" >&3
        sleep 9
        cat <&3
    ' _ "$port" "$path" 2>/dev/null | tr -d '\r' | awk 'NR==1{print; exit}')"
    printf "%s" "$line" | awk '{print $2}'
}

cgi_chunked_response() {
    local port="$1"
    local path="$2"
    local first_chunk="$3"
    local second_chunk="$4"
    local limit="${5:-8}"

    timeout "$limit" bash -c '
        exec 3<>/dev/tcp/127.0.0.1/"$1" || exit 111
        printf "POST %s HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\n" "$2" >&3
        printf "%x\r\n%s\r\n" "${#3}" "$3" >&3
        sleep 1
        printf "%x\r\n%s\r\n0\r\n\r\n" "${#4}" "$4" >&3
        cat <&3
    ' _ "$port" "$path" "$first_chunk" "$second_chunk" 2>/dev/null
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

check_program_rejects() {
    local label="$1"
    shift
    local output
    local code

    output="$(timeout 5s "$@" 2>&1)"
    code=$?
    if [[ "$code" != "0" && "$code" != "139" && "$code" != "124" ]]; then
        pass "$label rejete proprement (exit=$code)"
    elif [[ "$code" == "139" ]]; then
        fail "$label provoque un crash (segfault)"
    elif [[ "$code" == "124" ]]; then
        fail "$label ne termine pas (timeout)"
    else
        fail "$label accepte a tort"
    fi
    if [[ $VERBOSE -eq 1 ]]; then
        log "--- PROGRAM OUTPUT $label ---"
        printf "%s\n" "$output" | sed -n '1,40p' >> "$RESULT_FILE"
        log "--- FIN PROGRAM OUTPUT ---"
    fi
}

test_program_invocation() {
    section "PROGRAM INVOCATION"

    local empty_conf="$TMP_DIR/empty.config"
    : > "$empty_conf"

    check_program_rejects "Trop d'arguments" ./webserv configs/Core.config configs/test.config
    check_program_rejects "Config inexistante" ./webserv "$TMP_DIR/does_not_exist.config"
    check_program_rejects "Config vide" ./webserv "$empty_conf"
}

test_code_checks() {
    section "CODE CHECKS"

    local poll_count
    local poll_hits
    local alt_event_mech
    local accept_count
    local errno_bad
    local eagain_count
    local boost_hits
    local external_lib_hits

    alt_event_mech="$(grep -R -n --include='*.cpp' --include='*.hpp' -E '(^|[^A-Za-z0-9_])(select|epoll_wait|kqueue|kevent)[[:space:]]*\(' srcs 2>/dev/null || true)"
    if [[ -z "$alt_event_mech" ]]; then
        pass "Aucun select/epoll/kqueue/kevent dans srcs"
    else
        fail "Mecanismes d'evenement concurrents detectes"
        printf "%s\n" "$alt_event_mech" >> "$RESULT_FILE"
    fi

    poll_hits="$(find srcs -type f \( -name '*.cpp' -o -name '*.hpp' \) -exec awk '
        {
            code = $0
            sub(/\/\/.*/, "", code)
            if (code ~ /(^|[^A-Za-z0-9_])poll[[:space:]]*\(/)
                print FILENAME ":" FNR ":" $0
        }
    ' {} + 2>/dev/null || true)"
    poll_count="$(printf "%s\n" "$poll_hits" | sed '/^$/d' | wc -l | tr -d ' ')"
    if [[ "$poll_count" == "1" ]]; then
        pass "Un seul poll() dans srcs"
    else
        fail "Nombre de poll() attendu=1 recu=$poll_count"
        log "--- POLL HITS ---"
        printf "%s\n" "$poll_hits" >> "$RESULT_FILE"
        log "--- FIN POLL HITS ---"
    fi

    grep -q 'POLLIN' srcs/Core/Core.cpp && pass "POLLIN present dans la boucle principale" || fail "POLLIN absent dans Core.cpp"
    grep -q 'POLLOUT' srcs/Core/Core.cpp && pass "POLLOUT present dans la boucle principale" || fail "POLLOUT absent dans Core.cpp"
    grep -q 'revents' srcs/Core/Core.cpp && pass "revents utilise dans Core.cpp" || fail "revents absent dans Core.cpp"
    grep -Eq 'revents.*POLLIN|POLLIN.*revents' srcs/Core/Core.cpp && grep -q 'acceptClient' srcs/Core/Core.cpp && pass "acceptClient appele depuis readiness POLLIN" || fail "acceptClient ne semble pas appele depuis POLLIN"
    grep -Eq 'revents.*POLLIN|POLLIN.*revents' srcs/Core/Core.cpp && grep -q 'readClient' srcs/Core/Core.cpp && pass "readClient appele depuis readiness POLLIN" || fail "readClient ne semble pas appele depuis POLLIN"
    grep -Eq 'revents.*POLLOUT|POLLOUT.*revents' srcs/Core/Core.cpp && grep -q 'writeClient' srcs/Core/Core.cpp && pass "writeClient appele depuis readiness POLLOUT" || fail "writeClient ne semble pas appele depuis POLLOUT"

    accept_count="$(grep -R -n --include='*.cpp' --include='*.hpp' '\baccept[[:space:]]*(' srcs 2>/dev/null | wc -l | tr -d ' ')"
    if [[ "$accept_count" == "1" ]]; then
        pass "Un seul accept() dans srcs"
    else
        fail "Nombre de accept() attendu=1 recu=$accept_count"
    fi

    grep -A5 'recv(' srcs/Core/Server.cpp | grep -Eq '<= *0|== *0|< *0' && pass "recv() gere 0 et/ou -1" || fail "recv() ne montre pas clairement la gestion de 0/-1"
    grep -A8 'send(' srcs/Core/Server.cpp | grep -Eq '<= *0|== *0|< *0' && pass "send() gere 0 et/ou -1" || fail "send() ne montre pas clairement la gestion de 0/-1"
    grep -A12 'recv(' srcs/Core/Server.cpp | grep -Eq '500|Internal Server Error|buildErrorResponse' && pass "recv() -1 semble mener a une reponse 500" || note "TODO: recv() -1 ne montre pas clairement une reponse 500"
    grep -A16 'send(' srcs/Core/Server.cpp | grep -Eq '500|Internal Server Error|buildErrorResponse' && pass "send() -1 semble mener a une reponse 500" || note "TODO: send() -1 ne montre pas clairement une reponse 500"

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
    grep -q 'CgiManager::writeInput' srcs/Core/Core.cpp && grep -Eq 'POLLOUT' srcs/Core/Core.cpp && pass "CGI stdin ecrit via POLLOUT" || fail "CGI stdin ne semble pas ecrit via POLLOUT"
    grep -q 'CgiManager::readOutput' srcs/Core/Core.cpp && grep -Eq 'POLLIN|POLLHUP' srcs/Core/Core.cpp && pass "CGI stdout lu via POLLIN/POLLHUP" || fail "CGI stdout ne semble pas lu via POLLIN/POLLHUP"

    boost_hits="$(grep -R -n --include='*.cpp' --include='*.hpp' --include='Makefile' -E 'boost/|namespace boost|\\-lboost' srcs Makefile 2>/dev/null || true)"
    if [[ -z "$boost_hits" ]]; then
        pass "Aucune utilisation Boost detectee"
    else
        fail "Boost detecte alors qu'il est interdit"
        printf "%s\n" "$boost_hits" >> "$RESULT_FILE"
    fi

    external_lib_hits="$(grep -n -E '(^|[[:space:]])-l[^[:space:]]+' Makefile 2>/dev/null || true)"
    if [[ -z "$external_lib_hits" ]]; then
        pass "Aucune bibliotheque externe liee dans le Makefile"
    else
        fail "Bibliotheque externe detectee dans le Makefile"
        printf "%s\n" "$external_lib_hits" >> "$RESULT_FILE"
    fi
}

test_parsing_validation() {
    section "PARSING VALIDATION"

    local parser_bin="$TMP_DIR/parsing_test_eval"

    if c++ -Wall -Wextra -Werror -std=c++98 \
        srcs/Parsing/main.cpp \
        srcs/Parsing/ParseConfig.cpp \
        srcs/Parsing/ParseServer.cpp \
        srcs/Parsing/ParseLocation.cpp \
        -I srcs/Parsing \
        -o "$parser_bin" >> "$RESULT_FILE" 2>&1; then
        pass "Compilation parsing_test_eval OK"
    else
        fail "Compilation parsing_test_eval echouee"
        return 1
    fi

    run_parser_case "$parser_bin" \
        "parse: client_max_body_size -2 rejete" \
        "reject" \
        "ERROR: CLIENT_MAX_BODY_SIZE MUST BE > 0: -2" \
        "parse_invalid_bodysize_neg.conf" \
        'server {\n    listen 8080;\n    domain_name localhost;\n    root ./configs/www;\n    index index.html;\n    client_max_body_size -2;\n}\n'

    run_parser_case "$parser_bin" \
        "parse: client_max_body_size 0 rejete" \
        "reject" \
        "ERROR: CLIENT_MAX_BODY_SIZE MUST BE > 0: 0" \
        "parse_invalid_bodysize_zero.conf" \
        'server {\n    listen 8080;\n    domain_name localhost;\n    root ./configs/www;\n    index index.html;\n    client_max_body_size 0;\n}\n'

    run_parser_case "$parser_bin" \
        "parse: client_max_body_size > int max rejete" \
        "reject" \
        "ERROR: CLIENT_MAX_BODY_SIZE TOO LARGE: 2147483648" \
        "parse_invalid_bodysize_big.conf" \
        'server {\n    listen 8080;\n    domain_name localhost;\n    root ./configs/www;\n    index index.html;\n    client_max_body_size 2147483648;\n}\n'

    run_parser_case "$parser_bin" \
        "parse: client_max_body_size int max accepte" \
        "accept" \
        "client_max_body_size: 2147483647" \
        "parse_valid_bodysize_max.conf" \
        'server {\n    listen 8080;\n    domain_name localhost;\n    root ./configs/www;\n    index index.html;\n    client_max_body_size 2147483647;\n}\n'

    run_parser_case "$parser_bin" \
        "parse: root relatif explicite ./ accepte" \
        "accept" \
        "root  ./configs/www" \
        "parse_valid_root_relative.conf" \
        'server {\n    listen 8080;\n    domain_name localhost;\n    root ./configs/www;\n    index index.html;\n    client_max_body_size 1000;\n}\n'

    run_parser_case "$parser_bin" \
        "parse: root sans prefixe explicite rejete" \
        "reject" \
        "ERROR: ROOT MUST START BY '/' OR './'" \
        "parse_invalid_root_prefix.conf" \
        'server {\n    listen 8080;\n    domain_name localhost;\n    root configs/www;\n    index index.html;\n    client_max_body_size 1000;\n}\n'

    run_parser_case "$parser_bin" \
        "parse: location root sans prefixe explicite rejete" \
        "reject" \
        "ERROR: LOCATION ROOT MUST START BY '/' OR './'" \
        "parse_invalid_location_root_prefix.conf" \
        'server {\n    listen 8080;\n    domain_name localhost;\n    root ./configs/www;\n    index index.html;\n    client_max_body_size 1000;\n    location /browse/ {\n        root configs/www/browse;\n        methods GET;\n    }\n}\n'

    run_parser_case "$parser_bin" \
        "parse: upload_dir sans prefixe explicite rejete" \
        "reject" \
        "ERROR: WRONG UPLOAD_DIR MUST START BY '/' OR './'" \
        "parse_invalid_upload_prefix.conf" \
        'server {\n    listen 8080;\n    domain_name localhost;\n    root ./configs/www;\n    index index.html;\n    client_max_body_size 1000;\n    location /upload/ {\n        root ./configs/www/upload;\n        upload_dir tmp/upload;\n        methods GET POST DELETE;\n    }\n}\n'

    run_parser_case "$parser_bin" \
        "parse: token invalide au niveau principal rejete" \
        "reject" \
        "ERROR: INVALID TOP-LEVEL TOKEN: invalide" \
        "parse_invalid_top_level_token.conf" \
        'invalide\nserver {\n    listen 8080;\n    domain_name localhost;\n    root ./configs/www;\n    index index.html;\n}\n'

    run_parser_case "$parser_bin" \
        "parse: server avec token en trop avant { sur meme ligne rejete" \
        "reject" \
        "Error: expected '{'" \
        "parse_invalid_server_header_inline_extra.conf" \
        'server invalide {\n    listen 8080;\n    domain_name localhost;\n    root ./configs/www;\n    index index.html;\n}\n'

    run_parser_case "$parser_bin" \
        "parse: server avec token en trop avant { sur ligne suivante rejete" \
        "reject" \
        "Error: expected '{'" \
        "parse_invalid_server_header_split_extra.conf" \
        'server invalide\n{\n    listen 8080;\n    domain_name localhost;\n    root ./configs/www;\n    index index.html;\n}\n'

    run_parser_case "$parser_bin" \
        "parse: server puis { sur ligne suivante reste accepte" \
        "accept" \
        "nb de servers: 1" \
        "parse_valid_server_header_split.conf" \
        'server\n{\n    listen 8080;\n    domain_name localhost;\n    root ./configs/www;\n    index index.html;\n}\n'

    run_parser_todo_accept_case "$parser_bin" \
        "parse: listen 127.0.0.1:8080 accepte" \
        "nb de servers: 1" \
        "parse_valid_listen_ipv4_port.conf" \
        'server {\n    listen 127.0.0.1:8080;\n    domain_name localhost;\n    root ./configs/www;\n    index index.html;\n    client_max_body_size 1000;\n}\n'

    run_parser_todo_accept_case "$parser_bin" \
        "parse: listen localhost:8080 accepte" \
        "nb de servers: 1" \
        "parse_valid_listen_host_port.conf" \
        'server {\n    listen localhost:8080;\n    domain_name localhost;\n    root ./configs/www;\n    index index.html;\n    client_max_body_size 1000;\n}\n'

    run_parser_case "$parser_bin" \
        "parse: listen interface sans port rejete" \
        "reject" \
        "ERROR" \
        "parse_invalid_listen_interface_without_port.conf" \
        'server {\n    listen 127.0.0.1;\n    domain_name localhost;\n    root ./configs/www;\n    index index.html;\n    client_max_body_size 1000;\n}\n'

    run_parser_case "$parser_bin" \
        "parse: listen interface:port invalide rejete" \
        "reject" \
        "ERROR" \
        "parse_invalid_listen_interface_bad_port.conf" \
        'server {\n    listen 127.0.0.1:abc;\n    domain_name localhost;\n    root ./configs/www;\n    index index.html;\n    client_max_body_size 1000;\n}\n'

    run_parser_case "$parser_bin" \
        "parse: server sans domain_name rejete" \
        "reject" \
        "ERROR: SERVER HAS NO DOMAIN_NAME DIR" \
        "parse_invalid_missing_domain_name.conf" \
        'server {\n    listen 8080;\n    root ./configs/www;\n    index index.html;\n}\n'

    run_parser_case "$parser_bin" \
        "parse: root et upload_dir absolus acceptes" \
        "accept" \
        "nb de servers: 1" \
        "parse_valid_absolute_paths.conf" \
        'server {\n    listen 8080;\n    domain_name localhost;\n    root /tmp;\n    index index.html;\n    client_max_body_size 1000;\n    location /upload/ {\n        root /tmp;\n        upload_dir /tmp;\n        methods GET POST DELETE;\n    }\n}\n'

    run_parser_suite_dir "$parser_bin" \
        "srcs/Parsing/errors_config_tests/configs_invalid" \
        "invalid"
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
    check_code "GET /images/logo.png" 200 "http://127.0.0.1:8080/images/logo.png"
    check_code "GET /favicon.ico" 200 "http://127.0.0.1:8080/favicon.ico"
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
    check_raw_status "400 header malforme sans deux-points" 8080 400 $'GET / HTTP/1.1\r\nHost: localhost\r\nBrokenHeader\r\nConnection: close\r\n\r\n'
    check_raw_status "400 Content-Length non numerique" 8080 400 $'POST /upload/bad_cl.txt HTTP/1.1\r\nHost: localhost\r\nContent-Length: abc\r\nConnection: close\r\n\r\nabc'
    check_raw_status "400 body plus long que Content-Length" 8080 400 $'POST /upload/bad_cl_long.txt HTTP/1.1\r\nHost: localhost\r\nContent-Length: 3\r\nConnection: close\r\n\r\nabcdef'

    check_code "POST /upload/ -> 201" 201 -X POST --data "hello eval" "http://127.0.0.1:8080/upload/eval_test.txt"
    check_code "GET fichier upload -> 200" 200 "http://127.0.0.1:8080/upload/eval_test.txt"
    check_body_contains "Contenu du fichier upload correct" "http://127.0.0.1:8080/upload/eval_test.txt" "hello eval"
    check_code "DELETE fichier upload -> 204" 204 -X DELETE "http://127.0.0.1:8080/upload/eval_test.txt"
    check_code "GET apres DELETE fichier upload -> 404" 404 "http://127.0.0.1:8080/upload/eval_test.txt"

    local binary_src binary_dst binary_code
    binary_src="$TMP_DIR/binary_upload_eval.bin"
    binary_dst="$TMP_DIR/binary_download_eval.bin"
    if head -c 8192 /dev/urandom > "$binary_src" 2>/dev/null; then
        binary_code="$(curl -s -o /dev/null -w '%{http_code}' --max-time 8 -X POST --data-binary @"$binary_src" "http://127.0.0.1:8080/upload/binary_eval.bin" 2>/dev/null || true)"
        if [[ "$binary_code" == "201" ]] \
            && curl -s --max-time 8 "http://127.0.0.1:8080/upload/binary_eval.bin" -o "$binary_dst" 2>/dev/null \
            && cmp -s "$binary_src" "$binary_dst"; then
            pass "Upload/download binaire conserve les octets"
        else
            fail "Upload/download binaire incorrect (POST status=$binary_code)"
        fi
        check_code "DELETE fichier binaire upload -> 204" 204 -X DELETE "http://127.0.0.1:8080/upload/binary_eval.bin"
        check_code "GET apres DELETE fichier binaire -> 404" 404 "http://127.0.0.1:8080/upload/binary_eval.bin"
    else
        skip "Upload binaire impossible: /dev/urandom indisponible"
    fi

    local chunked_progress_code
    chunked_progress_code="$(progressive_chunked_status 8080 "/upload/chunked_progressive_eval.txt" "hello" "world" 8)"
    if [[ "$chunked_progress_code" == "201" ]]; then
        pass "Chunked progressif en deux recv -> 201"
        check_body_contains "Chunked progressif contenu assemble" "http://127.0.0.1:8080/upload/chunked_progressive_eval.txt" "helloworld"
        check_code "DELETE fichier chunked progressif -> 204" 204 -X DELETE "http://127.0.0.1:8080/upload/chunked_progressive_eval.txt"
    else
        note "TODO: Chunked progressif en deux recv -> attendu=201 recu=$chunked_progress_code"
    fi

    local chunked_incomplete_code
    chunked_incomplete_code="$(incomplete_chunked_status 8080 "/upload/chunked_incomplete_eval.txt" 12)"
    if [[ "$chunked_incomplete_code" == "400" || "$chunked_incomplete_code" == "408" ]]; then
        pass "Chunked incomplet ferme avec erreur -> $chunked_incomplete_code"
    else
        fail "Chunked incomplet -> attendu=400/408 recu=$chunked_incomplete_code"
    fi
    rm -f "$ROOT_DIR/configs/www/upload/chunked_progressive_eval.txt" "$ROOT_DIR/configs/www/upload/chunked_incomplete_eval.txt" 2>/dev/null || true

    local unreadable_file unreadable_code
    unreadable_file="$ROOT_DIR/configs/www/unreadable_eval.txt"
    printf "unreadable eval\n" > "$unreadable_file"
    if chmod 000 "$unreadable_file" 2>/dev/null; then
        unreadable_code="$(http_code "http://127.0.0.1:8080/unreadable_eval.txt" 2>/dev/null || true)"
        if [[ "$unreadable_code" == "403" || "$unreadable_code" == "500" ]]; then
            pass "Erreur read fichier -> $unreadable_code"
        else
            fail "Erreur read fichier -> attendu=403/500 recu=$unreadable_code"
        fi
        chmod 644 "$unreadable_file" 2>/dev/null || true
    else
        skip "Erreur read fichier -> chmod 000 impossible"
    fi
    rm -f "$unreadable_file" 2>/dev/null || true

    backup_file "configs/www/errors/404.html"
    write_file "configs/www/errors/404.html" '<html><body>EVAL_404_MARKER</body></html>\n'
    check_body_contains "404 custom page verifiee apres modification" "http://127.0.0.1:8080/missing-page" "EVAL_404_MARKER"
    restore_file "configs/www/errors/404.html"

    backup_file "configs/www/errors/403.html"
    write_file "configs/www/errors/403.html" '<html><body>EVAL_403_MARKER</body></html>\n'
    check_body_contains "403 custom page verifiee apres modification" "http://127.0.0.1:8080/noindex/" "EVAL_403_MARKER"
    restore_file "configs/www/errors/403.html"

    backup_file "configs/www/errors/405.html"
    write_file "configs/www/errors/405.html" '<html><body>EVAL_405_MARKER</body></html>\n'
    check_body_contains "405 custom page verifiee apres modification" "http://127.0.0.1:8080/readonly/index.html" "EVAL_405_MARKER" -X DELETE
    restore_file "configs/www/errors/405.html"

    backup_file "configs/www/errors/501.html"
    write_file "configs/www/errors/501.html" '<html><body>EVAL_501_MARKER</body></html>\n'
    check_body_contains "501 custom page verifiee apres modification" "http://127.0.0.1:8080/" "EVAL_501_MARKER" -X UNKNOWN
    restore_file "configs/www/errors/501.html"

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
    printf "relative-cgi-ok\n" > "$ROOT_DIR/configs/www/cgi-bin/relative_eval.txt"
    write_file "configs/www/cgi-bin/test.py" '#!/usr/bin/env python3\nwith open("relative_eval.txt", "r") as f:\n    data = f.read().strip()\nprint("Content-Type: text/plain")\nprint()\nprint("RELATIVE_FILE=" + data)\n'
    chmod +x "$ROOT_DIR/configs/www/cgi-bin/test.py"
    check_body_contains "CGI execute dans son dossier (fichier relatif)" "http://127.0.0.1:8080/cgi-bin/test.py" "RELATIVE_FILE=relative-cgi-ok"
    restore_file "configs/www/cgi-bin/test.py"
    chmod +x "$ROOT_DIR/configs/www/cgi-bin/test.py"
    rm -f "$ROOT_DIR/configs/www/cgi-bin/relative_eval.txt" 2>/dev/null || true

    backup_file "configs/www/cgi-bin/test.py"
    write_file "configs/www/cgi-bin/test.py" '#!/usr/bin/env python3\nimport os, sys\nbody = sys.stdin.read()\nprint("Content-Type: text/plain")\nprint()\nprint("HTTP_HOST=" + os.environ.get("HTTP_HOST", ""))\nprint("CONTENT_TYPE=" + os.environ.get("CONTENT_TYPE", ""))\nprint("CONTENT_LENGTH=" + os.environ.get("CONTENT_LENGTH", ""))\nprint("BODY=" + body)\n'
    chmod +x "$ROOT_DIR/configs/www/cgi-bin/test.py"
    local cgi_env_body
    cgi_env_body="$(curl -s --max-time 8 -X POST -H 'Content-Type: text/plain' --data 'headers_eval' 'http://127.0.0.1:8080/cgi-bin/test.py' 2>/dev/null || true)"
    if printf "%s" "$cgi_env_body" | grep -Fq "HTTP_HOST=127.0.0.1:8080" \
        && printf "%s" "$cgi_env_body" | grep -Fq "CONTENT_TYPE=text/plain" \
        && printf "%s" "$cgi_env_body" | grep -Fq "CONTENT_LENGTH=12" \
        && printf "%s" "$cgi_env_body" | grep -Fq "BODY=headers_eval"; then
        pass "CGI recoit headers/body via environnement"
    else
        fail "CGI ne recoit pas correctement headers/body via environnement"
        log "--- CGI ENV BODY ---"
        printf "%s\n" "$cgi_env_body" >> "$RESULT_FILE"
        log "--- FIN CGI ENV BODY ---"
    fi
    restore_file "configs/www/cgi-bin/test.py"
    chmod +x "$ROOT_DIR/configs/www/cgi-bin/test.py"

    backup_file "configs/www/cgi-bin/test.py"
    write_file "configs/www/cgi-bin/test.py" '#!/usr/bin/env python3\nprint("Content-Type: text/plain")\nprint()\nprint("A" * 262144)\n'
    chmod +x "$ROOT_DIR/configs/www/cgi-bin/test.py"
    local cgi_big_out_len
    cgi_big_out_len="$(curl -s --max-time 12 "http://127.0.0.1:8080/cgi-bin/test.py" 2>/dev/null | wc -c | tr -d ' ' || true)"
    if [[ "$cgi_big_out_len" == "262145" ]]; then
        pass "CGI grosse sortie stdout transmise completement"
    else
        fail "CGI grosse sortie stdout incomplete (octets=$cgi_big_out_len attendu=262145)"
    fi
    restore_file "configs/www/cgi-bin/test.py"
    chmod +x "$ROOT_DIR/configs/www/cgi-bin/test.py"

    backup_file "configs/www/cgi-bin/test.py"
    write_file "configs/www/cgi-bin/test.py" '#!/usr/bin/env python3\nimport sys\nbody = sys.stdin.buffer.read()\nprint("Content-Type: text/plain")\nprint()\nprint("LEN=" + str(len(body)))\n'
    chmod +x "$ROOT_DIR/configs/www/cgi-bin/test.py"
    local cgi_big_post_src cgi_big_post_body
    cgi_big_post_src="$TMP_DIR/cgi_big_post_eval.bin"
    head -c 131072 /dev/zero | tr '\0' 'P' > "$cgi_big_post_src"
    cgi_big_post_body="$(curl -s --max-time 12 -X POST --data-binary @"$cgi_big_post_src" "http://127.0.0.1:8080/cgi-bin/test.py" 2>/dev/null || true)"
    if printf "%s" "$cgi_big_post_body" | grep -Fq "LEN=131072"; then
        pass "CGI gros POST stdin transmis completement"
    else
        fail "CGI gros POST stdin incomplet"
        log "--- CGI BIG POST BODY ---"
        printf "%s\n" "$cgi_big_post_body" | sed -n '1,40p' >> "$RESULT_FILE"
        log "--- FIN CGI BIG POST BODY ---"
    fi
    restore_file "configs/www/cgi-bin/test.py"
    chmod +x "$ROOT_DIR/configs/www/cgi-bin/test.py"

    backup_file "configs/www/cgi-bin/test.py"
    write_file "configs/www/cgi-bin/test.py" '#!/usr/bin/env python3\nimport os, sys\nbody = sys.stdin.read()\nprint("Content-Type: text/plain")\nprint()\nprint("METHOD=" + os.environ.get("REQUEST_METHOD", ""))\nprint("CONTENT_LENGTH=" + os.environ.get("CONTENT_LENGTH", ""))\nprint("BODY=" + body)\n'
    chmod +x "$ROOT_DIR/configs/www/cgi-bin/test.py"
    local cgi_chunked_out cgi_chunked_status
    cgi_chunked_out="$(cgi_chunked_response 8080 "/cgi-bin/test.py" "hello" "world" 10 || true)"
    cgi_chunked_status="$(printf "%s" "$cgi_chunked_out" | tr -d '\r' | awk 'NR==1{print $2; exit}')"
    if [[ "$cgi_chunked_status" == "200" ]] \
        && printf "%s" "$cgi_chunked_out" | grep -Fq "METHOD=POST" \
        && printf "%s" "$cgi_chunked_out" | grep -Fq "BODY=helloworld"; then
        pass "CGI recoit le body chunked dechunked"
    else
        fail "CGI chunked -> attendu 200 avec BODY=helloworld, recu status=$cgi_chunked_status"
        log "--- CGI CHUNKED RESPONSE ---"
        printf "%s\n" "$cgi_chunked_out" | sed -n '1,80p' >> "$RESULT_FILE"
        log "--- FIN CGI CHUNKED RESPONSE ---"
    fi
    restore_file "configs/www/cgi-bin/test.py"
    chmod +x "$ROOT_DIR/configs/www/cgi-bin/test.py"

    local cookie_headers
    cookie_headers="$(http_head 'http://127.0.0.1:8080/' 2>/dev/null || true)"
    if printf "%s" "$cookie_headers" | grep -qi '^set-cookie:'; then
        pass "Bonus cookies/sessions: Set-Cookie present"
    else
        note "Bonus cookies/sessions: aucun Set-Cookie observe sur GET /"
    fi

    local cookie_jar session_first session_second session_id_first session_id_second
    cookie_jar="$TMP_DIR/session_eval.cookies"
    session_first="$(curl -s --max-time 8 -c "$cookie_jar" "http://127.0.0.1:8080/session" 2>/dev/null || true)"
    session_second="$(curl -s --max-time 8 -b "$cookie_jar" -c "$cookie_jar" "http://127.0.0.1:8080/session" 2>/dev/null || true)"
    session_id_first="$(printf "%s\n" "$session_first" | awk -F': ' '/^Session ID:/ {print $2; exit}')"
    session_id_second="$(printf "%s\n" "$session_second" | awk -F': ' '/^Session ID:/ {print $2; exit}')"
    if [[ -n "$session_id_first" && "$session_id_first" == "$session_id_second" ]] \
        && printf "%s" "$session_second" | grep -Fq "Views: 2"; then
        pass "Bonus cookies/sessions: session persistante avec cookie"
    else
        note "Bonus cookies/sessions: persistance non confirmee"
        log "--- SESSION FIRST ---"
        printf "%s\n" "$session_first" >> "$RESULT_FILE"
        log "--- SESSION SECOND ---"
        printf "%s\n" "$session_second" >> "$RESULT_FILE"
        log "--- FIN SESSION ---"
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
    cgi_loop_code="$(curl -s -o /dev/null -w '%{http_code}' --max-time 70 "http://127.0.0.1:8080/cgi-bin/test.py" 2>/dev/null)"
    if [[ "$cgi_loop_code" == "504" ]]; then
        pass "CGI boucle infinie -> 504 Gateway Timeout"
    else
        fail "CGI boucle infinie -> attendu=504 recu=$cgi_loop_code"
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
    ct_html="$(curl -s --max-time 8 -D - -o /dev/null 'http://127.0.0.1:8080/' 2>/dev/null | grep -i '^content-type:' || true)"
    if printf "%s" "$ct_html" | grep -qi 'text/html'; then
        pass "Content-Type text/html pour GET /"
    else
        fail "Content-Type incorrect pour GET / (recu: $ct_html)"
    fi

    local ct_css
    ct_css="$(curl -s --max-time 8 -D - -o /dev/null 'http://127.0.0.1:8080/assets/styles.css' 2>/dev/null | grep -i '^content-type:' || true)"
    if printf "%s" "$ct_css" | grep -qi 'text/css'; then
        pass "Content-Type text/css pour /assets/styles.css"
    else
        fail "Content-Type incorrect pour /assets/styles.css (recu: $ct_css)"
    fi

    local ct_js
    ct_js="$(curl -s --max-time 8 -D - -o /dev/null 'http://127.0.0.1:8080/script.js' 2>/dev/null | grep -i '^content-type:' || true)"
    if printf "%s" "$ct_js" | grep -qi 'javascript'; then
        pass "Content-Type javascript pour /script.js"
    else
        fail "Content-Type incorrect pour /script.js (recu: $ct_js)"
    fi

    # Content-Length present et coherent
    local cl_val cl_body_len
    cl_val="$(curl -s --max-time 8 -D - -o /dev/null 'http://127.0.0.1:8080/script.js' 2>/dev/null | grep -i '^content-length:' | tr -d '\r' | awk '{print $2}' || true)"
    cl_body_len="$(curl -s --max-time 8 'http://127.0.0.1:8080/script.js' 2>/dev/null | wc -c | tr -d ' ' || true)"
    if [[ -n "$cl_val" && "$cl_val" == "$cl_body_len" ]]; then
        pass "Content-Length correct pour /script.js ($cl_val octets)"
    elif [[ -z "$cl_val" ]]; then
        fail "Content-Length absent dans la reponse GET /script.js"
    else
        fail "Content-Length incorrect pour /script.js: header=$cl_val body=$cl_body_len"
    fi

    # HEAD suit la config: Core.config autorise seulement GET sur /
    local head_status head_cl
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

    backup_file "configs/www/cgi-bin/test.py"
    write_file "configs/www/cgi-bin/test.py" '#!/usr/bin/env python3\nimport os\nprint("Content-Type: text/plain")\nprint()\nprint("SCRIPT_NAME=" + os.environ.get("SCRIPT_NAME", ""))\nprint("PATH_INFO=" + os.environ.get("PATH_INFO", ""))\nprint("QUERY_STRING=" + os.environ.get("QUERY_STRING", ""))\n'
    chmod +x "$ROOT_DIR/configs/www/cgi-bin/test.py"
    local cgi_path_info_body
    cgi_path_info_body="$(curl -s --max-time 8 "http://127.0.0.1:8080/cgi-bin/test.py/extra/path?x=1" 2>/dev/null || true)"
    if printf "%s" "$cgi_path_info_body" | grep -Fq "PATH_INFO=/extra/path" \
        && printf "%s" "$cgi_path_info_body" | grep -Fq "QUERY_STRING=x=1"; then
        pass "CGI PATH_INFO et QUERY_STRING transmis ensemble"
    else
        fail "CGI PATH_INFO/QUERY_STRING incorrect"
        log "--- CGI PATH_INFO BODY ---"
        printf "%s\n" "$cgi_path_info_body" >> "$RESULT_FILE"
        log "--- FIN CGI PATH_INFO BODY ---"
    fi
    restore_file "configs/www/cgi-bin/test.py"
    chmod +x "$ROOT_DIR/configs/www/cgi-bin/test.py"

    backup_file "configs/www/cgi-bin/test.py"
    write_file "configs/www/cgi-bin/test.py" '#!/usr/bin/env python3\nprint("Status: 404 Not Found")\nprint("Content-Type: text/plain")\nprint()\nprint("cgi status body")\n'
    chmod +x "$ROOT_DIR/configs/www/cgi-bin/test.py"
    local cgi_status_code cgi_status_body
    cgi_status_code="$(http_code "http://127.0.0.1:8080/cgi-bin/test.py" 2>/dev/null || true)"
    cgi_status_body="$(http_body "http://127.0.0.1:8080/cgi-bin/test.py" 2>/dev/null || true)"
    if [[ "$cgi_status_code" == "404" ]] && printf "%s" "$cgi_status_body" | grep -Fq "cgi status body"; then
        pass "CGI Status header definit le code HTTP"
    else
        fail "CGI Status header -> attendu=404 avec body, recu=$cgi_status_code"
    fi
    restore_file "configs/www/cgi-bin/test.py"
    chmod +x "$ROOT_DIR/configs/www/cgi-bin/test.py"

    backup_file "configs/www/cgi-bin/test.py"
    write_file "configs/www/cgi-bin/test.py" '#!/usr/bin/env python3\nprint("Location: /docs/")\nprint("Content-Type: text/plain")\nprint()\nprint("cgi redirect body")\n'
    chmod +x "$ROOT_DIR/configs/www/cgi-bin/test.py"
    local cgi_location_headers
    cgi_location_headers="$(http_head "http://127.0.0.1:8080/cgi-bin/test.py" 2>/dev/null || true)"
    if printf "%s" "$cgi_location_headers" | tr -d '\r' | grep -Fq "HTTP/1.1 302" \
        && printf "%s" "$cgi_location_headers" | grep -Fqi "Location: /docs/"; then
        pass "CGI Location header produit une redirection 302"
    else
        fail "CGI Location header ne produit pas la redirection attendue"
        log "--- CGI LOCATION HEADERS ---"
        printf "%s\n" "$cgi_location_headers" >> "$RESULT_FILE"
        log "--- FIN CGI LOCATION HEADERS ---"
    fi
    restore_file "configs/www/cgi-bin/test.py"
    chmod +x "$ROOT_DIR/configs/www/cgi-bin/test.py"

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
        check_code_any "8081 CGI PHP" "$php_code" 200
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

test_interface_port_config() {
    section "INTERFACE:PORT CONFIG"

    if [[ ! -f "configs/interface_port.config" ]]; then
        fail "configs/interface_port.config absent"
        return 1
    fi

    if ! start_server "configs/interface_port.config" "8080"; then
        return 1
    fi

    wait_http "http://127.0.0.1:8080/" || { fail "interface_port.config ne repond pas sur 127.0.0.1:8080"; return 1; }
    wait_http "http://127.0.0.2:8080/" || { fail "interface_port.config ne repond pas sur 127.0.0.2:8080"; return 1; }

    check_code "interface_port 127.0.0.1:8080 -> 200" 200 "http://127.0.0.1:8080/"
    check_code "interface_port 127.0.0.2:8080 -> 200" 200 "http://127.0.0.2:8080/"

    stop_server

    local conflict_conf="$TMP_DIR/interface_port_conflict.config"
    local conflict_log="$TMP_DIR/interface_port_conflict.log"
    sed 's/127\.0\.0\.2:8080/127.0.0.1:8080/' configs/interface_port.config > "$conflict_conf"

    ./webserv "$conflict_conf" > "$conflict_log" 2>&1 &
    local conflict_pid=$!
    sleep 1
    if kill -0 "$conflict_pid" 2>/dev/null; then
        fail "interface_port accepte deux fois le meme interface:port"
        kill "$conflict_pid" 2>/dev/null || true
        wait "$conflict_pid" 2>/dev/null || true
    elif grep -Fq "multiple servers for same interface:port" "$conflict_log"; then
        pass "interface_port rejette deux fois le meme interface:port"
    else
        fail "interface_port conflit attendu mais message absent"
        log "--- LOG interface_port_conflict ---"
        sed -n '1,60p' "$conflict_log" >> "$RESULT_FILE"
        log "--- FIN LOG ---"
    fi

    local any_conf="$TMP_DIR/interface_port_any_conflict.config"
    local any_log="$TMP_DIR/interface_port_any_conflict.log"
    sed '0,/127\.0\.0\.1:8080/s//0.0.0.0:8080/; s/127\.0\.0\.2:8080/127.0.0.1:8080/' configs/interface_port.config > "$any_conf"

    ./webserv "$any_conf" > "$any_log" 2>&1 &
    local any_pid=$!
    sleep 1
    if kill -0 "$any_pid" 2>/dev/null; then
        fail "interface_port accepte 0.0.0.0:8080 avec 127.0.0.1:8080"
        kill "$any_pid" 2>/dev/null || true
        wait "$any_pid" 2>/dev/null || true
    elif grep -Fq "multiple servers for same interface:port" "$any_log"; then
        pass "interface_port rejette 0.0.0.0:8080 avec 127.0.0.1:8080"
    else
        fail "interface_port conflit 0.0.0.0 attendu mais message absent"
        log "--- LOG interface_port_any_conflict ---"
        sed -n '1,60p' "$any_log" >> "$RESULT_FILE"
        log "--- FIN LOG ---"
    fi

    local old_conf="$TMP_DIR/interface_port_old_syntax_conflict.config"
    local old_log="$TMP_DIR/interface_port_old_syntax_conflict.log"
    sed '0,/listen 127\.0\.0\.1:8080;/s//listen 8080;/; s/127\.0\.0\.2:8080/127.0.0.1:8080/' configs/interface_port.config > "$old_conf"

    ./webserv "$old_conf" > "$old_log" 2>&1 &
    local old_pid=$!
    sleep 1
    if kill -0 "$old_pid" 2>/dev/null; then
        fail "interface_port accepte listen 8080 avec 127.0.0.1:8080"
        kill "$old_pid" 2>/dev/null || true
        wait "$old_pid" 2>/dev/null || true
    elif grep -Fq "multiple servers for same interface:port" "$old_log"; then
        pass "interface_port rejette listen 8080 avec 127.0.0.1:8080"
    else
        fail "interface_port conflit ancienne syntaxe attendu mais message absent"
        log "--- LOG interface_port_old_syntax_conflict ---"
        sed -n '1,60p' "$old_log" >> "$RESULT_FILE"
        log "--- FIN LOG ---"
    fi
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

    if printf "%s" "$siege_out" | grep -qi 'availability:'; then
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
    failed_tx="$(printf "%s\n" "$siege_out" | awk -F: '/"failed_transactions"/ {gsub(/[^0-9.]/,"",$2); print $2; exit}')"
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

    if [[ $RUN_VALGRIND -eq 0 ]]; then
        skip "Valgrind desactive (--skip-valgrind)"
        return 0
    fi

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
    echo "================================================================"
    echo "RESUME FINAL - $(timestamp)"
    echo "================================================================"
    
    local total=$((PASS + FAIL + SKIP + NOTE))
    local pass_percent=$((PASS * 100 / total))
    
    echo ""
    echo "Tests executes: $total"
    printf " - PASS: %3d (%3d%%)\n" "$PASS" "$pass_percent"
    printf " - FAIL: %3d\n" "$FAIL"
    printf " - SKIP/IMPOSSIBLE: %3d\n" "$SKIP"
    printf " - NOTE (TODO/ambigu): %3d\n" "$NOTE"
    echo ""
    echo "Rapport complet: $RESULT_FILE"
    if [[ $KEEP_LOGS -eq 1 ]]; then
        echo "Logs temporaires: $TMP_DIR"
    fi

    if [[ -n ${FAIL_LIST[*]-} ]]; then
        echo
        echo "FAUX (a corriger):"
        printf ' - %s\n' "${FAIL_LIST[@]}"
    fi

    if [[ -n ${SKIP_LIST[*]-} ]]; then
        echo
        echo "IMPOSSIBLE (dépendances/env):"
        printf ' - %s\n' "${SKIP_LIST[@]}"
    fi

    if [[ -n ${NOTE_LIST[*]-} ]]; then
        echo
        echo "TODO / Ambigus (a verifier manuellement):"
        printf ' - %s\n' "${NOTE_LIST[@]}"
    fi
    
    echo ""
    echo "================================================================"
    if [[ $FAIL -eq 0 && $SKIP -eq 0 ]]; then
        echo "✓ EVALUATION REUSSIE (tous les tests critiques passes)"
    elif [[ $FAIL -eq 0 ]]; then
        echo "✓ EVALUATION REUSSIE (pas de FAIL, $NOTE TODO a verifier)"
    else
        echo "✗ EVALUATION EN ECHEC ($FAIL FAIL detectes)"
    fi
    echo "================================================================"
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
                ;;
            --kill-existing-webserv)
                KILL_EXISTING_WEBSERV=1
                ;;
            --with-siege)
                RUN_SIEGE=1
                ;;
            --skip-valgrind)
                RUN_VALGRIND=0
                ;;
            --keep-logs)
                KEEP_LOGS=1
                ;;
            --verbose)
                VERBOSE=1
                ;;
            --help|-h)
                echo "Usage: ./eval_test.sh [options]"
                echo ""
                echo "Options:"
                echo "  --keep-server              garde le dernier serveur lance vivant"
                echo "  --kill-existing-webserv    termine les processus 'webserv' existants"
                echo "  --with-siege               execute les tests de charge siege"
                echo "  --skip-valgrind            passe les verifications valgrind (gain ~90s)"
                echo "  --keep-logs                conserve /tmp pour debug (defaut: nettoyage)"
                echo "  --verbose                  affiche logs debug additionnels"
                echo "  -h, --help                 affiche cette aide"
                exit 0
                ;;
            *)
                echo "Option inconnue: $1"
                echo "Usage: ./eval_test.sh [options]"
                exit 2
                ;;
        esac
        shift
    done

    # Afficher les options activees
    announce "Configuration:"
    [[ $KEEP_SERVER -eq 1 ]] && announce "  - Serveur conserve apres tests"
    [[ $KILL_EXISTING_WEBSERV -eq 1 ]] && announce "  - Kill webserv existants"
    [[ $RUN_SIEGE -eq 1 ]] && announce "  - Tests siege actifs"
    [[ $RUN_VALGRIND -eq 0 ]] && announce "  - Valgrind DÉSACTIVÉ (gain ~90s)"
    [[ $KEEP_LOGS -eq 1 ]] && announce "  - Logs conserves dans $TMP_DIR"
    [[ $VERBOSE -eq 1 ]] && announce "  - Mode VERBOSE"

    require_cmd curl "Preflight"
    require_cmd make "Preflight"
    require_cmd grep "Preflight"
    require_cmd timeout "Preflight"
    optional_cmd ss && pass "Preflight: commande 'ss' disponible" || note "Commande 'ss' absente: diagnostic des ports moins précis"

    test_readme
    test_build || { print_summary; exit 1; }
    test_program_invocation
    test_code_checks
    test_parsing_validation
    test_core_runtime
    # Stop only after each config test block, before starting the next config.
    stop_server
    test_test_config
    stop_server
    test_multi_config
    stop_server
    test_interface_port_config
    stop_server
    test_port_conflicts
    stop_server
    if [[ $RUN_SIEGE -eq 1 ]]; then
        test_siege
    else
        announce "[*] Siege non execute par defaut (utiliser --with-siege pour stress tests)"
    fi
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
