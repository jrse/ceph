// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:nil -*-
// vim: ts=8 sw=2 sts=2 expandtab

#include "kafka_mock.h"
#include <librdkafka/rdkafka.h>
#include <cstring>
#include <vector>
#include <string>
#include <utility>

namespace kafka_mock {

static std::string valid_host = "localhost";
static int conf_set_result = 0; // RD_KAFKA_CONF_OK
static int produce_result = 0;
static unsigned produce_count = 0;
static std::vector<std::pair<std::string, std::string>> conf_sets;

void reset() {
  valid_host = "localhost";
  conf_set_result = 0;
  produce_result = 0;
  produce_count = 0;
  conf_sets.clear();
}

void set_valid_host(const std::string& host) {
  valid_host = host;
}

void set_conf_set_result(int result) {
  conf_set_result = result;
}

const std::vector<std::pair<std::string, std::string>>& get_conf_sets() {
  return conf_sets;
}

void set_produce_result(int result) {
  produce_result = result;
}

unsigned get_produce_count() {
  return produce_count;
}

} // namespace kafka_mock

// --- librdkafka C API mock implementations ---

// Use a dummy non-null pointer for conf and producer
static char dummy_conf;
static char dummy_producer;
static char dummy_topic;

const char *rd_kafka_topic_name(const rd_kafka_topic_t *rkt) {
  return "mock-topic";
}

rd_kafka_resp_err_t rd_kafka_last_error() {
  return RD_KAFKA_RESP_ERR_NO_ERROR;
}

const char *rd_kafka_err2str(rd_kafka_resp_err_t err) {
  return "mock-error";
}

rd_kafka_conf_t *rd_kafka_conf_new() {
  kafka_mock::conf_sets.clear();
  return reinterpret_cast<rd_kafka_conf_t*>(&dummy_conf);
}

rd_kafka_conf_res_t rd_kafka_conf_set(rd_kafka_conf_t *conf,
                                       const char *name,
                                       const char *value,
                                       char *errstr, size_t errstr_size) {
  kafka_mock::conf_sets.emplace_back(name, value);
  return static_cast<rd_kafka_conf_res_t>(kafka_mock::conf_set_result);
}

void rd_kafka_conf_set_dr_msg_cb(rd_kafka_conf_t *conf,
                                  void (*dr_msg_cb)(rd_kafka_t *rk,
                                                    const rd_kafka_message_t *rkmessage,
                                                    void *opaque)) {}

void rd_kafka_conf_set_opaque(rd_kafka_conf_t *conf, void *opaque) {}

void rd_kafka_conf_set_log_cb(rd_kafka_conf_t *conf,
                               void (*log_cb)(const rd_kafka_t *rk,
                                              int level,
                                              const char *fac,
                                              const char *buf)) {}

void rd_kafka_conf_set_error_cb(rd_kafka_conf_t *conf,
                                 void (*error_cb)(rd_kafka_t *rk,
                                                  int err,
                                                  const char *reason,
                                                  void *opaque)) {}

rd_kafka_t *rd_kafka_new(rd_kafka_type_t type, rd_kafka_conf_t *conf,
                          char *errstr, size_t errstr_size) {
  // always succeed - return a non-null dummy pointer
  return reinterpret_cast<rd_kafka_t*>(&dummy_producer);
}

void rd_kafka_conf_destroy(rd_kafka_conf_t *conf) {}

rd_kafka_resp_err_t rd_kafka_flush(rd_kafka_t *rk, int timeout_ms) {
  return RD_KAFKA_RESP_ERR_NO_ERROR;
}

void rd_kafka_destroy(rd_kafka_t *rk) {}

rd_kafka_topic_t *rd_kafka_topic_new(rd_kafka_t *rk, const char *topic,
                                      rd_kafka_topic_conf_t *conf) {
  return reinterpret_cast<rd_kafka_topic_t*>(&dummy_topic);
}

int rd_kafka_produce(rd_kafka_topic_t *rkt, int32_t partition,
                      int msgflags,
                      void *payload, size_t len,
                      const void *key, size_t keylen,
                      void *msg_opaque) {
  kafka_mock::produce_count++;
  return kafka_mock::produce_result;
}

int rd_kafka_poll(rd_kafka_t *rk, int timeout_ms) {
  return 0;
}

void rd_kafka_topic_destroy(rd_kafka_topic_t *rkt) {}

void *rd_kafka_opaque(const rd_kafka_t *rk) {
  return nullptr;
}

void rd_kafka_set_log_level(rd_kafka_t *rk, int level) {}
