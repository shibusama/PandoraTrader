#pragma once
#include <map>
#include <string>
#include <algorithm>
//#include "cwContext.h" // 保留，含 cwPositionPtr, cwOrderPtr 等定义
#include "cwBasicKindleStrategy.h" // 包含 GetPositionsAndActiveOrders 定义
#include "cwBasicCout.h"

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))

enum class CloseState {
	Waiting,
	OrderSent,
	PendingCancel,
	Closed,
	Failed
};

struct CloserInstrumentState {
	cwPositionPtr position;
	CloseState state = CloseState::Waiting;
	int retryCount = 0;

	bool isDone() const { return state == CloseState::Closed || state == CloseState::Failed; }
};

class cwCloserLoop {
public:
	explicit cwCloserLoop(cwBasicKindleStrategy* strategy);

	void Run();
	cwBasicCout				m_cwShow;

private:
	void UpdatePositions();
	void HandleInstrument(const std::string& instrumentID, CloserInstrumentState& state);
	bool IsAllDone() const;
	bool TryAggressiveClose(cwMarketDataPtr pPriceData, cwPositionPtr pPos);
	bool IsPendingOrder(std::string instrumentID);
	cwOrderPtr SafeLimitOrder(cwMarketDataPtr pPriceData, int volume, double slipTick, double tickSize);
	template <typename MapType>
	std::vector<typename MapType::key_type> ExtractMapKeys(const MapType& m);

	cwBasicKindleStrategy* strategy;

	std::map<std::string, CloserInstrumentState> instrumentStates;
	std::map<std::string, cwPositionPtr> closerCurrentPosMap;
	std::map<cwActiveOrderKey, cwOrderPtr> closerWaitOrderList;
	static constexpr int MAX_RETRY_COUNT = 3;
	static constexpr int SLEEP_INTERVAL_MS = 5000;
	static constexpr double DEFAULT_SLIP_TICK = 1.0;
};
