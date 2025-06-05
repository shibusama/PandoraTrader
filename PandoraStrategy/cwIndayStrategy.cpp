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

	GetPositionsAndActiveOrders(InstrumentID, pPos, strategyWaitOrderList); // ��ȡָ���ֲֺ͹ҵ��б�

	if (!(cwOrderInfo.find(productID) == cwOrderInfo.end())) {

		orderInfo& info = cwOrderInfo[productID];

		int now = GetCurrentTimeInSeconds();

		bool hasOrder = IsPendingOrder(InstrumentID);

		if (lastCloseAttemptTime[InstrumentID] == 0 || now - lastCloseAttemptTime[InstrumentID] >= 5) //�ҵ���ʱ�����ж��ǵ�һ�ģ�5�����ø��ݺ�Լ��Ծ�ȶ�̬����ʱ�䣻���߻����̿ڼ۲��ж��Ƿ񳷵��������ơ�
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
					m_cwShow.AddLog("[%s] ���ָ���ѷ��͡�", InstrumentID);
				}
				else {
					m_cwShow.AddLog("[%s] ���ָ���ʧ�ܣ������ԡ�", InstrumentID);
				}
				return;
			}
			else if (hasOrder)
			{
				if (++closeAttemptCount[InstrumentID] > 3) {
					m_cwShow.AddLog("[%s] ��������������δ���ϵ��ӣ����˹���顣", InstrumentID);
					return;
				}
				bool allCancelled = true;
				for (auto& [key, order] : strategyWaitOrderList)
				{
					if (!CancelOrder(order)) {
						allCancelled = false;
						m_cwShow.AddLog("[%s] ����ʧ��: OrderRef=%s", InstrumentID, order->OrderRef);
					}
				}
				if (allCancelled) {
					m_cwShow.AddLog("[%s] ���йҵ��ѳ������������µ�", InstrumentID);
				}
				else {
					m_cwShow.AddLog("[%s] ����δ�ɹ������������ȴ����ּ���", InstrumentID);
				}
			}
		}
	}

	if (IsClosingTime(hour, minute) && !instrumentCloseFlag[InstrumentID])
	{
		int now = GetCurrentTimeInSeconds();

		if (lastCloseAttemptTime[InstrumentID] == 0 || now - lastCloseAttemptTime[InstrumentID] >= 5)
		{
			lastCloseAttemptTime[InstrumentID] = now;  // ���³���ʱ��

			bool hasPos = (pPos && (pPos->LongPosition->TotalPosition > 0 || pPos->ShortPosition->TotalPosition > 0));
			bool hasOrder = strategyWaitOrderList.empty();

			// ��� 1���޳ֲ� + �޹ҵ� => ������
			if (!hasPos && !hasOrder)
			{
				m_cwShow.AddLog("[%s] �ֲ������ϡ�", InstrumentID);
				instrumentCloseFlag[InstrumentID] = true;
				closeAttemptCount.erase(InstrumentID);
				return;
			}
			// ��� 2���гֲ� + �޹ҵ� => ���ι���ֵ�
			else if (hasPos && !hasOrder)
			{
				bool success = TryAggressiveClose(pPriceData, pPos);
				if (success) {
					m_cwShow.AddLog("[%s] ���ָ���ѷ��͡�", InstrumentID);
				}
				else {
					m_cwShow.AddLog("[%s] ���ָ���ʧ�ܣ������ԡ�", InstrumentID);
				}
			}
			// ��� 3���йҵ� �� �гֲ� => ���� + ���¹���ֵ�
			else
			{
				if (++closeAttemptCount[InstrumentID] > 3) {
					std::cout << "[" << InstrumentID << "] �������ȴ���������������δ��ֲֳ֣����˹���顣" << std::endl;
					instrumentCloseFlag[InstrumentID] = true;
					return;
				}
				bool allCancelled = true;
				for (auto& [key, order] : strategyWaitOrderList)
				{
					if (!CancelOrder(order)) {
						allCancelled = false;
						m_cwShow.AddLog("[%s] ����ʧ��: OrderRef=%s", InstrumentID, order->OrderRef);
					}
				}
				if (allCancelled) {
					m_cwShow.AddLog("[%s] ���йҵ��ѳ������������µ�", InstrumentID);
				}
				else {
					m_cwShow.AddLog("[%s] ����δ�ɹ������������ȴ����ּ���", InstrumentID);
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

	GetPositionsAndActiveOrders(pPriceData->InstrumentID, pPos, strategyWaitOrderList); // ��ȡָ���ֲֺ͹ҵ��б�


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
	// ����ҵ��� key
	cwActiveOrderKey key(pOrder->OrderRef, pOrder->InstrumentID);

	// ����ֻ������ WaitOrderList ��׷�ٵĹҵ�
	auto it = strategyWaitOrderList.find(key);
	if (it == strategyWaitOrderList.end()) return;


	auto status = pOrder->OrderStatus;// ����״̬����Ҫ�ж��Ƿ������
	auto submitStatus = pOrder->OrderSubmitStatus;// �����ύ״̬����Ҫ�ж��Ƿ������

	if (status == CW_FTDC_OST_AllTraded ||   // ȫ���ɽ�
		status == CW_FTDC_OST_Canceled)    // ����     
	{
		// ��־��¼
		std::cout << "Order Finished - InstrumentID: " << pOrder->InstrumentID
			<< ", Ref: " << pOrder->OrderRef
			<< ", Status: " << status << std::endl;

		// �Ƴ��ùҵ�
		strategyWaitOrderList.erase(it);
	}
	else if (submitStatus == CW_FTDC_OSS_InsertRejected)// �ܵ�
	{
		// ��־��¼
		std::cout << "Order Finished - InstrumentID: " << pOrder->InstrumentID
			<< ", Ref: " << pOrder->OrderRef
			<< ", Status: " << submitStatus << std::endl;
	}
	else {
		// ��ѡ����Ҳ���Ը��¸ùҵ�����Ϣ�����ֳɽ������ȣ�
	}

}

void cwIndayStrategy::OnOrderCanceled(cwOrderPtr pOrder)
{
	if (pOrder->OrderStatus == '5') { // �ܵ�
		std::cout << "[AutoClose] �ܵ�: " << pOrder->InstrumentID << std::endl;
		//pendingContracts.erase(pOrder->InstrumentID); // ǿ���Ƴ�����������
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

	// ÿ 1 �봥��һ�Σ����󶨾����Լ
	//SetTimer(1, 1000);

	// ÿ 2 �봥��һ�Σ���ĳ����Լ
	//SetTimer(2, 2000, "au2508");
}

void cwIndayStrategy::UpdateBarData() {

	//�������ݿ�����
	sqlite3* mydb = OpenDatabase("dm.db");
	if (mydb)
	{
		std::string tar_contract_sql = "SELECT * FROM futureinfo;";
		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(mydb, tar_contract_sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
			while (sqlite3_step(stmt) == SQLITE_ROW) {
				std::string contract = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));//Ŀ���Լ
				int multiple = sqlite3_column_int(stmt, 1);//��Լ����
				std::string Fac = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));//Fac
				int Rs = sqlite3_column_int(stmt, 3); // ...
				int Rl = sqlite3_column_int(stmt, 4); // ...
				std::string code = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5)); //Ŀ���Լ����
				double accfactor = sqlite3_column_double(stmt, 6);//��֤����
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
			int comboMultiple = 2;  // ��ϲ����������ܸ�
			int tarCount = tarFutInfo.size();  // Ŀ���Լ����
			double numLimit = comboMultiple * 1000000 / tarCount / comBarInfo.barFlow[futInfMng.first].back() / futInfMng.second.multiple;  // ���Ըܸ�����1000000 Ϊ���Ի����ʽ�λ�� 20ΪĿǰ����Ʒ�ֵĽ���ֵ�� ���̼۸񣬱�֤�����
			countLimitCur[futInfMng.first] = (numLimit >= 1) ? static_cast<int>(numLimit) : 1;  // ���� ȡ��һ��
		}
		CloseDatabase(mydb);  // ���ر����ݿ�����
	}
	for (const auto& futInfMng : tarFutInfo) { instrumentCloseFlag[futInfMng.first] = false; }
}

