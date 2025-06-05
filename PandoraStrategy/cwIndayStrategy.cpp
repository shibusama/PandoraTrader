#include "cwIndayStrategy.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <unordered_map>
#include <algorithm>
#include <iostream>
#include <string>
#include <map>
#include <regex>
#include <cmath>
#include <exception>
#include <format>
#include "myStructs.h"
#include "sqlite3.h"
#include "utils.hpp"
#include "sqlLiteHelp.hpp"

cwIndayStrategy::cwIndayStrategy()
{
}

cwIndayStrategy::~cwIndayStrategy()
{
}

void cwIndayStrategy::PriceUpdate(cwMarketDataPtr pPriceData)
{
	if (pPriceData.get() == NULL) { return; }

	auto [hour, minute, second] = IsTradingTime();

	auto& InstrumentID = pPriceData->InstrumentID;

	std::string productID(GetProductID(InstrumentID));

	std::map<cwActiveOrderKey, cwOrderPtr> strategyWaitOrderList;

	cwPositionPtr pPos = nullptr;

	GetPositionsAndActiveOrders(InstrumentID, pPos, strategyWaitOrderList); // 获取指定持仓和挂单列表

	if (!(cwOrderInfo.find(productID) == cwOrderInfo.end())) {

		orderInfo& info = cwOrderInfo[productID];

		int now = GetCurrentTimeInSeconds();

		bool hasOrder = IsPendingOrder(InstrumentID);

		if (lastCloseAttemptTime[InstrumentID] == 0 || now - lastCloseAttemptTime[InstrumentID] >= 5) //挂单超时撤单判断是单一的：5秒设置根据合约活跃度动态调整时间；或者基于盘口价差判断是否撤单更具优势。
		{
			lastCloseAttemptTime[InstrumentID] = now;

			bool result = false;

			if (pPos != nullptr) {
				if (pPos->LongPosition && info.volume == pPos->LongPosition->TodayPosition) {
					result = true;
				}
				else if (pPos->ShortPosition && info.volume == pPos->ShortPosition->TodayPosition) {
					result = true;
				}
			}

			if (result) {
				cwOrderInfo.erase(productID);
				closeAttemptCount.erase(InstrumentID);
				return;
			}
			if (!hasOrder) {
				bool success = TryAggressiveClose(pPriceData, pPos);
				if (success) {
					m_cwShow.AddLog("[%s] 清仓指令已发送。", InstrumentID);
				}
				else {
					m_cwShow.AddLog("[%s] 清仓指令发送失败，将重试。", InstrumentID);
				}
				return;
			}
			else if (hasOrder)
			{
				if (++closeAttemptCount[InstrumentID] > 3) {
					m_cwShow.AddLog("[%s] 超过最大次数，还未挂上单子，请人工检查。", InstrumentID);
					return;
				}
				bool allCancelled = true;
				for (auto& [key, order] : strategyWaitOrderList)
				{
					if (!CancelOrder(order)) {
						allCancelled = false;
						m_cwShow.AddLog("[%s] 撤单失败: OrderRef=%s", InstrumentID, order->OrderRef);
					}
				}
				if (allCancelled) {
					m_cwShow.AddLog("[%s] 所有挂单已撤，下轮重试下单", InstrumentID);
				}
				else {
					m_cwShow.AddLog("[%s] 存在未成功撤销订单，等待下轮继续", InstrumentID);
				}
			}
		}
	}

	if (IsClosingTime(hour, minute) && !instrumentCloseFlag[InstrumentID])
	{
		int now = GetCurrentTimeInSeconds();

		if (lastCloseAttemptTime[InstrumentID] == 0 || now - lastCloseAttemptTime[InstrumentID] >= 5)
		{
			lastCloseAttemptTime[InstrumentID] = now;  // 更新尝试时间

			bool hasPos = (pPos && (pPos->LongPosition->TotalPosition > 0 || pPos->ShortPosition->TotalPosition > 0));
			bool hasOrder = strategyWaitOrderList.empty();

			// 情况 1：无持仓 + 无挂单 => 清仓完毕
			if (!hasPos && !hasOrder)
			{
				m_cwShow.AddLog("[%s] 持仓清空完毕。", InstrumentID);
				instrumentCloseFlag[InstrumentID] = true;
				closeAttemptCount.erase(InstrumentID);
				return;
			}
			// 情况 2：有持仓 + 无挂单 => 初次挂清仓单
			else if (hasPos && !hasOrder)
			{
				bool success = TryAggressiveClose(pPriceData, pPos);
				if (success) {
					m_cwShow.AddLog("[%s] 清仓指令已发送。", InstrumentID);
				}
				else {
					m_cwShow.AddLog("[%s] 清仓指令发送失败，将重试。", InstrumentID);
				}
			}
			// 情况 3：有挂单 或 有持仓 => 撤单 + 重新挂清仓单
			else
			{
				if (++closeAttemptCount[InstrumentID] > 3) {
					std::cout << "[" << InstrumentID << "] 超过最大等待次数，可能仍有未清仓持仓，请人工检查。" << std::endl;
					instrumentCloseFlag[InstrumentID] = true;
					return;
				}
				bool allCancelled = true;
				for (auto& [key, order] : strategyWaitOrderList)
				{
					if (!CancelOrder(order)) {
						allCancelled = false;
						m_cwShow.AddLog("[%s] 撤单失败: OrderRef=%s", InstrumentID, order->OrderRef);
					}
				}
				if (allCancelled) {
					m_cwShow.AddLog("[%s] 所有挂单已撤，下轮重试下单", InstrumentID);
				}
				else {
					m_cwShow.AddLog("[%s] 存在未成功撤销订单，等待下轮继续", InstrumentID);
				}
			}
		}
	}
}

