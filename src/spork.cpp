// Copyright (c) 2014-2016 The Dash developers
// Copyright (c) 2016-2017 The PIVX developers
// Copyright (c) 2017-2018 The XDNA Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "spork.h"
#include "base58.h"
#include "key.h"
#include "main.h"
#include "net.h"
#include "protocol.h"
#include "sync.h"
#include "util.h"
#include <boost/lexical_cast.hpp>

using namespace std;
using namespace boost;

class CSporkMessage;
class CSporkManager;

CSporkManager sporkManager;

std::map<uint256, CSporkMessage> mapSporks;
std::map<int, CSporkMessage> mapSporksActive;


void ProcessSpork(CNode* pfrom, std::string& strCommand, CDataStream& vRecv)
{
    if (fLiteMode) return; //disable all obfuscation/masternode related functionality

    if (strCommand == "spork") {
        //LogPrintf("ProcessSpork::spork\n");
        CDataStream vMsg(vRecv);
        CSporkMessage spork;
        vRecv >> spork;

        if (chainActive.Tip() == NULL) return;

        uint256 hash = spork.GetHash();
        if (mapSporksActive.count(spork.nSporkID)) {
            if (mapSporksActive[spork.nSporkID].nTimeSigned >= spork.nTimeSigned) {
                if (fDebug) LogPrintf("spork - seen %s block %d \n", hash.ToString(), chainActive.Tip()->nHeight);
                return;
            } else {
                if (fDebug) LogPrintf("spork - got updated spork %s block %d \n", hash.ToString(), chainActive.Tip()->nHeight);
            }
        }

        LogPrintf("spork - new %s ID %d Time %d bestHeight %d\n", hash.ToString(), spork.nSporkID, spork.nValue, chainActive.Tip()->nHeight);

        if (!sporkManager.CheckSignature(spork)) {
            LogPrintf("spork - invalid signature\n");
            Misbehaving(pfrom->GetId(), 100);
            return;
        }

        mapSporks[hash] = spork;
        mapSporksActive[spork.nSporkID] = spork;
        sporkManager.Relay(spork);

        //does a task if needed
        ExecuteSpork(spork.nSporkID, spork.nValue);
    }
    if (strCommand == "getsporks") {
        std::map<int, CSporkMessage>::iterator it = mapSporksActive.begin();

        while (it != mapSporksActive.end()) {
            pfrom->PushMessage("spork", it->second);
            it++;
        }
    }
}

