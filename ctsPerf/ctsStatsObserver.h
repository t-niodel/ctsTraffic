#pragma once

// cpp headers
#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <vector>
#include <set>
#include <unordered_map>
#include <map>
// os headers
#include <Windows.h>
// Winsock2 is needed for IPHelper headers
// ReSharper disable once CppUnusedIncludeDirective
#include <WinSock2.h>
#include <ws2ipdef.h>
#include <Iphlpapi.h>
#include <Tcpestats.h>

// ctl headers
#include <ctString.hpp>
#include <ctSockaddr.hpp>
#include <ctThreadPoolTimer.hpp>
#include <ctMath.hpp>


namespace ctsPerf {

class ctsStatsObserver
{
public:
    ctsStatsObserver(
        std::set<std::wstring>* globalTrackedStats, 
        std::set<std::wstring>* detailTrackedStats, 
        BOOLEAN livePrintGlobalStats, 
        BOOLEAN livePrintDetailStats,
        ctsEstats* estats)
        : maxHistoryLength(maxHistoryLength),
          globalTrackedStats(globalTrackedStats),
          detailTrackedStats(detailTrackedStats),
          printGlobal(livePrintGlobalStats),
          printDetail(livePrintDetailStats),
          estats(estats),
          globalStatsWriter(L"LiveData\\GlobalSummary_0.csv"),
          perConnectionStatsWriter(L"LiveData\\DetailSummary_0.csv")
    {}

    // Statistics summary data structure
    typedef struct detailedStats {
        size_t  samples = 0;
        ULONG64 min = ULONG_MAX;
        ULONG64 max = ULONG_MAX;
        DOUBLE mean = -0.00001;
        DOUBLE stddev = -0.00001;
        DOUBLE median = -0.00001;
        DOUBLE iqr = -0.00001;
    } DETAILED_STATS;
    // Representation of the %change of each statistic since the last poll
    typedef struct detailedStatsChange {
        DOUBLE samples = 1.0;
        DOUBLE min = 0.0;
        DOUBLE max = 0.0;
        DOUBLE mean = 0.0;
        DOUBLE stddev = 0.0;
        DOUBLE median = 0.0;
        DOUBLE iqr = 0.0;
    } DETAILED_STATS_PERCENT_CHANGE;




    // Generate a DETAILED_STATS struct for the given statistic across all connections.
    //      Min/Max: global min/max, Mean: mean of means, Median: median of medians,
    //      StdDev: stddev of means, IQR: iqr of medians.
    std::tuple<DETAILED_STATS, DETAILED_STATS_PERCENT_CHANGE> GatherGlobalStatisticSummary(std::wstring statName)
    {
        // Handle different data sources
        switch (trackedStatisticsDataSources.at(statName))
        {
            case TcpEstats:
                return _GatherGlobalStatisticSummary(statName, estats->GetStatsData(statName));
            default: // Never occurs bc this is an enum
                return {};
        }
    }
    // Generate a vector of DETAILED_STATS structs representing summaries for the given statistic for each connection.
    std::vector<std::tuple<DETAILED_STATS, DETAILED_STATS_PERCENT_CHANGE>> GatherPerConnectionStatisticSummaries(std::wstring statName)
    {
        // Handle different data sources
        switch (trackedStatisticsDataSources.at(statName))
        {
            case TcpEstats:
                return _GatherPerConnectionStatisticSummaries(statName, estats->GetStatsData(statName));
            default: // Never occurs bc this is an enum
                return {};
        }
    }

