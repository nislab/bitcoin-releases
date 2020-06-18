//Custom log file system written to monitor the behaviour of
//compact blocks, getblocktxn, and blocktxn messages on a bitcoin node
//Soft forks created by two blocks coming in are also recorded

#include "logFile.h"
#include "logging.h"
#include <chrono>
#include <shutdown.h>

#define UNIX_TIMESTAMP \
    std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count())
#define UNIX_SYSTEM_TIMESTAMP \
    std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count())

bool debug = false;
Env env;
const static std::string nodeID = env.getUserName();
static std::string strDataDir;
static std::string directory;
#if LOG_ORPHAN_TRANSACTIONS
    static std::string orphanTXdir;
#endif
#if FALAFEL_RECEIVER
    static std::string invRXdir;
#if LOG_FALAFEL_ORPHAN_TRANSACTIONS
    static std::string falafelOrphanTXdir;
#endif
#endif
#if (LOG_TRANSACTIONS || LOG_TRANSACTION_INVS || LOG_TX_RELAY || LOG_FALAFEL_TRANSACTIONS || FALAFEL_RECEIVER)
    static std::string txdir;
#endif
#if LOG_NEIGHBOR_ADDRESSES
    static std::string addrLoggerdir;
    static int64_t addrLoggerTimeoutSecs = 1;
#endif

void dumpMemPool(std::string fileName = "", INVTYPE type = FALAFEL_SENT, INVEVENT event = BEFORE, int counter = 0);

/*
 * Create a human readable timestamp
 */
std::string createTimeStamp()
{
    time_t currTime;
    struct tm* timeStamp;
    std::string timeString;

    time(&currTime); //returns seconds since epoch
    timeStamp = localtime(&currTime); //converts seconds to tm struct
    timeString = asctime(timeStamp); //converts tm struct to readable timestamp string
    timeString.back() = ' '; //replaces newline with space character
    return timeString + UNIX_SYSTEM_TIMESTAMP + " : ";
}

/*
 * Initialize log file system that records events in Bitcoin
 */
bool initLogger()
{
    // paths to different directories
    strDataDir = GetDataDir().string();
    directory  = strDataDir + "/expLogFiles/";
#if LOG_ORPHAN_TRANSACTIONS
    orphanTXdir = directory + "/parents/";
#endif
#if FALAFEL_RECEIVER
    invRXdir   = directory + "/received/";
#if LOG_FALAFEL_ORPHAN_TRANSACTIONS
    falafelOrphanTXdir = directory + "/falafel_parents/";
#endif
#endif
#if (LOG_TRANSACTIONS || LOG_TRANSACTION_INVS || LOG_TX_RELAY || LOG_FALAFEL_TRANSACTIONS || FALAFEL_RECEIVER)
    txdir      = directory + "/txs/";
#endif

    // create directories if they do not exist
    // - create main directory if it does not exist
    boost::filesystem::path dir(directory);
    if(!(boost::filesystem::exists(dir)))
    {
        if(LogInstance().m_print_to_console)
            std::cout << "Directory <" << dir << "> doesn't exist; creating directory" << std::endl;
        if(boost::filesystem::create_directory(dir))
        {
            if(LogInstance().m_print_to_console)
                std::cout << "Directory <" << dir << "> created successfully" << std::endl;
        }
        else
        {
            if(LogInstance().m_print_to_console)
                std::cout << "Directory <" << dir << "> not created" << std::endl;
            return false;
        }
    }

#if LOG_ORPHAN_TRANSACTIONS
    // - create directory to record information about received parents of orphan transactions
    //   if it does not exit
    boost::filesystem::path OT(orphanTXdir);
    if(!(boost::filesystem::exists(OT)))
    {
        if(LogInstance().m_print_to_console)
            std::cout << "Directory <" << OT << "> doesn't exist; creating directory" << std::endl;
        if(boost::filesystem::create_directory(OT))
        {
            if(LogInstance().m_print_to_console)
                std::cout << "Directory <" << OT << "> created successfully" << std::endl;
        }
        else
        {
            if(LogInstance().m_print_to_console)
                std::cout << "Directory <" << OT << "> not created" << std::endl;
            return false;
        }
    }
#endif

#if FALAFEL_RECEIVER
    // - create directory to record information about received inv messages
    //   if it does not exit
    boost::filesystem::path RX(invRXdir);
    if(!(boost::filesystem::exists(RX)))
    {
        if(LogInstance().m_print_to_console)
            std::cout << "Directory <" << RX << "> doesn't exist; creating directory" << std::endl;
        if(boost::filesystem::create_directory(RX))
        {
            if(LogInstance().m_print_to_console)
                std::cout << "Directory <" << RX << "> created successfully" << std::endl;
        }
        else
        {
            if(LogInstance().m_print_to_console)
                std::cout << "Directory <" << RX << "> not created" << std::endl;
            return false;
        }
    }
#if LOG_FALAFEL_ORPHAN_TRANSACTIONS
    // - create directory to record information about received parents of orphan transactions
    //   if it does not exit
    boost::filesystem::path FOT(falafelOrphanTXdir);
    if(!(boost::filesystem::exists(FOT)))
    {
        if(LogInstance().m_print_to_console)
            std::cout << "Directory <" << FOT << "> doesn't exist; creating directory" << std::endl;
        if(boost::filesystem::create_directory(FOT))
        {
            if(LogInstance().m_print_to_console)
                std::cout << "Directory <" << FOT << "> created successfully" << std::endl;
        }
        else
        {
            if(LogInstance().m_print_to_console)
                std::cout << "Directory <" << FOT << "> not created" << std::endl;
            return false;
        }
    }
#endif
#endif

#if (LOG_TRANSACTIONS || LOG_TRANSACTION_INVS || LOG_TX_RELAY || LOG_FALAFEL_TRANSACTIONS || FALAFEL_RECEIVER)
    // - create directory to record information about received tx & inv
    //   if it does not exit
    boost::filesystem::path TXInv(txdir);
    if(!(boost::filesystem::exists(TXInv)))
    {
        if(LogInstance().m_print_to_console)
            std::cout << "Directory <" << TXInv << "> doesn't exist; creating directory" << std::endl;
        if(boost::filesystem::create_directory(TXInv))
        {
            if(LogInstance().m_print_to_console)
                std::cout << "Directory <" << TXInv << "> created successfully" << std::endl;
        }
        else
        {
            if(LogInstance().m_print_to_console)
                std::cout << "Directory <" << TXInv << "> not created" << std::endl;
            return false;
        }
    }
#endif

    return true;
}

