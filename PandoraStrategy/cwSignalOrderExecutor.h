#pragma once

#include <string>
#include <map>
#include <memory>
//#include "YourFrameworkHeaders.h" // 替换为你自己的包含项
#include "cwBasicKindleStrategy.h" // 包含 GetPositionsAndActiveOrders 定义
#include "myStructs.h"
#include "Utils.hpp"

class cwSignalOrderExecutor {
    cwSignalOrderExecutor(cwBasicKindleStrategy* context, std::map<std::string, orderInfo>& cwOrderInfo, std::map<cwActiveOrderKey, cwOrderPtr>& WaitOrderList);
public:
    void OnPriceUpdate(cwMarketDataPtr pPriceData);

private:
    bool IsNormalTradingTime(int hour, int minute);
    bool IsAllClosed(cwPositionPtr pPos, int targetVol);
    void CancelExistingOrders(const std::string& instrumentID);
    void TryAggressiveClose(cwMarketDataPtr pPriceData, cwPositionPtr pPos);

    cwBasicKindleStrategy* ctx;

    std::map<std::string, int> lastCloseAttemptTime;
    std::map<std::string, int> closeAttemptCount;

    // 外部依赖（建议通过构造传入或改为静态依赖）
    std::map<std::string, orderInfo>& cwOrderInfo; //这里有引用成员，所以必须设定初始值****************很重要
    std::map<cwActiveOrderKey, cwOrderPtr>& WaitOrderList;
};
