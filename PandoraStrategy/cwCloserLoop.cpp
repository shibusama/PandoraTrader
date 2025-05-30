//#include "cwCloserLoop.h"
//#include <iostream>
////
//cwCloserLoop::cwCloserLoop(cwBasicKindleStrategy* strategy, std::map<cwActiveOrderKey, cwOrderPtr>& sharedOrderList)
//	: strategy(strategy), closerWaitOrderList(sharedOrderList){
//}
//
//void cwCloserLoop::Run() {
//	UpdatePositions();
//	std::vector<std::string> instruments = ExtractMapKeys(closerCurrentPosMap);
//	strategy->SubScribePrice(instruments);
//
//	int loopCount = 0;
//	const int maxLoop = 10;
//
//	while (!IsAllDone() && loopCount++ < maxLoop) {
//		strategy->GetPositionsAndActiveOrders(closerCurrentPosMap, closerWaitOrderList);
//
//		for (auto& [id, state] : instrumentStates) {
//			if (state.isDone()) continue;
//			HandleInstrument(id, state);
//		}
//
//		cwSleep(5000);
//	}
//
//	strategy->UnSubScribePrice(instruments);
//	std::cout << "清仓任务结束。" << std::endl;
//}
//
//void cwCloserLoop::UpdatePositions() {
//	closerCurrentPosMap.clear();
//	strategy->GetPositions(closerCurrentPosMap);
//
//	for (auto& [id, pos] : closerCurrentPosMap) {
//		// 假设 pos 有 LongPos 和 ShortPos 字段
//		if (pos->LongPosition->TotalPosition == 0 && pos->ShortPosition->TotalPosition == 0) {
//			continue;  // 跳过持仓为0的合约
//		}
//
//		// 打印持仓合约及持仓数量
//		m_cwShow.AddLog("[持仓合约] %s 多头: %d 空头: %d",
//			id.c_str(),
//			pos->LongPosition->TotalPosition,
//			pos->ShortPosition->TotalPosition);
//		// 添加到 instrumentStates
//		instrumentStates[id] = CloserInstrumentState{ pos, CloseState::Waiting, 0 };
//	}
//}
//
//void cwCloserLoop::HandleInstrument(const std::string& id, CloserInstrumentState& state) {
//	auto md = strategy->GetLastestMarketData(id);
//	if (!md) {
//		m_cwShow.AddLog("[%s] 无有效行情数据，跳过。", id.c_str());
//		return;
//	}
//
//	bool noLong = state.position->LongPosition->TotalPosition == 0;
//	bool noShort = state.position->ShortPosition->TotalPosition == 0;
//	bool noOrder = !IsPendingOrder(id);
//
//	// 清仓完成检查
//	if (noLong && noShort && noOrder) {
//		m_cwShow.AddLog("[%s] 持仓清空完毕。", id.c_str());
//		state.state = CloseState::Closed;
//		return;
//	}
//
//	// 超过最大重试次数
//	if (state.retryCount >= 3) {
//		m_cwShow.AddLog("[%s] 超过最大尝试次数，清仓失败。", id.c_str());
//		state.state = CloseState::Failed;
//		return;
//	}
//
//	/****************************************************************************/
//	// 如果有持仓但无挂单，直接发送清仓订单
//	if ((!noLong || !noShort) && noOrder) {
//		bool success = TryAggressiveClose(md, state.position);
//		if (success) {
//			m_cwShow.AddLog("[%s] 清仓指令已发送。", id.c_str());
//			state.state = CloseState::OrderSent;
//		}
//		else {
//			m_cwShow.AddLog("[%s] 清仓指令发送失败，将重试。", id.c_str());
//			++state.retryCount;
//		}
//		return;
//	}
//
//	// 如果有挂单，先撤单
//	if (!noOrder) {
//		std::map<cwActiveOrderKey, cwOrderPtr> closerWaitOrderList;
//		cwPositionPtr unused;
//		strategy->GetPositionsAndActiveOrders(id, unused, closerWaitOrderList);
//
//		bool allCancelled = true;
//		for (auto& [key, order] : closerWaitOrderList) {
//			if (order->OrderStatus == CW_FTDC_OST_AllTraded || order->OrderStatus == CW_FTDC_OST_Canceled) continue;
//			if (!strategy->CancelOrder(order)) {
//				allCancelled = false;
//				m_cwShow.AddLog("[%s] 撤单失败: OrderRef=%s", id.c_str(), order->OrderRef);
//			}
//		}
//
//		if (allCancelled) {
//			m_cwShow.AddLog("[%s] 所有挂单已撤，下轮重试下单", id.c_str());
//		}
//		else {
//			m_cwShow.AddLog("[%s] 存在未成功撤销订单，等待下轮继续", id.c_str());
//		}
//		++state.retryCount;
//	}
//}
//
//bool cwCloserLoop::IsAllDone() const {
//	for (const auto& [id, state] : instrumentStates) {
//		if (!state.isDone()) return false;
//	}
//	return true;
//}
//
//bool cwCloserLoop::TryAggressiveClose(cwMarketDataPtr md, cwPositionPtr pPos)
//{
//	auto& InstrumentID = md->InstrumentID;
//	auto& longPos = pPos->LongPosition->TotalPosition;
//	auto& shortPos = pPos->ShortPosition->TotalPosition;
//	auto tickSize = strategy->GetTickSize(InstrumentID);
//
//	bool success = true;
//
//	if (longPos > 0) {
//		auto order = SafeLimitOrder(md, -longPos, 1, tickSize);
//		if (!order) success = false;
//	}
//	if (shortPos > 0) {
//		auto order = SafeLimitOrder(md, shortPos, 1, tickSize);
//		if (!order) success = false;
//	}
//	return success;
//}
//
//bool cwCloserLoop::IsPendingOrder(std::string instrumentID)
//{
//    std::map<cwActiveOrderKey, cwOrderPtr> closerWaitOrderList;
//    strategy->GetActiveOrders(closerWaitOrderList);
//    
//    for (const auto& [key, order] : closerWaitOrderList) {
//        if (key.InstrumentID == instrumentID &&
//            order->OrderStatus == CW_FTDC_OST_NoTradeQueueing) { // 假设这个枚举存在
//            return true;
//        }
//    }
//    return false;
//}
//
//
//cwOrderPtr cwCloserLoop::SafeLimitOrder(cwMarketDataPtr md, int volume, double slipTick, double tickSize)
//{
//	if (tickSize <= 0) {
//		m_cwShow.AddLog("[SafeLimitOrder] Invalid tick size for %s", md->InstrumentID);
//		return nullptr;
//	}
//
//	if (volume == 0) {
//		m_cwShow.AddLog("[SafeLimitOrder] Volume = 0, no order sent.");
//		return nullptr;
//	}
//
//	double upLimit = md->UpperLimitPrice;
//	double lowLimit = md->LowerLimitPrice;
//	double slip = slipTick * tickSize;
//	double safePrice = 0.0;
//	double raw_price = 0.0;  // 声明在函数作用域
//
//	if (volume > 0) { // 买入
//		raw_price = md->AskPrice1;
//
//		// 检查卖一价有效性
//		if (raw_price <= 0 || raw_price > upLimit) {
//			// 卖一价无效或已涨停，直接用涨停价减少滑价
//			safePrice = upLimit - slip;
//			m_cwShow.AddLog("[SafeLimitOrder] Ask price invalid/limit up, using adjusted limit price");
//		}
//		else {
//			// 正常情况，使用卖一价（对手价）
//			safePrice = raw_price;
//		}
//	}
//	else if (volume < 0) { // 卖出
//		raw_price = md->BidPrice1;
//
//		// 检查买一价有效性  
//		if (raw_price <= 0 || raw_price < lowLimit) {
//			// 买一价无效或已跌停，直接用跌停价加少量滑价
//			safePrice = lowLimit + slip;
//			m_cwShow.AddLog("[SafeLimitOrder] Bid price invalid/limit down, using adjusted limit price");
//		}
//		else {
//			// 正常情况，使用买一价（对手价）
//			safePrice = raw_price;
//		}
//	}
//
//	// 最终价格必须是tickSize的整数倍
//	safePrice = round(safePrice / tickSize) * tickSize;
//
//	// 关键修正：确保价格在涨跌停范围内
//	safePrice = MAX(safePrice, lowLimit);
//	safePrice = MIN(safePrice, upLimit);
//
//	// 最终下单
//	cwOrderPtr order = strategy->EasyInputOrder(
//		md->InstrumentID,
//		volume,
//		safePrice
//	);
//
//	if (order) {
//		m_cwShow.AddLog("[SafeLimitOrder] Order sent successfully: %s volume=%d price=%.2f (raw=%.2f)",
//			md->InstrumentID, volume, safePrice, raw_price);
//	}
//	else {
//		m_cwShow.AddLog("[SafeLimitOrder] Order failed: %s volume=%d price=%.2f",
//			md->InstrumentID, volume, safePrice);
//	}
//	return order;
//}
//
//template <typename MapType>
//std::vector<typename MapType::key_type> cwCloserLoop::ExtractMapKeys(const MapType& m) {
//	std::vector<typename MapType::key_type> keys;
//	keys.reserve(m.size()); // 预分配空间
//	for (const auto& pair : m) {
//		keys.push_back(pair.first);
//	}
//	return keys;
//}