#if LOG_NEIGHBOR_ADDRESSES
/*
 * Callback for thread responsible for logging information about connected peers
 */
void AddrLoggerThread(CConnman* connman)
{
    // record address of connected peers every one second
    while(true)
    {
        // put this thread to sleep for a second
        boost::this_thread::sleep_for(boost::chrono::seconds{addrLoggerTimeoutSecs});
        std::vector<CNodeStats> vstats;
        // get stats for connected nodes
        connman->GetNodeStats(vstats);
        std::string fileName = addrLoggerdir + UNIX_SYSTEM_TIMESTAMP + ".txt";
        std::ofstream fnOut(fileName, std::ofstream::out);
        // log stat for each connected node
        for (auto stat : vstats)
            fnOut << stat.addr.ToStringIP() << std::endl;
        fnOut.close();
    }
}

/*
 * Initialize log file system that logs information about connected peers
 */
bool initAddrLogger()
{
    // path to directory where addresses of connected peers are logged
    addrLoggerdir = directory + "/addrs/";

    // create directory if it does not exist
    boost::filesystem::path addr(addrLoggerdir);
    if(!(boost::filesystem::exists(addr)))
    {
        if(LogInstance().m_print_to_console)
            std::cout << "Directory <" << addr << "> doesn't exist; creating directory" << std::endl;
        if(boost::filesystem::create_directory(addr))
        {
            if(LogInstance().m_print_to_console)
                std::cout << "Directory <" << addr << "> created successfully" << std::endl;
        }
        else
        {
            if(LogInstance().m_print_to_console)
                std::cout << "Directory <" << addr << "> not created" << std::endl;
            return false;
        }
    }
    return true;
}
#endif

/*
 * Log information about transactions in a block
 */
