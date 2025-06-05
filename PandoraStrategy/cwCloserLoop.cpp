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
	std::cout << "清仓任务结束。" << std::endl;
}

void cwCloserLoop::UpdatePositions() {
	closerCurrentPosMap.clear();
	strategy->GetPositions(closerCurrentPosMap);

	for (auto& [id, pos] : closerCurrentPosMap) {
		// 假设 pos 有 LongPos 和 ShortPos 字段
		if (pos->LongPosition->TotalPosition == 0 && pos->ShortPosition->TotalPosition == 0) {
			continue;  // 跳过持仓为0的合约
		}

		// 打印持仓合约及持仓数量
		m_cwShow.AddLog("[持仓合约] %s 多头: %d 空头: %d",
			id.c_str(),
			pos->LongPosition->TotalPosition,
			pos->ShortPosition->TotalPosition);
		// 添加到 instrumentStates
		instrumentStates[id] = CloserInstrumentState{ pos, CloseState::Waiting, 0 };
	}
}

void cwCloserLoop::HandleInstrument(const std::string& id, CloserInstrumentState& state) {
	auto md = strategy->GetLastestMarketData(id);
	if (!md) {
		m_cwShow.AddLog("[%s] 无有效行情数据，跳过。", id.c_str());
		return;
	}

	bool noLong = state.position->LongPosition->TotalPosition == 0;
	bool noShort = state.position->ShortPosition->TotalPosition == 0;
	bool noOrder = !IsPendingOrder(id);

	// 清仓完成检查
	if (noLong && noShort && noOrder) {
		m_cwShow.AddLog("[%s] 持仓清空完毕。", id.c_str());
		state.state = CloseState::Closed;
		return;
	}

	// 超过最大重试次数
	if (state.retryCount >= 3) {
		m_cwShow.AddLog("[%s] 超过最大尝试次数，清仓失败。", id.c_str());
		state.state = CloseState::Failed;
		return;
	}

	/****************************************************************************/
	// 如果有持仓但无挂单，直接发送清仓订单
	if ((!noLong || !noShort) && noOrder) {
		bool success = TryAggressiveClose(md, state.position);
		if (success) {
			m_cwShow.AddLog("[%s] 清仓指令已发送。", id.c_str());
			state.state = CloseState::OrderSent;
		}
		else {
			m_cwShow.AddLog("[%s] 清仓指令发送失败，将重试。", id.c_str());
			++state.retryCount;
		}
		return;
	}

	// 如果有挂单，先撤单
	if (!noOrder) {
		std::map<cwActiveOrderKey, cwOrderPtr> closerWaitOrderList;
		cwPositionPtr unused;
		strategy->GetPositionsAndActiveOrders(id, unused, closerWaitOrderList);

		bool allCancelled = true;
		for (auto& [key, order] : closerWaitOrderList) {
			if (order->OrderStatus == CW_FTDC_OST_AllTraded || order->OrderStatus == CW_FTDC_OST_Canceled) continue;
			if (!strategy->CancelOrder(order)) {
				allCancelled = false;
				m_cwShow.AddLog("[%s] 撤单失败: OrderRef=%s", id.c_str(), order->OrderRef);
			}
		}

		if (allCancelled) {
			m_cwShow.AddLog("[%s] 所有挂单已撤，下轮重试下单", id.c_str());
		}
		else {
			m_cwShow.AddLog("[%s] 存在未成功撤销订单，等待下轮继续", id.c_str());
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
            order->OrderStatus == CW_FTDC_OST_NoTradeQueueing) { // 假设这个枚举存在
            return true;
        }
    }
    return false;
}
}

