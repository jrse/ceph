// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:nil -*-
// vim: ts=8 sw=2 sts=2 expandtab

#include "rgw_kafka.h"
#include "common/ceph_context.h"
#include "kafka_mock.h"
#include <gtest/gtest.h>
#include <algorithm>

using namespace rgw;

class CctCleaner {
  CephContext* cct;
public:
  CctCleaner(CephContext* _cct) : cct(_cct) {}
  ~CctCleaner() {
#ifdef WITH_CRIMSON
    delete cct;
#else
    cct->put();
#endif
  }
};

auto cct = new CephContext(CEPH_ENTITY_TYPE_CLIENT);
CctCleaner cleaner(cct);

class TestKafka : public ::testing::Test {
protected:
  kafka::connection_id_t conn_id;

  void SetUp() override {
    kafka_mock::reset();
    ASSERT_TRUE(kafka::init(cct));
  }

  void TearDown() override {
    kafka::shutdown();
  }
};

// helper to check if a specific conf key=value was set
bool has_conf(const std::string& key, const std::string& value) {
  const auto& sets = kafka_mock::get_conf_sets();
  return std::any_of(sets.begin(), sets.end(),
    [&](const auto& p) { return p.first == key && p.second == value; });
}

bool has_conf_key(const std::string& key) {
  const auto& sets = kafka_mock::get_conf_sets();
  return std::any_of(sets.begin(), sets.end(),
    [&](const auto& p) { return p.first == key; });
}

// =============================================================================
// Basic connection tests
// =============================================================================

TEST_F(TestKafka, PlaintextConnectionOK)
{
  const auto count = kafka::get_connection_count();
  auto rc = kafka::connect(conn_id, "kafka://localhost:9092",
    false, false, boost::none, boost::none, boost::none, boost::none,
    boost::none, boost::none, boost::none, boost::none);
  EXPECT_TRUE(rc);
  EXPECT_EQ(kafka::get_connection_count(), count + 1);
  EXPECT_TRUE(has_conf("bootstrap.servers", "localhost:9092"));
  EXPECT_FALSE(has_conf_key("security.protocol"));
}

TEST_F(TestKafka, ConnectionReuse)
{
  auto rc = kafka::connect(conn_id, "kafka://localhost:9092",
    false, false, boost::none, boost::none, boost::none, boost::none,
    boost::none, boost::none, boost::none, boost::none);
  EXPECT_TRUE(rc);
  const auto count = kafka::get_connection_count();

  kafka::connection_id_t conn_id2;
  rc = kafka::connect(conn_id2, "kafka://localhost:9092",
    false, false, boost::none, boost::none, boost::none, boost::none,
    boost::none, boost::none, boost::none, boost::none);
  EXPECT_TRUE(rc);
  EXPECT_EQ(kafka::get_connection_count(), count);
}

// =============================================================================
// SSL tests
// =============================================================================

TEST_F(TestKafka, SSLConnection)
{
  const std::string ca_location = "/path/to/ca.crt";
  auto rc = kafka::connect(conn_id, "kafka://localhost:9093",
    true, true, ca_location, boost::none,
    boost::none, boost::none, boost::none,
    boost::none, boost::none, boost::none);
  EXPECT_TRUE(rc);
  EXPECT_TRUE(has_conf("security.protocol", "SSL"));
  EXPECT_TRUE(has_conf("ssl.ca.location", "/path/to/ca.crt"));
}

// =============================================================================
// mTLS tests
// =============================================================================

TEST_F(TestKafka, MTLSConnection)
{
  const std::string ca_location = "/path/to/ca.crt";
  const std::string ssl_cert = "/path/to/client.crt";
  const std::string ssl_key = "/path/to/client.key";
  auto rc = kafka::connect(conn_id, "kafka://localhost:9093",
    true, true, ca_location, boost::none,
    boost::none, boost::none, boost::none,
    ssl_cert, ssl_key, boost::none);
  EXPECT_TRUE(rc);
  EXPECT_TRUE(has_conf("security.protocol", "SSL"));
  EXPECT_TRUE(has_conf("ssl.ca.location", "/path/to/ca.crt"));
  EXPECT_TRUE(has_conf("ssl.certificate.location", "/path/to/client.crt"));
  EXPECT_TRUE(has_conf("ssl.key.location", "/path/to/client.key"));
  EXPECT_FALSE(has_conf_key("ssl.key.password"));
}

