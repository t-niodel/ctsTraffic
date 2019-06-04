/*

Copyright (c) Microsoft Corporation
All rights reserved.

Licensed under the Apache License, Version 2.0 (the ""License""); you may not use this file except in compliance with the License. You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0

THIS CODE IS PROVIDED ON AN  *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION ANY IMPLIED WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR PURPOSE, MERCHANTABLITY OR NON-INFRINGEMENT.

See the Apache Version 2.0 License for specific language governing permissions and limitations under the License.

*/

#pragma once

// cpp headers
#include <string>
#include <iostream>
#include <vector>
#include <set>
#include <unordered_map>
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

namespace ctsPerf {

namespace details {

    // Invalid*EstatsValues come only when we have enabled Application Verifier
    // pageheap will pack uninitialized heap with this bit pattern
    // This helps us detect if we are trying to read uninitialized memory in our structs
    // returned from estats* APIs
    static const unsigned long InvalidLongEstatsValue = 0xc0c0c0c0;
    static const unsigned long long InvalidLongLongEstatsValue = 0xc0c0c0c0c0c0c0c0;

    // Will also check for -1 since we will initialize our structs with that value before handing them down
    // if we see -1 it tells us that there's not an ESTATS value in this case
    inline
    bool IsRodValueValid(_In_ LPCWSTR name, ULONG t) noexcept
    {
#ifdef _TESTING_ESTATS_VALUES
        if (t == InvalidLongEstatsValue)
        {
            wprintf(L"\t** %ws : %lu\n", name, t);
            return false;
        }
#endif
        if (t == 0xffffffff)
        {
            return false;
        }

        UNREFERENCED_PARAMETER(name);
        return true;
    }
    inline
    bool IsRodValueValid(_In_ LPCWSTR name, ULONG64 t) noexcept
    {
#ifdef _TESTING_ESTATS_VALUES
        if (t == InvalidLongLongEstatsValue)
        {
            wprintf(L"\t** %ws : %llu\n", name, t);
            return false;
        }
#endif
        if (t == 0xffffffffffffffff)
        {
            return false;
        }
        
        UNREFERENCED_PARAMETER(name);
        return true;
    }


    template <TCP_ESTATS_TYPE TcpType>
    struct EstatsTypeConverter {};

    template <>
    struct EstatsTypeConverter<TcpConnectionEstatsSynOpts> {
        typedef void* read_write_type;
        typedef TCP_ESTATS_SYN_OPTS_ROS_v0 read_only_static_type;
        typedef void* read_only_dynamic_type;
    };
    template<>
    struct EstatsTypeConverter<TcpConnectionEstatsData> {
        typedef TCP_ESTATS_DATA_RW_v0 read_write_type;
        typedef void* read_only_static_type;
        typedef TCP_ESTATS_DATA_ROD_v0 read_only_dynamic_type;
    };
    template<>
    struct EstatsTypeConverter<TcpConnectionEstatsSndCong> {
        typedef TCP_ESTATS_SND_CONG_RW_v0 read_write_type;
        typedef TCP_ESTATS_SND_CONG_ROS_v0 read_only_static_type;
        typedef TCP_ESTATS_SND_CONG_ROD_v0 read_only_dynamic_type;
    };
    template <>
    struct EstatsTypeConverter<TcpConnectionEstatsPath> {
        typedef TCP_ESTATS_PATH_RW_v0 read_write_type;
        typedef void* read_only_static_type;
        typedef TCP_ESTATS_PATH_ROD_v0 read_only_dynamic_type;
    };
    template <>
    struct EstatsTypeConverter<TcpConnectionEstatsSendBuff> {
        typedef TCP_ESTATS_SEND_BUFF_RW_v0 read_write_type;
        typedef void* read_only_static_type;
        typedef TCP_ESTATS_SEND_BUFF_ROD_v0 read_only_dynamic_type;
    };
    template <>
    struct EstatsTypeConverter<TcpConnectionEstatsRec> {
        typedef TCP_ESTATS_REC_RW_v0 read_write_type;
        typedef void* read_only_static_type;
        typedef TCP_ESTATS_REC_ROD_v0 read_only_dynamic_type;
    };
    template <>
    struct EstatsTypeConverter<TcpConnectionEstatsObsRec> {
        typedef TCP_ESTATS_OBS_REC_RW_v0 read_write_type;
        typedef void* read_only_static_type;
        typedef TCP_ESTATS_OBS_REC_ROD_v0 read_only_dynamic_type;
    };
    template <>
    struct EstatsTypeConverter<TcpConnectionEstatsBandwidth> {
        typedef TCP_ESTATS_BANDWIDTH_RW_v0 read_write_type;
        typedef void* read_only_static_type;
        typedef TCP_ESTATS_BANDWIDTH_ROD_v0 read_only_dynamic_type;
    };
    template <>
    struct EstatsTypeConverter<TcpConnectionEstatsFineRtt> {
        typedef TCP_ESTATS_FINE_RTT_RW_v0 read_write_type;
        typedef void* read_only_static_type;
        typedef TCP_ESTATS_FINE_RTT_ROD_v0 read_only_dynamic_type;
    };


    template <TCP_ESTATS_TYPE TcpType>
    void SetPerConnectionEstats(const PMIB_TCPROW tcpRow, typename EstatsTypeConverter<TcpType>::read_write_type *pRw)  // NOLINT
    {
        const auto err = ::SetPerTcpConnectionEStats(
            tcpRow,
            TcpType,
            reinterpret_cast<PUCHAR>(pRw), 0, static_cast<ULONG>(sizeof(*pRw)),
            0);
        if (err != 0) {
            throw ctl::ctException(err, L"SetPerTcpConnectionEStats", false);
        }
    }
    template <TCP_ESTATS_TYPE TcpType>
    void SetPerConnectionEstats(const PMIB_TCP6ROW tcpRow, typename EstatsTypeConverter<TcpType>::read_write_type *pRw)  // NOLINT
    {
        const auto err = ::SetPerTcp6ConnectionEStats(
            tcpRow,
            TcpType,
            reinterpret_cast<PUCHAR>(pRw), 0, static_cast<ULONG>(sizeof(*pRw)),
            0);
        if (err != 0) {
            throw ctl::ctException(err, L"SetPerTcp6ConnectionEStats", false);
        }
    }


    // TcpConnectionEstatsSynOpts is unique in that there isn't a RW type to query for
    inline ULONG GetPerConnectionStaticEstats(const PMIB_TCPROW tcpRow, TCP_ESTATS_SYN_OPTS_ROS_v0* pRos) noexcept  // NOLINT
    {
        return ::GetPerTcpConnectionEStats(
            tcpRow,
            TcpConnectionEstatsSynOpts,
            nullptr, 0, 0, // read-write information
            reinterpret_cast<PUCHAR>(pRos), 0, static_cast<ULONG>(sizeof(*pRos)), // read-only static information
            nullptr, 0, 0); // read-only dynamic information
    }
    template <TCP_ESTATS_TYPE TcpType>
    ULONG GetPerConnectionStaticEstats(const PMIB_TCPROW tcpRow, typename EstatsTypeConverter<TcpType>::read_only_static_type *pRos) noexcept  // NOLINT
    {
        typename EstatsTypeConverter<TcpType>::read_write_type rw;

        auto error = ::GetPerTcpConnectionEStats(
            tcpRow,
            TcpType,
            reinterpret_cast<PUCHAR>(&rw), 0, static_cast<ULONG>(sizeof(rw)), // read-write information
            reinterpret_cast<PUCHAR>(pRos), 0, static_cast<ULONG>(sizeof(*pRos)), // read-only static information
            nullptr, 0, 0); // read-only dynamic information
        // only return success if the read-only dynamic struct returned that this was enabled
        // else the read-only static information is not populated
        if (error == ERROR_SUCCESS && !rw.EnableCollection)
        {
            error = ERROR_NO_DATA;
        }
        return error;
    }

