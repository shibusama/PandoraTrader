#include "Class1.h"
#include "SqlliteHelp.h"
#include "Strformatdate.h"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <string>
#include <map>
#include <regex>
#include <cmath>
#include <exception>
#include <chrono>

using namespace std;
namespace MyTrade {

	// 创建数据库连接
	sqlite3* Class1::cnn = SqlliteHelp::OpenDatabase("D:/sqllite/tmp.db");
	sqlite3* Class1::cnnSys = SqlliteHelp::OpenDatabase("D:/sqllite/tmp.db");

	map<mainCtrKeys, mainCtrValues>* Class1::MainInf = new map<mainCtrKeys, mainCtrValues>;
	map<string, vector<barFuture>>* Class1::barFlow = new map<string, vector<barFuture>>;

	map<string, vector<barFuture>>* Class1::barFlowCur = new map<string, vector<barFuture>>;
	map<string, double>* Class1::factorDictCur = new map<string, double>;
	map<string, string>* Class1::codeTractCur = new map<string, string>;
	map<string, futInfMng>* Class1::futInfDict = new map<string, futInfMng>;

	map<string, vector<double>>* Class1::queueBar = new map<string, vector<double>>;
	map<string, vector<double>>* Class1::retBar = new map<string, vector<double>>;

	map<string, catePortInf>* Class1::spePos = new map<string, catePortInf>;
	map<string, paraMng>* Class1::verDictCur = new map<string, paraMng>;
	map<string, int>* Class1::countLimitCur = new map<string, int>;

	vector<string>* Class1::tarCateList = new vector<string>;

	string cursor_str = Strformatdate::getCurrentDateString(); // 交易当天日期

	double Class1::ArithmeticMean(const vector<double>& arr) {//计算简单算数平均值
		if (arr.empty()) {
			return 0.0;
		}
		double sum = 0.0;
		for (const double num : arr) {
			sum += num;
		}
		return sum / static_cast<double>(arr.size());
	}

	double Class1::SampleStd(const vector<double>& arr) {
		double mean = ArithmeticMean(arr);
		double result = 0.0;
		for (const double num : arr) {
			result += pow(num - mean, 2);
		}
		return sqrt(result / (static_cast<double>(arr.size()) - 1));
	}

