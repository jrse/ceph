// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:nil -*-
// vim: ts=8 sw=2 sts=2 expandtab

#pragma once

#include <string>
#include <vector>
#include <utility>

// mock control interface for kafka unit tests
namespace kafka_mock {

// reset all mock state
void reset();

// control whether rd_kafka_new() succeeds (returns non-null)
void set_valid_host(const std::string& host);

// control whether rd_kafka_conf_set() succeeds
void set_conf_set_result(int result); // 0 = RD_KAFKA_CONF_OK

// get the list of conf_set calls that were made (name, value pairs)
const std::vector<std::pair<std::string, std::string>>& get_conf_sets();

// control whether rd_kafka_produce() succeeds
void set_produce_result(int result); // 0 = success, -1 = failure

// get number of produce calls
unsigned get_produce_count();

} // namespace kafka_mock