TEST_F(TestKafka, MTLSConnectionWithKeyPassword)
{
  const std::string ca_location = "/path/to/ca.crt";
  const std::string ssl_cert = "/path/to/client.crt";
  const std::string ssl_key = "/path/to/client.key";
  const std::string ssl_key_pass = "my-key-password";
  auto rc = kafka::connect(conn_id, "kafka://localhost:9093",
    true, true, ca_location, boost::none,
    boost::none, boost::none, boost::none,
    ssl_cert, ssl_key, ssl_key_pass);
  EXPECT_TRUE(rc);
  EXPECT_TRUE(has_conf("security.protocol", "SSL"));
  EXPECT_TRUE(has_conf("ssl.ca.location", "/path/to/ca.crt"));
  EXPECT_TRUE(has_conf("ssl.certificate.location", "/path/to/client.crt"));
  EXPECT_TRUE(has_conf("ssl.key.location", "/path/to/client.key"));
  EXPECT_TRUE(has_conf("ssl.key.password", "my-key-password"));
}

TEST_F(TestKafka, MTLSConnectionDifferentCertsAreDifferentConnections)
{
  const std::string ca_location = "/path/to/ca.crt";
  const std::string ssl_cert1 = "/path/to/client1.crt";
  const std::string ssl_key1 = "/path/to/client1.key";
  auto rc = kafka::connect(conn_id, "kafka://localhost:9093",
    true, true, ca_location, boost::none,
    boost::none, boost::none, boost::none,
    ssl_cert1, ssl_key1, boost::none);
  EXPECT_TRUE(rc);
  const auto count = kafka::get_connection_count();

  const std::string ssl_cert2 = "/path/to/client2.crt";
  const std::string ssl_key2 = "/path/to/client2.key";
  kafka::connection_id_t conn_id2;
  rc = kafka::connect(conn_id2, "kafka://localhost:9093",
    true, true, ca_location, boost::none,
    boost::none, boost::none, boost::none,
    ssl_cert2, ssl_key2, boost::none);
  EXPECT_TRUE(rc);
  // different client certs should create a different connection
  EXPECT_EQ(kafka::get_connection_count(), count + 1);
}

TEST_F(TestKafka, MTLSSameCertsReuseConnection)
{
  const std::string ca_location = "/path/to/ca.crt";
  const std::string ssl_cert = "/path/to/client.crt";
  const std::string ssl_key = "/path/to/client.key";
  auto rc = kafka::connect(conn_id, "kafka://localhost:9093",
    true, true, ca_location, boost::none,
    boost::none, boost::none, boost::none,
    ssl_cert, ssl_key, boost::none);
  EXPECT_TRUE(rc);
  const auto count = kafka::get_connection_count();

  kafka::connection_id_t conn_id2;
  rc = kafka::connect(conn_id2, "kafka://localhost:9093",
    true, true, ca_location, boost::none,
    boost::none, boost::none, boost::none,
    ssl_cert, ssl_key, boost::none);
  EXPECT_TRUE(rc);
  // same certs should reuse the connection
  EXPECT_EQ(kafka::get_connection_count(), count);
}

// =============================================================================
// SASL tests
// =============================================================================

TEST_F(TestKafka, SASLPlaintextConnection)
{
  const std::string mechanism = "SCRAM-SHA-512";
  const std::string user = "alice";
  const std::string password = "alice-secret";
  auto rc = kafka::connect(conn_id, "kafka://localhost:9095",
    false, false, boost::none, mechanism,
    user, password,
    boost::none, boost::none, boost::none, boost::none);
  EXPECT_TRUE(rc);
  EXPECT_TRUE(has_conf("security.protocol", "SASL_PLAINTEXT"));
  EXPECT_TRUE(has_conf("sasl.username", "alice"));
  EXPECT_TRUE(has_conf("sasl.password", "alice-secret"));
  EXPECT_TRUE(has_conf("sasl.mechanism", "SCRAM-SHA-512"));
}

