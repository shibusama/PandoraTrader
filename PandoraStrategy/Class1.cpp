#include "Class1.h"
#include "SqlliteHelp.h"
#include "Strformatdate.h"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <string>
#include <map>
#include <regex>

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

	double Class1::ArithmeticMean(double arr[], int size) {//计算简单算数平均值
		double result = 0;
		// 遍历数组中的每个元素
		for (int i = 0; i < size; ++i) {
			result += arr[i];
		}
		// 计算平均值
		return result / size;
	}

	double Class1::SampleStd(double arr[], int size) {
		double mean = ArithmeticMean(arr, size);
		double result = 0;
		// 遍历数组，计算每个元素与平均值差值的平方，并累加到 result 中
		for (int i = 0; i < size; ++i) {
			result += pow((arr[i] - mean), 2);
		}
		// 计算样本标准差
		return sqrt((result / (size - 1)));
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

