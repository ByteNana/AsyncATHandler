#include "HybridTest.h"

HTEST_CASE(SendSyncBasicCommand) {
  HTEST_BEGIN();
  HTEST_SLEEP(10);

  /* TODO: Make this piece of code hybrid: */
  std::atomic<bool> complete{false};
  std::thread responder([&]() {
    HTEST_SLEEP(10);
    mockStream->InjectRxData("AT\r\n");
    mockStream->InjectRxData("OK\r\n");
    complete = true;
  });

  mockStream->ClearTxData();

  String response;
  bool success = ctx.handler->sendSync("AT", response, 3000);

  while (!complete) HTEST_SLEEP(10);
  HTEST_SLEEP(10);

  HTEST_EQ_STR(mockStream->GetTxData().c_str(), "AT\r\n");
  HTEST_TRUE(success);
  HTEST_TRUE(response.indexOf("OK") != -1);

  responder.join();

  HTEST_END();
}

HTEST_BONES();