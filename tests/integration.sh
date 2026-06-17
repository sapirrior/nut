#!/bin/bash
set -e

NURL=${NURL:-../nurl}
HTTPBIN=https://jsonplaceholder.typicode.com

fail() {
    echo "FAIL: $1"
    exit 1
}

echo "Running integration tests..."

# Test GET
status=$(${NURL} ${HTTPBIN}/posts/1 -w '%{http_code}' -o /dev/null -s)
[ "$status" = "200" ] || fail "GET expected 200, got $status"
echo "PASS: GET"

# Test POST JSON
status=$(${NURL} ${HTTPBIN}/posts -j -d '{"title":"foo","body":"bar","userId":1}' -w '%{http_code}' -o /dev/null -s)
[ "$status" = "201" ] || fail "POST JSON expected 201, got $status"
echo "PASS: POST JSON"

# Test TLS verify (should fail without -k)
set +e
output=$(${NURL} https://expired.badssl.com/ -v 2>&1)
set -e
if echo "$output" | grep -q "TLS handshake or certificate verification failed"; then
    echo "PASS: TLS_VERIFY"
else
    echo "Output was: $output"
    fail "TLS_VERIFY"
fi

# Test --fail
set +e
${NURL} ${HTTPBIN}/status/404 -f -s
ret=$?
set -e
[ $ret -ne 0 ] || fail "FAIL_FLAG expected non-zero exit, got $ret"
echo "PASS: FAIL_FLAG"

echo "All integration tests passed."