int logFile(CBlockHeaderAndShortTxIDs &Cblock, std::string from, std::string fileName)
{
    static int inc = 0; //file increment
    std::string timeString = createTimeStamp();
    if(fileName == "") fileName = directory + "logNode_" + nodeID + ".txt";
    else fileName = directory + fileName;
    std::string compactBlock = directory + std::to_string(inc) + "_cmpctblock.txt";
    std::vector<uint64_t> txid;
    std::ofstream fnOut;
    std::ofstream fnCmpct;

    fnOut.open(fileName, std::ofstream::app);
    fnCmpct.open(compactBlock, std::ofstream::out);

    fnOut << timeString << "CMPCTRECIVED - compact block received from " << from << std::endl;
    fnOut << timeString << "CMPCTBLKHASH - " << Cblock.header.GetHash().ToString() << std::endl;
    txid = Cblock.getTXID();

    fnCmpct << Cblock.header.GetHash().ToString() << std::endl;

    for(unsigned int i = 0; i < txid.size(); i++)
    {
        fnCmpct << txid[i] << std::endl;
    }

    fnOut << createTimeStamp() << "CMPCTSAVED - " << compactBlock << " file created" << std::endl;

    if(debug){
        std::cout << "inc: " << inc << std::endl;
        std::cout << "fileName: " << fileName << " --- cmpctblock file: " << compactBlock << std::endl;
        std::cout << timeString << "CMPCTRECIVED - compact block received from " << from << std::endl;
    }

    inc++;
    fnCmpct.close();
    fnOut.close();

    dumpMemPool();

    return inc - 1;
}

void logFile(BlockTransactionsRequest &req, int inc, std::string fileName)
{
    std::string timeString = createTimeStamp();
    if(fileName == "") fileName = directory + "logNode_" + nodeID + ".txt";
    else fileName = directory + fileName;
    std::string reqFile = directory + std::to_string(inc) + "_getblocktxn.txt";
    std::vector<uint64_t> txid;
    std::ofstream fnOut;
    std::ofstream fnReq;

    fnOut.open(fileName, std::ofstream::app);
    fnReq.open(reqFile, std::ofstream::out);

    fnOut << timeString << "FAILCMPCT - getblocktxn message sent for cmpctblock #" << inc << std::endl;
    fnOut << timeString << "REQSENT - cmpctblock #" << inc << " is missing " << req.indexes.size() << " tx"<< std::endl;

    fnReq << timeString << "indexes requested for missing tx from cmpctblock #" << inc << std::endl;
    for(unsigned int i = 0; i < req.indexes.size(); i++)
    {
        fnReq << req.indexes[i] << std::endl;
    }

    fnOut << timeString << "REQSAVED -  " << reqFile << " file created" << std::endl;

    if(debug){
        std::cout << "logFile for block req called" << std::endl;
        std::cout << "filename: " << fileName << " --- reqFile: " << reqFile << std::endl;
        std::cout << timeString << "REQSAVED -  " << reqFile << " file created" << std::endl;
    }


    fnOut.close();
    fnReq.close();
}

void logFile(std::string info, INVTYPE type, INVEVENT event, int counter, std::string fileName)
{
	if(info == "mempool")
	{
		dumpMemPool(fileName, type, event, counter);
		return;
	}
}

/*
 * Log some information to a file
 */
void logFile(std::string info, std::string fileName, std::string from)
{
    std::string timeString = createTimeStamp();
    if(fileName == "") fileName = directory + "logNode_" + nodeID + ".txt";
    else fileName = directory + fileName;
    std::ofstream fnOut;
    fnOut.open(fileName,std::ofstream::app);

    fnOut << timeString << info; //Thu Aug 10 11:31:32 2017\n is printed

    if (!from.empty())
        fnOut << " " << from;
    fnOut << std::endl;

    if(debug){
        if(!fnOut.is_open()) std::cout << "fnOut failed" << std::endl;
        std::cout << "logfile for string " << std::endl;
        std::cout << "fileName:" << fileName << std::endl;
        std::cout << timeString << info << std::endl;
    }

    fnOut.close();
}

/*
 * Log contents of a block inv message
 */
