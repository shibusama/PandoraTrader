#include "cwCloserLoop.h"
#include <iostream>
//
cwCloserLoop::cwCloserLoop(cwBasicKindleStrategy* strategy)
	: strategy(strategy) {
}

void cwCloserLoop::Run() {
	UpdatePositions();
	std::vector<std::string> instruments = ExtractMapKeys(closerCurrentPosMap);
	strategy->SubScribePrice(instruments);

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

	strategy->UnSubScribePrice(instruments);
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

	bool noLong = state.position->LongPosition->TodayPosition == 0;
	bool noShort = state.position->ShortPosition->TodayPosition == 0;
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

void cwCloserLoop::TryAggressiveClose(cwMarketDataPtr md, cwPositionPtr pPos)
{
	auto& InstrumentID = md->InstrumentID;
	auto& longPos = pPos->LongPosition->TotalPosition;
	auto& shortPos = pPos->ShortPosition->TotalPosition;
	auto tickSize = strategy->GetTickSize(InstrumentID);

	if (longPos > 0) {
		SafeLimitOrder(md, -longPos, 1, tickSize);
	}
	if (shortPos > 0) {
		SafeLimitOrder(md, shortPos, 1, tickSize);
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

void cwCloserLoop::SafeLimitOrder(cwMarketDataPtr md, int volume, double slipTick,double tickSize)
{

	double upLimit = md->UpperLimitPrice;
	double lowLimit = md->LowerLimitPrice;
	
	if (tickSize <= 0) {
		m_cwShow.AddLog("[SafeLimitOrder] Invalid tick size for %s", md->InstrumentID);

	}
	// 滑价保护
	auto &raw_price = md->LastPrice;
	double safePrice = raw_price;
	double slip = slipTick * tickSize;

	if (volume > 0) { // 买
		if (raw_price >= upLimit) {
			safePrice = upLimit - slip;
			safePrice = safePrice = MAX(safePrice, lowLimit + tickSize); // 防止越界
		}
	}
	else if (volume < 0) { // 卖
		if (raw_price <= lowLimit) {
			safePrice = lowLimit + slip;
			safePrice = safePrice = MIN(safePrice, upLimit - tickSize); // 防止越界
		}
	}
	else {
		m_cwShow.AddLog("[SafeLimitOrder] Volume = 0, no order sent.");
	}

	// 最终下单
	cwOrderPtr order = strategy->EasyInputOrder(
		md->InstrumentID,
		volume,
		safePrice
	);
	m_cwShow.AddLog("[SafeLimitOrder] Order sent: %s volume=%d price=%.2f (raw=%.2f)", md->InstrumentID, volume, safePrice, raw_price);

}

template <typename MapType>
std::vector<typename MapType::key_type> cwCloserLoop::ExtractMapKeys(const MapType& m) {
	std::vector<typename MapType::key_type> keys;
	keys.reserve(m.size()); // 预分配空间
	for (const auto& pair : m) {
		keys.push_back(pair.first);
	}
	return keys;
}
