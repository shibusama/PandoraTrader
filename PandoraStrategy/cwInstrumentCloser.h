#pragma once
#pragma once
#include <string>
#include <map>
#include <memory>
#include <iostream>
//#include "cwContext.h"  // 假设定义了 cwPositionPtr、cwOrderPtr、cwActiveOrderKey 等
#include "cwBasicKindleStrategy.h" // 包含 GetPositionsAndActiveOrders 定义
#include "Utils.hpp"

class cwInstrumentCloser {
public:
    cwInstrumentCloser(cwBasicKindleStrategy* context, const std::string& instrumentID);

    void RunOnce(int hour, int minute, int second);
    bool IsFinished() const;

private:
    void FetchData();                       // 获取最新持仓和挂单
    void CancelPendingOrders();            // 撤销挂单
    void TryClose();                       // 发出清仓单
    bool HasPosition() const;
    bool HasPendingOrder() const;
    void TryAggressiveClose(cwMarketDataPtr pPriceData, cwPositionPtr pPos);

private:
    cwBasicKindleStrategy* ctx;
    std::string instrumentID;

    cwPositionPtr position;
    std::map<cwActiveOrderKey, cwOrderPtr> waitOrders;

    int lastAttemptTime = 0;
    int retryCount = 0;
    bool finished = false;
};