    // TcpConnectionEstatsSynOpts is unique in that there isn't a RW type to query for
    inline ULONG GetPerConnectionStaticEstats(const PMIB_TCP6ROW tcpRow, TCP_ESTATS_SYN_OPTS_ROS_v0* pRos) noexcept  // NOLINT
    {
        return ::GetPerTcp6ConnectionEStats(
            tcpRow,
            TcpConnectionEstatsSynOpts,
            nullptr, 0, 0, // read-write information
            reinterpret_cast<PUCHAR>(pRos), 0, static_cast<ULONG>(sizeof(*pRos)), // read-only static information
            nullptr, 0, 0); // read-only dynamic information
    }
    template <TCP_ESTATS_TYPE TcpType>
    ULONG GetPerConnectionStaticEstats(const PMIB_TCP6ROW tcpRow, typename EstatsTypeConverter<TcpType>::read_only_static_type *pRos) noexcept  // NOLINT
    {
        typename EstatsTypeConverter<TcpType>::read_write_type rw;

        auto error = ::GetPerTcp6ConnectionEStats(
            tcpRow,
            TcpType,
            reinterpret_cast<PUCHAR>(&rw), 0, static_cast<ULONG>(sizeof(rw)), // read-write information
            reinterpret_cast<PUCHAR>(pRos), 0, static_cast<ULONG>(sizeof(*pRos)), // read-only static information
            nullptr, 0, 0); // read-only dynamic information
        // only return success if the read-only dynamic struct returned that this was enabled
        // else the read-only static information is not populated
        if (error == ERROR_SUCCESS && !rw.EnableCollection)
        {
            error = ERROR_NO_DATA;
        }
        return error;
    }


    template <TCP_ESTATS_TYPE TcpType>
    ULONG GetPerConnectionDynamicEstats(const PMIB_TCPROW tcpRow, typename EstatsTypeConverter<TcpType>::read_only_dynamic_type *pRod) noexcept  // NOLINT
    {
        typename EstatsTypeConverter<TcpType>::read_write_type rw;

        auto error = ::GetPerTcpConnectionEStats(
            tcpRow,
            TcpType,
            reinterpret_cast<PUCHAR>(&rw), 0, static_cast<ULONG>(sizeof(rw)), // read-write information
            nullptr, 0, 0, // read-only static information
            reinterpret_cast<PUCHAR>(pRod), 0, static_cast<ULONG>(sizeof(*pRod))); // read-only dynamic information
        // only return success if the read-only dynamic struct returned that this was enabled
        // else the read-only static information is not populated
        if (error == ERROR_SUCCESS && !rw.EnableCollection)
        {
            error = ERROR_NO_DATA;
        }
        return error;
    }
    template <TCP_ESTATS_TYPE TcpType>
    ULONG GetPerConnectionDynamicEstats(const PMIB_TCP6ROW tcpRow, typename EstatsTypeConverter<TcpType>::read_only_dynamic_type *pRod) noexcept  // NOLINT
    {
        typename EstatsTypeConverter<TcpType>::read_write_type rw;

        auto error = ::GetPerTcp6ConnectionEStats(
            tcpRow,
            TcpType,
            reinterpret_cast<PUCHAR>(&rw), 0, static_cast<ULONG>(sizeof(rw)), // read-write information
            nullptr, 0, 0, // read-only static information
            reinterpret_cast<PUCHAR>(pRod), 0, static_cast<ULONG>(sizeof(*pRod))); // read-only dynamic information
        // only return success if the read-only dynamic struct returned that this was enabled
        // else the read-only static information is not populated
        if (error == ERROR_SUCCESS && !rw.EnableCollection)
        {
            error = ERROR_NO_DATA;
        }
        return error;
    }



    // the root template type that each ESTATS_TYPE will specialize for
    template <TCP_ESTATS_TYPE TcpType>
    class EstatsDataTracking {
        EstatsDataTracking() = default;
        ~EstatsDataTracking() = default;
    public:
        EstatsDataTracking(const EstatsDataTracking&) = delete;
        EstatsDataTracking& operator=(const EstatsDataTracking&) = delete;
        EstatsDataTracking(EstatsDataTracking&&) = delete;
        EstatsDataTracking& operator=(EstatsDataTracking&&) = delete;

        static LPCWSTR PrintHeader() = delete;

        void PrintData() const = delete;

        template <typename PTCPROW>
        void StartTracking(PTCPROW tcpRow) const = delete;

        template <typename PTCPROW>
        void UpdateData(PTCPROW tcpRow, const ctl::ctSockaddr& localAddr, const ctl::ctSockaddr& remoteAddr) = delete;
    };
    template <>
    class EstatsDataTracking<TcpConnectionEstatsSynOpts> {
    public:
        static LPCWSTR PrintHeader() noexcept
        {
            return L"Mss-Received,Mss-Sent";
        }
        std::wstring PrintData() const
        {
            return L"," + std::to_wstring(MssRcvd) + L"," + std::to_wstring(MssSent);
        }
        std::unordered_map<std::wstring, ULONG> getData()
        {
            return {
                {L"MssRcvd", MssRcvd},
                {L"MssSent", MssSent},
            };
        }

        template <typename PTCPROW>
        void StartTracking(const PTCPROW) const noexcept
        {
            // always on
        }
        template <typename PTCPROW>
        void UpdateData(const PTCPROW tcpRow, const ctl::ctSockaddr &, const ctl::ctSockaddr &, EstatsDataTracking<TcpConnectionEstatsSynOpts> *previousData)
        {
            if (MssRcvd == 0) {
                TCP_ESTATS_SYN_OPTS_ROS_v0 Ros;
                FillMemory(&Ros, sizeof Ros, -1);
                if (0 == GetPerConnectionStaticEstats(tcpRow, &Ros)) {

                    if (IsRodValueValid(L"TcpConnectionEstatsSynOpts - MssRcvd", Ros.MssRcvd)) {
                        MssRcvd = Ros.MssRcvd - previousData->getData().find("MssRcvd")->second;
                    }
                    if (IsRodValueValid(L"TcpConnectionEstatsSynOpts - MssSent", Ros.MssSent)) {
                        MssSent = Ros.MssSent - previousData->getData().find("MssSent")->second;
                    }
                }
            }
        }

    private:
        ULONG MssRcvd = 0;
        ULONG MssSent = 0;
    };
    template <>
    class EstatsDataTracking<TcpConnectionEstatsData> {
    public:
        static LPCWSTR PrintHeader() noexcept
        {
            return L"Bytes-In,Bytes-Out";
        }
        std::wstring PrintData() const
        {
            return L"," + std::to_wstring(DataBytesIn) + L"," + std::to_wstring(DataBytesOut);
        }
        std::unordered_map<std::wstring, ULONG64> getData()
        {
            return {
                {L"DataBytesIn", DataBytesIn},
                {L"DataBytesOut", DataBytesOut},
            };
        }