// grab the spork, otherwise say it's off
bool IsSporkActive(int nSporkID)
{
    int64_t r = -1;

    if (mapSporksActive.count(nSporkID)) {
        r = mapSporksActive[nSporkID].nValue;
    } else {
        if (nSporkID == SPORK_1_SWIFTTX) r = SPORK_1_SWIFTTX_DEFAULT;
        if (nSporkID == SPORK_2_SWIFTTX_BLOCK_FILTERING) r = SPORK_2_SWIFTTX_BLOCK_FILTERING_DEFAULT;
        if (nSporkID == SPORK_3_MAX_VALUE) r = SPORK_3_MAX_VALUE_DEFAULT;
        if (nSporkID == SPORK_4_MASTERNODE_PAYMENT_ENFORCEMENT) r = SPORK_4_MASTERNODE_PAYMENT_ENFORCEMENT_DEFAULT;
        if (nSporkID == SPORK_5_RECONSIDER_BLOCKS) r = SPORK_5_RECONSIDER_BLOCKS_DEFAULT;
        if (nSporkID == SPORK_6_MN_WINNER_MINIMUM_AGE) r = SPORK_6_MN_WINNER_MINIMUM_AGE_DEFAULT;
        if (nSporkID == SPORK_7_NEW_PROTOCOL_ENFORCEMENT) r = SPORK_7_NEW_PROTOCOL_ENFORCEMENT_DEFAULT;
        if (nSporkID == SPORK_8_MIN_STAKE_INPUT) r = SPORK_8_MIN_STAKE_INPUT_DEFAULT;
        if (nSporkID == SPORK_9_BLOCK_REWARD) r = SPORK_9_BLOCK_REWARD_DEFAULT;
        if (nSporkID == SPORK_10_TIER_1_COLLATERAL) r = SPORK_10_TIER_1_COLLATERAL_DEFAULT;
        if (nSporkID == SPORK_11_TIER_2_COLLATERAL) r = SPORK_11_TIER_2_COLLATERAL_DEFAULT;
        if (nSporkID == SPORK_12_TIER_3_COLLATERAL) r = SPORK_12_TIER_3_COLLATERAL_DEFAULT;
        if (nSporkID == SPORK_13_TIER_4_COLLATERAL) r = SPORK_13_TIER_4_COLLATERAL_DEFAULT;
        if (nSporkID == SPORK_14_TIER_5_COLLATERAL) r = SPORK_14_TIER_5_COLLATERAL_DEFAULT;
        if (nSporkID == SPORK_15_TIER_1_BLOCK_PERCENT) r = SPORK_15_TIER_1_BLOCK_PERCENT_DEFAULT;
        if (nSporkID == SPORK_16_TIER_2_BLOCK_PERCENT) r = SPORK_16_TIER_2_BLOCK_PERCENT_DEFAULT;
        if (nSporkID == SPORK_17_TIER_3_BLOCK_PERCENT) r = SPORK_17_TIER_3_BLOCK_PERCENT_DEFAULT;
        if (nSporkID == SPORK_18_TIER_4_BLOCK_PERCENT) r = SPORK_18_TIER_4_BLOCK_PERCENT_DEFAULT;
        if (nSporkID == SPORK_19_TIER_5_BLOCK_PERCENT) r = SPORK_19_TIER_5_BLOCK_PERCENT_DEFAULT;
        if (nSporkID == SPORK_20_SKIP_MN_SYNC) r = SPORK_20_SKIP_MN_SYNC_DEFAULT;
        if (nSporkID == SPORK_21_ACTIVATE_MIN_STAKE) r = SPORK_21_ACTIVATE_MIN_STAKE_DEFAULT;
        if (nSporkID == SPORK_22_COIN_MATURITY) r = SPORK_22_COIN_MATURITY_DEFAULT;
        if (nSporkID == SPORK_23_FREEZE_CHAIN) r = SPORK_23_FREEZE_CHAIN_DEFAULT;

        if (r == -1) LogPrintf("GetSpork::Unknown Spork %d\n", nSporkID);
    }
    if (r == -1) r = 4070908800; //return 2099-1-1 by default

    return r < GetTime();
}

