#include "cwSignalOrderExecutor.h"

cwSignalOrderExecutor::cwSignalOrderExecutor(cwBasicKindleStrategy* context, std::map<std::string, orderInfo>& cwOrderInfo, std::map<cwActiveOrderKey, cwOrderPtr>& WaitOrderList)
	:ctx(context), cwOrderInfo(cwOrderInfo), WaitOrderList(WaitOrderList) {
}

void cwSignalOrderExecutor::OnPriceUpdate(cwMarketDataPtr pPriceData) {
	if (!pPriceData) return;

	auto [hour, minute, second] = IsTradingTime();
	std::string instrumentID = pPriceData->InstrumentID;
	std::string productID(ctx->GetProductID(instrumentID.c_str()));

	if (!IsNormalTradingTime(hour, minute)) return;
	if (cwOrderInfo.find(productID) == cwOrderInfo.end()) return;

	orderInfo& info = cwOrderInfo[productID];
	cwPositionPtr pPos = nullptr;
	ctx->GetPositionsAndActiveOrders(instrumentID, pPos, WaitOrderList);

	int now = GetCurrentTimeInSeconds();
	bool hasOrder = IsPendingOrder(instrumentID, WaitOrderList);

	if (lastCloseAttemptTime[instrumentID] == 0 || now - lastCloseAttemptTime[instrumentID] >= 5) {
		lastCloseAttemptTime[instrumentID] = now;

		if (IsAllClosed(pPos, info.volume)) {
			cwOrderInfo.erase(productID);
			closeAttemptCount.erase(instrumentID);
			return;
		}

		if (!hasOrder) {
			ctx->EasyInputOrder(info.szInstrumentID.c_str(), info.volume, info.price);
		}
		else {
			if (++closeAttemptCount[instrumentID] > 3) {
				std::cout << "[" << instrumentID << "] 清仓尝试次数过多，请人工干预。" << std::endl;
				return;
			}

			CancelExistingOrders(instrumentID);
			TryAggressiveClose(pPriceData, pPos);
		}
	}
}

bool cwSignalOrderExecutor::IsAllClosed(cwPositionPtr pPos, int targetVol) {
	if (!pPos) return false;
	return (targetVol == pPos->LongPosition->TodayPosition || targetVol == pPos->ShortPosition->TodayPosition);
}

void cwSignalOrderExecutor::CancelExistingOrders(const std::string& instrumentID) {
	for (auto& [key, order] : WaitOrderList) {
		if (key.InstrumentID == instrumentID) {
			ctx->CancelOrder(order);
		}
	}
}

bool cwSignalOrderExecutor::IsNormalTradingTime(int hour, int minute) {
	// 根据你框架的定义写一个是否处于正常交易时段的判断
	return (hour >= 9 && hour < 15); // 示例
}

void cwSignalOrderExecutor::TryAggressiveClose(cwMarketDataPtr pPriceData, cwPositionPtr pPos)
{
	auto& InstrumentID = pPriceData->InstrumentID;
	double aggressiveBid = pPriceData->BidPrice1 + ctx->GetTickSize(InstrumentID);
	double aggressiveAsk = pPriceData->AskPrice1 - ctx->GetTickSize(InstrumentID);
	if (pPos->LongPosition->TotalPosition > 0 && aggressiveBid > 1e-6)
	{
		ctx->EasyInputMultiOrder(InstrumentID, -pPos->LongPosition->TotalPosition, aggressiveBid);
		std::cout << "[" << InstrumentID << "] 平多仓 -> 数量: " << pPos->LongPosition->TotalPosition << ", 价格: " << aggressiveBid << std::endl;
	}// 重新挂 Bid
	if (pPos->ShortPosition->TotalPosition > 0 && aggressiveAsk > 1e-6)
	{
		ctx->EasyInputMultiOrder(InstrumentID, pPos->ShortPosition->TotalPosition, aggressiveAsk);
		std::cout << "[" << InstrumentID << "] 平空仓 -> 数量: " << pPos->ShortPosition->TotalPosition << ", 价格: " << aggressiveAsk << std::endl;
	}// 重新挂 Ask
}
