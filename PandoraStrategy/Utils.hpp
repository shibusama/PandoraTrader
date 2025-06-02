#pragma once
#include <ctime>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <vector> 
#include <chrono>
#include <algorithm>  // std::find_if
#include <iterator>   // std::distance
#include <functional> // std::function

struct timePara {
	int hour;
	int minute;
	int second;

};

// 获取当前日期YYYYMMDD
std::string GetTodayDate() {
	std::time_t now = std::time(nullptr);
	std::tm localTime;

#if defined(_WIN32) || defined(_WIN64)
	// Windows
	localtime_s(&localTime, &now);
#else
	// Linux/macOS
	localtime_r(&now, &localTime);
#endif

	std::ostringstream oss;
	oss << std::put_time(&localTime, "%Y%m%d");

	return oss.str();
}

//计算简单算数平均值
double ArithmeticMean(const std::vector<double>& arr) {
	if (arr.empty()) {
		return 0.0;
	}
	double sum = 0.0;
	for (const double num : arr) {
		sum += num;
	}
	return sum / static_cast<double>(arr.size());
}
//计算简单标准差
double SampleStd(const std::vector<double>& arr) {
	std::vector<double> cleanArr;
	cleanArr.reserve(arr.size());

	bool hasNaN = false;

	for (size_t i = 0; i < arr.size(); ++i) {
		double v = arr[i];
		if (std::isnan(v)) {
			hasNaN = true;
			std::cerr << "[SampleStd] Warning: NaN detected at index " << i << std::endl;
		}
		else {
			cleanArr.push_back(v);
		}
	}

	if (cleanArr.size() <= 1) {
		std::cerr << "[SampleStd] Error: Not enough valid data points to compute sample standard deviation (valid count = "
			<< cleanArr.size() << ")." << std::endl;
		return std::numeric_limits<double>::quiet_NaN();
	}

	double sum = 0.0;
	for (double v : cleanArr) {
		sum += v;
	}
	double mean = sum / static_cast<double>(cleanArr.size());

	double result = 0.0;
	for (double v : cleanArr) {
		result += (v - mean) * (v - mean);
	}

	double stddev = std::sqrt(result / static_cast<double>(cleanArr.size() - 1));

	if (std::isnan(stddev)) {
		std::cerr << "[SampleStd] Error: Computed standard deviation is NaN (possible numerical instability)." << std::endl;
	}

	return stddev;
}

timePara IsTradingTime() {
	auto now = std::chrono::system_clock::now();
	std::time_t now_time = std::chrono::system_clock::to_time_t(now);
	std::tm local_time;
#ifdef _WIN32
	localtime_s(&local_time, &now_time); // Windows 平台
#else
	localtime_r(&now_time, &local_time); // POSIX (Linux/macOS)
#endif

	int hour = local_time.tm_hour;
	int minute = local_time.tm_min;
	int second = local_time.tm_sec;

	timePara timePara_ = { hour ,minute ,second };
	return timePara_;
}

// 泛型函数：根据自定义条件查找结构体在 vector 中的索引
template <typename T>
int findIndex(const std::vector<T>& vec, std::function<bool(const T&)> condition) {
	auto it = std::find_if(vec.begin(), vec.end(), condition);
	if (it != vec.end()) {
		return static_cast<int>(std::distance(vec.begin(), it));
	}
	else {
		return -1; // 没找到返回 -1
	}
}

inline bool IsNormalTradingTime(int hour, int minute) {
	int time = hour * 100 + minute;
	return
		(time >= 901 && time < 1015) ||  // 上午前段
		(time >= 1030 && time < 1130) ||  // 上午后段
		(time >= 1330 && time < 1445) || // 下午
		(time >= 1300 && time <= 2359);    
}

inline bool IsClosingTime(int hour, int minute) { return (hour == 14 && minute >= 45) || (hour == 15 && minute == 0); }

inline bool IsAfterMarket(int hour, int minute) { return hour == 15 && minute >= 0 && minute < 10; }

inline int GetCurrentTimeInSeconds() { time_t now = std::time(nullptr);return static_cast<int>(now); }

inline bool AllInstrumentClosed(const std::map<std::string, bool>& instrumentCloseFlag) {
	for (const auto& [id, flag] : instrumentCloseFlag) {
		if (!flag) return false; // 有一个没完成，立即返回 false
	}
	return true;
}

bool IsPendingOrder(std::string instrumentID, std::map<cwActiveOrderKey, cwOrderPtr> WaitOrderList)
{
	for (auto& [key, order] : WaitOrderList) {
		if (key.InstrumentID == instrumentID) {
			return true;
		}
	}
	return false;
}