        template <typename PTCPROW>
        void StartTracking(const PTCPROW tcpRow) const
        {
            TCP_ESTATS_DATA_RW_v0 Rw;
            Rw.EnableCollection = TRUE;
            SetPerConnectionEstats<TcpConnectionEstatsData>(tcpRow, &Rw);
        }
        template <typename PTCPROW>
        void UpdateData(const PTCPROW tcpRow, const ctl::ctSockaddr &, const ctl::ctSockaddr &, EstatsDataTracking<TcpConnectionEstatsData> *previousData)
        {
            TCP_ESTATS_DATA_ROD_v0 Rod;
            FillMemory(&Rod, sizeof Rod, -1);
            if (0 == GetPerConnectionDynamicEstats<TcpConnectionEstatsData>(tcpRow, &Rod)) {

                if (IsRodValueValid(L"TcpConnectionEstatsData - DataBytesIn", Rod.DataBytesIn)) {
                    DataBytesIn = Rod.DataBytesIn - previousData->getData().find("DataBytesIn")->second;
                }
                if (IsRodValueValid(L"TcpConnectionEstatsData - DataBytesOut", Rod.DataBytesOut)) {
                    DataBytesOut = Rod.DataBytesOut - previousData->getData().find("DataBytesOut")->second;
                }
            }
        }

    private:
        ULONG64 DataBytesIn = 0;
        ULONG64 DataBytesOut = 0;
    };
    template <>
    class EstatsDataTracking<TcpConnectionEstatsSndCong> {
    public:
        static LPCWSTR PrintHeader() noexcept
        {
            return L"CongWin,"
                L"XIntoReceiverLimited,XIntoSenderLimited,XIntoCongestionLimited,"
                L"BytesSentRecvLimited,BytesSentSenderLimited,BytesSentCongLimited";
        }
        std::wstring PrintData() const
        {
            return ctl::ctString::format_string(
                   L",%lu%lu,%lu,%lu,%Iu,%Iu,%Iu",
                   conjestionWindow,
                   transitionsIntoReceiverLimited,
                   transitionsIntoSenderLimited,
                   transitionsIntoCongestionLimited,
                   bytesSentInReceiverLimited,
                   bytesSentInSenderLimited,
                   bytesSentInCongestionLimited);
        }
        std::unordered_map<std::wstring, ULONG> getData()
        {
            return {
                {L"conjestionWindow", conjestionWindow},
                {L"bytesSentInReceiverLimited", bytesSentInReceiverLimited},
                {L"bytesSentInSenderLimited", bytesSentInSenderLimited},
                {L"bytesSentInCongestionLimited", bytesSentInCongestionLimited},
                {L"transitionsIntoReceiverLimited", transitionsIntoReceiverLimited},
                {L"transitionsIntoSenderLimited", transitionsIntoSenderLimited},
                {L"transitionsIntoCongestionLimited", transitionsIntoCongestionLimited},
            };
        }

        template <typename PTCPROW>
        void StartTracking(const PTCPROW tcpRow) const
        {
            TCP_ESTATS_SND_CONG_RW_v0 Rw;
            Rw.EnableCollection = TRUE;
            SetPerConnectionEstats<TcpConnectionEstatsSndCong>(tcpRow, &Rw);
        }
        template <typename PTCPROW>
        void UpdateData(const PTCPROW tcpRow, const ctl::ctSockaddr& localAddr, const ctl::ctSockaddr& remoteAddr, EstatsDataTracking<TcpConnectionEstatsSndCong> *previousData)
        {
            TCP_ESTATS_SND_CONG_ROD_v0 Rod;
            FillMemory(&Rod, sizeof Rod, -1);
            if (0 == GetPerConnectionDynamicEstats<TcpConnectionEstatsSndCong>(tcpRow, &Rod)) {

#ifdef _TESTING_ESTATS_VALUES
                if (Rod.CurCwnd == InvalidLongEstatsValue ||
                    Rod.SndLimBytesRwin == InvalidLongLongEstatsValue ||
                    Rod.SndLimBytesSnd == InvalidLongLongEstatsValue ||
                    Rod.SndLimBytesCwnd == InvalidLongLongEstatsValue ||
                    Rod.SndLimTransRwin == InvalidLongEstatsValue ||
                    Rod.SndLimTransSnd == InvalidLongEstatsValue ||
                    Rod.SndLimTransCwnd == InvalidLongEstatsValue)
                {
                    WCHAR local_address[ctl::IP_STRING_MAX_LENGTH] = {};
                    (void)localAddr.writeCompleteAddress(local_address);
                    WCHAR remote_address[ctl::IP_STRING_MAX_LENGTH] = {};
                    (void)remoteAddr.writeCompleteAddress(remote_address);

                    printf(
                        "[%ws : %ws] Bad TcpConnectionEstatsSndCong (TCP_ESTATS_SND_CONG_ROD_v0): "
                        "CurCwnd (%lX) "
                        "SndLimBytesRwin (%IX) "
                        "SndLimBytesSnd (%IX) "
                        "SndLimBytesCwnd (%IX) "
                        "SndLimTransRwin (%lX) "
                        "SndLimTransSnd (%lX) "
                        "SndLimTransCwnd (%lX)\n",
                        local_address,
                        remote_address,
                        Rod.CurCwnd,
                        Rod.SndLimBytesRwin,
                        Rod.SndLimBytesSnd,
                        Rod.SndLimBytesCwnd,
                        Rod.SndLimTransRwin,
                        Rod.SndLimTransSnd,
                        Rod.SndLimTransCwnd);
                }
#endif
                UNREFERENCED_PARAMETER(localAddr);
                UNREFERENCED_PARAMETER(remoteAddr);

                if (IsRodValueValid(L"TcpConnectionEstatsSndCong - CurCwnd", Rod.CurCwnd)) {
                    conjestionWindow = Rod.CurCwnd;
                }
                if (IsRodValueValid(L"TcpConnectionEstatsSndCong - SndLimBytesRwin", Rod.SndLimBytesRwin)) {
                    bytesSentInReceiverLimited = Rod.SndLimBytesRwin - previousData->getData().find("bytesSentInReceiverLimited")->second;
                }
                if (IsRodValueValid(L"TcpConnectionEstatsSndCong - SndLimBytesSnd", Rod.SndLimBytesSnd)) {
                    bytesSentInSenderLimited = Rod.SndLimBytesSnd - previousData->getData().find("bytesSentInSenderLimited")->second;
                }
                if (IsRodValueValid(L"TcpConnectionEstatsSndCong - SndLimBytesCwnd", Rod.SndLimBytesCwnd)) {
                    bytesSentInCongestionLimited = Rod.SndLimBytesCwnd - previousData->getData().find("bytesSentInCongestionLimited")->second;
                }
                if (IsRodValueValid(L"TcpConnectionEstatsSndCong - SndLimTransRwin", Rod.SndLimTransRwin)) {
                    transitionsIntoReceiverLimited = Rod.SndLimTransRwin - previousData->getData().find("transitionsIntoReceiverLimited")->second;
                }
                if (IsRodValueValid(L"TcpConnectionEstatsSndCong - SndLimTransSnd", Rod.SndLimTransSnd)) {
                    transitionsIntoSenderLimited = Rod.SndLimTransSnd - previousData->getData().find("transitionsIntoSenderLimited")->second;
                }
                if (IsRodValueValid(L"TcpConnectionEstatsSndCong - SndLimTransCwnd", Rod.SndLimTransCwnd)) {
                    transitionsIntoCongestionLimited = Rod.SndLimTransCwnd - previousData->getData().find("transitionsIntoCongestionLimited")->second;
                }
            }
        }

