#!/usr/bin/env bash
set -euo pipefail

# Teuthology-style local Kafka runner (no docker-compose)
# - Downloads Kafka
# - Generates TLS/mTLS certs via kafka-security.sh
# - Writes server.properties similar to qa/tasks/kafka.py
# - Starts zookeeper + kafka
# - Creates SCRAM users
# - Runs bucket notification kafka marker suites
#
# Usage:
#   chmod +x run-kafka-teuthology-like.sh
#   ./run-kafka-teuthology-like.sh
#
# Optional env:
#   CEPH_ROOT=/home/jaraco/Documents/projects/ceph/ceph
#   KAFKA_VERSION=3.9.2
#   KEEP_KAFKA=1        # keep kafka dir on exit

CEPH_ROOT="${CEPH_ROOT:-/home/jaraco/Documents/projects/ceph/ceph}"
KAFKA_VERSION="${KAFKA_VERSION:-3.9.2}"
SCALA_VER="2.13"
KAFKA_SRC_DIR="kafka_${SCALA_VER}-${KAFKA_VERSION}"
KAFKA_DIR="${CEPH_ROOT}/.kafka-local/kafka-runtime-${SCALA_VER}-${KAFKA_VERSION}"
KAFKA_TGZ="kafka_${SCALA_VER}-${KAFKA_VERSION}.tgz"
KAFKA_URL="https://downloads.apache.org/kafka/${KAFKA_VERSION}/${KAFKA_TGZ}"

PLAINTEXT_PORT=9092
SSL_PORT=9093
SASL_SSL_PORT=9096
SASL_PLAINTEXT_PORT=9095

BN_DIR="${CEPH_ROOT}/src/test/rgw/bucket_notification"
VENV_PY="${CEPH_ROOT}/.venv/bin/python"

mkdir -p "${CEPH_ROOT}/.kafka-local"

cleanup() {
  set +e
  if [[ -f "${KAFKA_DIR}/zookeeper.pid" ]]; then
    kill "$(cat "${KAFKA_DIR}/zookeeper.pid")" 2>/dev/null || true
  fi
  if [[ -f "${KAFKA_DIR}/kafka.pid" ]]; then
    kill "$(cat "${KAFKA_DIR}/kafka.pid")" 2>/dev/null || true
  fi
  pkill -f "kafka.Kafka.*${KAFKA_DIR}/config/server.properties" 2>/dev/null || true
  pkill -f "QuorumPeerMain.*${KAFKA_DIR}/config/zookeeper.properties" 2>/dev/null || true
  sleep 2
  if [[ "${KEEP_KAFKA:-0}" != "1" ]]; then
    rm -rf "${KAFKA_DIR}"
  fi
}
trap cleanup EXIT

echo "==> Preparing Kafka dir: ${KAFKA_DIR}"
rm -rf "${KAFKA_DIR}"
mkdir -p "${CEPH_ROOT}/.kafka-local"
cd "${CEPH_ROOT}/.kafka-local"

if [[ ! -f "${KAFKA_TGZ}" ]]; then
  echo "==> Downloading Kafka ${KAFKA_VERSION}"
  curl -fL "${KAFKA_URL}" -o "${KAFKA_TGZ}"
fi

echo "==> Extracting Kafka"
tar -xzf "${KAFKA_TGZ}"
mv "${KAFKA_SRC_DIR}" "${KAFKA_DIR}"

echo "==> Generating certs with kafka-security.sh"
HOST_IP="127.0.0.1"
(
  cd "${BN_DIR}"
  KAFKA_CERT_HOSTNAME="localhost" KAFKA_CERT_IP="${HOST_IP}" bash ./kafka-security.sh
)

# Reuse the exact cert artifacts produced by the bucket_notification flow
cp "${BN_DIR}/server.keystore.jks" "${KAFKA_DIR}/server.keystore.jks"
cp "${BN_DIR}/server.truststore.jks" "${KAFKA_DIR}/server.truststore.jks"
cp "${BN_DIR}/y-ca.crt" "${KAFKA_DIR}/y-ca.crt"
cp "${BN_DIR}/y-ca.key" "${KAFKA_DIR}/y-ca.key"
cp "${BN_DIR}/client.crt" "${KAFKA_DIR}/client.crt"
cp "${BN_DIR}/client.key" "${KAFKA_DIR}/client.key"