void cwIndayStrategy::AutoCloseAllPositionsLoop() {
	std::map<std::string, cwPositionPtr> CurrentPosMap;   // ��Լ -> ��λ��Ϣ
	std::map<cwActiveOrderKey, cwOrderPtr> WaitOrderList; // ��Լ -> ������Ϣ
	std::map<std::string, int> pendingRetryCounter;       // ��Լ -> �ҳ�������
	std::map<std::string, bool> instrumentCloseFlag;      // ��Լ -> �Ƿ񴥷�ƽ��

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
				if (!md) { std::cout << "[" << id << "] ����Ч�������ݣ�������" << std::endl; continue; }

				bool noLong = pos->LongPosition->YdPosition == 0;
				bool noShort = pos->ShortPosition->YdPosition == 0;
				bool noOrder = !IsPendingOrder(id);

				// ��� 1: �޳ֲ� + �޹ҵ� => ������
				if (noLong && noShort && noOrder)
				{
					std::cout << "[" << id << "] �ֲ������ϡ�" << std::endl;
					instrumentCloseFlag[id] = true;
					continue;
				}
				//��� 2: �гֲ� + �޹ҵ� => ����ƽ�ֵ�
				else if ((!noLong || !noShort) && noOrder) {
					TryAggressiveClose(md, CurrentPosMap[id]);
					std::cout << "[" << md->InstrumentID << "] ���ָ���ѷ��͡�" << std::endl;
				}
				// ��� 3: �йҵ� �� �гֲ� => ���� + ���¹���ֵ�
				else
				{
					if (pendingRetryCounter[id] >= 3) {
						std::cout << "[" << id << "] ��������Դ��������ʧ�ܡ�" << std::endl;
						instrumentCloseFlag[id] = true;
						continue;
					}
					else {
						++pendingRetryCounter[id];
						std::cout << "[" << id << "] ���ڹҵ��������عң����Ե� " << pendingRetryCounter[id] << " �Σ�" << std::endl;

						for (auto& [key, order] : WaitOrderList) { if (key.InstrumentID == id) { CancelOrder(order); } }// ����
						if (CurrentPosMap[id]) { TryAggressiveClose(md, CurrentPosMap[id]); }//�ع�
					}
				}
			}
			cwSleep(5000);
		}
		else
		{
			std::cout << "���гֲ�����գ��޹ҵ����˳����ѭ����" << std::endl;
			break;
		}
	}
}

