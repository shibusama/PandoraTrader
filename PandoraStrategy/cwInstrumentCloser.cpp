#include "cwInstrumentCloser.h"
#include <algorithm>
#include <chrono>
//#include "Utils.hpp"

cwInstrumentCloser::cwInstrumentCloser(cwBasicKindleStrategy* context, const std::string& instrumentID)
    : ctx(context), instrumentID(instrumentID) {
}

void cwInstrumentCloser::RunOnce(int hour, int minute, int second) {
    if (finished || !IsClosingTime(hour, minute)) return;

    int now = GetCurrentTimeInSeconds();
    if (lastAttemptTime != 0 && now - lastAttemptTime < 5) return;
    lastAttemptTime = now;

    FetchData();

    if (!HasPosition() && !HasPendingOrder()) {
        std::cout << "[" << instrumentID << "] �����ϡ�" << std::endl;
        finished = true;
        return;
    }

    if (HasPosition() && !HasPendingOrder()) {
        TryClose();
        std::cout << "[" << instrumentID << "] ���ι���ֵ���" << std::endl;
        return;
    }

    if (++retryCount > 3) {
        std::cout << "[" << instrumentID << "] ����������Դ��������ʧ�ܡ�" << std::endl;
        finished = true;
        return;
    }

    CancelPendingOrders();
    TryClose();

    int count = std::count_if(waitOrders.begin(), waitOrders.end(),
        [&](const auto& pair) { return pair.first.InstrumentID == instrumentID; });

    std::cout << "[" << instrumentID << "] �����عҵ� " << retryCount << " �Σ���ǰ�ҵ���: " << count << std::endl;
}

bool cwInstrumentCloser::IsFinished() const {
    return finished;
}

void cwInstrumentCloser::FetchData() {
    position = nullptr;
    waitOrders.clear();
    ctx->GetPositionsAndActiveOrders(instrumentID, position, waitOrders);
}

void cwInstrumentCloser::CancelPendingOrders() {
    for (auto& [key, order] : waitOrders) {
        if (key.InstrumentID == instrumentID) {
            ctx->CancelOrder(order);
        }
    }
}

void cwInstrumentCloser::TryClose() {
    auto md = ctx->GetLastestMarketData(instrumentID);
    if (!md || !position) return;
    TryAggressiveClose(md, position);
}

bool cwInstrumentCloser::HasPosition() const {
    return position && (position->LongPosition->TotalPosition > 0 || position->ShortPosition->TotalPosition > 0);
}

bool cwInstrumentCloser::HasPendingOrder() const {
    return IsPendingOrder(instrumentID, waitOrders);
}

void cwInstrumentCloser::TryAggressiveClose(cwMarketDataPtr pPriceData, cwPositionPtr pPos)
{
    auto& InstrumentID = pPriceData->InstrumentID;
    double aggressiveBid = pPriceData->BidPrice1 + ctx->GetTickSize(InstrumentID);
    double aggressiveAsk = pPriceData->AskPrice1 - ctx->GetTickSize(InstrumentID);
    if (pPos->LongPosition->TotalPosition > 0 && aggressiveBid > 1e-6)
    {
        ctx->EasyInputMultiOrder(InstrumentID, -pPos->LongPosition->TotalPosition, aggressiveBid);
        std::cout << "[" << InstrumentID << "] ƽ��� -> ����: " << pPos->LongPosition->TotalPosition << ", �۸�: " << aggressiveBid << std::endl;
    }// ���¹� Bid
    if (pPos->ShortPosition->TotalPosition > 0 && aggressiveAsk > 1e-6)
    {
        ctx->EasyInputMultiOrder(InstrumentID, pPos->ShortPosition->TotalPosition, aggressiveAsk);
        std::cout << "[" << InstrumentID << "] ƽ�ղ� -> ����: " << pPos->ShortPosition->TotalPosition << ", �۸�: " << aggressiveAsk << std::endl;
    }// ���¹� Ask
}