    private:
        ULONG conjestionWindow;

        SIZE_T bytesSentInReceiverLimited = 0;
        SIZE_T bytesSentInSenderLimited = 0;
        SIZE_T bytesSentInCongestionLimited = 0;

        ULONG transitionsIntoReceiverLimited = 0;
        ULONG transitionsIntoSenderLimited = 0;
        ULONG transitionsIntoCongestionLimited = 0;
    };
    template <>
    class EstatsDataTracking<TcpConnectionEstatsPath> {
    public:
        static LPCWSTR PrintHeader() noexcept
        {
            return L"BytesRetrans,DupeAcks,SelectiveAcks,CongSignals,MaxSegSize,"
                L"RetransTimer,"
                L"RTT";
        }
        std::wstring PrintData() const
        {
            return ctl::ctString::format_string(
                L",%lu,%lu,%lu,%lu,%lu,%lu,%lu",
                bytesRetrans,
                dupAcksRcvd,
                sacksRcvd,
                congestionSignals,
                maxSegmentSize,
                retransmitTimer,
                roundTripTime);
        }
        std::unordered_map<std::wstring, ULONG> getData()
        {
            return {
                {L"retransmitTimer", retransmitTimer},
                {L"roundTripTime", roundTripTime},
                {L"bytesRetrans", bytesRetrans},
                {L"dupAcksRcvd", dupAcksRcvd},
                {L"sacksRcvd", sacksRcvd},
                {L"congestionSignals", congestionSignals},
                {L"maxSegmentSize", maxSegmentSize},
            };
        }

        template <typename PTCPROW>
        void StartTracking(const PTCPROW tcpRow) const
        {
            TCP_ESTATS_PATH_RW_v0 Rw;
            Rw.EnableCollection = TRUE;
            SetPerConnectionEstats<TcpConnectionEstatsPath>(tcpRow, &Rw);
        }
        template <typename PTCPROW>
        void UpdateData(const PTCPROW tcpRow, const ctl::ctSockaddr &localAddr, const ctl::ctSockaddr &remoteAddr, EstatsDataTracking<TcpConnectionEstatsPath> *previousData)
        {
            TCP_ESTATS_PATH_ROD_v0 Rod;
            FillMemory(&Rod, sizeof Rod, -1);
            if (0 == GetPerConnectionDynamicEstats<TcpConnectionEstatsPath>(tcpRow, &Rod)) {

#ifdef _TESTING_ESTATS_VALUES
                if (Rod.CurRto == InvalidLongEstatsValue ||
                    Rod.SmoothedRtt == InvalidLongEstatsValue ||
                    Rod.BytesRetrans == InvalidLongEstatsValue ||
                    Rod.DupAcksIn == InvalidLongEstatsValue ||
                    Rod.SacksRcvd == InvalidLongEstatsValue ||
                    Rod.CongSignals == InvalidLongEstatsValue ||
                    Rod.CurMss == InvalidLongEstatsValue)
                {
                    WCHAR local_address[ctl::IP_STRING_MAX_LENGTH] = {};
                    (void)localAddr.writeCompleteAddress(local_address);
                    WCHAR remote_address[ctl::IP_STRING_MAX_LENGTH] = {};
                    (void)remoteAddr.writeCompleteAddress(remote_address);

                    printf(
                        "[%ws : %ws] Bad TcpConnectionEstatsPath (TCP_ESTATS_PATH_ROD_v0): "
                        "CurRto (%lX) "
                        "SmoothedRtt (%lX) "
                        "BytesRetrans (%lX) "
                        "DupAcksIn (%lX) "
                        "SacksRcvd (%lX) "
                        "CongSignals (%lX) "
                        "CurMss (%lX)\n",
                        local_address,
                        remote_address,
                        Rod.CurRto,
                        Rod.SmoothedRtt,
                        Rod.BytesRetrans,
                        Rod.DupAcksIn,
                        Rod.SacksRcvd,
                        Rod.CongSignals,
                        Rod.CurMss);
                }
#endif
                UNREFERENCED_PARAMETER(localAddr);
                UNREFERENCED_PARAMETER(remoteAddr);

                if (IsRodValueValid(L"TcpConnectionEstatsPath - CurRto", Rod.CurRto)) {
                    retransmitTimer = Rod.CurRto;
                }
                if (IsRodValueValid(L"TcpConnectionEstatsPath - SmoothedRtt", Rod.SmoothedRtt)) {
                    roundTripTime = Rod.SmoothedRtt;
                }
                if (IsRodValueValid(L"TcpConnectionEstatsPath - BytesRetrans", Rod.BytesRetrans)) {
                    bytesRetrans = Rod.BytesRetrans - previousData->getData().find("bytesRetrans")->second;
                }
                if (IsRodValueValid(L"TcpConnectionEstatsPath - DupAcksIn", Rod.DupAcksIn)) {
                    dupAcksRcvd = Rod.DupAcksIn - previousData->getData().find("dupAcksRcvd")->second;
                }
                if (IsRodValueValid(L"TcpConnectionEstatsPath - SacksRcvd", Rod.SacksRcvd)) {
                    sacksRcvd = Rod.SacksRcvd - previousData->getData().find("sacksRcvd")->second;
                }
                if (IsRodValueValid(L"TcpConnectionEstatsPath - CongSignals", Rod.CongSignals)) {
                    congestionSignals = Rod.CongSignals - previousData->getData().find("congestionSignals")->second;
                }
                if (IsRodValueValid(L"TcpConnectionEstatsPath - CurMss", Rod.CurMss)) {
                    maxSegmentSize = Rod.CurMss;
                }
            }
        }

    private:
        ULONG retransmitTimer;
        ULONG roundTripTime;
        ULONG bytesRetrans = 0;
        ULONG dupAcksRcvd = 0;
        ULONG sacksRcvd = 0;
        ULONG congestionSignals = 0;
        ULONG maxSegmentSize = 0;
    };
    template <>
    class EstatsDataTracking<TcpConnectionEstatsRec> {
    public:
        static LPCWSTR PrintHeader() noexcept
        {
            return L"LocalRecvWin(cur),LocalRecvWin(min),LocalRecvWin(max)";
        }
        std::wstring PrintData() const
        {
            std::wstring formattedString(L",");

            formattedString += (curReceiveWindow == InvalidLongEstatsValue) ? 
                L"(bad)," : 
                ctl::ctString::format_string(L"%lu,", curReceiveWindow);

            formattedString += (minReceiveWindow == InvalidLongEstatsValue) ?
                L"(bad)," :
                ctl::ctString::format_string(L"%lu,", minReceiveWindow);

            formattedString += (maxReceiveWindow == InvalidLongEstatsValue) ?
                L"(bad)," :
                ctl::ctString::format_string(L"%lu,", maxReceiveWindow);

            return formattedString;
        }
        std::unordered_map<std::wstring, ULONG> getData()
        {
            return {
                {L"curReceiveWindow", curReceiveWindow},
                {L"minReceiveWindow", minReceiveWindow},
                {L"maxReceiveWindow", maxReceiveWindow},
            };
        }