	/*INIT DAILY DATA*/
	void Class1::UpdateBarData() {
		// 获取60天近期交易日序列
		cout << "UPDATE BAR DATA >>>>>>" << endl;
		string sqlTradingDay = "select * from tradeday where tradingday < '" + cursor_str + "' order by tradingday DESC Limit 60;";
		sqlite3_stmt* stmt = nullptr;
		vector<string> tradeDate;
		if (sqlite3_prepare_v2(cnnSys, sqlTradingDay.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
			while (sqlite3_step(stmt) == SQLITE_ROW) {
				tradeDate.push_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
			}
		}
		sqlite3_finalize(stmt);
		sort(tradeDate.begin(), tradeDate.end());
		string preCur_str = tradeDate.back();
		map<string, string> calendar;
		for (const string& x : tradeDate) {
			tm tm = {};
			istringstream ss(x);
			ss >> get_time(&tm, "%Y%m%d");
			ostringstream oss;
			oss << put_time(&tm, "%Y-%m-%d");
			calendar[oss.str()] = x;
		}

		// 加载当天策略配置参数
		cout << " LOADING - verDictCur >>>" << endl;
		// -> g.verDictCur
		string sqlFacTag = "select * from facTag where tradingday='" + cursor_str + "';";
		sqlite3_stmt* stmt2 = nullptr;
		if (sqlite3_prepare_v2(cnn, sqlFacTag.c_str(), -1, &stmt2, nullptr) == SQLITE_OK) {
			while (sqlite3_step(stmt2) == SQLITE_ROW) {
				string contract = reinterpret_cast<const char*>(sqlite3_column_text(stmt2, 0));
				string Fac = reinterpret_cast<const char*>(sqlite3_column_text(stmt2, 1));
				string ver = reinterpret_cast<const char*>(sqlite3_column_text(stmt2, 2));
				int Rs = stoi(ver.substr(0, ver.find('-')));
				int Rl = stoi(ver.substr(ver.find('-') + 1));
				verDictCur->insert({ contract,{Fac, ver, "minute", Rs, Rl } });
				//(*verDictCur)[contract] = { Fac, ver, "minute", Rs, Rl };
			}
		}
		sqlite3_finalize(stmt2);
		for (const auto& kvp : *verDictCur) {
			cout << kvp.first << " - " << kvp.second.ver << " " << kvp.second.Fac << ";" << endl;
		}
		// 需要参与交易的交易代码 
		vector<string> tarCateList;
		for (const auto& kvp : *verDictCur) {
			tarCateList.push_back(kvp.first);
		}

		// g.futureinfo = 加载合约信息
		cout << " 加载合约信息 >>>" << endl;
		string sqlFutInfo = "select code, exchange, multiple, ticksize, marginrate from futureinfo;";
		sqlite3_stmt* stmt3 = nullptr;
		if (sqlite3_prepare_v2(cnnSys, sqlFutInfo.c_str(), -1, &stmt3, nullptr) == SQLITE_OK) {
			while (sqlite3_step(stmt3) == SQLITE_ROW) {
				string prefix = reinterpret_cast<const char*>(sqlite3_column_text(stmt3, 0));
				(*futInfDict)[prefix] = { reinterpret_cast<const char*>(sqlite3_column_text(stmt3, 1)),
									stoi(reinterpret_cast<const char*>(sqlite3_column_text(stmt3, 2))),
									stod(reinterpret_cast<const char*>(sqlite3_column_text(stmt3, 3))),
									stod(reinterpret_cast<const char*>(sqlite3_column_text(stmt3, 4))) };
			}
		}
		sqlite3_finalize(stmt3);


		// g.MainInf
		// MainInf 需要调整 
		map<string, string> tradeSftDict;
		for (size_t i = 0; i < tradeDate.size(); ++i) {
			tradeSftDict[tradeDate[i]] = (i == tradeDate.size() - 1) ? cursor_str : tradeDate[i + 1];
		}

		string sqlMainContract = "select tradingday, prefix, code, factor,accfactor from TraderOvk where tradingday>='" + tradeDate.front() + "' and tradingday<='" + preCur_str + "';";
		sqlite3_stmt* stmt4 = nullptr;
		if (sqlite3_prepare_v2(cnn, sqlMainContract.c_str(), -1, &stmt4, nullptr) == SQLITE_OK) {
			while (sqlite3_step(stmt4) == SQLITE_ROW) {
				string prefix = reinterpret_cast<const char*>(sqlite3_column_text(stmt4, 1));
				if (find(tarCateList.begin(), tarCateList.end(), prefix) != tarCateList.end()) {
					mainCtrKeys key = { tradeSftDict[reinterpret_cast<const char*>(sqlite3_column_text(stmt4, 0))], prefix };
					mainCtrValues value = { reinterpret_cast<const char*>(sqlite3_column_text(stmt4, 2)),
						stod(reinterpret_cast<const char*>(sqlite3_column_text(stmt4, 3))),
						stod(reinterpret_cast<const char*>(sqlite3_column_text(stmt4, 4))) };
					(*MainInf)[key] = value;
				}
			}
		}
		sqlite3_finalize(stmt4);

		// g.codeTarctCur = 目标交易合约
		for (const auto& sr : *MainInf) {
			if (sr.first.date == cursor_str) {
				(*codeTractCur)[sr.first.contract] = sr.second.code;
			}
		}
		// g.factorDictCur = 因子数据
		for (const auto& sr : *MainInf) {
			if (sr.first.date == cursor_str) {
				(*factorDictCur)[sr.second.code] = sr.second.accfactor;
			}
		}
		// g.barFlow
		// 加载历史分钟数据 >>>>>
		for (const string& date : tradeDate) {
			map<string, mainCtrValues> tmp;
			for (const auto& sr : (*MainInf)) {
				if (sr.first.date == date) {
					tmp[sr.second.code] = sr.second;
				}
			}
			vector<string> codes;
			for (const auto& kv : tmp) {
				codes.push_back(kv.first);
			}
			string codeSql;
			for (const string& code : codes) {
				codeSql += "'" + code + "',";
			}
			codeSql.pop_back();  // 去掉最后的逗号

			// 从数据库中读取数据 D:/Database/Sqlite - UPS
			string dbPath = "D:/Database/Sqlite-UPS/his" + date.substr(0, 4) + ".db";
			sqlite3* cnnBar;
			sqlite3_open(dbPath.c_str(), &cnnBar);
			string qur_codes = "select code, tradingday, timestamp, volume, closeprice  from bf" + date + " where code in (" + codeSql + ") and actionday='" + date.substr(0, 4) + "-" + date.substr(4, 2) + "-" + date.substr(6, 2) + "'"
				" and timestamp>='09:01:00' and timestamp<='14:45:00';";
			sqlite3_stmt* stmt5;
			if (sqlite3_prepare_v2(cnnBar, qur_codes.c_str(), -1, &stmt5, nullptr) == SQLITE_OK) {
				while (sqlite3_step(stmt5) == SQLITE_ROW) {
					string code = reinterpret_cast<const char*>(sqlite3_column_text(stmt5, 0));
					string contract = regex_replace(code, regex("\\d"), "");
					barFuture bf = { code, reinterpret_cast<const char*>(sqlite3_column_text(stmt5, 1)),
									reinterpret_cast<const char*>(sqlite3_column_text(stmt5, 2)),
									stoi(reinterpret_cast<const char*>(sqlite3_column_text(stmt5, 3))),
									stod(reinterpret_cast<const char*>(sqlite3_column_text(stmt5, 4))) };
					if ((*barFlow).count(contract) > 0) {
						(*barFlow)[contract].push_back(bf);
					}
					else {
						(*barFlow)[contract] = vector<barFuture>{ bf };
					}
				}
			}
			sqlite3_finalize(stmt5);
			sqlite3_close(cnnBar);
			cout << "### " << date << " over >>>" << endl;
		}
		// g.queueBar = 对刚加载的历史分钟数据复权处理 <期货换月会跳价，所以需要复权使得数据连续>
		//queueBar
		for (const auto& pair : *barFlow) {
			vector<barFuture> queueCtr(pair.second.begin(), pair.second.end());
			sort(queueCtr.begin(), queueCtr.end(), [](const barFuture& a, const barFuture& b) {
				if (a.tradingday == b.tradingday) {
					return a.timestamp < b.timestamp;
				}
				return a.tradingday < b.tradingday;
				});
			vector<double> tmpQueueBar;
			for (const barFuture& x : queueCtr) {
				mainCtrKeys key = { calendar[x.tradingday], pair.first };
				tmpQueueBar.push_back(x.price / (*MainInf)[key].accfactor);
			}
			(*queueBar)[pair.first] = tmpQueueBar;
		}
		// g.retBar = 对复权后的价格数据转为收益率数据
		map<string, vector<double>> retBar;
		for (const auto& pair : (*queueBar)) {
			vector<double> tmpRetBar;
			tmpRetBar.push_back(0);
			for (size_t i = 1; i < pair.second.size(); ++i) {
				tmpRetBar.push_back(pair.second[i] / pair.second[i - 1] - 1);
			}
			retBar[pair.first] = tmpRetBar;
		}

		// countLimt = 更具前一天的收盘价计算当天交易每个合约对应的数量
		for (const auto& pair : (*verDictCur)) {
			int comboMultiple = 2;  // 组合策略做几倍杠杆
			double numLimit = comboMultiple * 1000000 / 20 / (*barFlow).at(pair.first).back().price / (*futInfDict).at(pair.first).multiple;  // 策略杠杆数，1000000 为策略基本资金单位， 20为目前覆盖品种的近似值， 收盘价格，保证金乘数
			(*countLimitCur)[pair.first] = (numLimit >= 1) ? static_cast<int>(numLimit) : 1;  // 整数 取舍一下
		}

		// g.barFlowCur  = 创建新的用来收录当天的行情数据
		for (const auto& pair : (*factorDictCur)) {
			string contract = regex_replace(pair.first, regex("\\d"), "");
			(*barFlowCur)[contract] = vector<barFuture>();
		}
		// 关闭数据库连接
		sqlite3_close(cnnSys);
		sqlite3_close(cnn);

	}

	void Class1::UpdateFlow(unordered_map<string, cwMarketDataPtr> code2data, unordered_map<string, PositionFieldPtr> curPos) {
		// 记录最新持仓状况（方向，数量，成本价格，开仓成本，数量）
		(*spePos).clear();
		for (const auto& pair : curPos) {
			string codeDR = pair.first;
			PositionFieldPtr positionField = pair.second;

			if (positionField->TodayPosition != 0) {
				catePortInf cateInf;
				cateInf.direction = positionField->PosiDirection; //持仓方向
				cateInf.volume = positionField->TodayPosition;//持仓数量
				cateInf.openCost = positionField->OpenCost;//开仓成本
				string instrumentIDWithoutDigits = regex_replace(positionField->InstrumentID, regex("\\d"), "");
				if ((*futInfDict).count(instrumentIDWithoutDigits) > 0) {
					cateInf.costPrice = (positionField->OpenCost / positionField->TodayPosition / (*futInfDict)[instrumentIDWithoutDigits].multiple);
				}
				else {
					cout << "Error: No multiple information for " << positionField->InstrumentID << endl;
					continue;
				}
				if (cateInf.direction == "Long") {
					cateInf.amount = cateInf.volume;
				}
				else {
					cateInf.amount = -1 * cateInf.volume;
				}
				(*spePos)[positionField->InstrumentID] = cateInf;
			}
		}
		// 用 code2data 最新的切片行情数据更新 barFlowCur & queueBar & retBar 
		for (const auto& pair : (*factorDictCur)) {
			string code = pair.first;
			double factor = pair.second;
			if (code2data.count(code) > 0) {
				string contract = regex_replace(code, regex("\\d"), "");
				(*barFlowCur)[contract].push_back(barFuture{
					code2data[code]->InstrumentID,
					code2data[code]->TradingDay,
					code2data[code]->UpdateTime,
					code2data[code]->Volume,
					code2data[code]->LastPrice,
					});
				// g.queueBar/g.retBar -> update
				double curPrice = code2data[code]->LastPrice;
				(*queueBar)[contract].push_back(curPrice / factor);
				if ((*queueBar)[contract].size() >= 2) {
					(*retBar)[contract].push_back((curPrice / factor) / (*queueBar)[contract][(*queueBar)[contract].size() - 2] - 1);
				}
				else {
					(*retBar)[contract].push_back(0); // 处理数据不足的情况，例如添加默认值或不添加数据
				}
				if ((*queueBar)[contract].size() > 1) {
					(*queueBar)[contract].erase((*queueBar)[contract].begin());
					(*retBar)[contract].erase((*retBar)[contract].begin());
				}
			}
			else {
				cout << "MISS " << code << " Info >>> " << endl;
			}
		}
		// 需要录入的 code 不存在则会报错,  "注意休市时间是没有行情数据的"
		//cout << chrono::system_clock::now() << " - >>>" << endl;
	}

	/*STRATEGY PART*/
	vector<cwOrderPtr> Class1::StrategyTick(unordered_map<string, cwMarketDataPtr> code2data/*数据*/) {
		// 当前策略设计的逻辑是对每个品种都进行单独的测试管理, 只是在仓位设置上进行等权重的去分配,所以每个品种的交易信号都应该单独做计算 
		vector<cwOrderPtr> ordersTar;
		cout << " start " << "StrategyTick " << endl;
		for (const string& contract : (*tarCateList)) {
			try {
				cout << "##  " << contract << endl;
				const cwMarketDataPtr& barBook = code2data.at((*codeTractCur).at(contract));

				vector<double> retBarSubsetLong;
				vector<double> retBarSubsetShort;

				// 计算 stdLong
				auto startIndexLong = max(0, static_cast<int>((*retBar)[contract].size()) - (*verDictCur)[contract].Rl);
				for (size_t i = startIndexLong; i < min((*retBar)[contract].size(), static_cast<size_t>(startIndexLong + (*verDictCur)[contract].Rl)); ++i) {
					retBarSubsetLong.push_back((*retBar)[contract][i]);
				}
				double stdLong = SampleStd(retBarSubsetLong);

				// 计算 stdShort
				auto startIndexShort = max(0, static_cast<int>((*retBar)[contract].size()) - (*verDictCur)[contract].Rs);
				for (size_t i = startIndexShort; i < min((*retBar)[contract].size(), static_cast<size_t>(startIndexShort + (*verDictCur)[contract].Rs)); ++i) {
					retBarSubsetShort.push_back((*retBar)[contract][i]);
				}
				double stdShort = SampleStd(retBarSubsetShort);

				// 对于每个品种直接设置 单组合固定的张数
				long posV = ((*spePos).count((*codeTractCur)[contract]) > 0) ? (*spePos)[(*codeTractCur)[contract]].volume : 0;
				long posC = posV; // 可平仓组合
				long posO = (*countLimitCur)[contract] - posC; // 可开仓组合  

				cout << "    " << contract << " = PosC " << posC << " - PosO " << posO << "   Fac = " << (*verDictCur)[contract].Fac << " >>>" << endl;

				// Spe Sta 0903 <可开仓位小于 0 代表已经开有多余的头寸，需要额外平仓处理， 特殊情况>
				if (posO < 0) {
					auto orders = StrategyPosSpeC(contract, barBook, posO);
					ordersTar.insert(ordersTar.end(), orders.begin(), orders.end());
					continue;
				}

				// trader 
				if (posC > 0) {
					auto orders = StrategyPosClose(contract, barBook, stdLong, stdShort);
					ordersTar.insert(ordersTar.end(), orders.begin(), orders.end());
				}

				if (posO > 0) {
					auto orders = StrategyPosOpen(contract, barBook, stdLong, stdShort);
					ordersTar.insert(ordersTar.end(), orders.begin(), orders.end());
				}
			}
			catch (const std::exception& ex) {
				std::cout << "ERROR " << contract << " ------------" << std::endl;
				std::cout << ex.what() << std::endl;
			}
		}
		return ordersTar;
	}

	// 开仓交易 条件
	vector<cwOrderPtr> Class1::StrategyPosOpen(string contract, cwMarketDataPtr barBook, double stdLong, double stdShort) {
		vector<cwOrderPtr> orders;
		if ((*queueBar)[contract].back() < (*queueBar)[contract][(*queueBar).size() - (*verDictCur)[contract].Rs] && stdShort > stdLong) {
			int tarVolume = (*countLimitCur)[contract];
			string key = (*codeTractCur)[contract] + "=" + Strformatdate::getCurrentDateString(); // 假设存在函数 getCurrentTimeString 获取当前时间的字符串表示
			(*spePos)[key] = catePortInf{ "Long",{},barBook->LastPrice,{},tarVolume };
			char DireSlc = (*verDictCur)[contract].Fac == "Mom_std_bar_re_dym" ? '0' : '1'; // 假设 0 表示 Buy，1 表示 Sell

			//cwOrderPtr order;
			cwOrderPtr order = make_shared<ORDERFIELD>();
			strcpy(order->InstrumentID, (*codeTractCur)[contract].c_str());
			order->Direction = DireSlc;
			strcpy(order->CombOffsetFlag, "open");
			order->VolumeTotalOriginal = tarVolume;
			order->LimitPrice = (*barBook).LastPrice;
			orders.push_back(order);
		}
		else if ((*queueBar)[contract].back() > (*queueBar)[contract][(*queueBar).size() - 500] && stdShort > stdLong) {
			int tarVolume = (*countLimitCur)[contract];
			string key = (*codeTractCur)[contract] + "=" + Strformatdate::getCurrentDateString();
			(*spePos)[key] = catePortInf{ "Short",{}, barBook->LastPrice, {},tarVolume };
			char DireSlc = (*verDictCur)[contract].Fac == "Mom_std_bar_re_dym" ? '1' : '0';

			cwOrderPtr order = make_shared<ORDERFIELD>();
			strcpy(order->InstrumentID, (*codeTractCur)[contract].c_str());
			order->Direction = DireSlc;
			strcpy(order->CombOffsetFlag, "open");
			order->VolumeTotalOriginal = tarVolume;
			order->LimitPrice = (*barBook).LastPrice;
			orders.push_back(order);
		}
		return orders;
	}

	// 平仓交易 条件
	vector<cwOrderPtr> Class1::StrategyPosClose(string contract, cwMarketDataPtr barBook, double stdLong, double stdShort) {
		vector<cwOrderPtr> orders;
		string code = (*codeTractCur)[contract];// 当前持仓代码
		string dire = (*spePos)[code].direction; // 当前持仓方向
		auto DireREFunc = [](const string& x) -> string {
			if (x == "Long") {
				return "Short";
			}
			else if (x == "Short") {
				return "Long";
			}
			else {
				return "Miss";
			}
			};
		string FacDirection = (*verDictCur)[contract].Fac == "Mom_std_bar_re_dym" ? dire : DireREFunc(dire);//根据策略类型调整交易方向Fac
		//Fac方向 =买 && （最新价格 > 短期价格 || 短期波动率<=长期波动率）
		if (FacDirection == "Long" && ((*queueBar)[contract].back() > (*queueBar)[contract][(*queueBar)[contract].size() - (*verDictCur)[contract].Rs] || stdShort <= stdLong)) {
			int tarVolume = (*spePos)[code].volume;
			(*spePos).erase(code);

			char DireSlc = (*verDictCur)[contract].Fac == "Mom_std_bar_re_dym" ? '1' : '0';  // 假设 1 表示 Sell，0 表示 Buy
			cwOrderPtr order = make_shared<ORDERFIELD>();
			strcpy(order->InstrumentID, (*codeTractCur)[contract].c_str());
			order->Direction = DireSlc;
			strcpy(order->CombOffsetFlag, "Close");
			order->VolumeTotalOriginal = tarVolume;
			order->LimitPrice = (*barBook).LastPrice;
			orders.push_back(order);
		}
		else if (FacDirection == "Short" && ((*queueBar)[contract].back() < (*queueBar)[contract][(*queueBar)[contract].size() - (*verDictCur)[contract].Rs] || stdShort <= stdLong)) {
			int tarVolume = (*spePos)[code].volume;
			(*spePos).erase(code);
			char DireSlc = (*verDictCur)[contract].Fac == "Mom_std_bar_re_dym" ? '0' : '1';  // 假设 1 表示 Sell，0 表示 Buy
			cwOrderPtr order = make_shared<ORDERFIELD>();
			strcpy(order->InstrumentID, (*codeTractCur)[contract].c_str());
			order->Direction = DireSlc;
			strcpy(order->CombOffsetFlag, "Close");
			order->VolumeTotalOriginal = tarVolume;
			order->LimitPrice = (*barBook).LastPrice;
			orders.push_back(order);
		}
		return orders;
	}

	// 特殊平仓处理方式
	vector<cwOrderPtr> Class1::StrategyPosSpeC(string contract, cwMarketDataPtr barBook, long posO) {
		vector<cwOrderPtr> orders;
		int tarVolume = abs(static_cast<int>(posO));
		string dire = (*spePos)[(*codeTractCur)[contract]].direction;
		char DireSlc = ((*spePos)[(*codeTractCur)[contract]].direction == "Long") ? '1' : '0';  // 假设 1 表示 Sell，0 表示 Buy
		cwOrderPtr order = make_shared<ORDERFIELD>();
		strcpy(order->InstrumentID, (*codeTractCur)[contract].c_str());
		order->Direction = DireSlc;
		strcpy(order->CombOffsetFlag, "Close");
		order->VolumeTotalOriginal = tarVolume;
		order->LimitPrice = (*barBook).LastPrice;
		orders.push_back(order);
		return orders;
	}

	vector<cwOrderPtr> Class1::HandBar(unordered_map<string, cwMarketDataPtr> code2data/*昨仓数据*/, unordered_map<string, PositionFieldPtr> curPos) {
		auto sTime = std::chrono::system_clock::now();
		std::vector<std::string> ff;
		for (const auto& pair : (*codeTractCur)) {
			string key = pair.first;
			string value = pair.second;
			auto it = find((*tarCateList).begin(), (*tarCateList).end(), key);
			if (it != (*tarCateList).end()) {
				ff.push_back(value);
			}
		}
		while (true) {
			bool allContained = true;
			for (const auto& code : ff) {
				if (code2data.count(code) == 0) {
					allContained = false;
					break;
				}
			}
			if (allContained) break;
			cout << "Sleep for 10 seconds.";
			this_thread::sleep_for(chrono::seconds(10));
			cout << string(5, ' ') << "Count " << code2data.size() << ";";
			cout << endl;
			auto span = std::chrono::system_clock::now() - sTime;
			auto ss = std::chrono::duration_cast<std::chrono::seconds>(span).count();
			if (ss > 59) {
				return vector<cwOrderPtr>();
			}
		}
		UpdateFlow(code2data, curPos);
		cout << " --- " << "updateFLow --------------------" << endl;

		vector<cwOrderPtr> ordersTar = StrategyTick(code2data);
		cout << " +++ " << ordersTar.size() << endl;

		int i = 0;
		for (size_t i = 0; i < ordersTar.size(); ++i) {
			cwOrderPtr ord = make_shared<ORDERFIELD>(ordersTar[i]);

			/* cwOrderPtr ord = ordersTar[i];*/
			if (ord->Direction == 0) {  // 假设 0 表示 Buy
				std::string instrument = regex_replace(ord->InstrumentID, std::regex("\\d"), "");
				(*ord).LimitPrice += (*futInfDict)[instrument].ticksize * 2;
			}
			else if ((*ord).Direction == 1) {  // 假设 1 表示 Sell
				string instrument = regex_replace((*ord).InstrumentID, std::regex("\\d"), "");
				(*ord).LimitPrice -= (*futInfDict)[instrument].ticksize * 2;
			}


			try {
				if (code2data.count((*ord).InstrumentID) > 0) {
					(*ord).LimitPrice = min((*ord).LimitPrice, code2data[((*ord).InstrumentID)]->UpperLimitPrice);
					(*ord).LimitPrice = max((*ord).LimitPrice, code2data[((*ord).InstrumentID)]->LowerLimitPrice);
				}
				else {
					std::cout << "###### Miss " << (*ord).InstrumentID << " LimitPrice >>>>>>>>>>>>>>>" << std::endl;
				}
			}
			catch (const std::exception& e) {
				std::cout << "###### Miss " << (*ord).InstrumentID << " LimitPrice >>>>>>>>>>>>>>>" << std::endl;
			}


			try {
				if ((*ord).CombOffsetFlag == "Open") {
					if (code2data.count((*ord).InstrumentID) > 0) {
						double upperLower = 0.85 * code2data.at((*ord).InstrumentID)->UpperLimitPrice + 0.15 * code2data.at((*ord).InstrumentID)->LowerLimitPrice;
						double lowerUpper = 0.15 * code2data.at((*ord).InstrumentID)->UpperLimitPrice + 0.85 * code2data.at((*ord).InstrumentID)->LowerLimitPrice;


						if ((*ord).LimitPrice > upperLower && (*ord).Direction == 1) {
							ord = cwOrderPtr();
						}
						else if ((*ord).LimitPrice < lowerUpper && (*ord).Direction == 0) {
							ord = cwOrderPtr();
						}
					}
				}
			}
			catch (const std::exception& e) {
				std::cout << "###### Miss2 " << (*ord).InstrumentID << " LimitPrice >>>>>>>>>>>>>>>" << std::endl;
			}
			// 由于使用范围 for 循环，这里不需要使用索引更新，修改直接生效
		}
		return ordersTar;
	}

	void Class1::StoreBaseData() {
		// 后续可以弄的交易日志，不影响策略 

		// 根据当前日期选择写入的数据库 his
		std::string cursor_str = "20241225"; // 假设 cursor_str 已经定义
		std::string dbPath = "his" + cursor_str.substr(0, 4) + ".db";
		sqlite3* cnnBar = SqlliteHelp::OpenDatabase(dbPath.c_str());

		std::string createTableCmd = "CREATE TABLE bf" + cursor_str + " (code TEXT, exchange TEXT, tradingday TEXT, timestamp TEXT, volume BIGINT, turnover FLOAT,"
			" openprice FLOAT, highprice FLOAT,  lowprice FLOAT,  closeprice FLOAT);";
		SQLiteCommand cmd(cnnBar->get());
		cmd.setCommandText(createTableCmd);
		cmd.executeNonQuery();


		// 也是一条一条的写入? 只写入主力数据
		for (const auto& contractPair : *barFlowCur) {
			const std::string& contract = contractPair.first;
			const std::vector<barFuture>& barSlcVec = contractPair.second;
			for (const barFuture& barSlc : barSlcVec) {
				std::string insertCmd = "INSERT INTO bf" + cursor_str + " (code, actionday, tradingday, timestamp, volume, closeprice) "
					"VALUES ('" + barSlc.code + "', '" + barSlc.tradingday + "', '" + barSlc.tradingday + "', 'DEFULT_TEST', "
					+ std::to_string(barSlc.volume) + ", " + std::to_string(barSlc.price) + ");";
				cmd.setCommandText(insertCmd);
				cmd.executeNonQuery();
			}
		}
		// 当天成交的 order 要以什么样的形式进行记录?
	}
}