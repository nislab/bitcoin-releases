//Custom log file system written to monitor the behaviour of
//compact blocks, getblocktxn, and blocktxn messages on a bitcoin node
//Soft forks created by two blocks coming in are also recorded

#include "logFile.h"
#include <chrono>
#include <unistd.h>
#include "net.h"
#include <string>
#include <inttypes.h>
#include <stdio.h>
#include <init.h>


#define UNIX_TIMESTAMP \
    std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count())
#define UNIX_SYSTEM_TIMESTAMP \
    std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count())

bool debug = false;
Env env;
const static std::string nodeID = env.getUserName();
static std::string strDataDir;
static std::string directory;
static std::string invRXdir;
static std::string txdir;
static std::string cmpctblkdir;
static std::string cmpctReqTxdir;
static std::string cmpctblkAddTxDir;
static std::string grphnReqTxdir;
static std::string grphnblkTxDir;
static std::string blockTxDir;
static std::string mempoolFileDir;
static std::string addrLoggerdir;
static std::string cpudir;
static int64_t addrLoggerTimeoutSecs = 1;
// static int inc = 0;
extern CTxMemPool mempool;

void dumpMemPool(std::string fileName = "", std::string from = "", INVTYPE type = FALAFEL_SENT, INVEVENT event = BEFORE, int counter = 0);

bool createDir(std::string dirName)
{
    boost::filesystem::path dir(dirName);
    if(!(boost::filesystem::exists(dir)))
    {
        if(fPrintToConsole)
            std::cout << "Directory <" << dir << "> doesn't exist; creating directory" << std::endl;
        if(boost::filesystem::create_directory(dir))
        {
            if(fPrintToConsole)
                std::cout << "Directory <" << dir << "> created successfully" << std::endl;
        }
        else
        {
            if(fPrintToConsole)
                std::cout << "Directory <" << dir << "> not created" << std::endl;
            return false;
        }
    }
    return true;
}

/*
 * Initialize log file system that records events in Bitcoin
 */
bool initLogger()
{
    // paths to different directories
    strDataDir       = GetDataDir().string();
    directory        = strDataDir + "/expLogFiles/";
    invRXdir         = directory  + "/received/";
    txdir            = directory  + "/txs/";
    cmpctblkdir      = directory  + "/cmpctblk/";
    cmpctReqTxdir    = directory  + "/getblocktxn/";
    grphnReqTxdir    = directory  + "/grapheneblockreqtxs/";
    mempoolFileDir   = directory  + "/mempool/";
    blockTxDir       = directory  + "/blocktxs/";
    grphnblkTxDir    = directory  + "/grphnblkTxDir/";
    cmpctblkAddTxDir = directory  + "/cmpctblkaddtxn/";

    // create directories if they do not exist

    // - create main directory if it does not exist
    if(!createDir(directory))
        return false;

    // - create directory to record information about received inv messages
    //   if it does not exist
    if(!createDir(invRXdir))
        return false;

    // - create directory to record information about received tx & inv
    //   if it does not exist
    if(!createDir(txdir))
        return false;

    // - create directory to record information about received compact blocks
    //   if it does not exist
    if(!createDir(cmpctblkdir))
        return false;

    // - create directory to record information about reqiested missing txs (compact blocks)
    //   if it does not exist
    if(!createDir(cmpctReqTxdir))
        return false;

    // - create directory to record information about addition transactions in a compact block
    // if it does not exist
    if(!createDir(cmpctblkAddTxDir))
        return false;

    // - create directory to record information about reqiested missing txs (graphene blocks)
    //   if it does not exist
    if(!createDir(grphnReqTxdir))
        return false;

    // - create directory to record information about mempool when block is received
    //   if it does not exist
    if(!createDir(mempoolFileDir))
        return false;

    // - create directory to record transactions in a normal block
    //   if it does not exist
    if(!createDir(blockTxDir))
        return false;

    if(!createDir(grphnblkTxDir))
        return false;

    return true;
}

/*
 * Callback for thread responsible for logging information about connected peers
 */