        template <typename PTCPROW>
        void StartTracking(const PTCPROW tcpRow) const
        {
            TCP_ESTATS_REC_RW_v0 Rw;
            Rw.EnableCollection = TRUE;
            SetPerConnectionEstats<TcpConnectionEstatsRec>(tcpRow, &Rw);
        }

        template <typename PTCPROW>
        void UpdateData(const PTCPROW tcpRow, const ctl::ctSockaddr &localAddr, const ctl::ctSockaddr &remoteAddr, EstatsDataTracking<TcpConnectionEstatsRec> *previousData)
        {
            TCP_ESTATS_REC_ROD_v0 Rod;
            FillMemory(&Rod, sizeof Rod, -1);
            if (0 == GetPerConnectionDynamicEstats<TcpConnectionEstatsRec>(tcpRow, &Rod)) {

#ifdef _TESTING_ESTATS_VALUES
                if (Rod.CurRwinSent == InvalidLongEstatsValue ||
                    Rod.MinRwinSent == InvalidLongEstatsValue ||
                    Rod.MaxRwinSent == InvalidLongEstatsValue ||
                    (Rod.MinRwinSent != InvalidLongEstatsValue && Rod.MinRwinSent > Rod.MaxRwinSent && Rod.MaxRwinSent > 0))
                {
                    WCHAR local_address[ctl::IP_STRING_MAX_LENGTH] = {};
                    (void)localAddr.writeCompleteAddress(local_address);
                    WCHAR remote_address[ctl::IP_STRING_MAX_LENGTH] = {};
                    (void)remoteAddr.writeCompleteAddress(remote_address);

                    printf(
                        "[%ws : %ws] Bad TcpConnectionEstatsRec (TCP_ESTATS_REC_ROD_v0): "
                        "CurRwinSent (%lX) "
                        "MinRwinSent (%lX) "
                        "MaxRwinSent (%lX)\n",
                        local_address,
                        remote_address,
                        Rod.CurRwinSent,
                        Rod.MinRwinSent,
                        Rod.MaxRwinSent);
                }
#endif
                UNREFERENCED_PARAMETER(localAddr);
                UNREFERENCED_PARAMETER(remoteAddr);

                if (IsRodValueValid(L"TcpConnectionEstatsRec - CurRwinSent", Rod.CurRwinSent)) {
                    curReceiveWindow = Rod.CurRwinSent;
                }
                if (IsRodValueValid(L"TcpConnectionEstatsRec - MinRwinSent", Rod.MinRwinSent)) {
                    minReceiveWindow = Rod.MinRwinSent;
                }
                if (IsRodValueValid(L"TcpConnectionEstatsRec - MaxRwinSent", Rod.MaxRwinSent)) {
                    maxReceiveWindow = Rod.MaxRwinSent;
                }
            }
        }

    private:
        ULONG curReceiveWindow = 0;
        ULONG minReceiveWindow = 0;
        ULONG maxReceiveWindow = 0;
    };
    template <>
    class EstatsDataTracking<TcpConnectionEstatsObsRec> {
    public:
        static LPCWSTR PrintHeader() noexcept
        {
            return L"LocalRecvWin(cur),LocalRecvWin(min),LocalRecvWin(max)";
        }
        std::wstring PrintData() const
        {
            std::wstring formattedString(L",");

            formattedString += (curReceiveWindow == InvalidLongEstatsValue) ? 
                L"(bad)," : 
                ctl::ctString::format_string(L"%lu,", curReceiveWindow);

            formattedString += (minReceiveWindow == InvalidLongEstatsValue) ? 
                L"(bad)," : 
                ctl::ctString::format_string(L"%lu,", minReceiveWindow);

            formattedString += (maxReceiveWindow == InvalidLongEstatsValue) ? 
                L"(bad)," : 
                ctl::ctString::format_string(L"%lu,", maxReceiveWindow);

            return formattedString;
        }
        std::unordered_map<std::wstring, ULONG> getData()
        {
            return {
                {L"curReceiveWindow", curReceiveWindow},
                {L"minReceiveWindow", minReceiveWindow},
                {L"maxReceiveWindow", maxReceiveWindow},
            };
        }

        template <typename PTCPROW>
        void StartTracking(const PTCPROW tcpRow) const
        {
            TCP_ESTATS_OBS_REC_RW_v0 Rw;
            Rw.EnableCollection = TRUE;
            SetPerConnectionEstats<TcpConnectionEstatsObsRec>(tcpRow, &Rw);
        }
        template <typename PTCPROW>
        void UpdateData(const PTCPROW tcpRow, const ctl::ctSockaddr &localAddr, const ctl::ctSockaddr &remoteAddr, EstatsDataTracking<TcpConnectionEstatsObsRec> *previousData)
        {
            TCP_ESTATS_OBS_REC_ROD_v0 Rod;
            FillMemory(&Rod, sizeof Rod, -1);
            if (0 == GetPerConnectionDynamicEstats<TcpConnectionEstatsObsRec>(tcpRow, &Rod)) {

#ifdef _TESTING_ESTATS_VALUES
                if (Rod.CurRwinRcvd == InvalidLongEstatsValue ||
                    Rod.MinRwinRcvd == InvalidLongEstatsValue ||
                    Rod.MaxRwinRcvd == InvalidLongEstatsValue ||
                    (Rod.MinRwinRcvd != InvalidLongEstatsValue && Rod.MinRwinRcvd != 0xffffffff && Rod.MinRwinRcvd > Rod.MaxRwinRcvd && Rod.MaxRwinRcvd > 0))
                {
                    WCHAR local_address[ctl::IP_STRING_MAX_LENGTH] = {};
                    (void)localAddr.writeCompleteAddress(local_address);
                    WCHAR remote_address[ctl::IP_STRING_MAX_LENGTH] = {};
                    (void)remoteAddr.writeCompleteAddress(remote_address);

                    printf(
                        "[%ws : %ws] Bad TcpConnectionEstatsObsRec (TCP_ESTATS_OBS_REC_ROD_v0): "
                        "CurRwinRcvd (%lX) "
                        "MinRwinRcvd (%lX) "
                        "MaxRwinRcvd (%lX)\n",
                        local_address,
                        remote_address,
                        Rod.CurRwinRcvd,
                        Rod.MinRwinRcvd,
                        Rod.MaxRwinRcvd);
                }
#endif
                UNREFERENCED_PARAMETER(localAddr);
                UNREFERENCED_PARAMETER(remoteAddr);

                if (IsRodValueValid(L"TcpConnectionEstatsObsRec - CurRwinRcvd", Rod.CurRwinRcvd)) {
                    curReceiveWindow = Rod.CurRwinRcvd;
                }
                if (IsRodValueValid(L"TcpConnectionEstatsObsRec - MinRwinRcvd", Rod.MinRwinRcvd)) {
                    minReceiveWindow = Rod.MinRwinRcvd;
                }
                if (IsRodValueValid(L"TcpConnectionEstatsObsRec - MaxRwinRcvd", Rod.MaxRwinRcvd)) {
                    maxReceiveWindow = Rod.MaxRwinRcvd;
                }
            }
        }

