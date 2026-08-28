// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_PRIMITIVES_PUREHEADER_H
#define BITCOIN_PRIMITIVES_PUREHEADER_H

#include <serialize.h>
#include <uint256.h>
#include <util/time.h>

/**
 * The plain 80-byte block header, with no auxiliary proof-of-work payload.
 *
 * This is split out from CBlockHeader (primitives/block.h) specifically so that
 * CBlockHeader::GetHash() — inherited unchanged from here — never depends on the
 * auxpow blob CBlockHeader may carry. AuxPoW's whole mechanism relies on a parent
 * chain's coinbase committing to a *fixed* child block hash that exists before the
 * auxpow proof around it is built; if GetHash() covered the auxpow bytes, that
 * commitment target would change depending on data it's supposed to be fixed
 * before. See src/auxpow.h for the auxiliary proof-of-work payload itself.
 *
 * The parent header embedded inside CAuxPow is also typed as CPureBlockHeader
 * (never CBlockHeader), which structurally forbids a parent block from itself
 * carrying an auxpow blob — i.e. no nested/recursive AuxPoW.
 */
class CPureBlockHeader
{
public:
    // header
    int32_t nVersion;
    uint256 hashPrevBlock;
    uint256 hashMerkleRoot;
    uint32_t nTime;
    uint32_t nBits;
    uint32_t nNonce;

    CPureBlockHeader()
    {
        SetNull();
    }

    SERIALIZE_METHODS(CPureBlockHeader, obj) { READWRITE(obj.nVersion, obj.hashPrevBlock, obj.hashMerkleRoot, obj.nTime, obj.nBits, obj.nNonce); }

    void SetNull()
    {
        nVersion = 0;
        hashPrevBlock.SetNull();
        hashMerkleRoot.SetNull();
        nTime = 0;
        nBits = 0;
        nNonce = 0;
    }

    bool IsNull() const
    {
        return (nBits == 0);
    }

    uint256 GetHash() const;

    NodeSeconds Time() const
    {
        return NodeSeconds{std::chrono::seconds{nTime}};
    }

    int64_t GetBlockTime() const
    {
        return (int64_t)nTime;
    }
};

#endif // BITCOIN_PRIMITIVES_PUREHEADER_H
