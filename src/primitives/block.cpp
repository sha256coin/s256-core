// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <primitives/block.h>

#include <tinyformat.h>

#include <sstream>

// S256: deliberately NOT overriding CBlockHeader::GetHash() here (unlike
// upstream, which added one computed as `HashWriter{} << *this` for its
// single, non-AuxPoW header class). In this fork *this would be statically
// typed CBlockHeader, whose SERIALIZE_METHODS conditionally include the
// auxpow blob — hashing that in would break the core AuxPoW invariant that
// GetHash() covers only the 6 pure header fields (a parent chain's coinbase
// must commit to this hash before the auxpow proof exists; see the class
// comment in block.h and CPureBlockHeader::GetHash() in pureheader.cpp,
// which this class continues to inherit unchanged).
std::string CBlock::ToString() const
{
    std::stringstream s;
    s << strprintf("CBlock(hash=%s, ver=0x%08x, hashPrevBlock=%s, hashMerkleRoot=%s, nTime=%u, nBits=%08x, nNonce=%u, vtx=%u)\n",
        GetHash().ToString(),
        nVersion,
        hashPrevBlock.ToString(),
        hashMerkleRoot.ToString(),
        nTime, nBits, nNonce,
        vtx.size());
    for (const auto& tx : vtx) {
        s << "  " << tx->ToString() << "\n";
    }
    return s.str();
}