void AddrLoggerThread()
{
    // record address of connected peers every addrLoggerTimeoutSecs second
    while(!ShutdownRequested())
    {
        // put this thread to sleep for a second
        boost::this_thread::sleep_for(boost::chrono::seconds{addrLoggerTimeoutSecs});

        std::string fileName = addrLoggerdir + UNIX_SYSTEM_TIMESTAMP + ".txt";
        std::ofstream fnOut(fileName, std::ofstream::out);

        // log stat for each connected node (CHECK THAT THIS IS BEHAVING AS INTENDED)
        for(CNode * pNode : vNodes)
        {
            fnOut << pNode->addr.ToStringIP() << ", " << pNode->GrapheneCapable() << std::endl;
            // if(!pNode->GrapheneCapable())
            //     pNode->fDisconnect = true;
        }

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
    if(!createDir(addrLoggerdir))
        return false;

    return true;
}

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
 * Log information about transactions in a compact block
 */
void logFile(CompactBlock & Cblock, std::string from, std::string fileName)
{
    std::string timeString = createTimeStamp();
    if(fileName == "") fileName = directory + "logNode_" + nodeID + ".txt";
    else fileName = directory + fileName;
    std::string blockHash = Cblock.header.GetHash().ToString();
    std::string compactBlock = cmpctblkdir + blockHash;

    if(!createDir(compactBlock))
    {
        logFile("ERROR -- couldn't create directory <" + compactBlock + ">...");
        StartShutdown();
        return;
    }

    compactBlock += '/' + from;

    if(boost::filesystem::exists(compactBlock))
    {
        logFile("WARN -- In <" + std::string(__func__) + ">: " + std::to_string(__LINE__) + ": <" + compactBlock + "> already exists.. possible duplicate.. skipping");
        // StartShutdown();
        return;
    }

    std::vector<uint64_t> txid;
    std::ofstream fnOut;
    std::ofstream fnCmpct;

    fnOut.open(fileName, std::ofstream::app);
    fnCmpct.open(compactBlock, std::ofstream::out);

    fnOut << timeString << "CMPCTRECIVED - compact block received; hash: " << blockHash
          << "; from " << from << "; prefilledtxns: " << Cblock.prefilledtxn.size()
          << "; shorttxids: " << Cblock.shorttxids.size()
          << "; block size: " << Cblock.GetSize() << " (bytes)" << std::endl;

    txid = Cblock.getTXID();

    fnCmpct << blockHash << std::endl << Cblock.shorttxidk0 << std::endl << Cblock.shorttxidk1 << std::endl;

    for(unsigned int i = 0; i < txid.size(); i++)
    {
        fnCmpct << txid[i] << std::endl;
    }

    fnOut << createTimeStamp() << "CMPCTSAVED - " << compactBlock << " file created" << std::endl;

    if(debug){
        // std::cout << "inc: " << inc << std::endl;
        std::cout << "fileName: " << fileName << " --- cmpctblock file: " << compactBlock << std::endl;
        std::cout << timeString << "CMPCTRECIVED - compact block received from " << from << std::endl;
    }

    fnCmpct.close();
    fnOut.close();

    dumpMemPool(blockHash, from);
    // inc++;

    // return inc - 1;
}

void logFile(std::vector<uint32_t> req, std::string blockHash, std::string from, uint64_t reqSize, std::string fileName)
{
    std::string timeString = createTimeStamp();
    if(fileName == "") fileName = directory + "logNode_" + nodeID + ".txt";
    else fileName = directory + fileName;
    std::string reqFile = cmpctReqTxdir + blockHash;

    if(!createDir(reqFile))
    {
        logFile("ERROR -- couldn't create directory <" + reqFile + ">...");
        StartShutdown();
        return;
    }

    reqFile += '/' + from;

    if(boost::filesystem::exists(reqFile))
    {
        logFile("WARN -- In <" + std::string(__func__) + ">: " + std::to_string(__LINE__) + ": <" + reqFile + "> already exists.. possible duplicate.. skipping");
        // StartShutdown();
        return;
    }
    std::ofstream fnOut;
    std::ofstream fnReq;

    fnOut.open(fileName, std::ofstream::app);
    fnReq.open(reqFile, std::ofstream::out);

    fnOut << timeString << "FAILCMPCT - getblocktxn message of size " << reqSize << " (bytes) sent for cmpctblock: " << blockHash << std::endl;
    fnOut << timeString << "REQSENT - cmpctblock " << blockHash << " is missing " << req.size() << " tx" << std::endl;

    fnReq << timeString << "indexes requested for missing tx from cmpctblock: " << blockHash << std::endl;
    for(unsigned int i = 0; i < req.size(); i++)
    {
        fnReq << req[i] << std::endl;
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

void logFile(std::vector<PrefilledTransaction> prefilledtxns, std::string blockHash, std::string from, std::string fileName)
{
    std::string timeString = createTimeStamp();
    if(fileName == "") fileName = directory + "logNode_" + nodeID + ".txt";
    else fileName = directory + fileName;
    std::string txDir = cmpctblkAddTxDir + blockHash;

    if(!createDir(txDir))
    {
        logFile("ERROR -- couldn't create directory <" + txDir + ">...");
        StartShutdown();
        return;
    }

    txDir += "/" + from;

    if(boost::filesystem::exists(txDir))
    {
        logFile("WARN -- In <" + std::string(__func__) + ">: " + std::to_string(__LINE__) + ": <" + txDir + "> already exists.. possible duplicate.. skipping");
        // StartShutdown();
        return;
    }
    std::ofstream fnOut;
    std::ofstream fnTx;

    fnOut.open(fileName, std::ofstream::app);
    fnTx.open(txDir, std::ofstream::out);

    fnOut << timeString << "CMPCTBLCKADDTX -- " << prefilledtxns.size()  << " additional txs in block " << blockHash << " from " << from << std::endl;

    for(PrefilledTransaction prefilledtxn : prefilledtxns)
        fnTx << prefilledtxn.tx.GetHash().ToString() << std::endl;

    fnTx.close();
    fnOut.close();
}

void logFile(std::vector<CTransaction> vtx, std::string blockHash, std::string from, std::string fileName)
{
    std::string timeString = createTimeStamp();
    if(fileName == "") fileName = directory + "logNode_" + nodeID + ".txt";
    else fileName = directory + fileName;
    std::string txDir = grphnblkTxDir + blockHash;

    if(!createDir(txDir))
    {
        logFile("ERROR -- couldn't create directory <" + txDir + ">...");
        StartShutdown();
        return;
    }

    txDir += "/" + from;

    if(boost::filesystem::exists(txDir))
    {
        logFile("WARN -- In <" + std::string(__func__) + ">: " + std::to_string(__LINE__) + ": <" + txDir + "> already exists.. possible duplicate.. skipping");
        // StartShutdown();
        return;
    }
    std::ofstream fnOut;
    std::ofstream fnTx;

    fnOut.open(fileName, std::ofstream::app);
    fnTx.open(txDir, std::ofstream::out);

    fnOut << timeString << "GRPHNBLCKRECMTXS -- " << vtx.size() << " txs of block: " << blockHash << " saved to file <" << txDir << ">" << std::endl;
    for(CTransaction tx : vtx)
        fnTx << tx.GetHash().ToString() << std::endl;

    fnOut.close();
    fnTx.close();
}

/*
 * Log transactions in a normal block to file
 */
void logFile(std::vector<CTransactionRef> vtx, std::string blockHash, std::string from, BlockType bType, std::string fileName)
{
    std::string timeString = createTimeStamp();
    if(fileName == "") fileName = directory + "logNode_" + nodeID + ".txt";
    else fileName = directory + fileName;

    std::string op = bType == BlockType::COMPACT ? "CMPCT" : bType == BlockType::GRAPHENE ? "GRPHN" : "NORMAL";
    std::string txDir = blockTxDir + "/" + op;

    if(!createDir(txDir))
    {
        logFile("ERROR -- couldn't create directory <" + txDir + ">...");
        StartShutdown();
        return;
    }

    std::string txFile = txDir + "/" + blockHash;

    if(!createDir(txFile))
    {
        logFile("ERROR -- couldn't create directory <" + txFile + ">...");
        StartShutdown();
        return;
    }

    txFile += '/' + from;

    if(boost::filesystem::exists(txFile))
    {
        logFile("WARN -- In <" + std::string(__func__) + ">: " + std::to_string(__LINE__) + ": <" + txFile + "> already exists.. possible duplicate.. skipping");
        // StartShutdown();
        return;
    }

    std::ofstream fnOut;
    std::ofstream fnTx;

    fnOut.open(fileName, std::ofstream::app);
    fnTx.open(txFile, std::ofstream::out);

    fnOut << timeString << op << "BLCKTXS -- " << vtx.size() << " txs of block: " << blockHash << " saved to file <" << txFile << ">" << std::endl;
    for(CTransactionRef tx : vtx)
        fnTx << tx->GetHash().ToString() << std::endl;

    fnOut.close();
    fnTx.close();
}

// TODO: remove
/*
 * Log the header, ip of peer and dump the mempool for a graphene block
 */
#if 0
int logFile(std::string header, std::string from, std::string fileName)
{
    std::string timeString = createTimeStamp();
    if(fileName == "") fileName = directory + "logNode_" + nodeID + ".txt";
    else fileName = directory + fileName;
    std::ofstream fnOut;

    fnOut.open(fileName, std::ofstream::app);

    fnOut << timeString << "GRRECEIVED - Graphene block received from peer:  " << from << std::endl;
    fnOut << timeString << "GRHEADER - " << header << std::endl;

    dumpMemPool();

    fnOut.close();

    return inc - 1;
}
#endif

/*
 * Log missing transactions in a graphene block
 */
void logFile(std::set<uint64_t> missingTxs, CNode* pfrom, std::string blockHash, std::string fileName)
{
    std::string timeString = createTimeStamp();
    if(fileName == "") fileName = directory + "logNode_" + nodeID + ".txt";
    else fileName = directory + fileName;
    std::string grapheneBlock = grphnReqTxdir + blockHash;

    if(!createDir(grapheneBlock))
    {
        logFile("ERROR -- couldn't create directory <" + grapheneBlock + ">...");
        StartShutdown();
        return;
    }

    grapheneBlock += '/' + pfrom->GetLogName();

    if(boost::filesystem::exists(grapheneBlock))
    {
        logFile("WARN -- In <" + std::string(__func__) + ">: " + std::to_string(__LINE__) + ": <" + grapheneBlock + "> already exists.. possible duplicate.. skipping");
        // StartShutdown();
        return;
    }

    std::ofstream fnGraphene;
    std::ofstream fnOut;

    fnOut.open(fileName, std::ofstream::app);
    fnGraphene.open(grapheneBlock, std::ofstream::out);

    if(missingTxs.size() > 0)
        fnOut << timeString << "GRMISSINGTX - Missing " << missingTxs.size() << " transactions from graphene block: " << blockHash << std::endl;
    else
        fnOut << timeString << "GRNOMISSINGTXS - All txs needed to reconstruct this graphene block were found in our mempool: " << blockHash << std::endl;

    // fnGraphene << header << std::endl;
    fnGraphene << std::to_string(pfrom->gr_shorttxidk0) << "\n" << std::to_string(pfrom->gr_shorttxidk1) << "\n" << NegotiateGrapheneVersion(pfrom) << std::endl;

    for(auto itr : missingTxs)
    {
        fnGraphene << itr << std::endl;
    }

    fnGraphene.close();
}

void logFile(std::string info, INVTYPE type, INVEVENT event, int counter, std::string fileName)
{
	if(info == "mempool")
	{
		dumpMemPool(fileName, "", type, event, counter);
		return;
	}
}

/*
 * Log some information to a file
 */
void logFile(std::string info, std::string fileName)
{
    if(info == "mempool")
    {
        dumpMemPool(fileName);
        return;
    }
    std::string timeString = createTimeStamp();
    if(fileName == "") fileName = directory + "logNode_" + nodeID + ".txt";
    else fileName = directory + fileName;
    std::ofstream fnOut;
    fnOut.open(fileName,std::ofstream::app);

    fnOut << timeString << info << std::endl; //Thu Aug 10 11:31:32 2017\n is printed

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
int logFile(std::vector<CInv> vInv, INVTYPE type, std::string fileName)
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
    }
    // log contents of an inv message that is received from a peer
    else if(type == FALAFEL_RECEIVED)
    {
        std::string vecFile = invRXdir + std::to_string(count) + "_vecFile_invreceived.txt";
        std::ofstream fnVec;
        fnVec.open(vecFile, std::ofstream::out);

        fnOut << timeString << "INVRX --- received inv" << std::endl;
        fnVec << timeString << std::to_string(vInv.size()) << std::endl;

        for(unsigned int  ii = 0; ii < vInv.size(); ii++)
        {
            fnVec << vInv[ii].ToString() << std::endl; //protocol.* file contains CInv class
        }

        fnOut << timeString << "INVSAVED --- received inv saved to: " << vecFile << std::endl;

        fnVec.close();
    }

    fnOut.close();

    return count++;
}

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

/*
 * Log arrival of a valid transaction to file
 */
void logFile(CTransaction tx, std::string from, std::string fileName)
{
    if(fileName == "") fileName = txdir + "txs_" + nodeID + ".txt";
    else fileName = txdir + fileName;
    std::ofstream fnOut(fileName, std::ofstream::app);
    fnOut << createTimeStamp() << tx.GetHash().ToString() << " from " << from << std::endl;
    fnOut.close();
}

/*
 * Dump current state of the mempool to a file
 */
void dumpMemPool(std::string fileName, std::string from, INVTYPE type, INVEVENT event, int counter)
{
    std::string timeString = createTimeStamp();
    std::ofstream fnOut;
    std::string sysCmd;
    std::string mempoolFile;
    fnOut.open(directory + "logNode_" + nodeID + ".txt", std::ofstream::app);
    // dump mempool before or after an inv message is received
    // (to use with finding how the inv message affects the mempool)
    if(type == FALAFEL_RECEIVED)
    {
        mempoolFile = invRXdir + std::to_string(counter) + ((event == BEFORE)? "_before" : "_after") + "_mempoolFile.txt";
        fnOut << timeString << "INV" << ((event == BEFORE) ? "B" : "A") << "DMPMEMPOOL --- Dumping mempool to file: " << mempoolFile << std::endl;
    }
    else
    {
	    mempoolFile = mempoolFileDir + fileName;

        if(!createDir(mempoolFile))
        {
            logFile("ERROR -- couldn't create directory <" + mempoolFile + ">...");
            StartShutdown();
            return;
        }

        mempoolFile += '/' + from;

        if(boost::filesystem::exists(mempoolFile))
        {
            logFile("WARN -- In <" + std::string(__func__) + ">: " + std::to_string(__LINE__) + ": <" + mempoolFile + "> already exists.. possible duplicate.. skipping");
            // StartShutdown();
            return;
        }

	    fnOut << timeString << "DMPMEMPOOL --- Dumping mempool to file: " << mempoolFile << std::endl;
    }

    std::vector<uint256> vtxid;
    mempool.queryHashes(vtxid);

    std::ofstream fnMP(mempoolFile, std::ofstream::out);

    for (auto txid : vtxid)
        fnMP << txid.ToString() << std::endl;

    fnOut.close();
    fnMP.close();
}

long getProcessCPUStats()
{
    std::ifstream fnIn("/proc/" + std::to_string(getpid()) + "/stat");
    std::string line;
    getline(fnIn, line);

    std::istringstream ss(line);
    // refer to 'man proc', section '/proc/[pid]/stat'
    long utime = 0, stime = 0, cutime = 0, cstime = 0;
    for(int i = 1; i <= 17; i++)
    {
        std::string word;
        ss >> word;
        switch(i)
        {
            case 14: utime     = std::stoi(word); break;
            case 15: stime     = std::stoi(word); break;
            case 16: cutime    = std::stoi(word); break;
            case 17: cstime    = std::stoi(word); break;
            default: break;
        }
    }

    return utime + stime + cutime + cstime;
}

long getTotalTime()
{
    std::ifstream fnIn("/proc/stat");
    std::string line;
    getline(fnIn, line);

    std::stringstream ss(line);
    std::string word;
    ss >> word;
    long total = 0;
    while(ss >> word)
        total += std::stol(word);

    return total;
}

void CPUUsageLoggerThread()
{
    std::ofstream fnOut(directory + "/cpuusage_" + nodeID + ".txt", std::ofstream::app);
    while(true)
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
