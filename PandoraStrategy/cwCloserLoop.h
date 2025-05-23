#pragma once
#include <map>
#include <string>
//#include "cwContext.h" // 保留，含 cwPositionPtr, cwOrderPtr 等定义
#include "cwBasicKindleStrategy.h" // 包含 GetPositionsAndActiveOrders 定义
#include "cwBasicCout.h"

enum class CloseState {
    Waiting,
    OrderSent,
    PendingCancel,
    Closed,
    Failed
};

struct CloserInstrumentState {
    cwPositionPtr position;
    CloseState state = CloseState::Waiting;
    int retryCount = 0;

    bool isDone() const { return state == CloseState::Closed || state == CloseState::Failed; }
};

class cwCloserLoop {
public:
    explicit cwCloserLoop(cwBasicKindleStrategy* strategy);

    void Run();
    cwBasicCout				m_cwShow;

private:
    void UpdatePositions();
    void HandleInstrument(const std::string& instrumentID, CloserInstrumentState& state);
    bool IsAllDone() const;
    void TryAggressiveClose(cwMarketDataPtr pPriceData, cwPositionPtr pPos);
    bool IsPendingOrder(std::string instrumentID);
    cwOrderPtr SafeLimitOrder(const char* instrumentID, int volume, double rawPrice, double slipTick);

    cwBasicKindleStrategy* strategy;

    std::map<std::string, CloserInstrumentState> instrumentStates;
    std::map<std::string, cwPositionPtr> closerCurrentPosMap;
    std::map<cwActiveOrderKey, cwOrderPtr> closerWaitOrderList;
};