    void PrintDataUpdate() {
        // Do not do live updates if there are no tracked stats
        if (globalTrackedStats->empty() && detailTrackedStats->empty()) {return;}

        if (printGlobal || printDetail) {
            clear_screen();
        }

        // -- Global summary table --
        if (!globalTrackedStats->empty()) {
            if (printGlobal) {PrintStdHeader(L"GLobal Statistics", 12);}
            OpenAndStartGlobalStatSummaryCSV();
            for (std::wstring stat : *globalTrackedStats)
            {

                auto detailedStatsSummary = GatherGlobalStatisticSummary(stat);
                if (printGlobal) {PrintGlobalStatSummary(stat, detailedStatsSummary, 12);}
                SaveGlobalStatSummaryLineToCSV(stat, detailedStatsSummary);
            }

            if (printGlobal) {
                PrintStdFooter();
                std::wcout << std::endl;
            }
        }

        // -- Detailed Pre-Statistic Results --
        if (!detailTrackedStats->empty()) {
            OpenAndStartDetailStatSummaryCSV();
            for (std::wstring stat : *detailTrackedStats)
            {
                if (printDetail) {PrintStdHeader(stat, 12);}
                SaveDetailStatHeaderLineToCSV(stat);

                auto detailedStatsSummaries = GatherPerConnectionStatisticSummaries(stat);

                for (auto summary : detailedStatsSummaries)
                {
                    if (printDetail) {PrintPerConnectionStatSummary(summary, 12);}
                    SaveDetailStatSummaryLineToCSV(summary);
                }
                if (printDetail) {PrintStdFooter();}
            }
        }
    }

private:
    // References to objects which hold stats
    ctsEstats* estats;

    // "Live" (per-poll) .csv writers
    ctsWriteDetails globalStatsWriter;
    ctsWriteDetails perConnectionStatsWriter;
    // Counters for filenames
    ULONG globalFileNumber = 0;
    ULONG detailFileNumber = 0;
    // Console output formatting
    HANDLE hConsole;
    WORD orig_wAttributes;
    const WORD BACKGROUND_MASK = 0x00F0;

    // Max number of values to keep in history
    const ULONG maxHistoryLength;

    // Mapping of which source each stat is tracked in
    std::map<std::wstring, STAT_DATA_SOURCE> trackedStatisticsDataSources {
        {L"MssRcvd",                          TcpEstats},
        {L"MssSent",                          TcpEstats},
        {L"DataBytesIn",                      TcpEstats},
        {L"DataBytesOut",                     TcpEstats},
        {L"conjestionWindow",                 TcpEstats},
        {L"bytesSentInReceiverLimited",       TcpEstats},
        {L"bytesSentInSenderLimited",         TcpEstats},
        {L"bytesSentInCongestionLimited",     TcpEstats},
        {L"transitionsIntoReceiverLimited",   TcpEstats},
        {L"transitionsIntoSenderLimited",     TcpEstats},
        {L"transitionsIntoCongestionLimited", TcpEstats},
        {L"retransmitTimer",                  TcpEstats},
        {L"roundTripTime",                    TcpEstats},
        {L"bytesRetrans",                     TcpEstats},
        {L"dupAcksRcvd",                      TcpEstats},
        {L"sacksRcvd",                        TcpEstats},
        {L"congestionSignals",                TcpEstats},
        {L"maxSegmentSize",                   TcpEstats},
        {L"curLocalReceiveWindow",            TcpEstats},
        {L"minLocalReceiveWindow",            TcpEstats},
        {L"maxLocalReceiveWindow",            TcpEstats},
        {L"curRemoteReceiveWindow",           TcpEstats},
        {L"minRemoteReceiveWindow",           TcpEstats},
        {L"maxRemoteReceiveWindow",           TcpEstats},
        {L"outboundBandwidth",                TcpEstats},
        {L"inboundBandwidth",                 TcpEstats},
        {L"outboundInstability",              TcpEstats},
        {L"inboundInstability",               TcpEstats}
    };

    // List of enabled global stats
    std::set<std::wstring>* globalTrackedStats;
    std::set<std::wstring>* detailTrackedStats;

    const BOOLEAN printGlobal;
    const BOOLEAN printDetail;