// grab the value of the spork on the network, or the default
int64_t GetSporkValue(int nSporkID)
{
    int64_t r = -1;

    if (mapSporksActive.count(nSporkID)) {
        r = mapSporksActive[nSporkID].nValue;
    } else {
        if (nSporkID == SPORK_1_SWIFTTX) r = SPORK_1_SWIFTTX_DEFAULT;
        if (nSporkID == SPORK_2_SWIFTTX_BLOCK_FILTERING) r = SPORK_2_SWIFTTX_BLOCK_FILTERING_DEFAULT;
        if (nSporkID == SPORK_3_MAX_VALUE) r = SPORK_3_MAX_VALUE_DEFAULT;
        if (nSporkID == SPORK_4_MASTERNODE_PAYMENT_ENFORCEMENT) r = SPORK_4_MASTERNODE_PAYMENT_ENFORCEMENT_DEFAULT;
        if (nSporkID == SPORK_5_RECONSIDER_BLOCKS) r = SPORK_5_RECONSIDER_BLOCKS_DEFAULT;
        if (nSporkID == SPORK_6_MN_WINNER_MINIMUM_AGE) r = SPORK_6_MN_WINNER_MINIMUM_AGE_DEFAULT;
        if (nSporkID == SPORK_7_NEW_PROTOCOL_ENFORCEMENT) r = SPORK_7_NEW_PROTOCOL_ENFORCEMENT_DEFAULT;
        if (nSporkID == SPORK_8_MIN_STAKE_INPUT) r = SPORK_8_MIN_STAKE_INPUT_DEFAULT;
        if (nSporkID == SPORK_9_BLOCK_REWARD) r = SPORK_9_BLOCK_REWARD_DEFAULT;
        if (nSporkID == SPORK_10_TIER_1_COLLATERAL) r = SPORK_10_TIER_1_COLLATERAL_DEFAULT;
        if (nSporkID == SPORK_11_TIER_2_COLLATERAL) r = SPORK_11_TIER_2_COLLATERAL_DEFAULT;
        if (nSporkID == SPORK_12_TIER_3_COLLATERAL) r = SPORK_12_TIER_3_COLLATERAL_DEFAULT;
        if (nSporkID == SPORK_13_TIER_4_COLLATERAL) r = SPORK_13_TIER_4_COLLATERAL_DEFAULT;
        if (nSporkID == SPORK_14_TIER_5_COLLATERAL) r = SPORK_14_TIER_5_COLLATERAL_DEFAULT;
        if (nSporkID == SPORK_15_TIER_1_BLOCK_PERCENT) r = SPORK_15_TIER_1_BLOCK_PERCENT_DEFAULT;
        if (nSporkID == SPORK_16_TIER_2_BLOCK_PERCENT) r = SPORK_16_TIER_2_BLOCK_PERCENT_DEFAULT;
        if (nSporkID == SPORK_17_TIER_3_BLOCK_PERCENT) r = SPORK_17_TIER_3_BLOCK_PERCENT_DEFAULT;
        if (nSporkID == SPORK_18_TIER_4_BLOCK_PERCENT) r = SPORK_18_TIER_4_BLOCK_PERCENT_DEFAULT;
        if (nSporkID == SPORK_19_TIER_5_BLOCK_PERCENT) r = SPORK_19_TIER_5_BLOCK_PERCENT_DEFAULT;
        if (nSporkID == SPORK_20_SKIP_MN_SYNC) r = SPORK_20_SKIP_MN_SYNC_DEFAULT;
        if (nSporkID == SPORK_21_ACTIVATE_MIN_STAKE) r = SPORK_21_ACTIVATE_MIN_STAKE_DEFAULT;
        if (nSporkID == SPORK_22_COIN_MATURITY) r = SPORK_22_COIN_MATURITY_DEFAULT;
        if (nSporkID == SPORK_23_FREEZE_CHAIN) r = SPORK_23_FREEZE_CHAIN_DEFAULT;


        if (r == -1) LogPrintf("GetSpork::Unknown Spork %d\n", nSporkID);
    }

    return r;
}

void ExecuteSpork(int nSporkID, int nValue)
{
    //correct fork via spork technology
    if (nSporkID == SPORK_5_RECONSIDER_BLOCKS && nValue > 0) {
        LogPrintf("Spork::ExecuteSpork -- Reconsider Last %d Blocks\n", nValue);

        ReprocessBlocks(nValue);
    }
}

void ReprocessBlocks(int nBlocks)
{
    std::map<uint256, int64_t>::iterator it = mapRejectedBlocks.begin();
    while (it != mapRejectedBlocks.end()) {
        //use a window twice as large as is usual for the nBlocks we want to reset
        if ((*it).second > GetTime() - (nBlocks * 60 * 5)) {
            BlockMap::iterator mi = mapBlockIndex.find((*it).first);
            if (mi != mapBlockIndex.end() && (*mi).second) {
                LOCK(cs_main);

                CBlockIndex* pindex = (*mi).second;
                LogPrintf("ReprocessBlocks - %s\n", (*it).first.ToString());

                CValidationState state;
                ReconsiderBlock(state, pindex);
            }
        }
        ++it;
    }

    CValidationState state;
    {
        LOCK(cs_main);
        DisconnectBlocksAndReprocess(nBlocks);
    }

    if (state.IsValid()) {
        ActivateBestChain(state);
    }
}

bool CSporkManager::CheckSignature(CSporkMessage& spork)
{
    //note: need to investigate why this is failing
    std::string strMessage = boost::lexical_cast<std::string>(spork.nSporkID) + boost::lexical_cast<std::string>(spork.nValue) + boost::lexical_cast<std::string>(spork.nTimeSigned);
    CPubKey pubkey(ParseHex(Params().SporkKey()));

    std::string errorMessage = "";
    if (!obfuScationSigner.VerifyMessage(pubkey, spork.vchSig, strMessage, errorMessage)) {
        return false;
    }

    return true;
}

