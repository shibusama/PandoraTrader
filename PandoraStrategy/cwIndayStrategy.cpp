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
#include "cwCloserLoop.h"

//清仓所需全局变量
static std::unordered_map<std::string, bool> instrumentCloseFlag;      // 是否触发收盘平仓
static std::unordered_map<std::string, int> lastCloseAttemptTime;      // 合约->上次清仓尝试时间戳（秒）
static std::unordered_map<std::string, int> closeAttemptCount;         // 用于控制重挂频率（每个合约）

//交易所需全局变量
static std::unordered_map<std::string, orderInfo> cwOrderInfo;

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

	if (IsNormalTradingTime(hour, minute) && !(cwOrderInfo.find(productID) == cwOrderInfo.end())) {

		orderInfo& info = cwOrderInfo[productID];
		cwPositionPtr pPos = nullptr;
		GetPositionsAndActiveOrders(InstrumentID, pPos, strategyWaitOrderList); // 获取指定持仓和挂单列表

		int now = GetCurrentTimeInSeconds();
		bool hasOrder = IsPendingOrder(InstrumentID);

		if (lastCloseAttemptTime[InstrumentID] == 0 || now - lastCloseAttemptTime[InstrumentID] >= 5) //挂单超时撤单判断是单一的：5秒设置根据合约活跃度动态调整时间；或者基于盘口价差判断是否撤单更具优势。
		{
			lastCloseAttemptTime[InstrumentID] = now;

			bool result = (info.volume == pPos->LongPosition->TodayPosition) ? true : (info.volume == pPos->ShortPosition->TodayPosition) ? true : false;

			if (result) {
				cwOrderInfo.erase(productID);
				closeAttemptCount.erase(InstrumentID);
				return;
			}
			if (!hasOrder) {
				EasyInputOrder(info.szInstrumentID.c_str(), info.volume, info.price);
			}
			else if (hasOrder)
			{
				if (++closeAttemptCount[InstrumentID] > 3) {
					std::cout << "[" << InstrumentID << "] 超过最大次数，还未挂上单子，请人工检查。" << std::endl;
					return;
				}
				for (auto& [key, order] : strategyWaitOrderList)
				{
					if (key.InstrumentID == InstrumentID) {
						CancelOrder(order);
					}
				}
				std::cout << "[" << InstrumentID << "] 撤销未成交挂单，准备重新挂单..." << std::endl;
				if (pPos) { TryAggressiveClose(pPriceData, pPos); }
				int count = std::count_if(strategyWaitOrderList.begin(), strategyWaitOrderList.end(), [&](const auto& pair) { return pair.first.InstrumentID == InstrumentID; });
				std::cout << "[" << InstrumentID << "] 等待挂单成交中，挂单数：" << count << std::endl;
			}

		}
	}

	if (IsClosingTime(hour, minute) && !instrumentCloseFlag[InstrumentID])
	{
		int now = GetCurrentTimeInSeconds();

		if (lastCloseAttemptTime[InstrumentID] == 0 || now - lastCloseAttemptTime[InstrumentID] >= 5)
		{
			lastCloseAttemptTime[InstrumentID] = now;                       // 更新尝试时间

			cwPositionPtr pPos = nullptr;

			GetPositionsAndActiveOrders(InstrumentID, pPos, strategyWaitOrderList); // 获取指定持仓和挂单列表

			bool hasPos = (pPos && (pPos->LongPosition->TotalPosition > 0 || pPos->ShortPosition->TotalPosition > 0));
			bool hasOrder = IsPendingOrder(InstrumentID);

			// 情况 1：无持仓 + 无挂单 => 清仓完毕
			if (!hasPos && !hasOrder)
			{
				std::cout << "[" << InstrumentID << "] 持仓清空完毕。" << std::endl;
				instrumentCloseFlag[InstrumentID] = true;
				closeAttemptCount.erase(InstrumentID);
				return;
			}
			// 情况 2：有持仓 + 无挂单 => 初次挂清仓单
			else if (hasPos && !hasOrder)
			{
				TryAggressiveClose(pPriceData, pPos);
				std::cout << "[" << InstrumentID << "] 清仓指令已发送。" << std::endl;
				return;
			}
			// 情况 3：有挂单 或 有持仓 => 撤单 + 重新挂清仓单
			else
			{
				if (++closeAttemptCount[InstrumentID] > 3) {
					std::cout << "[" << InstrumentID << "] 超过最大等待次数，可能仍有未清仓持仓，请人工检查。" << std::endl;
					instrumentCloseFlag[InstrumentID] = true;
					return;
				}
				else
				{
					for (auto& [key, order] : strategyWaitOrderList) {
						if (key.InstrumentID == InstrumentID) {
							CancelOrder(order);
						}
					}
					std::cout << "[" << InstrumentID << "] 撤销未成交挂单，准备重新挂单..." << std::endl;
					if (pPos) { TryAggressiveClose(pPriceData, pPos); }
					int count = std::count_if(strategyWaitOrderList.begin(), strategyWaitOrderList.end(), [&](const auto& pair) { return pair.first.InstrumentID == InstrumentID; });
					std::cout << "[" << InstrumentID << "] 等待挂单成交中，挂单数：" << count << std::endl;
				}
			}
		}
	}

	//cwInstrumentCloser closer(this, "rb2409");
	//closer.RunOnce(hour, minute, second);
	//if (closer.IsFinished()) {
	//	// done
	//}

}

