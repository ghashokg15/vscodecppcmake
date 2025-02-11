#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <thread>
#include <cassert>
#include <cmath>
#include <unordered_map>
#include <limits>

struct StockData {
    std::string date;
    double open, high, low, close;
};

class StockAnalyzer {
private:
    std::vector<StockData> historicalData;

public:
    void loadHistoricalData(const std::string& filename) {
        std::ifstream file(filename);
        std::string line;
        if (!file.is_open()) {
            std::cerr << "Error opening file!" << std::endl;
            return;
        }
        std::getline(file, line); // Skip header
        while (std::getline(file, line)) {
            std::stringstream ss(line);
            std::string index, date, open, high, low, close;
            std::getline(ss, index, ',');
            std::getline(ss, date, ',');
            std::getline(ss, open, ',');
            std::getline(ss, high, ',');
            std::getline(ss, low, ',');
            std::getline(ss, close, ',');
            
            historicalData.push_back({date, std::stod(open), std::stod(high), std::stod(low), std::stod(close)});
        }
        file.close();
    }

    double calculateMovingAverage(int period) {
        if (historicalData.size() < period) return -1;
        double sum = 0;
        for (size_t i = historicalData.size() - period; i < historicalData.size(); ++i) {
            sum += historicalData[i].close;
        }
        return sum / period;
    }

    void findBestTradePeriod() {
        if (historicalData.empty()) return;
        double minPrice = std::numeric_limits<double>::max();
        double maxProfit = 0;
        std::string buyDate, sellDate;
        
        for (const auto& data : historicalData) {
            if (data.low < minPrice) {
                minPrice = data.low;
                buyDate = data.date;
            }
            double profit = data.high - minPrice;
            if (profit > maxProfit) {
                maxProfit = profit;
                sellDate = data.date;
            }
        }
        std::cout << "Best time to buy: " << buyDate << " at " << minPrice << std::endl;
        std::cout << "Best time to sell: " << sellDate << " with profit " << maxProfit << std::endl;
    }
};

void runStockAnalysis() {
    StockAnalyzer analyzer;
    analyzer.loadHistoricalData("NIFTY 50_Historical_PR_01022024to11022025.csv");
    double ma50 = analyzer.calculateMovingAverage(50);
    std::cout << "50-day Moving Average: " << ma50 << std::endl;
    analyzer.findBestTradePeriod();
}

void testMovingAverage() {
    StockAnalyzer analyzer;
    analyzer.loadHistoricalData("./data/hist-data1.csv");
    assert(analyzer.calculateMovingAverage(1) >= 0);
    std::cout << "Test passed: Moving Average Calculation" << std::endl;
}

int main() {
    std::thread analysisThread(runStockAnalysis);
    analysisThread.join();
    
    testMovingAverage();
    return 0;
}
