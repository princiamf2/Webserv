#!/bin/bash

CXX="c++"
CXXFLAGS="-Wall -Wextra -Werror -std=c++98"
SRC="../main.cpp ../ParseConfig.cpp ../ParseServer.cpp ../ParseLocation.cpp"

PASS=0
FAIL=0

run_tests() {
    DIR="$1"
    MODE="$2"

    for file in "$DIR"/*.config; do
        [ -e "$file" ] || continue

        NAME=$(basename "$file" .config)
        EXEC="./webserv_test_$NAME"

        echo "========================================"
        echo "TEST: $NAME [$MODE]"

        $CXX $CXXFLAGS $SRC -o "$EXEC"
        if [ $? -ne 0 ]; then
            echo "❌ COMPILATION ERROR"
            FAIL=$((FAIL + 1))
            echo
            continue
        fi

        OUTPUT=$("$EXEC" "$file" 2>&1)
        echo "$OUTPUT"

        if [ "$MODE" = "invalid" ]; then
            if echo "$OUTPUT" | grep -q "nb de servers: 0"; then
                echo "✅ PASS (correctly rejected)"
                PASS=$((PASS + 1))
            else
                echo "❌ FAIL (should have been rejected)"
                FAIL=$((FAIL + 1))
            fi
        else
            if echo "$OUTPUT" | grep -Eq "nb de servers: [1-9][0-9]*"; then
                echo "✅ PASS (correctly accepted)"
                PASS=$((PASS + 1))
            else
                echo "❌ FAIL (should have been accepted)"
                FAIL=$((FAIL + 1))
            fi
        fi

        rm -f "$EXEC"
        echo
    done
}

run_tests "./configs_invalid" "invalid"
run_tests "./configs_valid" "valid"

echo "========================================"
echo "RESULT:"
echo "PASS: $PASS"
echo "FAIL: $FAIL"