bool CSporkManager::Sign(CSporkMessage& spork)
{
    std::string strMessage = boost::lexical_cast<std::string>(spork.nSporkID) + boost::lexical_cast<std::string>(spork.nValue) + boost::lexical_cast<std::string>(spork.nTimeSigned);

    CKey key2;
    CPubKey pubkey2;
    std::string errorMessage = "";

    if (!obfuScationSigner.SetKey(strMasterPrivKey, errorMessage, key2, pubkey2)) {
        LogPrintf("CMasternodePayments::Sign - ERROR: Invalid masternodeprivkey: '%s'\n", errorMessage);
        return false;
    }

    if (!obfuScationSigner.SignMessage(strMessage, errorMessage, spork.vchSig, key2)) {
        LogPrintf("CMasternodePayments::Sign - Sign message failed");
        return false;
    }

    if (!obfuScationSigner.VerifyMessage(pubkey2, spork.vchSig, strMessage, errorMessage)) {
        LogPrintf("CMasternodePayments::Sign - Verify message failed");
        return false;
    }

    return true;
}

bool CSporkManager::UpdateSpork(int nSporkID, int64_t nValue)
{
    CSporkMessage msg;
    msg.nSporkID = nSporkID;
    msg.nValue = nValue;
    msg.nTimeSigned = GetTime();

    if (Sign(msg)) {
        Relay(msg);
        mapSporks[msg.GetHash()] = msg;
        mapSporksActive[nSporkID] = msg;
        return true;
    }

    return false;
}

void CSporkManager::Relay(CSporkMessage& msg)
{
    CInv inv(MSG_SPORK, msg.GetHash());
    RelayInv(inv);
}

bool CSporkManager::SetPrivKey(std::string strPrivKey)
{
    CSporkMessage msg;

    // Test signing successful, proceed
    strMasterPrivKey = strPrivKey;

    Sign(msg);

    if (CheckSignature(msg)) {
        LogPrintf("CSporkManager::SetPrivKey - Successfully initialized as spork signer\n");
        return true;
    } else {
        return false;
    }
}

int CSporkManager::GetSporkIDByName(std::string strName)
{
    if (strName == "SPORK_1_SWIFTTX") return SPORK_1_SWIFTTX;
    if (strName == "SPORK_2_SWIFTTX_BLOCK_FILTERING") return SPORK_2_SWIFTTX_BLOCK_FILTERING;
    if (strName == "SPORK_3_MAX_VALUE") return SPORK_3_MAX_VALUE;
    if (strName == "SPORK_4_MASTERNODE_PAYMENT_ENFORCEMENT") return SPORK_4_MASTERNODE_PAYMENT_ENFORCEMENT;
    if (strName == "SPORK_5_RECONSIDER_BLOCKS") return SPORK_5_RECONSIDER_BLOCKS;
    if (strName == "SPORK_6_MN_WINNER_MINIMUM_AGE") return SPORK_6_MN_WINNER_MINIMUM_AGE;
    if (strName == "SPORK_7_NEW_PROTOCOL_ENFORCEMENT") return SPORK_7_NEW_PROTOCOL_ENFORCEMENT;
    if (strName == "SPORK_8_MIN_STAKE_INPUT") return SPORK_8_MIN_STAKE_INPUT;
    if (strName == "SPORK_9_BLOCK_REWARD") return SPORK_9_BLOCK_REWARD;
    if (strName == "SPORK_10_TIER_1_COLLATERAL") return SPORK_10_TIER_1_COLLATERAL;
    if (strName == "SPORK_11_TIER_2_COLLATERAL") return SPORK_11_TIER_2_COLLATERAL;
    if (strName == "SPORK_12_TIER_3_COLLATERAL") return SPORK_12_TIER_3_COLLATERAL;
    if (strName == "SPORK_13_TIER_4_COLLATERAL") return SPORK_13_TIER_4_COLLATERAL;
    if (strName == "SPORK_14_TIER_5_COLLATERAL") return SPORK_14_TIER_5_COLLATERAL;
    if (strName == "SPORK_15_TIER_1_BLOCK_PERCENT") return SPORK_15_TIER_1_BLOCK_PERCENT;
    if (strName == "SPORK_16_TIER_2_BLOCK_PERCENT") return SPORK_16_TIER_2_BLOCK_PERCENT;
    if (strName == "SPORK_17_TIER_3_BLOCK_PERCENT") return SPORK_17_TIER_3_BLOCK_PERCENT;
    if (strName == "SPORK_18_TIER_4_BLOCK_PERCENT") return SPORK_18_TIER_4_BLOCK_PERCENT;
    if (strName == "SPORK_19_TIER_5_BLOCK_PERCENT") return SPORK_19_TIER_5_BLOCK_PERCENT;
    if (strName == "SPORK_20_SKIP_MN_SYNC") return SPORK_20_SKIP_MN_SYNC;
    if (strName == "SPORK_21_ACTIVATE_MIN_STAKE") return SPORK_21_ACTIVATE_MIN_STAKE;
    if (strName == "SPORK_22_COIN_MATURITY") return SPORK_22_COIN_MATURITY;
    if (strName == "SPORK_23_FREEZE_CHAIN") return SPORK_23_FREEZE_CHAIN;

    return -1;
}