int logFile(std::vector<CInv> vInv, INVTYPE type, std::string from, std::string fileName)
{
    static int count = 0;
    std::string timeString = createTimeStamp();
    if(fileName == "") fileName = directory + "logNode_" + nodeID + ".txt";
    else fileName = directory + fileName;
    std::ofstream fnOut;
    fnOut.open(fileName,std::ofstream::app);

    // log contents of an inv message that is being sent to peers
    if(type == FALAFEL_SENT)
    {
#if FALAFEL_SENDER
        std::string vecFile = directory + std::to_string(count) + "_vecFile_invsent.txt";
        std::ofstream fnVec;
        fnVec.open(vecFile, std::ofstream::out);

        fnOut << timeString << "VECGEN --- generated std::vector of tx to sync" << std::endl;
        fnVec << timeString << std::to_string(vInv.size()) << std::endl;

        for(unsigned int  ii = 0; ii < vInv.size(); ii++)
        {
            fnVec << vInv[ii].ToString() << std::endl; //protocol.* file contains CInv class
        }

        fnOut << timeString << "VECSAVED --- saved file of tx std::vector: " << vecFile << std::endl;

        fnVec.close();
#endif
    }
    // log contents of an inv message that is received from a peer
    else if(type == FALAFEL_RECEIVED)
    {
#if FALAFEL_RECEIVER
        std::string vecFile = invRXdir + std::to_string(count) + "_vecFile_invreceived.txt";
        std::ofstream fnVec;
        fnVec.open(vecFile, std::ofstream::out);

        fnOut << timeString << "INVRX --- received inv from " << from << std::endl;
        fnVec << timeString << std::to_string(vInv.size()) << std::endl;

        for(unsigned int  ii = 0; ii < vInv.size(); ii++)
        {
            fnVec << vInv[ii].ToString() << std::endl; //protocol.* file contains CInv class
        }

        fnOut << timeString << "INVSAVED --- received inv saved to: " << vecFile << std::endl;

        fnVec.close();
#endif
    }

    fnOut.close();

    return count++;
}

#if (LOG_TRANSACTION_INVS || FALAFEL_RECEIVER)
/*
 * Log hash of a transaction inv to file
 */
void logFile(CInv inv, std::string from, std::string fileName)
{
    if(fileName == "") fileName = txdir + "txinvs_" + nodeID + ".txt";
    else fileName = txdir + fileName;
    std::ofstream fnOut(fileName, std::ofstream::app);
    fnOut << createTimeStamp() << inv.hash.ToString() << " from " << from << std::endl;
    fnOut.close();
}
#endif

#if (LOG_TRANSACTIONS || LOG_FALAFEL_TRANSACTIONS)
/*
 * Log arrival of a valid transaction to file as well as validation time
 */
void logFile(CTransaction tx, std::string from, std::string duration, std::string fileName)
{
    if(fileName == "") fileName = txdir + "txs_" + nodeID + ".txt";
    else fileName = txdir + fileName;
    std::ofstream fnOut(fileName, std::ofstream::app);
    fnOut << createTimeStamp() << tx.GetHash().ToString() << " from " << from << " validated in " << duration << std::endl;
    fnOut.close();
}
#endif

#if LOG_TX_RELAY
void logFile(CInv inv, std::string to, int dummy)
{
    std::string timeString = createTimeStamp();
    std::string fileName = txdir + "txs_relay_queue_" + nodeID + ".txt";
    std::ofstream fnOut(fileName, std::ofstream::app);

    try
    {
        assert(inv.type == MSG_TX);
        fnOut << timeString << inv.hash.ToString() << " to " << to << std::endl;
    }
    catch (...)
    {
        fnOut << timeString << "NOTTXRELAY --- " << inv.hash.ToString() << "to" << to << std::endl;
    }

    fnOut.close();
}

void logFile(std::vector<CInv> vInv, std::string to)
{
    std::string fileName = txdir + "txs_relay_sent_" + nodeID + ".txt";
    std::ofstream fnOut(fileName, std::ofstream::app);
    std::string timeString = createTimeStamp();

    for(unsigned int  ii = 0; ii < vInv.size(); ii++)
    {
        if(vInv[ii].type == MSG_TX)
            fnOut << timeString << vInv[ii].ToString() << " to " << to << std::endl;
    }

    fnOut.close();
}
#endif

/*
 * Dump current state of the mempool to a file
 */
