#include "cwCloserLoop.h"
#include <iostream>
//


void cwCloserLoop::Run() {
	UpdatePositions();
	std::vector<std::string> instruments = ExtractMapKeys(closerCurrentPosMap);
	strategy->SubScribePrice(instruments);

	int loopCount = 0;
	const int maxLoop = 10;

	while (!IsAllDone() && loopCount++ < maxLoop) {
		strategy->GetPositionsAndActiveOrders(closerCurrentPosMap, closerWaitOrderList);

		for (auto& [id, state] : instrumentStates) {
			if (state.isDone()) continue;
			HandleInstrument(id, state);
		}

		cwSleep(5000);
	}

	strategy->UnSubScribePrice(instruments);
	std::cout << "������������" << std::endl;
}

void cwCloserLoop::UpdatePositions() {
	closerCurrentPosMap.clear();
	strategy->GetPositions(closerCurrentPosMap);

	for (auto& [id, pos] : closerCurrentPosMap) {
		// ���� pos �� LongPos �� ShortPos �ֶ�
		if (pos->LongPosition->TotalPosition == 0 && pos->ShortPosition->TotalPosition == 0) {
			continue;  // �����ֲ�Ϊ0�ĺ�Լ
		}

		// ��ӡ�ֲֺ�Լ���ֲ�����
		m_cwShow.AddLog("[�ֲֺ�Լ] %s ��ͷ: %d ��ͷ: %d",
			id.c_str(),
			pos->LongPosition->TotalPosition,
			pos->ShortPosition->TotalPosition);
		// ��ӵ� instrumentStates
		instrumentStates[id] = CloserInstrumentState{ pos, CloseState::Waiting, 0 };
	}
}

void cwCloserLoop::HandleInstrument(const std::string& id, CloserInstrumentState& state) {
	auto md = strategy->GetLastestMarketData(id);
	if (!md) {
		m_cwShow.AddLog("[%s] ����Ч�������ݣ�������", id.c_str());
		return;
	}

	bool noLong = state.position->LongPosition->TotalPosition == 0;
	bool noShort = state.position->ShortPosition->TotalPosition == 0;
	bool noOrder = !IsPendingOrder(id);

	// �����ɼ��
	if (noLong && noShort && noOrder) {
		m_cwShow.AddLog("[%s] �ֲ������ϡ�", id.c_str());
		state.state = CloseState::Closed;
		return;
	}

	// ����������Դ���
	if (state.retryCount >= 3) {
		m_cwShow.AddLog("[%s] ��������Դ��������ʧ�ܡ�", id.c_str());
		state.state = CloseState::Failed;
		return;
	}

	/****************************************************************************/
	// ����гֲֵ��޹ҵ���ֱ�ӷ�����ֶ���
	if ((!noLong || !noShort) && noOrder) {
		bool success = TryAggressiveClose(md, state.position);
		if (success) {
			m_cwShow.AddLog("[%s] ���ָ���ѷ��͡�", id.c_str());
			state.state = CloseState::OrderSent;
		}
		else {
			m_cwShow.AddLog("[%s] ���ָ���ʧ�ܣ������ԡ�", id.c_str());
			++state.retryCount;
		}
		return;
	}

	// ����йҵ����ȳ���
	if (!noOrder) {
		std::map<cwActiveOrderKey, cwOrderPtr> closerWaitOrderList;
		cwPositionPtr unused;
		strategy->GetPositionsAndActiveOrders(id, unused, closerWaitOrderList);

		bool allCancelled = true;
		for (auto& [key, order] : closerWaitOrderList) {
			if (order->OrderStatus == CW_FTDC_OST_AllTraded || order->OrderStatus == CW_FTDC_OST_Canceled) continue;
			if (!strategy->CancelOrder(order)) {
				allCancelled = false;
				m_cwShow.AddLog("[%s] ����ʧ��: OrderRef=%s", id.c_str(), order->OrderRef);
			}
		}

		if (allCancelled) {
			m_cwShow.AddLog("[%s] ���йҵ��ѳ������������µ�", id.c_str());
		}
		else {
			m_cwShow.AddLog("[%s] ����δ�ɹ������������ȴ����ּ���", id.c_str());
		}
		++state.retryCount;
	}
}

bool cwCloserLoop::IsAllDone() const {
	for (const auto& [id, state] : instrumentStates) {
		if (!state.isDone()) return false;
	}
	return true;
}

bool cwCloserLoop::IsPendingOrder(std::string instrumentID)
{
    std::map<cwActiveOrderKey, cwOrderPtr> closerWaitOrderList;
    strategy->GetActiveOrders(closerWaitOrderList);
    
    for (const auto& [key, order] : closerWaitOrderList) {
        if (key.InstrumentID == instrumentID &&
            order->OrderStatus == CW_FTDC_OST_NoTradeQueueing) { // �������ö�ٴ���
            return true;
        }
    }
    return false;
}
}

