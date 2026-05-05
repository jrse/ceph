#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CEPH_ROOT="$(cd "${SCRIPT_DIR}/../../../.." && pwd)"

exec "${SCRIPT_DIR}/run-kafka-teuthology-like.sh"