    private:
        ULONG curReceiveWindow = 0;
        ULONG minReceiveWindow = 0;
        ULONG maxReceiveWindow = 0;
    };
    // TODO: Implement
    template <>
    class EstatsDataTracking<TcpConnectionEstatsBandwidth> {
    public:
        static LPCWSTR PrintHeader() noexcept
        {
            return L"";
        }
        std::wstring PrintData() const
        {
            return L"";
        }
        std::unordered_map<std::wstring, ULONG> getData()
        {
            return {};
        }

        template <typename PTCPROW>
        void StartTracking(const PTCPROW tcpRow) const
        {
            TCP_ESTATS_BANDWIDTH_RW_v0 Rw;
            Rw.EnableCollectionInbound = TcpBoolOptEnabled;
            Rw.EnableCollectionOutbound = TcpBoolOptEnabled;
            SetPerConnectionEstats<TcpConnectionEstatsBandwidth>(tcpRow, &Rw);
        }
        template <typename PTCPROW>
        void UpdateData(const PTCPROW tcpRow, const ctl::ctSockaddr &, const ctl::ctSockaddr &, EstatsDataTracking<TcpConnectionEstatsBandwidth> *previousData)
        {
            TCP_ESTATS_BANDWIDTH_ROD_v0 Rod;
            FillMemory(&Rod, sizeof Rod, -1);
            if (0 == GetPerConnectionDynamicEstats<TcpConnectionEstatsBandwidth>(tcpRow, &Rod)) {
                // store data from this instance
            }
        }
    };
    // TODO: Implement
    template <>
    class EstatsDataTracking<TcpConnectionEstatsFineRtt> {
    public:
        static LPCWSTR PrintHeader() noexcept
        {
            return L"";
        }
        std::wstring PrintData() const
        {
            return L"";
        }
        std::unordered_map<std::wstring, ULONG> getData()
        {
            return {};
        }

        template <typename PTCPROW>
        void StartTracking(const PTCPROW tcpRow) const
        {
            TCP_ESTATS_FINE_RTT_RW_v0 Rw;
            Rw.EnableCollection = TRUE;
            SetPerConnectionEstats<TcpConnectionEstatsFineRtt>(tcpRow, &Rw);
        }
        template <typename PTCPROW>
        void UpdateData(const PTCPROW tcpRow, const ctl::ctSockaddr &, const ctl::ctSockaddr &, EstatsDataTracking<TcpConnectionEstatsFineRtt> *previousData)
        {
            TCP_ESTATS_FINE_RTT_ROD_v0 Rod;
            FillMemory(&Rod, sizeof Rod, -1);
            if (0 == GetPerConnectionDynamicEstats<TcpConnectionEstatsFineRtt>(tcpRow, &Rod)) {
                // store data from this instance
            }
        }
    };

    template <TCP_ESTATS_TYPE TcpType>
    class EstatsDataPoint {
    public:
        static LPCWSTR PrintAddressHeader() noexcept
        {
            return L"LocalAddress,RemoteAddress";
        }
        static LPCWSTR PrintHeader()
        {
            return EstatsDataTracking<TcpType>::PrintHeader();
        }

        EstatsDataPoint(ctl::ctSockaddr local_addr, ctl::ctSockaddr remote_addr) noexcept :
            localAddr(std::move(local_addr)),
            remoteAddr(std::move(remote_addr))
        {
        }

        explicit EstatsDataPoint(const PMIB_TCPROW pTcpRow) noexcept :  // NOLINT
            localAddr(AF_INET),
            remoteAddr(AF_INET)
        {
            localAddr.setAddress(
                reinterpret_cast<const PIN_ADDR>(&pTcpRow->dwLocalAddr));
            localAddr.setPort(
                static_cast<unsigned short>(pTcpRow->dwLocalPort),
                ctl::ByteOrder::NetworkOrder);

            remoteAddr.setAddress(
                reinterpret_cast<const PIN_ADDR>(&pTcpRow->dwRemoteAddr));
            remoteAddr.setPort(
                static_cast<unsigned short>(pTcpRow->dwRemotePort),
                ctl::ByteOrder::NetworkOrder);
        }

        explicit EstatsDataPoint(const PMIB_TCP6ROW pTcpRow) noexcept :  // NOLINT
            localAddr(AF_INET6),
            remoteAddr(AF_INET6)
        {
            localAddr.setAddress(&pTcpRow->LocalAddr);
            localAddr.setPort(
                static_cast<unsigned short>(pTcpRow->dwLocalPort),
                ctl::ByteOrder::NetworkOrder);

            remoteAddr.setAddress(&pTcpRow->RemoteAddr);
            remoteAddr.setPort(
                static_cast<unsigned short>(pTcpRow->dwRemotePort),
                ctl::ByteOrder::NetworkOrder);
        }

        ~EstatsDataPoint() = default;
        EstatsDataPoint(const EstatsDataPoint&) = delete;
        EstatsDataPoint& operator=(const EstatsDataPoint&) = delete;
        EstatsDataPoint(EstatsDataPoint&&) = delete;
        EstatsDataPoint& operator=(EstatsDataPoint&&) = delete;

        bool operator< (const EstatsDataPoint<TcpType>& rhs) const noexcept
        {
            if (localAddr < rhs.localAddr) {
                return true;
            }
            if (localAddr == rhs.localAddr &&
                remoteAddr < rhs.remoteAddr) {
                return true;
            }
            return false;
        }
        bool operator==(const EstatsDataPoint<TcpType>& rhs) const noexcept
        {
            return (localAddr == rhs.localAddr) && 
                   (remoteAddr == rhs.remoteAddr);
        }

        std::wstring PrintAddresses() const
        {
            WCHAR local_string[ctl::IP_STRING_MAX_LENGTH];
            (void)localAddr.writeCompleteAddress(local_string);
            WCHAR remote_string[ctl::IP_STRING_MAX_LENGTH];
            (void)remoteAddr.writeCompleteAddress(remote_string);

            return ctl::ctString::format_string(
                L"%ws,%ws",
                local_string,
                remote_string);
        }
        std::wstring PrintData() const
        {
            return data.PrintData();
        }

        template <typename T>
        void StartTracking(T tcpRow) const
        {
            data.StartTracking(tcpRow);
        }

        template <typename T>
        void UpdateData(T tcpRow, ULONG currentCounter, EstatsDataTracking<TcpType>* previousData) const
        {
            latestCounter = currentCounter;
            data.UpdateData(tcpRow, localAddr, remoteAddr, previousData);
        }

        ctl::ctSockaddr LocalAddr() const noexcept
        {
            return localAddr;
        }
        ctl::ctSockaddr RemoteAddr() const noexcept
        {
            return remoteAddr;
        }

        ULONG LastestCounter() const noexcept
        {
            return latestCounter;
        }

    private:
        ctl::ctSockaddr localAddr;
        ctl::ctSockaddr remoteAddr;
        // the tracking object must be mutable because EstatsDataPoint instances
        // are stored in a std::set container, and access to objects in a std::set
        // must be const (since you are not allowed to modify std::set objects in-place)
        mutable EstatsDataTracking<TcpType> data;
        mutable ULONG latestCounter = 0;
    };
} // namespace



class ctsEstats
{
public:
    ctsEstats() :
        pathInfoWriter(L"EstatsPathInfo.csv"),
        receiveWindowWriter(L"EstatsReceiveWindow.csv"),
        senderCongestionWriter(L"EstatsSenderCongestion.csv"),
        tcpTable(StartingTableSize)
    {
    }