    // Storage of past data
    std::map<std::tuple<std::wstring, ctl::ctSockaddr, ctl::ctSockaddr>, DETAILED_STATS> previousPerConnectionStatsSummaries;
    std::map<std::wstring, DETAILED_STATS> previousGlobalStatsSummaries;

    typedef enum statDataSource {
        TcpEstats,
    } STAT_DATA_SOURCE;

    template<typename T>
    DOUBLE PercentChange(T oldVal, T newVal) {
        if(oldVal == newVal) {
            return 0.0;
        }
        if ((oldVal == 0)) {
            return 1.0;
        }
        else if ((newVal == 0)) {
            return -1.0;
        }
        else if (newVal > oldVal) {
            return (static_cast<DOUBLE>(newVal - oldVal) / oldVal);
        }
        else if (newVal < oldVal) {
            return -1 * (static_cast<DOUBLE>(oldVal - newVal) / oldVal);
        }
        else {
            return 0.0;
        }
    }

    // Actual function, wrapper handles differing datastructure types
    template <TCP_ESTATS_TYPE TcpType>
    std::tuple<DETAILED_STATS, DETAILED_STATS_PERCENT_CHANGE> _GatherGlobalStatisticSummary(std::wstring statName, std::vector<std::vector<ULONG64>> data)
    {
        std::vector<ULONG64> mins;
        std::vector<ULONG64> maxs;
        std::vector<DOUBLE> means;
        std::vector<DOUBLE> medians;

        for (std::vector<ULONG64> values : data)
        {
            if (values.empty()) {continue;} // Ignore empty entries

            sort(std::begin(values), std::end(values));

            mins.push_back(*std::min_element(std::begin(values), std::end(values)));
            maxs.push_back(*std::max_element(std::begin(values), std::end(values)));
            means.push_back(std::get<0>(ctl::ctSampledStandardDeviation(std::begin(values), std::end(values))));
            medians.push_back(std::get<1>(ctl::ctInterquartileRange(std::begin(values), std::end(values))));
        }

        // If no data was collected, return an empty struct
        if (mins.empty()) {return {};}

        auto mstddev_tuple = ctl::ctSampledStandardDeviation(std::begin(means), std::end(means));
        auto interquartile_tuple = ctl::ctInterquartileRange(std::begin(medians), std::end(medians));

        // Build summary struct
        DETAILED_STATS s = {
            std::size(mins),
            *std::min_element(std::begin(mins), std::end(mins)),
            *std::max_element(std::begin(maxs), std::end(maxs)),
            std::get<0>(mstddev_tuple),
            std::get<1>(mstddev_tuple),
            std::get<1>(interquartile_tuple),
            std::get<2>(interquartile_tuple) - std::get<0>(interquartile_tuple)
        };

        // Build a struct marking %change of each value
        // Handle case where no previous summary exists
        DETAILED_STATS_PERCENT_CHANGE c;
        try {
            DETAILED_STATS s_prev = previousGlobalStatsSummaries.at(statName);
            c = {
                PercentChange(s_prev.samples, s.samples),
                PercentChange(s_prev.min, s.min),
                PercentChange(s_prev.max, s.max),
                PercentChange(s_prev.mean, s.mean),
                PercentChange(s_prev.stddev, s.stddev),
                PercentChange(s_prev.median, s.median),
                PercentChange(s_prev.iqr, s.iqr)
            };
        }
        catch (std::out_of_range&) {
            c = {};
        }

        // Update previous tracked with this new summary
        previousGlobalStatsSummaries.insert_or_assign(statName, s);

        return std::make_tuple(s, c);
    }
    // Actual function, wrapper handles differing datastructure types
    template <TCP_ESTATS_TYPE TcpType>
    std::vector<std::tuple<DETAILED_STATS, DETAILED_STATS_PERCENT_CHANGE>> _GatherPerConnectionStatisticSummaries(std::wstring statName, std::vector<std::vector<ULONG64>> data)
    {
        std::vector<std::tuple<DETAILED_STATS, DETAILED_STATS_PERCENT_CHANGE>> perConnectionSatisticSummaries;

        for (std::vector<ULONG64> values : data)
        {
            if (values.empty()) {continue;} // Ignore empty entries

            sort(std::begin(values), std::end(values));
            auto mstddev_tuple = ctl::ctSampledStandardDeviation(std::begin(values), std::end(values));
            auto interquartile_tuple = ctl::ctInterquartileRange(std::begin(values), std::end(values));

            DETAILED_STATS s = {
                std::size(values),
                *std::min_element(std::begin(values), std::end(values)),
                *std::max_element(std::begin(values), std::end(values)),
                std::get<0>(mstddev_tuple),
                std::get<1>(mstddev_tuple),
                std::get<1>(interquartile_tuple),
                std::get<2>(interquartile_tuple) - std::get<0>(interquartile_tuple)
            };

            // Tuple to identify this entry in the previous summaries tracking structure
            auto perConnectionStatIDTuple = std::make_tuple(
                statName,
                entry.LocalAddr(),
                entry.RemoteAddr()
            );

            // Build a struct marking %change of each value
            // Handle case where no previous summary exists
            DETAILED_STATS_PERCENT_CHANGE c;
            try {
                DETAILED_STATS s_prev = previousPerConnectionStatsSummaries.at(perConnectionStatIDTuple);
                c = {
                    PercentChange(s_prev.samples, s.samples),
                    PercentChange(s_prev.min, s.min),
                    PercentChange(s_prev.max, s.max),
                    PercentChange(s_prev.mean, s.mean),
                    PercentChange(s_prev.stddev, s.stddev),
                    PercentChange(s_prev.median, s.median),
                    PercentChange(s_prev.iqr, s.iqr)
                };
            }
            catch (std::out_of_range&) {
                c = {};
            }

            previousPerConnectionStatsSummaries.insert_or_assign(perConnectionStatIDTuple, s);

            perConnectionSatisticSummaries.push_back(std::make_tuple(s, c));
        }

        return perConnectionSatisticSummaries;
    }