void cwIndayStrategy::OnBar(cwMarketDataPtr pPriceData, int iTimeScale, cwBasicKindleStrategy::cwKindleSeriesPtr pKindleSeries) {
	if (pPriceData.get() == NULL) { return; }

	timePara tp{};
	tp = IsTradingTime();
	auto [hour, minute, second] = std::make_tuple(tp.hour, tp.minute, tp.second);

	//if (IsNormalTradingTime(hour, minute))
	//{
	UpdateCtx(pPriceData);

	cwPositionPtr pPos = nullptr;

	GetPositionsAndActiveOrders(pPriceData->InstrumentID, pPos, strategyWaitOrderList); // 获取指定持仓和挂单列表


	if (!pPos)
	{
		StrategyPosOpen(pPriceData, cwOrderInfo);
	}
	else
	{
		StrategyPosClose(pPriceData, pPos, cwOrderInfo);
	}
	//}
	//else if (IsAfterMarket(hour, minute))
	//{
	//	std::cout << "----------------- TraderOver ----------------" << std::endl;
	//	std::cout << "--------------- StoreBaseData ---------------" << std::endl;
	//	std::cout << "---------------------------------------------" << std::endl;
	//}
	//else
	//{
	//	if (minute == 0 || minute == 10 || minute == 20 || minute == 30 || minute == 40 || minute == 50)
	//	{
	//		std::cout << "waiting" << hour << "::" << minute << "::" << second << std::endl;
	//	}
	//}
};

void cwIndayStrategy::OnRtnTrade(cwTradePtr pTrade)
{
}

void cwIndayStrategy::OnRtnOrder(cwOrderPtr pOrder, cwOrderPtr pOriginOrder)
{
	if (pOrder == nullptr) return;
	// 构造挂单的 key
	cwActiveOrderKey key(pOrder->OrderRef, pOrder->InstrumentID);

	// 我们只关心在 WaitOrderList 中追踪的挂单
	auto it = strategyWaitOrderList.find(key);
	if (it == strategyWaitOrderList.end()) return;


	auto status = pOrder->OrderStatus;// 报单状态（主要判断是否结束）
	auto submitStatus = pOrder->OrderSubmitStatus;// 报单提交状态（主要判断是否结束）

	if (status == CW_FTDC_OST_AllTraded ||   // 全部成交
		status == CW_FTDC_OST_Canceled)    // 撤单     
	{
		// 日志记录
		std::cout << "Order Finished - InstrumentID: " << pOrder->InstrumentID
			<< ", Ref: " << pOrder->OrderRef
			<< ", Status: " << status << std::endl;

		// 移除该挂单
		strategyWaitOrderList.erase(it);
	}
	else if (submitStatus == CW_FTDC_OSS_InsertRejected)// 拒单
	{
		// 日志记录
		std::cout << "Order Finished - InstrumentID: " << pOrder->InstrumentID
			<< ", Ref: " << pOrder->OrderRef
			<< ", Status: " << submitStatus << std::endl;
	}
	else {
		// 可选：你也可以更新该挂单的信息（部分成交数量等）
	}

}