    ctsEstats(const ctsEstats&) = delete;
    ctsEstats& operator=(const ctsEstats&) = delete;
    ctsEstats(ctsEstats&&) = delete;
    ctsEstats& operator=(ctsEstats&&) = delete;


    ~ctsEstats() noexcept
    {
        timer.stop_all_timers();

        try {
            writeMostRecentData();
        }
        catch (const std::exception& e) {
            wprintf(L"~Estats exception: %ws\n", ctl::ctString::format_exception(e).c_str());
        }
    }

    void writeMostRecentData()
    {
        for (const auto &entry : pathInfoDataTable.back())
        {
            pathInfoWriter.write_row(
                entry.PrintAddresses() +
                entry.PrintData());
        }

        for (const auto &entry : localReceiveWindowDataTable.back())
        {
            const details::EstatsDataPoint<TcpConnectionEstatsObsRec> matchingData(
                entry.LocalAddr(),
                entry.RemoteAddr());

            const auto foundEntry = remoteReceiveWindowDataTable.back().find(matchingData);
            if (foundEntry != remoteReceiveWindowDataTable.back().end())
            {
                receiveWindowWriter.write_row(
                    entry.PrintAddresses() +
                    entry.PrintData() +
                    foundEntry->PrintData());
            }
        }

        for (const auto &entry : senderCongestionDataTable.back())
        {
            const details::EstatsDataPoint<TcpConnectionEstatsData> matchingData(
                entry.LocalAddr(),
                entry.RemoteAddr());

            const auto foundEntry = byteTrackingDataTable.back().find(matchingData);
            if (foundEntry != byteTrackingDataTable.back().end())
            {
                senderCongestionWriter.write_row(
                    entry.PrintAddresses() +
                    entry.PrintData() +
                    foundEntry->PrintData());
            }
        }
    }

    bool start() noexcept
    {
        // ReSharper disable once CppInitializedValueIsAlwaysRewritten
        auto started = false;
        try {
            pathInfoWriter.create_file(
                std::wstring(details::EstatsDataPoint<TcpConnectionEstatsPath>::PrintAddressHeader()) +
                L"," + details::EstatsDataPoint<TcpConnectionEstatsPath>::PrintHeader());
            receiveWindowWriter.create_file(
                std::wstring(details::EstatsDataPoint<TcpConnectionEstatsRec>::PrintAddressHeader()) +
                L"," + details::EstatsDataPoint<TcpConnectionEstatsRec>::PrintHeader() +
                L"," + details::EstatsDataPoint<TcpConnectionEstatsObsRec>::PrintHeader());
            senderCongestionWriter.create_file(
                std::wstring(details::EstatsDataPoint<TcpConnectionEstatsSndCong>::PrintAddressHeader()) +
                L"," + details::EstatsDataPoint<TcpConnectionEstatsSndCong>::PrintHeader() +
                L"," + details::EstatsDataPoint<TcpConnectionEstatsData>::PrintHeader());

            started = UpdateEstats();
        }
        catch (const std::exception& e) {
            wprintf(L"ctsEstats::Start exception: %ws\n", ctl::ctString::format_exception(e).c_str());
            started = false;
        }

        if (!started) {
            timer.stop_all_timers();
        }

        return started;
    }

private:
    ctl::ctThreadpoolTimer timer;

    std::vector<std::set<details::EstatsDataPoint<TcpConnectionEstatsSynOpts>>> synOptsDataTable;
    std::vector<std::set<details::EstatsDataPoint<TcpConnectionEstatsData>>> byteTrackingDataTable;
    std::vector<std::set<details::EstatsDataPoint<TcpConnectionEstatsPath>>> pathInfoDataTable;
    std::vector<std::set<details::EstatsDataPoint<TcpConnectionEstatsRec>>> localReceiveWindowDataTable;
    std::vector<std::set<details::EstatsDataPoint<TcpConnectionEstatsObsRec>>> remoteReceiveWindowDataTable;
    std::vector<std::set<details::EstatsDataPoint<TcpConnectionEstatsSndCong>>> senderCongestionDataTable;

    ctsWriteDetails pathInfoWriter;
    ctsWriteDetails receiveWindowWriter;
    ctsWriteDetails senderCongestionWriter;

    // since updates are always serialized on a timer, just reuse the same buffer
    static const ULONG StartingTableSize = 4096;
    std::vector<char> tcpTable;
    ULONG tableCounter = 0;

    bool UpdateEstats() noexcept
    try
    {
        bool accessDenied = false;
        ++tableCounter;
        try {
            // IPv4
            RefreshIPv4Data();
            const auto pIpv4TcpTable = reinterpret_cast<PMIB_TCPTABLE>(&tcpTable[0]);
            // Update all stats for each IPv4 entry
            for (unsigned count = 0; count < pIpv4TcpTable->dwNumEntries; ++count)
            {
                // Ignore listening, closed, and closing connections
                const auto tableEntry = &pIpv4TcpTable->table[count];
                if (tableEntry->dwState == MIB_TCP_STATE_LISTEN ||
                    tableEntry->dwState == MIB_TCP_STATE_TIME_WAIT ||
                    tableEntry->dwState == MIB_TCP_STATE_DELETE_TCB) {
                    continue;
                }

                try {
                    UpdateDataPoints(synOptsDataTable, tableEntry);
                    UpdateDataPoints(byteTrackingDataTable, tableEntry);
                    UpdateDataPoints(pathInfoDataTable, tableEntry);
                    UpdateDataPoints(localReceiveWindowDataTable, tableEntry);
                    UpdateDataPoints(remoteReceiveWindowDataTable, tableEntry);
                    UpdateDataPoints(senderCongestionDataTable, tableEntry);
                }
                catch (const ctl::ctException& e) {
                    if (e.why() == ERROR_ACCESS_DENIED) {
                        accessDenied = true;
                        throw;
                    }
                }
            }

            // IPv6
            RefreshIPv6Data();
            const auto pIpv6TcpTable = reinterpret_cast<PMIB_TCP6TABLE>(&tcpTable[0]);
            // Update all stats for each IPv6 entry
            for (unsigned count = 0; count < pIpv6TcpTable->dwNumEntries; ++count)
            {
                // Ignore listening, closed, and closing connections
                const auto tableEntry = &pIpv6TcpTable->table[count];
                if (tableEntry->State == MIB_TCP_STATE_LISTEN ||
                    tableEntry->State == MIB_TCP_STATE_TIME_WAIT ||
                    tableEntry->State == MIB_TCP_STATE_DELETE_TCB) {
                    continue;
                }

                try {
                    UpdateDataPoints(synOptsDataTable, tableEntry);
                    UpdateDataPoints(byteTrackingDataTable, tableEntry);
                    UpdateDataPoints(pathInfoDataTable, tableEntry);
                    UpdateDataPoints(localReceiveWindowDataTable, tableEntry);
                    UpdateDataPoints(remoteReceiveWindowDataTable, tableEntry);
                    UpdateDataPoints(senderCongestionDataTable, tableEntry);
                }
                catch (const ctl::ctException& e) {
                    if (e.why() == ERROR_ACCESS_DENIED) {
                        accessDenied = true;
                        throw;
                    }
                }
            }

            RemoveStaleDataPoints();
        }
        catch (const std::exception& e) {
            wprintf(L"ctsEstats::UpdateEstats exception: %ws\n", ctl::ctString::format_exception(e).c_str());
        }

        if (!accessDenied) {
            // schedule timer from this moment
            timer.schedule_singleton([this]() { UpdateEstats(); }, 1000);
        }

        return !accessDenied;
    }
    catch (const std::exception& e) {
        wprintf(L"ctsEstats::UpdateEstats exception: %ws\n", ctl::ctString::format_exception(e).c_str());
        return false;
    }

