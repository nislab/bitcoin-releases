//Created by Nabeel Younis <nyounis@bu.edu> on 08/10/2017
//log file header function used to log certain activity of Bitcoin Core
#ifndef LOGFILE_H
#define LOGFILE_H

#include <iostream>
#include <fstream>
#include <string>
#include <ctime>
#include "blockencodings.h" // Compact block, getblocktxn, blocktxn, normal block
#include "protocol.h" //CInv
#include "validation.h" //for cs_main
#include "net.h" // CConnman

// get current user name for logfile
// https://stackoverflow.com/a/8953445
#include <pwd.h>
#include <unistd.h>

class Env
{
    public:
    static std::string getUserName()
    {
        register struct passwd *pw;
        register uid_t uid;

        uid = geteuid ();
        pw = getpwuid (uid);
        if (pw)
        {
                return std::string(pw->pw_name);
        }
        return std::string("");
    }
};

enum INVTYPE
{
    FALAFEL_SENT,
    FALAFEL_RECEIVED,
};

enum INVEVENT
{
    BEFORE,
    AFTER,
};

#define LOG_NEIGHBOR_ADDRESSES          0
#define LOG_CPU_USAGE                   0
#define ENABLE_FALAFEL_SYNC             0
#define ENABLE_FALAFEL_PING             0
#define FALAFEL_SENDER                  0
#define FALAFEL_RECEIVER                0
#define ENABLE_SPECIAL_SYNC             0
#define SYNC_ENTIRE_MEMPOOL_AT_CONNECT  0
#define LOG_TRANSACTION_INVS            0
#define LOG_TRANSACTIONS                0
#define LOG_ORPHAN_TRANSACTIONS         0
#define LOG_FALAFEL_TRANSACTIONS        0
#define LOG_FALAFEL_ORPHAN_TRANSACTIONS 0
#define LOG_TX_RELAY                    0

#if !ENABLE_FALAFEL_SYNC && (FALAFEL_SENDER || FALAFEL_RECEIVER)
    #error "FalafelSync must be enabled"
#endif

#if ENABLE_FALAFEL_SYNC && !(FALAFEL_SENDER ^ FALAFEL_RECEIVER)
    #error "Must be only Falafel sender or receiver"
#endif

#if !FALAFEL_SENDER && ENABLE_FALAFEL_PING
    #error "Must be a Falafel sender to enable ping"
#endif

#if !FALAFEL_SENDER && ENABLE_SPECIAL_SYNC
    #error "Must be a Falafel sender to enable special sync"
#endif

#if !FALAFEL_SENDER && SYNC_ENTIRE_MEMPOOL_AT_CONNECT
    #error "Must be a Falafel sender to enable syncing entire mempool at connect"
#endif

#if !FALAFEL_RECEIVER && (LOG_FALAFEL_TRANSACTIONS || LOG_FALAFEL_ORPHAN_TRANSACTIONS)
    #error "Must be a Falafel receiver to enable logging falafel (orphan) transactions"
#endif

#if FALAFEL_SENDER && (LOG_FALAFEL_TRANSACTIONS || LOG_FALAFEL_ORPHAN_TRANSACTIONS)
    #error "Cannot be a Falafel sender and log falafel (orphan) transactions"
#endif

#if SYNC_ENTIRE_MEMPOOL_AT_CONNECT &&!ENABLE_FALAFEL_PING
    #error "Ping must be enabled to sync entire mempool at connect"
#endif

#if FALAFEL_SENDER
    const std::vector<std::string> WhiteListedSubnets = {"128.197.128",};
    bool isNodeInWhiteListedSubnets(std::string node);
#endif

// function prototypes for different logging functions
std::string createTimeStamp();
bool initLogger();
#if LOG_NEIGHBOR_ADDRESSES
    bool initAddrLogger();
    void AddrLoggerThread(CConnman* connman);
#endif
#if LOG_CPU_USAGE
    bool initProcessCPUUsageLogger();
    void CPUUsageLoggerThread();
    unsigned long getProcessCPUStats();
    unsigned long getTotalTime();
#endif
void logFile(std::string info, std::string fileName = "", std::string from = ""); //logging a simple statement with timestamp
int  logFile(CBlockHeaderAndShortTxIDs &Cblock, std::string from, std::string fileName = "");//info from cmpctBlock
void logFile(BlockTransactionsRequest &req, int inc, std::string fileName = ""); //info from getblocktxn
#if ENABLE_FALAFEL_SYNC
    int  logFile(std::vector <CInv> vInv, INVTYPE type = FALAFEL_SENT, std::string from = "", std::string fileName = "");
#endif
void logFile(std::string info, INVTYPE type, INVEVENT = BEFORE, int counter = 0, std::string fileName = "");
#if (LOG_TRANSACTION_INVS || FALAFEL_RECEIVER)
    void logFile(CInv inv, std::string from, std::string fileName = "");
#endif
#if (LOG_TRANSACTIONS || LOG_FALAFEL_TRANSACTIONS)
    void logFile(CTransaction tx, std::string from, std::string duration, std::string fileName = "");
#endif
void logFile(std::deque<CInv>::iterator begin, std::deque<CInv>::iterator end, std::string from, std::string fileName = "");
#if LOG_TX_RELAY
    void logFile(CInv vInv, std::string to, int dummy);
    void logFile(std::vector <CInv> vInv, std::string to);
#endif

#endif