# test_bn.py expects client certs under KAFKA_DIR/config for receiver-side SSL
mkdir -p "${KAFKA_DIR}/config"
ln -sf ../client.crt "${KAFKA_DIR}/config/client.crt"
ln -sf ../client.key "${KAFKA_DIR}/config/client.key"

echo "==> Writing Kafka server.properties"
cat > "${KAFKA_DIR}/config/server.properties" <<EOF
broker.id=1
node.id=1
process.roles=broker,controller
controller.quorum.voters=1@localhost:29093
controller.listener.names=CONTROLLER
listeners=PLAINTEXT://0.0.0.0:${PLAINTEXT_PORT},SSL://0.0.0.0:${SSL_PORT},SASL_SSL://0.0.0.0:${SASL_SSL_PORT},SASL_PLAINTEXT://0.0.0.0:${SASL_PLAINTEXT_PORT},CONTROLLER://127.0.0.1:29093
advertised.listeners=PLAINTEXT://localhost:${PLAINTEXT_PORT},SSL://localhost:${SSL_PORT},SASL_SSL://localhost:${SASL_SSL_PORT},SASL_PLAINTEXT://localhost:${SASL_PLAINTEXT_PORT}
listener.security.protocol.map=PLAINTEXT:PLAINTEXT,SSL:SSL,SASL_SSL:SASL_SSL,SASL_PLAINTEXT:SASL_PLAINTEXT,CONTROLLER:PLAINTEXT
log.dirs=${KAFKA_DIR}/data/kafka-logs
num.network.threads=3
num.io.threads=8
socket.send.buffer.bytes=102400
socket.receive.buffer.bytes=102400
socket.request.max.bytes=104857600
num.partitions=1
num.recovery.threads.per.data.dir=1
offsets.topic.replication.factor=1
transaction.state.log.replication.factor=1
transaction.state.log.min.isr=1
log.retention.hours=168
log.segment.bytes=1073741824
log.retention.check.interval.ms=300000
group.initial.rebalance.delay.ms=0

ssl.keystore.location=${KAFKA_DIR}/server.keystore.jks
ssl.keystore.password=mypassword
ssl.key.password=mypassword
ssl.truststore.location=${KAFKA_DIR}/server.truststore.jks
ssl.truststore.password=mypassword
ssl.client.auth=requested

sasl.enabled.mechanisms=PLAIN,SCRAM-SHA-256,SCRAM-SHA-512
sasl.mechanism.inter.broker.protocol=PLAIN
inter.broker.listener.name=PLAINTEXT

listener.name.sasl_ssl.plain.sasl.jaas.config=org.apache.kafka.common.security.plain.PlainLoginModule required username="admin" password="admin-secret" user_alice="alice-secret";
listener.name.sasl_ssl.scram-sha-256.sasl.jaas.config=org.apache.kafka.common.security.scram.ScramLoginModule required username="admin" password="admin-secret";
listener.name.sasl_ssl.scram-sha-512.sasl.jaas.config=org.apache.kafka.common.security.scram.ScramLoginModule required username="admin" password="admin-secret";

listener.name.sasl_plaintext.plain.sasl.jaas.config=org.apache.kafka.common.security.plain.PlainLoginModule required username="admin" password="admin-secret" user_alice="alice-secret";
listener.name.sasl_plaintext.scram-sha-256.sasl.jaas.config=org.apache.kafka.common.security.scram.ScramLoginModule required username="admin" password="admin-secret";
listener.name.sasl_plaintext.scram-sha-512.sasl.jaas.config=org.apache.kafka.common.security.scram.ScramLoginModule required username="admin" password="admin-secret";
EOF

echo "==> Formatting KRaft metadata"
mkdir -p "${KAFKA_DIR}/data"
CLUSTER_ID="$(${KAFKA_DIR}/bin/kafka-storage.sh random-uuid)"
"${KAFKA_DIR}/bin/kafka-storage.sh" format -t "${CLUSTER_ID}" -c "${KAFKA_DIR}/config/server.properties"

