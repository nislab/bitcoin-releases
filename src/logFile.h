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
#define LOG_TRANSACTION_INVS            0
#define LOG_TRANSACTIONS                0
#define LOG_ORPHAN_TRANSACTIONS         0

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
void logFile(std::string info, INVTYPE type, INVEVENT = BEFORE, int counter = 0, std::string fileName = "");
#if LOG_TRANSACTION_INVS
    void logFile(CInv inv, std::string from, std::string fileName = "");
#endif
#if LOG_TRANSACTIONS
    void logFile(CTransaction tx, std::string from, std::string duration, std::string fileName = "");
#endif
void logFile(std::deque<CInv>::iterator begin, std::deque<CInv>::iterator end, std::string from, std::string fileName = "");

#endif