    void OpenAndStartGlobalStatSummaryCSV() {
        globalStatsWriter.setFilename(L"LiveData\\GlobalSummary_" + std::to_wstring(globalFileNumber) + L".csv");
        globalFileNumber++;
        globalStatsWriter.create_file(std::wstring(L"GLobal Statistic,Min,Min %change,Mean,Mean %change,Max,Max %change,StdDev,StdDev %change,Median,Median %change,IQR,IQR %change"));
    }
    void SaveGlobalStatSummaryLineToCSV(std::wstring title, std::tuple<DETAILED_STATS, DETAILED_STATS_PERCENT_CHANGE> summary) {
        globalStatsWriter.write_row(
            title + L"," +
            std::to_wstring(std::get<0>(summary).min) + L"," + std::to_wstring(std::get<1>(summary).min * 100) + L"," +
            std::to_wstring(std::get<0>(summary).mean) + L"," + std::to_wstring(std::get<1>(summary).mean * 100) + L"," +
            std::to_wstring(std::get<0>(summary).max) + L"," + std::to_wstring(std::get<1>(summary).max * 100) + L"," +
            std::to_wstring(std::get<0>(summary).stddev) + L"," + std::to_wstring(std::get<1>(summary).stddev * 100) + L"," +
            std::to_wstring(std::get<0>(summary).median) + L"," + std::to_wstring(std::get<1>(summary).median * 100) + L"," +
            std::to_wstring(std::get<0>(summary).iqr) + L"," + std::to_wstring(std::get<1>(summary).iqr * 100)
        );
    }

