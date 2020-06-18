// Initially written by Nabeel Younis in the NISLab at Boston University on 17/11/2017
// Brought to completion by Muhammad Anas Imtiaz.
// PIs: Professors David Starobinski and Ari Trachtenberg of BU ENG ECE

#include "txMempoolSync.h"
#include <vector>
#include <algorithm>
#include <shutdown.h>

#if FALAFEL_SENDER
// variable to hold prioritized indexed set of transactions in mempool
indexed_transaction_set syncTxIndex;
// threshold of minimum number of transactions to sync
const int mintxsync = 1000;
#if ENABLE_SPECIAL_SYNC
    // number of inv messages within which a larger inv messages will be sent
    const int numInvMsgs = 5;
#endif
// fraction of mempool to sync
const int fracMempoolTxs = 0.1;
// number of seconds for timer timeout
const int TIMER_TIMEOUT_SECONDS = 30;
#ifdef ENABLE_FALAFEL_PING
    // number of seconds for ping timer
    const int TIMER_PING_SECONDS = 15;
#endif
#if SYNC_ENTIRE_MEMPOOL_AT_CONNECT
std::map<std::string, bool> ConnectedNodes;
#endif
const int cleanUpCountDownLimit = 3;

static std::map<std::string, std::vector<uint256>> sentInvs;

uint256 genMagicHash()
{
    const char MAGIC_HASH_BYTES[4] = {'\xe1', '\xaf', '\xa1', '\x0f'};
    std::vector<unsigned char> MAGIC_HASH_VEC;
    for(int i = 0; i < 32; i++)
        MAGIC_HASH_VEC.push_back(MAGIC_HASH_BYTES[i % 4]);
    uint256 MAGIC_HASH = uint256(MAGIC_HASH_VEC);
    return MAGIC_HASH;
}

std::vector<CInv> generateVInv(std::string nodeName)
{
    std::vector<CInv> vInv;
    MPiter it = mempool.mapTx.get<ancestor_score>().begin();
    int txInMemPool = mempool.mapTx.get<ancestor_score>().size();

    int txToSync;

    CInv magichash(MSG_TX, genMagicHash());
    vInv.push_back(magichash);

    std::vector<uint256> temp;

#if SYNC_ENTIRE_MEMPOOL_AT_CONNECT
    if(ConnectedNodes[nodeName])
    {
        // if the node has just connected, send to it all transactions from mempool
        txToSync = txInMemPool;
        ConnectedNodes[nodeName] = false;
        logFile("JUSTCONNECTED --- node just connected; sending all transactions from mempool");
    }
    else
    {
#endif
#if ENABLE_SPECIAL_SYNC
        if(1 + std::rand() % numInvMsgs == numInvMsgs)
        {
            logFile("SPCLSYNC --- Sending special message with more transactions");
            txToSync = txInMemPool * (fracMempoolTxs + 0.5);
        }
        else
#endif
            // decide how many transactions to sync
            // case 1: sync 10 percent of transactions in mempool
            txToSync = txInMemPool * fracMempoolTxs;
        // case 2: if number of transactions in mempool is too low, sync all
        //         transactions in mempool
        if(txInMemPool < mintxsync) txToSync = txInMemPool;
            // case 3: if 10 percent of transactions in mempool lower than threshold,
            // sync threshold number of transactions
        else if(mintxsync > txToSync) txToSync = mintxsync;
#if SYNC_ENTIRE_MEMPOOL_AT_CONNECT
    }
#endif

    for(int ii = 0; ii < txToSync && it != mempool.mapTx.get<ancestor_score>().end();)
    {
        const uint256 txHash = it->GetTx().GetHash();
        if (std::find(sentInvs[nodeName].begin(), sentInvs[nodeName].end(), txHash) == sentInvs[nodeName].end())
        {
            // create an inventory of hash of current transaction
            CInv inv(MSG_TX, txHash);
            // add transaction to inventory vector
            vInv.push_back(inv);
            temp.push_back(txHash);
            ++ii;
        }
        ++it;
    }

    // log to file information regarding count of transactions to sync
    logFile("TXCOUNT --- tx in mempool: " + std::to_string(txInMemPool) +
            " --- tx sync count: " + std::to_string(txToSync) + " --- txs synced: " + std::to_string(temp.size()));

    if (temp.size() > 0)
        sentInvs[nodeName].insert(sentInvs[nodeName].end(), temp.begin(), temp.end());

    // write current state of mempool to a file
    // logFile("mempool");
    // write trasactions to sync to a file
    logFile(vInv, FALAFEL_SENT);

    // return vector containing transactions to sync
    return vInv;
}