void cwIndayStrategy::OnOrderCanceled(cwOrderPtr pOrder)
{
	if (pOrder->OrderStatus == '5') { // 拒单
		std::cout << "[AutoClose] 拒单: " << pOrder->InstrumentID << std::endl;
		//pendingContracts.erase(pOrder->InstrumentID); // 强制移除，避免阻塞
	}
}

void cwIndayStrategy::OnReady()
{

	UpdateBarData();

	//for (auto& futInfMng : tarFutInfo)
	//{
	//	SubcribeKindle(futInfMng.second.code.c_str(), cwKINDLE_TIMESCALE_1MIN, 50);
	//};

	SubcribeKindle("v2509", cwKINDLE_TIMESCALE_1MIN, 50);

	// 每 1 秒触发一次，不绑定具体合约
	//SetTimer(1, 1000);

	// 每 2 秒触发一次，绑定某个合约
	//SetTimer(2, 2000, "au2508");
}

void cwIndayStrategy::UpdateBarData() {

	//创建数据库连接
	sqlite3* mydb = OpenDatabase("dm.db");
	if (mydb)
	{
		std::string tar_contract_sql = "SELECT * FROM futureinfo;";
		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(mydb, tar_contract_sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
			while (sqlite3_step(stmt) == SQLITE_ROW) {
				std::string contract = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));//目标合约
				int multiple = sqlite3_column_int(stmt, 1);//合约乘数
				std::string Fac = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));//Fac
				int Rs = sqlite3_column_int(stmt, 3); // ...
				int Rl = sqlite3_column_int(stmt, 4); // ...
				std::string code = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5)); //目标合约代码
				double accfactor = sqlite3_column_double(stmt, 6);//保证金率
				tarFutInfo[contract] = { contract, multiple, Fac, Rs, Rl, code, accfactor };
			}
		}
		else if (sqlite3_prepare_v2(mydb, tar_contract_sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
			std::cerr << "SQL prepare failed: " << sqlite3_errmsg(mydb) << std::endl;
		}
		sqlite3_finalize(stmt);

		for (auto& futInfMng : tarFutInfo) {
			std::string ret_sql = std::format("SELECT ret FROM {} ORDER BY tradingday,timestamp ", futInfMng.first);
			sqlite3_stmt* stmt = nullptr;
			if (sqlite3_prepare_v2(mydb, ret_sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
				while (sqlite3_step(stmt) == SQLITE_ROW) {
					comBarInfo.retBar[futInfMng.first].push_back(sqlite3_column_double(stmt, 0));
				}
			}
			sqlite3_finalize(stmt);
		}

		for (auto& futInfMng : tarFutInfo) {
			std::string closeprice_sql = std::format("SELECT closeprice FROM {} ORDER BY tradingday,timestamp ", futInfMng.first);
			sqlite3_stmt* stmt = nullptr;
			if (sqlite3_prepare_v2(mydb, closeprice_sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
				while (sqlite3_step(stmt) == SQLITE_ROW) {
					comBarInfo.barFlow[futInfMng.first].push_back(sqlite3_column_double(stmt, 0));
				}
			}
			sqlite3_finalize(stmt);
		}

		for (auto& futInfMng : tarFutInfo) {
			std::string real_close_sql = std::format("SELECT real_close FROM {} ORDER BY tradingday,timestamp ", futInfMng.first);
			sqlite3_stmt* stmt = nullptr;
			if (sqlite3_prepare_v2(mydb, real_close_sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
				while (sqlite3_step(stmt) == SQLITE_ROW) {
					comBarInfo.queueBar[futInfMng.first].push_back(sqlite3_column_double(stmt, 0));
				}
			}
			sqlite3_finalize(stmt);
		}

		for (const auto& futInfMng : tarFutInfo) {
			int comboMultiple = 2;  // 组合策略做几倍杠杆
			int tarCount = tarFutInfo.size();  // 目标合约数量
			double numLimit = comboMultiple * 1000000 / tarCount / comBarInfo.barFlow[futInfMng.first].back() / futInfMng.second.multiple;  // 策略杠杆数，1000000 为策略基本资金单位， 20为目前覆盖品种的近似值， 收盘价格，保证金乘数
			countLimitCur[futInfMng.first] = (numLimit >= 1) ? static_cast<int>(numLimit) : 1;  // 整数 取舍一下
		}
		CloseDatabase(mydb);  // 最后关闭数据库连接
	}
	for (const auto& futInfMng : tarFutInfo) { instrumentCloseFlag[futInfMng.first] = false; }
}

void cwIndayStrategy::AutoCloseAllPositionsLoop() {
	std::map<std::string, cwPositionPtr> CurrentPosMap;   // 合约 -> 仓位信息
	std::map<cwActiveOrderKey, cwOrderPtr> WaitOrderList; // 合约 -> 订单信息
	std::map<std::string, int> pendingRetryCounter;       // 合约 -> 挂撤单次数
	std::map<std::string, bool> instrumentCloseFlag;      // 合约 -> 是否触发平仓

	GetPositions(CurrentPosMap);
	for (auto& [id, pos] : CurrentPosMap) { instrumentCloseFlag[id] = false; }

	while (true)
	{
		if (!AllInstrumentClosed(instrumentCloseFlag))
		{
			auto [hour, minute, second] = IsTradingTime();
			GetPositionsAndActiveOrders(CurrentPosMap, WaitOrderList);

			for (auto& [id, pos] : CurrentPosMap)
			{
				if (instrumentCloseFlag[id]) continue;
				auto md = GetLastestMarketData(id);
				if (!md) { std::cout << "[" << id << "] 无有效行情数据，跳过。" << std::endl; continue; }

				bool noLong = pos->LongPosition->YdPosition == 0;
				bool noShort = pos->ShortPosition->YdPosition == 0;
				bool noOrder = !IsPendingOrder(id);

				// 情况 1: 无持仓 + 无挂单 => 清仓完毕
				if (noLong && noShort && noOrder)
				{
					std::cout << "[" << id << "] 持仓清空完毕。" << std::endl;
					instrumentCloseFlag[id] = true;
					continue;
				}
				//情况 2: 有持仓 + 无挂单 => 发出平仓单
				else if ((!noLong || !noShort) && noOrder) {
					TryAggressiveClose(md, CurrentPosMap[id]);
					std::cout << "[" << md->InstrumentID << "] 清仓指令已发送。" << std::endl;
				}
				// 情况 3: 有挂单 或 有持仓 => 撤单 + 重新挂清仓单
				else
				{
					if (pendingRetryCounter[id] >= 3) {
						std::cout << "[" << id << "] 超过最大尝试次数，清仓失败。" << std::endl;
						instrumentCloseFlag[id] = true;
						continue;
					}
					else {
						++pendingRetryCounter[id];
						std::cout << "[" << id << "] 存在挂单，撤单重挂（尝试第 " << pendingRetryCounter[id] << " 次）" << std::endl;

						for (auto& [key, order] : WaitOrderList) { if (key.InstrumentID == id) { CancelOrder(order); } }// 撤单
						if (CurrentPosMap[id]) { TryAggressiveClose(md, CurrentPosMap[id]); }//重挂
					}
				}
			}
			cwSleep(5000);
		}
		else
		{
			std::cout << "所有持仓已清空，无挂单。退出清仓循环。" << std::endl;
			break;
		}
	}
}

void cwIndayStrategy::UpdateCtx(cwMarketDataPtr pPriceData)
{
	std::string productID(GetProductID(pPriceData->InstrumentID));

	//barFolw更新
	comBarInfo.barFlow[productID].push_back(pPriceData->LastPrice);
	//queueBar更新

	comBarInfo.queueBar[productID].push_back(pPriceData->LastPrice / tarFutInfo[productID].accfactor);
	//ret更新
	double last = comBarInfo.queueBar[productID][comBarInfo.queueBar[productID].size() - 1];
	double secondLast = comBarInfo.queueBar[productID][comBarInfo.queueBar[productID].size() - 2];
	comBarInfo.retBar[productID].push_back(last / secondLast - 1);
	//删除首位元素
	comBarInfo.barFlow[productID].pop_front();
	comBarInfo.queueBar[productID].pop_front();
	comBarInfo.retBar[productID].pop_front();
}

void cwIndayStrategy::StrategyPosOpen(cwMarketDataPtr pPriceData, std::unordered_map<std::string, orderInfo>& cwOrderInfo)
{
	std::string productID(GetProductID(pPriceData->InstrumentID));

	// 计算 stdLong
	std::vector<double> retBarSubsetLong(std::prev(comBarInfo.retBar[productID].end(), tarFutInfo[productID].Rl), comBarInfo.retBar[productID].end());
	double stdLong = SampleStd(retBarSubsetLong);

	// 计算 stdShort
	std::vector<double> retBarSubsetShort(std::prev(comBarInfo.retBar[productID].end(), tarFutInfo[productID].Rs), comBarInfo.retBar[productID].end());
	double stdShort = SampleStd(retBarSubsetShort);

	auto& barQueue = comBarInfo.queueBar[productID];

	// 最新价格 < 短期价格 && 短期波动率 > 长期波动率
	bool cond1 = (barQueue.back() < barQueue[barQueue.size() - tarFutInfo[productID].Rs] && stdShort > stdLong);
	// 最新价格 > 短期价格 && 短期波动率 > 长期波动率
	bool cond2 = (barQueue.back() > barQueue[barQueue.size() - 500] && stdShort > stdLong);

	if (cond1 || cond2) {
		int baseVolume = countLimitCur[productID];
		bool isMom = (tarFutInfo[productID].Fac == "Mom_std_bar_re_dym");

		cwOrderInfo[productID].volume = isMom ? baseVolume : -baseVolume;
		cwOrderInfo[productID].szInstrumentID = pPriceData->InstrumentID;
		cwOrderInfo[productID].price = comBarInfo.barFlow[productID].back();
	}
}

void cwIndayStrategy::StrategyPosClose(cwMarketDataPtr pPriceData, cwPositionPtr pPos, std::unordered_map<std::string, orderInfo>& cwOrderInfo)
{
	std::string productID(GetProductID(pPriceData->InstrumentID));

	auto& barQueue = comBarInfo.queueBar[productID];
	size_t rs = tarFutInfo[productID].Rs;

	// 计算 stdLong
	std::vector<double> retBarSubsetLong(std::prev(comBarInfo.retBar[productID].end(), tarFutInfo[productID].Rl), comBarInfo.retBar[productID].end());
	double stdLong = SampleStd(retBarSubsetLong);
	// 计算 stdShort
	std::vector<double> retBarSubsetShort(std::prev(comBarInfo.retBar[productID].end(), tarFutInfo[productID].Rs), comBarInfo.retBar[productID].end());
	double stdShort = SampleStd(retBarSubsetShort);

	std::string dire = GetPositionDirection(pPos);  // 当前持仓方向

	auto DireREFunc = [](const std::string& x) -> std::string {if (x == "Long") return "Short"; if (x == "Short") return "Long"; return "Miss"; };
	std::string FacDirection = (tarFutInfo[productID].Fac == "Mom_std_bar_re_dym") ? dire : DireREFunc(dire);

	//Fac方向 =买 && （最新价格 > 短期价格 || 短期波动率<=长期波动率）
	bool shouldOpenLong = FacDirection == "Long" && (barQueue.back() > barQueue[barQueue.size() - rs] || stdShort <= stdLong);
	//Fac方向 =卖 && （最新价格 < 短期价格 || 短期波动率<=长期波动率）
	bool shouldOpenShort = FacDirection == "Short" && (barQueue.back() < barQueue[barQueue.size() - rs] || stdShort <= stdLong);

	if (shouldOpenLong || shouldOpenShort) {
		int volSign = (tarFutInfo[productID].Fac == "Mom_std_bar_re_dym") ? 1 : -1;
		int baseVolume = countLimitCur[productID];

		cwOrderInfo[productID].volume = volSign * baseVolume;
		cwOrderInfo[productID].szInstrumentID = pPriceData->InstrumentID;
		cwOrderInfo[productID].price = comBarInfo.barFlow[productID].back();
	}
}

bool cwIndayStrategy::IsPendingOrder(std::string instrumentID)
{
	for (auto& [key, order] : strategyWaitOrderList) {
		if (key.InstrumentID == instrumentID) {
			return true;
		}
	}
	return false;
}

std::string cwIndayStrategy::GetPositionDirection(cwPositionPtr pPos)
{
	if (!pPos) return "other";

	bool hasLong = pPos->LongPosition && pPos->LongPosition->PosiDirection == CW_FTDC_D_Buy;
	bool hasShort = pPos->ShortPosition && pPos->ShortPosition->PosiDirection == CW_FTDC_D_Sell;

	if (hasLong && !hasShort) return "Long";
	if (!hasLong && hasShort) return "Short";
	return "other";
}

void cwIndayStrategy::OnStrategyTimer(int iTimerId, const char* szInstrumentID)
{
	if (iTimerId == 1)
	{
		std::map<std::string, cwPositionPtr> PositionMap;
		GetPositions(PositionMap);	///key OrderRef
		if (!PositionMap.empty())
		{
			m_cwShow.AddLog("%-12s %-10s %-8s %-12s %-12s %-14s %-10s",
				"InstrumentID", "Direction", "Volume", "OpenPriceAvg", "MktProfit", "ExchangeMargin", "OpenCost");
			for (const auto& [instrumentID, pos] : PositionMap)
			{
				if (pos->LongPosition->TotalPosition > 0)
				{
					auto& p = pos->LongPosition;
					m_cwShow.AddLog("%-12s %-10s %-8d %-12.1f %-12.1f %-14.1f %-10.1f",
						instrumentID.c_str(), "Long",
						p->TotalPosition, p->AveragePosPrice,
						p->PositionProfit, p->ExchangeMargin, p->OpenCost);
				}
				if (pos->ShortPosition->TotalPosition > 0)
				{
					auto& p = pos->ShortPosition;
					m_cwShow.AddLog("%-12s %-10s %-8d %-12.1f %-12.1f %-14.1f %-10.1f",
						instrumentID.c_str(), "Short",
						p->TotalPosition, p->AveragePosPrice,
						p->PositionProfit, p->ExchangeMargin, p->OpenCost);
				}
			}
		}
	}
	else if (iTimerId == 2)
	{
		printf("[定时器2] 每2秒触发，合约: %s\n", szInstrumentID);
	}
}

bool cwIndayStrategy::TryAggressiveClose(cwMarketDataPtr md, cwPositionPtr pPos)
{
	auto& InstrumentID = md->InstrumentID;
	auto& longPos = pPos->LongPosition->TotalPosition;
	auto& shortPos = pPos->ShortPosition->TotalPosition;
	auto tickSize = GetTickSize(InstrumentID);

	bool success = true;

	if (longPos > 0) {
		auto order = SafeLimitOrder(md, -longPos, 1, tickSize);
		if (!order) success = false;
	}
	if (shortPos > 0) {
		auto order = SafeLimitOrder(md, shortPos, 1, tickSize);
		if (!order) success = false;
	}
	return success;
}

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

cwOrderPtr cwIndayStrategy::SafeLimitOrder(cwMarketDataPtr md, int volume, double slipTick, double tickSize)
{
	if (tickSize <= 0) {
		m_cwShow.AddLog("[SafeLimitOrder] Invalid tick size for %s", md->InstrumentID);
		return nullptr;
	}

	if (volume == 0) {
		m_cwShow.AddLog("[SafeLimitOrder] Volume = 0, no order sent.");
		return nullptr;
	}

	double upLimit = md->UpperLimitPrice;
	double lowLimit = md->LowerLimitPrice;
	double slip = slipTick * tickSize;
	double safePrice = 0.0;
	double raw_price = 0.0;  // 声明在函数作用域

	if (volume > 0) { // 买入
		raw_price = md->AskPrice1;

		// 检查卖一价有效性
		if (raw_price <= 0 || raw_price > upLimit) {
			// 卖一价无效或已涨停，直接用涨停价减少滑价
			safePrice = upLimit - slip;
			m_cwShow.AddLog("[SafeLimitOrder] Ask price invalid/limit up, using adjusted limit price");
		}
		else {
			// 正常情况，使用卖一价（对手价）
			safePrice = raw_price;
		}
	}
	else if (volume < 0) { // 卖出
		raw_price = md->BidPrice1;

		// 检查买一价有效性  
		if (raw_price <= 0 || raw_price < lowLimit) {
			// 买一价无效或已跌停，直接用跌停价加少量滑价
			safePrice = lowLimit + slip;
			m_cwShow.AddLog("[SafeLimitOrder] Bid price invalid/limit down, using adjusted limit price");
		}
		else {
			// 正常情况，使用买一价（对手价）
			safePrice = raw_price;
		}
	}

	// 最终价格必须是tickSize的整数倍
	safePrice = round(safePrice / tickSize) * tickSize;

	// 关键修正：确保价格在涨跌停范围内
	safePrice = MAX(safePrice, lowLimit);
	safePrice = MIN(safePrice, upLimit);

	// 最终下单
	cwOrderPtr order = EasyInputOrder(
		md->InstrumentID,
		volume,
		safePrice
	);

	if (order) {
		m_cwShow.AddLog("[SafeLimitOrder] Order sent successfully: %s volume=%d price=%.2f (raw=%.2f)",
			md->InstrumentID, volume, safePrice, raw_price);
	}
	else {
		m_cwShow.AddLog("[SafeLimitOrder] Order failed: %s volume=%d price=%.2f",
			md->InstrumentID, volume, safePrice);
	}
	return order;

}