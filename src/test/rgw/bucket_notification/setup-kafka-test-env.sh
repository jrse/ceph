#!/bin/bash
# setup-kafka-test-env.sh
#
# Sets up the Kafka test environment for RGW bucket notification tests.
# This script:
#   1. Generates SSL certificates (CA, broker, client for mTLS)
#   2. Starts the Kafka broker via docker-compose
#   3. Waits for the broker to be ready
#
# Prerequisites:
#   - docker or podman with docker-compose/podman-compose
#   - openssl and keytool (from java-*-openjdk-headless)
#   - Network "podman" must exist (for podman: podman network create podman)
#
# Usage:
#   cd src/test/rgw/bucket_notification
#   bash setup-kafka-test-env.sh
#
# After setup, run tests with:
#   export KAFKA_DIR=$(pwd)
#   export BNTESTS_CONF=/path/to/bntests.conf
#   python -m pytest test_bn.py -m kafka_security_test -v -s
#
# Teardown:
#   docker-compose down

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

COMPOSE_CMD="docker-compose"
if command -v podman-compose &>/dev/null; then
  COMPOSE_CMD="podman-compose"
fi

echo "=== Step 1: Generate certificates ==="
bash kafka-security.sh
echo ""

echo "=== Step 2: Start Kafka broker ==="
$COMPOSE_CMD down 2>/dev/null || true
$COMPOSE_CMD up -d
echo ""

echo "=== Step 3: Wait for broker to be ready ==="
MAX_WAIT=60
ELAPSED=0
until nc -z localhost 9092 2>/dev/null; do
  if [ $ELAPSED -ge $MAX_WAIT ]; then
    echo "ERROR: Kafka broker did not start within ${MAX_WAIT}s"
    exit 1
  fi
  sleep 1
  ELAPSED=$((ELAPSED + 1))
done
echo "Broker is listening on port 9092 (after ${ELAPSED}s)"

# Wait a bit more for SCRAM users to be created by kafka-init
sleep 5
echo ""

echo "=== Setup complete ==="
echo ""
echo "Kafka listeners:"
echo "  PLAINTEXT:      broker:9092"
echo "  SSL (mTLS):     broker:9093"
echo "  INTERNAL:       broker:9094"
echo "  SASL_PLAINTEXT: broker:9095"
echo "  SASL_SSL:       broker:9096"
echo ""
echo "SCRAM users: alice/alice-secret, admin/admin-secret"
echo ""
echo "Certificates:"
echo "  CA cert:     $(pwd)/y-ca.crt"
echo "  Client cert: $(pwd)/client.crt"
echo "  Client key:  $(pwd)/client.key"
echo ""
echo "Run tests:"
echo "  export KAFKA_DIR=$(pwd)"
echo "  python -m pytest test_bn.py -m kafka_security_test -v -s"
echo ""
echo "Teardown:"
echo "  $COMPOSE_CMD down"