TEST_F(TestKafka, SASLSSLConnection)
{
  const std::string ca_location = "/path/to/ca.crt";
  const std::string mechanism = "SCRAM-SHA-512";
  const std::string user = "alice";
  const std::string password = "alice-secret";
  auto rc = kafka::connect(conn_id, "kafka://localhost:9096",
    true, true, ca_location, mechanism,
    user, password,
    boost::none, boost::none, boost::none, boost::none);
  EXPECT_TRUE(rc);
  EXPECT_TRUE(has_conf("security.protocol", "SASL_SSL"));
  EXPECT_TRUE(has_conf("sasl.username", "alice"));
  EXPECT_TRUE(has_conf("sasl.password", "alice-secret"));
  EXPECT_TRUE(has_conf("sasl.mechanism", "SCRAM-SHA-512"));
  EXPECT_TRUE(has_conf("ssl.ca.location", "/path/to/ca.crt"));
}

TEST_F(TestKafka, SASLSSLWithMTLS)
{
  const std::string ca_location = "/path/to/ca.crt";
  const std::string mechanism = "SCRAM-SHA-512";
  const std::string user = "alice";
  const std::string password = "alice-secret";
  const std::string ssl_cert = "/path/to/client.crt";
  const std::string ssl_key = "/path/to/client.key";
  auto rc = kafka::connect(conn_id, "kafka://localhost:9096",
    true, true, ca_location, mechanism,
    user, password,
    boost::none, ssl_cert, ssl_key, boost::none);
  EXPECT_TRUE(rc);
  EXPECT_TRUE(has_conf("security.protocol", "SASL_SSL"));
  EXPECT_TRUE(has_conf("sasl.username", "alice"));
  EXPECT_TRUE(has_conf("sasl.password", "alice-secret"));
  EXPECT_TRUE(has_conf("sasl.mechanism", "SCRAM-SHA-512"));
  EXPECT_TRUE(has_conf("ssl.ca.location", "/path/to/ca.crt"));
  EXPECT_TRUE(has_conf("ssl.certificate.location", "/path/to/client.crt"));
  EXPECT_TRUE(has_conf("ssl.key.location", "/path/to/client.key"));
}

// =============================================================================
// Connection identity tests (verifying hash/equality)
// =============================================================================

TEST_F(TestKafka, SSLvsPlaintextAreDifferentConnections)
{
  auto rc = kafka::connect(conn_id, "kafka://localhost:9092",
    false, false, boost::none, boost::none, boost::none, boost::none,
    boost::none, boost::none, boost::none, boost::none);
  EXPECT_TRUE(rc);
  const auto count = kafka::get_connection_count();

  const std::string ca_location = "/path/to/ca.crt";
  kafka::connection_id_t conn_id2;
  rc = kafka::connect(conn_id2, "kafka://localhost:9092",
    true, true, ca_location, boost::none,
    boost::none, boost::none, boost::none,
    boost::none, boost::none, boost::none);
  EXPECT_TRUE(rc);
  EXPECT_EQ(kafka::get_connection_count(), count + 1);
}

TEST_F(TestKafka, MTLSvsSSLOnlyAreDifferentConnections)
{
  // SSL only (no client cert)
  const std::string ca_location = "/path/to/ca.crt";
  auto rc = kafka::connect(conn_id, "kafka://localhost:9093",
    true, true, ca_location, boost::none,
    boost::none, boost::none, boost::none,
    boost::none, boost::none, boost::none);
  EXPECT_TRUE(rc);
  const auto count = kafka::get_connection_count();

  // SSL + mTLS (with client cert)
  const std::string ssl_cert = "/path/to/client.crt";
  const std::string ssl_key = "/path/to/client.key";
  kafka::connection_id_t conn_id2;
  rc = kafka::connect(conn_id2, "kafka://localhost:9093",
    true, true, ca_location, boost::none,
    boost::none, boost::none, boost::none,
    ssl_cert, ssl_key, boost::none);
  EXPECT_TRUE(rc);
  EXPECT_EQ(kafka::get_connection_count(), count + 1);
}