void cwIndayStrategy::OnBar(cwMarketDataPtr pPriceData, int iTimeScale, cwBasicKindleStrategy::cwKindleSeriesPtr pKindleSeries) {
	if (pPriceData.get() == NULL) { return; }

	timePara tp{};
	tp = IsTradingTime();
	auto [hour, minute, second] = std::make_tuple(tp.hour, tp.minute, tp.second);
	
 	if (IsNormalTradingTime(hour, minute))
	{
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
	}
	else if (IsAfterMarket(hour, minute))
	{
		std::cout << "----------------- TraderOver ----------------" << std::endl;
		std::cout << "--------------- StoreBaseData ---------------" << std::endl;
		std::cout << "---------------------------------------------" << std::endl;
	}
	else
	{
		if (minute == 0 || minute == 10 || minute == 20 || minute == 30 || minute == 40 || minute == 50)
		{
			std::cout << "waiting" << hour << "::" << minute << "::" << second << std::endl;
		}
	}
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

void cwIndayStrategy::TryAggressiveClose(cwMarketDataPtr pPriceData, cwPositionPtr pPos)
{
	auto& InstrumentID = pPriceData->InstrumentID;
	double aggressiveBid = pPriceData->BidPrice1 + GetTickSize(InstrumentID);
	double aggressiveAsk = pPriceData->AskPrice1 - GetTickSize(InstrumentID);
	if (pPos->LongPosition->TotalPosition > 0 && aggressiveBid > 1e-6)
	{
		EasyInputMultiOrder(InstrumentID, -pPos->LongPosition->TotalPosition, aggressiveBid);
		std::cout << "[" << InstrumentID << "] 平多仓 -> 数量: " << pPos->LongPosition->TotalPosition << ", 价格: " << aggressiveBid << std::endl;
	}// 重新挂 Bid
	if (pPos->ShortPosition->TotalPosition > 0 && aggressiveAsk > 1e-6)
	{
		EasyInputMultiOrder(InstrumentID, pPos->ShortPosition->TotalPosition, aggressiveAsk);
		std::cout << "[" << InstrumentID << "] 平空仓 -> 数量: " << pPos->ShortPosition->TotalPosition << ", 价格: " << aggressiveAsk << std::endl;
	}// 重新挂 Ask
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

cwOrderPtr cwIndayStrategy::SafeLimitOrder(const char* instrumentID, int volume, double rawPrice, double slipTick)
{
	auto md = GetLastestMarketData(instrumentID);
	if (!md) {
		m_cwShow.AddLog("[SafeLimitOrder] No market data for {}", instrumentID);
		return nullptr;
	}

	double upLimit = md->UpperLimitPrice;
	double lowLimit = md->LowerLimitPrice;
	double tickSize = GetTickSize(instrumentID);
	if (tickSize <= 0) {
		m_cwShow.AddLog("[SafeLimitOrder] Invalid tick size for {}", instrumentID);
		return nullptr;
	}

	// 滑价保护
	double safePrice = rawPrice;
	double slip = slipTick * tickSize;

	if (volume > 0) { // 买
		if (rawPrice >= upLimit) {
			safePrice = upLimit - slip;
			safePrice = (((safePrice) > (lowLimit + tickSize)) ? (safePrice) : (lowLimit + tickSize)); // 防止越界
		}
	}
	else if (volume < 0) { // 卖
		if (rawPrice <= lowLimit) {
			safePrice = lowLimit + slip;
			safePrice = (((safePrice) < (upLimit - tickSize)) ? (safePrice) : (upLimit - tickSize)); // 防止越界
		}
	}
	else {
		m_cwShow.AddLog("[SafeLimitOrder] Volume = 0, no order sent.");
		return nullptr;
	}

	// 最终下单
	cwOrderPtr order = EasyInputOrder(
		instrumentID,
		volume,
		safePrice
	);

	m_cwShow.AddLog("[SafeLimitOrder] Order sent: {} volume={} price={} (raw={})", instrumentID, volume, safePrice, rawPrice);

	return order;
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
