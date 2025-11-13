#pragma once
#include <AsyncATHandler.h>

#include <memory>

#include "../test_native/common.h"
#include "Stream.h"
#include "esp_log.h"
#include "mocks.h"

/* Common */
#define HTEST_SETUP()
#define HTEST_TEARDOWN()

#ifdef HTEST_ENV_HARDWARE

#error HYBRID TEST TO HARDWARE NOT DONE

#include <Arduino.h>
#include <unity.h>

class HTest {
 public:
  AsyncATHandler handler;
  HardwareMockStream mockStream;
  void setUp();
  void tearDown();

  bool send(const char* data);
};

#define HTEST_CASE(name) void test_##name(HTest& ctx)
#define HTEST_BEGIN() \
  UNITY_BEGIN();      \
  auto& ctx = *this;
#define HTEST_END() UNITY_END()
#define HTEST_RUN(fn)            \
  {                              \
    HTest ctx;                   \
    ctx.setUp();                 \
    RUN_TEST([]() { fn(ctx); }); \
    ctx.tearDown();              \
  }
#define HTEST_TRUE(x) TEST_ASSERT_TRUE(x)
#define HTEST_FALSE(x) TEST_ASSERT_FALSE(x)
#define HTEST_EQ_INT(a, b) TEST_ASSERT_EQUAL_INT(a, b)
#define HTEST_EQ_STR(a, b) TEST_ASSERT_EQUAL_STRING(a, b)
#define HTEST_EQ(a, b) HTestEq(a, b)
#define HTEST_NULL(x) TEST_ASSERT_NULL(x)
#define HTEST_NOT_NULL(x) TEST_ASSERT_NOT_NULL(x)

#define HTEST_SLEEP(ms) delay(ms)

void HTest::setUp() {
  Serial.begin(115200);
  delay(2000);
  HTEST_BEGIN();
  TEST_ASSERT_TRUE(handler.begin(mockStream));
}

void HTest::tearDown() { HTEST_END(); }

bool HTest::send(const char* data) {
  if (!data) return false;
  mockStream.write((const uint8_t*)data, strlen(data));
  return true;
}

#define HTEST_BONES() \
  void setup() {      \
    HTest test;       \
    test.setUp();     \
    test.tearDown();  \
  }                   \
  void loop() {}

#else /* Native */
#include <gtest/gtest.h>
using ::testing::NiceMock;

class HTest : public FreeRTOSTest {
 public:
  std::unique_ptr<AsyncATHandler> handler;
  std::unique_ptr<NiceMock<MockStream> > mockStream;

  void SetUp() override;
  void TearDown() override;
  bool send(const char* data);
};

#define HTEST_CASE(name) TEST_F(HTest, name)
#define HTEST_BEGIN() auto& ctx = *this;
#define HTEST_END()
#define HTEST_RUN(fn)
#define HTEST_TRUE(x) EXPECT_TRUE(x)
#define HTEST_FALSE(x) EXPECT_FALSE(x)
#define HTEST_EQ(a, b) HTestEq(a, b)
#define HTEST_EQ_STR(a, b) EXPECT_STREQ(a, b)
#define HTEST_BONES() FREERTOS_TEST_MAIN()
#define HTEST_NULL(x) EXPECT_EQ(x, nullptr)
#define HTEST_NOT_NULL(x) EXPECT_NE(x, nullptr)

#define HTEST_SLEEP(ms) std::this_thread::sleep_for(std::chrono::milliseconds(ms));

void HTest::SetUp() {
  FreeRTOSTest::SetUp();
  mockStream = std::make_unique<NiceMock<MockStream> >();
  mockStream->SetupDefaults();
  handler = std::make_unique<AsyncATHandler>();

  bool initSuccess = handler->begin(*mockStream);
  EXPECT_TRUE(initSuccess);
}

void HTest::TearDown() {
  if (handler) {
    while (true) {
      auto promise = handler->popCompletedPromise(0);
      if (!promise) break;
    }
    bool success = CleanupATHandler(handler.get());
    if (!success) { printf("WARNING: Handler teardown may have failed\n"); }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    handler.reset();
  }
  mockStream.reset();
  FreeRTOSTest::TearDown();
}

bool HTest::send(const char* data) {
  if (!handler || !data) return false;
  handler->sendSync(data, strlen(data));
  return true;
}

template <typename A, typename B>
inline void HTestEq(const A& a, const B& b) {
  if constexpr (std::is_pointer_v<A> && std::is_pointer_v<B>) {
    EXPECT_EQ(a, b);
  } else if constexpr (std::is_pointer_v<A> && !std::is_pointer_v<B>) {
    EXPECT_EQ(a, b.get());
  } else if constexpr (!std::is_pointer_v<A> && std::is_pointer_v<B>) {
    EXPECT_EQ(a.get(), b);
  } else {
    EXPECT_EQ(a, b);
  }
}

#endif