    void OpenAndStartDetailStatSummaryCSV() {
        globalStatsWriter.setFilename(L"LiveData\\DetailSummary_" + std::to_wstring(detailFileNumber) + L".csv");
        detailFileNumber++;
        globalStatsWriter.create_file(std::wstring(L"Samples,Min,Min %change,Mean,Mean %change,Max,Max %change,StdDev,StdDev %change,Median,Median %change,IQR,IQR %change"));
    }
    void SaveDetailStatHeaderLineToCSV(std::wstring title) {
        globalStatsWriter.write_empty_row();
        globalStatsWriter.write_row(title + L",Min,Min %change,Mean,Mean %change,Max,Max %change,StdDev,StdDev %change,Median,Median %change,IQR,IQR %change");
    }
    void SaveDetailStatSummaryLineToCSV(std::tuple<DETAILED_STATS, DETAILED_STATS_PERCENT_CHANGE> summary) {
        globalStatsWriter.write_row(
            std::to_wstring(std::get<0>(summary).samples) + L"," +
            std::to_wstring(std::get<0>(summary).min) + L"," + std::to_wstring(std::get<1>(summary).min * 100) + L"," +
            std::to_wstring(std::get<0>(summary).mean) + L"," + std::to_wstring(std::get<1>(summary).mean * 100) + L"," +
            std::to_wstring(std::get<0>(summary).max) + L"," + std::to_wstring(std::get<1>(summary).max * 100) + L"," +
            std::to_wstring(std::get<0>(summary).stddev) + L"," + std::to_wstring(std::get<1>(summary).stddev * 100) + L"," +
            std::to_wstring(std::get<0>(summary).median) + L"," + std::to_wstring(std::get<1>(summary).median * 100) + L"," +
            std::to_wstring(std::get<0>(summary).iqr) + L"," + std::to_wstring(std::get<1>(summary).iqr * 100)
        );
    }