void cwIndayStrategy::UpdateCtx(cwMarketDataPtr pPriceData)
{
	std::string productID(GetProductID(pPriceData->InstrumentID));

	//barFolw����
	comBarInfo.barFlow[productID].push_back(pPriceData->LastPrice);
	//queueBar����

	comBarInfo.queueBar[productID].push_back(pPriceData->LastPrice / tarFutInfo[productID].accfactor);
	//ret����
	double last = comBarInfo.queueBar[productID][comBarInfo.queueBar[productID].size() - 1];
	double secondLast = comBarInfo.queueBar[productID][comBarInfo.queueBar[productID].size() - 2];
	comBarInfo.retBar[productID].push_back(last / secondLast - 1);
	//ɾ����λԪ��
	comBarInfo.barFlow[productID].pop_front();
	comBarInfo.queueBar[productID].pop_front();
	comBarInfo.retBar[productID].pop_front();
}

void cwIndayStrategy::StrategyPosOpen(cwMarketDataPtr pPriceData, std::unordered_map<std::string, orderInfo>& cwOrderInfo)
{
	std::string productID(GetProductID(pPriceData->InstrumentID));

	// ���� stdLong
	std::vector<double> retBarSubsetLong(std::prev(comBarInfo.retBar[productID].end(), tarFutInfo[productID].Rl), comBarInfo.retBar[productID].end());
	double stdLong = SampleStd(retBarSubsetLong);

	// ���� stdShort
	std::vector<double> retBarSubsetShort(std::prev(comBarInfo.retBar[productID].end(), tarFutInfo[productID].Rs), comBarInfo.retBar[productID].end());
	double stdShort = SampleStd(retBarSubsetShort);

	auto& barQueue = comBarInfo.queueBar[productID];

	// ���¼۸� < ���ڼ۸� && ���ڲ����� > ���ڲ�����
	bool cond1 = (barQueue.back() < barQueue[barQueue.size() - tarFutInfo[productID].Rs] && stdShort > stdLong);
	// ���¼۸� > ���ڼ۸� && ���ڲ����� > ���ڲ�����
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

	// ���� stdLong
	std::vector<double> retBarSubsetLong(std::prev(comBarInfo.retBar[productID].end(), tarFutInfo[productID].Rl), comBarInfo.retBar[productID].end());
	double stdLong = SampleStd(retBarSubsetLong);
	// ���� stdShort
	std::vector<double> retBarSubsetShort(std::prev(comBarInfo.retBar[productID].end(), tarFutInfo[productID].Rs), comBarInfo.retBar[productID].end());
	double stdShort = SampleStd(retBarSubsetShort);

	std::string dire = GetPositionDirection(pPos);  // ��ǰ�ֲַ���

	auto DireREFunc = [](const std::string& x) -> std::string {if (x == "Long") return "Short"; if (x == "Short") return "Long"; return "Miss"; };
	std::string FacDirection = (tarFutInfo[productID].Fac == "Mom_std_bar_re_dym") ? dire : DireREFunc(dire);

	//Fac���� =�� && �����¼۸� > ���ڼ۸� || ���ڲ�����<=���ڲ����ʣ�
	bool shouldOpenLong = FacDirection == "Long" && (barQueue.back() > barQueue[barQueue.size() - rs] || stdShort <= stdLong);
	//Fac���� =�� && �����¼۸� < ���ڼ۸� || ���ڲ�����<=���ڲ����ʣ�
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
		printf("[��ʱ��2] ÿ2�봥������Լ: %s\n", szInstrumentID);
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
//            order->OrderStatus == CW_FTDC_OST_NoTradeQueueing) { // �������ö�ٴ���
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
	double raw_price = 0.0;  // �����ں���������

	if (volume > 0) { // ����
		raw_price = md->AskPrice1;

		// �����һ����Ч��
		if (raw_price <= 0 || raw_price > upLimit) {
			// ��һ����Ч������ͣ��ֱ������ͣ�ۼ��ٻ���
			safePrice = upLimit - slip;
			m_cwShow.AddLog("[SafeLimitOrder] Ask price invalid/limit up, using adjusted limit price");
		}
		else {
			// ���������ʹ����һ�ۣ����ּۣ�
			safePrice = raw_price;
		}
	}
	else if (volume < 0) { // ����
		raw_price = md->BidPrice1;

		// �����һ����Ч��  
		if (raw_price <= 0 || raw_price < lowLimit) {
			// ��һ����Ч���ѵ�ͣ��ֱ���õ�ͣ�ۼ���������
			safePrice = lowLimit + slip;
			m_cwShow.AddLog("[SafeLimitOrder] Bid price invalid/limit down, using adjusted limit price");
		}
		else {
			// ���������ʹ����һ�ۣ����ּۣ�
			safePrice = raw_price;
		}
	}

	// ���ռ۸������tickSize��������
	safePrice = round(safePrice / tickSize) * tickSize;

	// �ؼ�������ȷ���۸����ǵ�ͣ��Χ��
	safePrice = MAX(safePrice, lowLimit);
	safePrice = MIN(safePrice, upLimit);

	// �����µ�
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