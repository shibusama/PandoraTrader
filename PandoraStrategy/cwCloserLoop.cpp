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
//	std::cout << "������������" << std::endl;
//}
//
//void cwCloserLoop::UpdatePositions() {
//	closerCurrentPosMap.clear();
//	strategy->GetPositions(closerCurrentPosMap);
//
//	for (auto& [id, pos] : closerCurrentPosMap) {
//		// ���� pos �� LongPos �� ShortPos �ֶ�
//		if (pos->LongPosition->TotalPosition == 0 && pos->ShortPosition->TotalPosition == 0) {
//			continue;  // �����ֲ�Ϊ0�ĺ�Լ
//		}
//
//		// ��ӡ�ֲֺ�Լ���ֲ�����
//		m_cwShow.AddLog("[�ֲֺ�Լ] %s ��ͷ: %d ��ͷ: %d",
//			id.c_str(),
//			pos->LongPosition->TotalPosition,
//			pos->ShortPosition->TotalPosition);
//		// ��ӵ� instrumentStates
//		instrumentStates[id] = CloserInstrumentState{ pos, CloseState::Waiting, 0 };
//	}
//}
//
//void cwCloserLoop::HandleInstrument(const std::string& id, CloserInstrumentState& state) {
//	auto md = strategy->GetLastestMarketData(id);
//	if (!md) {
//		m_cwShow.AddLog("[%s] ����Ч�������ݣ�������", id.c_str());
//		return;
//	}
//
//	bool noLong = state.position->LongPosition->TotalPosition == 0;
//	bool noShort = state.position->ShortPosition->TotalPosition == 0;
//	bool noOrder = !IsPendingOrder(id);
//
//	// �����ɼ��
//	if (noLong && noShort && noOrder) {
//		m_cwShow.AddLog("[%s] �ֲ������ϡ�", id.c_str());
//		state.state = CloseState::Closed;
//		return;
//	}
//
//	// ����������Դ���
//	if (state.retryCount >= 3) {
//		m_cwShow.AddLog("[%s] ��������Դ��������ʧ�ܡ�", id.c_str());
//		state.state = CloseState::Failed;
//		return;
//	}
//
//	/****************************************************************************/
//	// ����гֲֵ��޹ҵ���ֱ�ӷ�����ֶ���
//	if ((!noLong || !noShort) && noOrder) {
//		bool success = TryAggressiveClose(md, state.position);
//		if (success) {
//			m_cwShow.AddLog("[%s] ���ָ���ѷ��͡�", id.c_str());
//			state.state = CloseState::OrderSent;
//		}
//		else {
//			m_cwShow.AddLog("[%s] ���ָ���ʧ�ܣ������ԡ�", id.c_str());
//			++state.retryCount;
//		}
//		return;
//	}
//
//	// ����йҵ����ȳ���
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
//				m_cwShow.AddLog("[%s] ����ʧ��: OrderRef=%s", id.c_str(), order->OrderRef);
//			}
//		}
//
//		if (allCancelled) {
//			m_cwShow.AddLog("[%s] ���йҵ��ѳ������������µ�", id.c_str());
//		}
//		else {
//			m_cwShow.AddLog("[%s] ����δ�ɹ������������ȴ����ּ���", id.c_str());
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
//            order->OrderStatus == CW_FTDC_OST_NoTradeQueueing) { // �������ö�ٴ���
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
//	double raw_price = 0.0;  // �����ں���������
//
//	if (volume > 0) { // ����
//		raw_price = md->AskPrice1;
//
//		// �����һ����Ч��
//		if (raw_price <= 0 || raw_price > upLimit) {
//			// ��һ����Ч������ͣ��ֱ������ͣ�ۼ��ٻ���
//			safePrice = upLimit - slip;
//			m_cwShow.AddLog("[SafeLimitOrder] Ask price invalid/limit up, using adjusted limit price");
//		}
//		else {
//			// ���������ʹ����һ�ۣ����ּۣ�
//			safePrice = raw_price;
//		}
//	}
//	else if (volume < 0) { // ����
//		raw_price = md->BidPrice1;
//
//		// �����һ����Ч��  
//		if (raw_price <= 0 || raw_price < lowLimit) {
//			// ��һ����Ч���ѵ�ͣ��ֱ���õ�ͣ�ۼ���������
//			safePrice = lowLimit + slip;
//			m_cwShow.AddLog("[SafeLimitOrder] Bid price invalid/limit down, using adjusted limit price");
//		}
//		else {
//			// ���������ʹ����һ�ۣ����ּۣ�
//			safePrice = raw_price;
//		}
//	}
//
//	// ���ռ۸������tickSize��������
//	safePrice = round(safePrice / tickSize) * tickSize;
//
//	// �ؼ�������ȷ���۸����ǵ�ͣ��Χ��
//	safePrice = MAX(safePrice, lowLimit);
//	safePrice = MIN(safePrice, upLimit);
//
//	// �����µ�
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
//	keys.reserve(m.size()); // Ԥ����ռ�
//	for (const auto& pair : m) {
//		keys.push_back(pair.first);
//	}
//	return keys;
//}
