#include "cwCloserLoop.h"
#include <iostream>
//
cwCloserLoop::cwCloserLoop(cwBasicKindleStrategy* strategy)
    : strategy(strategy) {}

void cwCloserLoop::Run() {
    UpdatePositions();

    int loopCount = 0;
    const int maxLoop = 3;

    while (!IsAllDone() && loopCount++ < maxLoop) {
        strategy->GetPositionsAndActiveOrders(closerCurrentPosMap, closerWaitOrderList);

        for (auto& [id, state] : instrumentStates) {
            if (state.isDone()) continue;
            HandleInstrument(id, state);
        }

        cwSleep(5000);
    }

    std::cout << "清仓任务结束。" << std::endl;
}

void cwCloserLoop::UpdatePositions() {
    closerCurrentPosMap.clear();
    strategy->GetPositions(closerCurrentPosMap);

    for (auto& [id, pos] : closerCurrentPosMap) {
        instrumentStates[id] = CloserInstrumentState{ pos, CloseState::Waiting, 0 };
    }
}

void cwCloserLoop::HandleInstrument(const std::string& id, CloserInstrumentState& state) {
    auto md = strategy->GetLastestMarketData(id);
    if (!md) {
        std::cout << "[" << id << "] 无有效行情数据，跳过。" << std::endl;
        return;
    }

    bool noLong = state.position->LongPosition->YdPosition == 0;
    bool noShort = state.position->ShortPosition->YdPosition == 0;
    bool noOrder = !IsPendingOrder(id);

    if (noLong && noShort && noOrder) {
        std::cout << "[" << id << "] 持仓清空完毕。" << std::endl;
        state.state = CloseState::Closed;
        return;
    }

    if ((!noLong || !noShort) && noOrder) {
        TryAggressiveClose(md, state.position);
        std::cout << "[" << id << "] 清仓指令已发送。" << std::endl;
        state.state = CloseState::OrderSent;
        return;
    }

    if (state.retryCount >= 3) {
        std::cout << "[" << id << "] 超过最大尝试次数，清仓失败。" << std::endl;
        state.state = CloseState::Failed;
        return;
    }

    std::map<cwActiveOrderKey, cwOrderPtr> activeOrders;
    cwPositionPtr unused;
    strategy->GetPositionsAndActiveOrders(id, unused, activeOrders);

    for (auto& [key, order] : activeOrders) {
        strategy->CancelOrder(order);
    }

    TryAggressiveClose(md, state.position);
    ++state.retryCount;

    std::cout << "[" << id << "] 存在挂单，撤单重挂（第 " << state.retryCount << " 次）" << std::endl;
}

bool cwCloserLoop::IsAllDone() const {
    for (const auto& [id, state] : instrumentStates) {
        if (!state.isDone()) return false;
    }
    return true;
}

void cwCloserLoop::TryAggressiveClose(cwMarketDataPtr pPriceData, cwPositionPtr pPos)
{
    auto& InstrumentID = pPriceData->InstrumentID;
    if (pPos->LongPosition->TotalPosition > 0)
    {
        SafeLimitOrder(InstrumentID, -pPos->LongPosition->TotalPosition,1);
        std::cout << "[" << InstrumentID << "] 平多仓 -> 数量: " << pPos->LongPosition->TotalPosition << ", 价格: " << "...." << std::endl;
    }
    if (pPos->ShortPosition->TotalPosition > 0 )
    {
        SafeLimitOrder(InstrumentID, pPos->ShortPosition->TotalPosition, 1);
        std::cout << "[" << InstrumentID << "] 平空仓 -> 数量: " << pPos->ShortPosition->TotalPosition << ", 价格: " << "..." << std::endl;
    }
}

bool cwCloserLoop::IsPendingOrder(std::string instrumentID)
{
    for (auto& [key, order] : closerWaitOrderList) {
        if (key.InstrumentID == instrumentID) {
            return true;
        }
    }
    return false;
}

void cwCloserLoop::SafeLimitOrder(const char* instrumentID, int volume, double slipTick)
{
    auto md = strategy->GetLastestMarketData(instrumentID);
    if (!md) {
        m_cwShow.AddLog("[SafeLimitOrder] No market data for {}", instrumentID);
    }

    double upLimit = md->UpperLimitPrice;
    double lowLimit = md->LowerLimitPrice;
    double tickSize = strategy->GetTickSize(instrumentID);
    if (tickSize <= 0) {
        m_cwShow.AddLog("[SafeLimitOrder] Invalid tick size for {}", instrumentID);
    }

    // 滑价保护
    double safePrice = md->LastPrice;
    double slip = slipTick * tickSize;

    if (volume > 0) { // 买
        if (md->LastPrice >= upLimit) {
            safePrice = upLimit - slip;
            safePrice = (((safePrice) > (lowLimit + tickSize)) ? (safePrice) : (lowLimit + tickSize)); // 防止越界
        }
    }
    else if (volume < 0) { // 卖
        if (md->LastPrice <= lowLimit) {
            safePrice = lowLimit + slip;
            safePrice = (((safePrice) < (upLimit - tickSize)) ? (safePrice) : (upLimit - tickSize)); // 防止越界
        }
    }
    else {
        m_cwShow.AddLog("[SafeLimitOrder] Volume = 0, no order sent.");
    }

    // 最终下单
    cwOrderPtr order = strategy->EasyInputOrder(
        instrumentID,
        volume,
        safePrice
    );

    m_cwShow.AddLog("[SafeLimitOrder] Order sent: {} volume={} price={} (raw={})", instrumentID, volume, safePrice, md->LastPrice);
}