    void ResetSetConsoleColor() {
        SetConsoleTextAttribute(hConsole, orig_wAttributes);
    }
    void SetConsoleColorConnectionStatus(BOOLEAN connectionOpen) {
        if (connectionOpen) {
            SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_BLUE | (orig_wAttributes & BACKGROUND_MASK));
        }
        else {
            ResetSetConsoleColor();
        }
    }
    void SetConsoleColorFromPercentChange(DOUBLE percentChange) {
        if(percentChange <= -1.0) { // Blue BG
            SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY | BACKGROUND_BLUE);
        }
        else if(percentChange < -0.25) { // Blue
            SetConsoleTextAttribute(hConsole, FOREGROUND_BLUE | FOREGROUND_INTENSITY | (orig_wAttributes & BACKGROUND_MASK));
        }
        else if(percentChange < -0.01) { // Cyan
            SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY | (orig_wAttributes & BACKGROUND_MASK));
        }
        else if(percentChange < 0.0) { // Green
            SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_INTENSITY | (orig_wAttributes & BACKGROUND_MASK));
        }
        else if (percentChange == 0.0) { // White -- "No Change"
            SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | (orig_wAttributes & BACKGROUND_MASK));
        }
        else if (percentChange < 0.01) { // Yellow
            SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY | (orig_wAttributes & BACKGROUND_MASK));
        }
        else if (percentChange < 0.25) { // Magenta
            SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY | (orig_wAttributes & BACKGROUND_MASK));
        }
        else if (percentChange < 1.0) { // Red
            SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY | (orig_wAttributes & BACKGROUND_MASK));
        }
        else if (percentChange >= 1.0){ // Red BG
            SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY | BACKGROUND_RED | (orig_wAttributes & BACKGROUND_MASK));
        }
        else { // Error state, should never happen
            SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY | BACKGROUND_RED | BACKGROUND_GREEN);
        }
    }
    void PrintStat(ULONG64 stat, DOUBLE percentChange, const int& width) {
        ResetSetConsoleColor();
        std::wcout << L" | ";

        SetConsoleColorFromPercentChange(percentChange);
        std::wcout << std::right << std::setw(width) << std::setfill(L' ') << stat;
    }
    void PrintStat(DOUBLE stat, DOUBLE percentChange, const int& width) {
        ResetSetConsoleColor();
        std::wcout << L" | ";

        std::wcout.precision(2);
        SetConsoleColorFromPercentChange(percentChange);
        std::wcout << std::right << std::setw(width) << std::setfill(L' ') << std::fixed << stat;
    }
    void PrintGlobalStatSummary(std::wstring title, std::tuple<DETAILED_STATS, DETAILED_STATS_PERCENT_CHANGE> summary, const int& width) {
        std::wcout << std::left << std::setw(20) << std::setfill(L' ') << title;

        PrintStat(std::get<0>(summary).min, std::get<1>(summary).min, width);
        PrintStat(std::get<0>(summary).mean, std::get<1>(summary).mean, width);
        PrintStat(std::get<0>(summary).max, std::get<1>(summary).max, width);
        PrintStat(std::get<0>(summary).stddev, std::get<1>(summary).stddev, width);
        PrintStat(std::get<0>(summary).median, std::get<1>(summary).median, width);
        PrintStat(std::get<0>(summary).iqr, std::get<1>(summary).iqr, width);

        ResetSetConsoleColor();
        std::wcout << L" |" << std::endl;
    }
    void PrintPerConnectionStatSummary(std::tuple<DETAILED_STATS, DETAILED_STATS_PERCENT_CHANGE> summary, const int& width) {
        SetConsoleColorConnectionStatus(std::get<1>(summary).samples > 0);
        std::wcout << L"Samples: ";
        std::wcout << std::left << std::setw(11) << std::setfill(L' ') << std::get<0>(summary).samples;

        PrintStat(std::get<0>(summary).min, std::get<1>(summary).min, width);
        PrintStat(std::get<0>(summary).mean, std::get<1>(summary).mean, width);
        PrintStat(std::get<0>(summary).max, std::get<1>(summary).max, width);
        PrintStat(std::get<0>(summary).stddev, std::get<1>(summary).stddev, width);
        PrintStat(std::get<0>(summary).median, std::get<1>(summary).median, width);
        PrintStat(std::get<0>(summary).iqr, std::get<1>(summary).iqr, width);

        ResetSetConsoleColor();
        std::wcout << L" |" << std::endl;
    }
    
    void PrintHeaderTitle(std::wstring title, const int& width) {
        std::wcout << L" | " << std::right << std::setw(width) << std::setfill(L' ') << title;
    }
    void PrintStdHeader(std::wstring firstColumnName, const int& width) {
        std::wcout << "---------------------------------------------------------------------------------------------------------------+" << std::endl;
        std::wcout << std::left << std::setw(20) << std::setfill(L' ') << firstColumnName;
            PrintHeaderTitle(L"Min", width);
            PrintHeaderTitle(L"Mean", width);
            PrintHeaderTitle(L"Max", width);
            PrintHeaderTitle(L"StdDev", width);
            PrintHeaderTitle(L"Median", width);
            PrintHeaderTitle(L"IQR", width);
        std::wcout << L" |" << std::endl;
        std::wcout << "---------------------------------------------------------------------------------------------------------------+" << std::endl;
    }
    void PrintStdFooter() {
        std::wcout << "---------------------------------------------------------------------------------------------------------------+" << std::endl;
    }


    void clear_screen() { 
        COORD tl = {0,0};
        CONSOLE_SCREEN_BUFFER_INFO s;
        HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);   
        GetConsoleScreenBufferInfo(console, &s);
        DWORD written, cells = s.dwSize.X * s.dwSize.Y;
        FillConsoleOutputCharacter(console, ' ', cells, tl, &written);
        FillConsoleOutputAttribute(console, s.wAttributes, cells, tl, &written);
        SetConsoleCursorPosition(console, tl);
    }
};

} // namespace