std::string CSporkManager::GetSporkNameByID(int id)
{
    if (id == SPORK_1_SWIFTTX) return "SPORK_1_SWIFTTX";
    if (id == SPORK_2_SWIFTTX_BLOCK_FILTERING) return "SPORK_2_SWIFTTX_BLOCK_FILTERING";
    if (id == SPORK_3_MAX_VALUE) return "SPORK_3_MAX_VALUE";
    if (id == SPORK_4_MASTERNODE_PAYMENT_ENFORCEMENT) return "SPORK_4_MASTERNODE_PAYMENT_ENFORCEMENT";
    if (id == SPORK_5_RECONSIDER_BLOCKS) return "SPORK_5_RECONSIDER_BLOCKS";
    if (id == SPORK_6_MN_WINNER_MINIMUM_AGE) return "SPORK_6_MN_WINNER_MINIMUM_AGE";
    if (id == SPORK_7_NEW_PROTOCOL_ENFORCEMENT) return "SPORK_7_NEW_PROTOCOL_ENFORCEMENT";
    if (id == SPORK_8_MIN_STAKE_INPUT) return "SPORK_8_MIN_STAKE_INPUT";
    if (id == SPORK_9_BLOCK_REWARD) return "SPORK_9_BLOCK_REWARD";
    if (id == SPORK_10_TIER_1_COLLATERAL) return "SPORK_10_TIER_1_COLLATERAL";
    if (id == SPORK_11_TIER_2_COLLATERAL) return "SPORK_11_TIER_2_COLLATERAL";
    if (id == SPORK_12_TIER_3_COLLATERAL) return "SPORK_12_TIER_3_COLLATERAL";
    if (id == SPORK_13_TIER_4_COLLATERAL) return "SPORK_13_TIER_4_COLLATERAL";
    if (id == SPORK_14_TIER_5_COLLATERAL) return "SPORK_14_TIER_5_COLLATERAL";
    if (id == SPORK_15_TIER_1_BLOCK_PERCENT) return "SPORK_15_TIER_1_BLOCK_PERCENT";
    if (id == SPORK_16_TIER_2_BLOCK_PERCENT) return "SPORK_16_TIER_2_BLOCK_PERCENT";
    if (id == SPORK_17_TIER_3_BLOCK_PERCENT) return "SPORK_17_TIER_3_BLOCK_PERCENT";
    if (id == SPORK_18_TIER_4_BLOCK_PERCENT) return "SPORK_18_TIER_4_BLOCK_PERCENT";
    if (id == SPORK_19_TIER_5_BLOCK_PERCENT) return "SPORK_19_TIER_5_BLOCK_PERCENT";
    if (id == SPORK_20_SKIP_MN_SYNC) return "SPORK_20_SKIP_MN_SYNC";
    if (id == SPORK_21_ACTIVATE_MIN_STAKE) return "SPORK_21_ACTIVATE_MIN_STAKE";
    if (id == SPORK_22_COIN_MATURITY) return "SPORK_22_COIN_MATURITY";
    if (id == SPORK_23_FREEZE_CHAIN) return "SPORK_23_FREEZE_CHAIN";

    return "Unknown";
}