echo "==> Starting Kafka"
(
  cd "${KAFKA_DIR}"
  nohup ./bin/kafka-server-start.sh ./config/server.properties > kafka.out 2>&1 &
  echo $! > kafka.pid
)

echo "==> Waiting for broker"
for i in $(seq 1 60); do
  if "${KAFKA_DIR}/bin/kafka-topics.sh" --list --bootstrap-server "localhost:${PLAINTEXT_PORT}" >/dev/null 2>&1; then
    break
  fi
  sleep 2
  if [[ "$i" -eq 60 ]]; then
    echo "Broker did not become ready in time"
    tail -n 200 "${KAFKA_DIR}/kafka.out" || true
    exit 1
  fi
done

echo "==> Creating SCRAM users"
run_kafka_configs_with_retry() {
  local tries=20
  local delay=2
  local n=1
  while true; do
    if "${KAFKA_DIR}/bin/kafka-configs.sh" "$@"; then
      return 0
    fi
    if [[ "$n" -ge "$tries" ]]; then
      echo "kafka-configs failed after ${tries} attempts: $*"
      return 1
    fi
    sleep "$delay"
    n=$((n + 1))
  done
}

for user in alice admin; do
  if [[ "$user" == "alice" ]]; then pw="alice-secret"; else pw="admin-secret"; fi
  run_kafka_configs_with_retry \
    --bootstrap-server "localhost:${PLAINTEXT_PORT}" \
    --alter --entity-type users --entity-name "${user}" \
    --add-config "SCRAM-SHA-256=[password=${pw}]"
  run_kafka_configs_with_retry \
    --bootstrap-server "localhost:${PLAINTEXT_PORT}" \
    --alter --entity-type users --entity-name "${user}" \
    --add-config "SCRAM-SHA-512=[password=${pw}]"

  DESC="$(${KAFKA_DIR}/bin/kafka-configs.sh \
    --bootstrap-server "localhost:${PLAINTEXT_PORT}" \
    --describe --entity-type users --entity-name "${user}" 2>&1 || true)"
  if [[ "$DESC" != *"SCRAM-SHA-256"* ]] || [[ "$DESC" != *"SCRAM-SHA-512"* ]]; then
    echo "SCRAM verification failed for user '${user}'. Output:"
    echo "$DESC"
    exit 1
  fi
done

echo "==> Ensuring RGW allows cleartext notification secrets"
"${CEPH_ROOT}/build/bin/ceph" -c "${CEPH_ROOT}/build/ceph.conf" -k "${CEPH_ROOT}/build/keyring" \
  config set client.rgw rgw_allow_notification_secrets_in_cleartext true --force

SETTING="$(${CEPH_ROOT}/build/bin/ceph -c "${CEPH_ROOT}/build/ceph.conf" -k "${CEPH_ROOT}/build/keyring" \
  config get client.rgw rgw_allow_notification_secrets_in_cleartext || true)"
if [[ "$SETTING" != "true" ]]; then
  echo "Failed to enable rgw_allow_notification_secrets_in_cleartext (got: '$SETTING')"
  exit 1
fi

echo "==> Running pytest markers (kafka_security_test, kafka_test)"
cd "${CEPH_ROOT}"
if [[ ! -x "${VENV_PY}" ]]; then
  echo "Missing venv python: ${VENV_PY}"
  exit 1
fi

export BNTESTS_CONF="${CEPH_ROOT}/src/test/rgw/bucket_notification/bntests.conf.SAMPLE"
export KAFKA_DIR="${KAFKA_DIR}"
export KAFKA_CERT_DIR="${BN_DIR}"
export BN_KAFKA_HOST="localhost"

"${VENV_PY}" -m pytest -s src/test/rgw/bucket_notification/test_bn.py -v -m 'kafka_security_test'
"${VENV_PY}" -m pytest -s src/test/rgw/bucket_notification/test_bn.py -v -m 'kafka_test'

echo "==> Done"