// initialize timer for triggering MempoolSync
bool initMempoolSyncTriggerTimer()
{
    logFile("TXMSTIMER --- Timer initialized");
    return true;
}

// remove tracking of transactions that are no longer in the mempool
// (for performance purposes)
void cleanUp()
{
    for (auto it = sentInvs.begin(); it != sentInvs.end(); it++)
        for (auto txHash : it->second)
            if (!mempool.exists(txHash))
                it->second.erase(std::find(it->second.begin(), it->second.end(), txHash));
}

// callback for thread responsible for running the timer that triggers MempoolSync
void mempoolSyncTriggerTimerThread(CConnman* connman)
{
    if (WhiteListedSubnets.size() > 0)
    {
        unsigned int cleanUpCountDown = 0;
        while (!ShutdownRequested())
        {
            // periodically remove transactions that are no longer in the mempool
            // from being tracked
            if (cleanUpCountDown % cleanUpCountDownLimit == 0)
            {
                cleanUp();
                cleanUpCountDown = 0;
            }
            ++cleanUpCountDown;

#if ENABLE_FALAFEL_PING
            boost::this_thread::sleep_for(boost::chrono::seconds{TIMER_TIMEOUT_SECONDS - TIMER_PING_SECONDS});
#else
            // sleep for some time before triggering MempoolSync
            boost::this_thread::sleep_for(boost::chrono::seconds{TIMER_TIMEOUT_SECONDS});
#endif

#if ENABLE_FALAFEL_PING
            connman->ForEachNode([&connman](CNode *pnode) {
                if (isNodeInWhiteListedSubnets(pnode->GetAddrName()))
                {
                    // ping node to check if it is connected
                    pnode->fPingQueued = true;
                    logFile("PINGSENT --- ping requested for node " + pnode->GetAddrName());
                }
            });

            boost::this_thread::sleep_for(boost::chrono::seconds{TIMER_PING_SECONDS});
#endif

            logFile("TRIGGER --- starting sync");

            // send list of transactions to peers
            connman->ForEachNode([&connman](CNode *pnode) {
                if (isNodeInWhiteListedSubnets(pnode->GetAddrName()))
                {
                    std::cout << ">> Node found <" << pnode->GetAddrName() << ">" << std::endl;
                    if (pnode->nVersion < INVALID_CB_NO_BAN_VERSION || pnode->fDisconnect) return;
#if ENABLE_FALAFEL_PING
                    if (pnode->nPingNonceSent != 0)
                    {
                        logFile("PINGTMOUT --- node unreachable: " + pnode->GetAddrName() + ", nonce: " + std::to_string(pnode->nPingNonceSent));
#if SYNC_ENTIRE_MEMPOOL_AT_CONNECT
                        if(ConnectedNodes.find(pnode->GetAddrName()) != ConnectedNodes.end())
                            ConnectedNodes.erase(pnode->GetAddrName());
#endif
                    }
                    else
#endif
                    {
#if ENABLE_FALAFEL_PING
                        logFile("PINGSUCCSS --- node reachable: " + pnode->GetAddrName() + ", nonce: " + std::to_string(pnode->nPingNonceSent));
#if SYNC_ENTIRE_MEMPOOL_AT_CONNECT
                        if(ConnectedNodes.find(pnode->GetAddrName()) == ConnectedNodes.end())
                            ConnectedNodes.insert(std::make_pair(pnode->GetAddrName(), true));
#endif
#endif
                        // get a vector of transaction messages to synchronize; no repetitions
                        std::vector <CInv> vInv = generateVInv(pnode->GetAddrName());
                        std::cout << ">> Sending message of size " << vInv.size() << std::endl;
                        if (vInv.size() > 0)
                        {
                            pnode->timeLastMempoolReq = GetTime();
                            connman->PushMessage(pnode,
                                                 CNetMsgMaker(pnode->GetSendVersion()).Make(NetMsgType::INV, vInv));
                            logFile("SYNCSENT --- sync message sent to: " + pnode->GetAddrName());
                        }
                    }
                }
            });
            logFile("ENDTRIG --- sync ended");
        }
    }
}
#endif