    void RefreshIPv4Data()
    {
        tcpTable.resize(tcpTable.capacity());
        auto table_size = static_cast<DWORD>(tcpTable.size());
        ZeroMemory(&tcpTable[0], table_size);

        ULONG error = ::GetTcpTable(
            reinterpret_cast<PMIB_TCPTABLE>(&tcpTable[0]),
            &table_size,
            FALSE); // no need to sort them
        if (ERROR_INSUFFICIENT_BUFFER == error) {
            tcpTable.resize(table_size);
            error = ::GetTcpTable(
                reinterpret_cast<PMIB_TCPTABLE>(&tcpTable[0]),
                &table_size,
                FALSE); // no need to sort them
        }
        if (error != ERROR_SUCCESS) {
            throw ctl::ctException(error, L"GetTcpTable", L"ctsEstats", false);
        }
    }
    void RefreshIPv6Data()
    {
        tcpTable.resize(tcpTable.capacity());
        auto table_size = static_cast<DWORD>(tcpTable.size());
        ZeroMemory(&tcpTable[0], table_size);

        ULONG error = ::GetTcp6Table(
            reinterpret_cast<PMIB_TCP6TABLE>(&tcpTable[0]),
            &table_size,
            FALSE); // no need to sort them
        if (ERROR_INSUFFICIENT_BUFFER == error) {
            tcpTable.resize(table_size);
            error = ::GetTcp6Table(
                reinterpret_cast<PMIB_TCP6TABLE>(&tcpTable[0]),
                &table_size,
                FALSE); // no need to sort them
        }
        if (error != ERROR_SUCCESS) {
            throw ctl::ctException(error, L"GetTcp6Table", L"ctsEstats", false);
        }
    }

    template <TCP_ESTATS_TYPE TcpType, typename Mibtype>
    void UpdateDataPoints(std::vector<std::set<details::EstatsDataPoint<TcpType>>>& dataTable, Mibtype tableEntry)
    {
        std::wcout << dataTable.size() << tableEntry->dwLocalAddr << std::endl;
        // const auto localAddr = tableEntry->dwLocalAddr;
        // const auto remoteAddr = tableEntry->dwRemoteAddr;

        // // Initialize a new tcp connection entry in the set
        // const auto emplaceResults = dataTable.back().emplace(tableEntry);

        // // Attempt to get previous entry

        // const auto previousDataPoint = dataTable.back().find(
        //     details::EstatsDataPoint<TcpType>(localAddr, remoteAddr));
        // // If previous entry present
        // if (previousDataPoint != dataTable.back().end())
        // {
        //     emplaceResults.first->UpdateData(tableEntry, tableCounter, &previousDataPoint);
        // }
        // // If this is the first entry of this connection
        // else
        // {
        //     emplaceResults.first->StartTracking(tableEntry);
        //     emplaceResults.first->UpdateData(tableEntry, tableCounter, nullptr);
        // }
    }

    //TODO
    void RemoveStaleDataPoints()
    {
        /*
        // walk the set of synOptsData. If an address wasn't found to have been updated
        // with the latest data, then we'll remove that tuple (local address + remote address)
        // from all the data sets and finish printing their rows
        auto foundInstance = std::find_if(
            std::begin(synOptsData),
            std::end(synOptsData),
            [&](const details::EstatsDataPoint<TcpConnectionEstatsSynOpts>& dataPoint)
        {
            return dataPoint.LastestCounter() != tableCounter;
        });

        while (foundInstance != std::end(synOptsData))
        {
            const ctl::ctSockaddr localAddr(foundInstance->LocalAddr());
            const ctl::ctSockaddr remoteAddr(foundInstance->RemoteAddr());

            const auto synOptsInstance = foundInstance;
            const auto byteTrackingInstance = byteTrackingData.find(
                details::EstatsDataPoint<TcpConnectionEstatsData>(localAddr, remoteAddr));
            const auto fByteTrackingInstanceFound = byteTrackingInstance != byteTrackingData.end();

            const auto pathInfoInstance = pathInfoData.find(
                details::EstatsDataPoint<TcpConnectionEstatsPath>(localAddr, remoteAddr));
            const auto fPathInfoInstanceFound = pathInfoInstance != pathInfoData.end();

            const auto localReceiveWindowInstance = localReceiveWindowData.find(
                details::EstatsDataPoint<TcpConnectionEstatsRec>(localAddr, remoteAddr));
            const auto fLocalReceiveWindowInstanceFound = localReceiveWindowInstance != localReceiveWindowData.end();

            const auto remoteReceiveWindowInstance = remoteReceiveWindowData.find(
                details::EstatsDataPoint<TcpConnectionEstatsObsRec>(localAddr, remoteAddr));
            const auto fRemoteReceiveWindowInstanceFound = remoteReceiveWindowInstance != remoteReceiveWindowData.end();

            const auto senderCongestionInstance = senderCongestionData.find(
                details::EstatsDataPoint<TcpConnectionEstatsSndCong>(localAddr, remoteAddr));
            const auto fSenderCongestionInstanceFound = senderCongestionInstance != senderCongestionData.end();

            if (fPathInfoInstanceFound) {
                pathInfoWriter.write_row(
                    pathInfoInstance->PrintAddresses() +
                    pathInfoInstance->PrintData());
            }

            if (fLocalReceiveWindowInstanceFound && fRemoteReceiveWindowInstanceFound) {
                receiveWindowWriter.write_row(
                    localReceiveWindowInstance->PrintAddresses() +
                    localReceiveWindowInstance->PrintData() +
                    remoteReceiveWindowInstance->PrintData());
            }
            
            if (fSenderCongestionInstanceFound && fByteTrackingInstanceFound) {
                senderCongestionWriter.write_row(
                    senderCongestionInstance->PrintAddresses() +
                    senderCongestionInstance->PrintData() +
                    byteTrackingInstance->PrintData());
            }

            synOptsData.erase(synOptsInstance);
            if (fByteTrackingInstanceFound) {
                byteTrackingData.erase(byteTrackingInstance);
            }
            if (fLocalReceiveWindowInstanceFound) {
                localReceiveWindowData.erase(localReceiveWindowInstance);
            }
            if (fRemoteReceiveWindowInstanceFound) {
                remoteReceiveWindowData.erase(remoteReceiveWindowInstance);
            }
            if (fSenderCongestionInstanceFound) {
                senderCongestionData.erase(senderCongestionInstance);
            }

            // update the while loop variable
            foundInstance = std::find_if(
                std::begin(synOptsData),
                std::end(synOptsData),
                [&](const details::EstatsDataPoint<TcpConnectionEstatsSynOpts>& dataPoint)
            {
                return dataPoint.LastestCounter() != tableCounter;
            });
        } // while loop
        */
    }
};
} // namespace