void dumpMemPool(std::string fileName, INVTYPE type, INVEVENT event, int counter)
{
    std::string timeString = createTimeStamp();
    if(fileName == "") fileName = directory + "logNode_" + nodeID + ".txt";
    else fileName = directory + fileName;
    std::ofstream fnOut;
    std::string sysCmd;
    std::string mempoolFile;
    fnOut.open(fileName,std::ofstream::app);
    // dump mempool before or after an inv message is received
    // (to use with finding how the inv message affects the mempool)
#if FALAFEL_RECEIVER
    if(type == FALAFEL_RECEIVED)
    {
        mempoolFile = invRXdir + std::to_string(counter) + ((event == BEFORE)? "_before" : "_after") + "_mempoolFile.txt";
        fnOut << timeString << "INV" << ((event == BEFORE) ? "B" : "A") << "DMPMEMPOOL --- Dumping mempool to file: " << mempoolFile << std::endl;
    }
    else
#endif
    {
	    static int count = 0;
	    mempoolFile = directory + std::to_string(count) + "_mempoolFile.txt";

	    fnOut << timeString << "DMPMEMPOOL --- Dumping mempool to file: " << mempoolFile << std::endl;
	    count++;
    }

    std::vector<uint256> vtxid;
    mempool.queryHashes(vtxid);

    std::ofstream fnMP(mempoolFile, std::ofstream::out);

    for (auto txid : vtxid)
        fnMP << txid.ToString() << std::endl;

    fnOut.close();
    fnMP.close();
}

/*
 * Log transactions that are delayed because of send buffer fullness
 */
void logFile(std::deque<CInv>::iterator begin, std::deque<CInv>::iterator end, std::string from, std::string fileName)
{
    static int count = 0;
    std::string timeString = createTimeStamp();
    if(fileName == "") fileName = directory + "logNode_" + nodeID + ".txt";
    else fileName = directory + fileName;
    std::string getdataFile = directory + std::to_string(count) + "_getdata_buffull.txt";
    std::ofstream fnOut;
    std::ofstream fnGD;

    fnOut.open(fileName, std::ofstream::app);
    fnGD.open(getdataFile, std::ofstream::out);

    fnOut << timeString << "SENDBUFFERFULL --- remaining transaction hashes to be sent to " << from << " saved to " << getdataFile << std::endl;
    while(begin != end)
    {
        fnGD << (*begin).hash.ToString() << std::endl;
    }

    fnOut.close();
    fnGD.close();

    count++;
}

#ifdef LOG_CPU_USAGE
unsigned long getProcessCPUStats()
{
    std::ifstream fnIn("/proc/" + std::to_string(getpid()) + "/stat");
    std::string line;
    getline(fnIn, line);

    std::istringstream ss(line);
    // refer to 'man proc', section '/proc/[pid]/stat'
    unsigned long utime = 0, stime = 0;
    long cutime = 0, cstime = 0;
    for(int i = 1; i <= 17; i++)
    {
        std::string word;
        ss >> word;
        switch(i)
        {
            case 14: utime     = std::stoul(word); break;
            case 15: stime     = std::stoul(word); break;
            case 16: cutime    = std::stol(word); break;
            case 17: cstime    = std::stol(word); break;
            default: break;
        }
    }

    return utime + stime + cutime + cstime;
}

unsigned long getTotalTime()
{
    std::ifstream fnIn("/proc/stat");
    std::string line;
    getline(fnIn, line);

    std::stringstream ss(line);
    std::string word;
    ss >> word;
    unsigned long total = 0;
    while(ss >> word)
        total += std::stol(word);

    return total;
}

void CPUUsageLoggerThread()
{
    std::ofstream fnOut(directory + "/cpuusage_" + nodeID + ".txt", std::ofstream::app);
    while(!ShutdownRequested())
    {
        long first = getProcessCPUStats();
        long firstTotalTime = getTotalTime();
        // put this thread to sleep for a second
        boost::this_thread::sleep_for(boost::chrono::milliseconds{10});
        fnOut << createTimeStamp() << (100.0 * (getProcessCPUStats() - first)/(getTotalTime() - firstTotalTime)) << std::endl;
    }
}

// empty: for future uses
bool initProcessCPUUsageLogger()
{
    return true;
}
#endif

#if FALAFEL_SENDER
bool isNodeInWhiteListedSubnets(std::string node)
{
    struct compare
    {
        std::string key;
        compare(std::string const k) : key(k) {}
        bool operator()(std::string subnet)
        {
            return key.find(subnet) != std::string::npos;
        }
    };
    return std::find_if(WhiteListedSubnets.begin(), WhiteListedSubnets.end(), compare(node)) != WhiteListedSubnets.end();
}
#endif
