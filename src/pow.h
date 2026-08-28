// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_POW_H
#define BITCOIN_POW_H

#include <consensus/params.h>

#include <cstdint>

class BlockValidationState;
class CBlockHeader;
class CBlockIndex;
class uint256;
class arith_uint256;

/**
 * Convert nBits value to target.
 *
 * @param[in] nBits     compact representation of the target
 * @param[in] pow_limit PoW limit (consensus parameter)
 *
 * @return              the proof-of-work target or nullopt if the nBits value
 *                      is invalid (due to overflow or exceeding pow_limit)
 */
std::optional<arith_uint256> DeriveTarget(unsigned int nBits, const uint256 pow_limit);

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params&);
unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params&);

/**
 * S256: per-block LWMA (N=96) difficulty adjustment, active from
 * Consensus::Params::LwmaStartHeight onward — see the implementation in
 * pow.cpp for why (the classic DAA above can leave the chain stuck at a
 * too-high difficulty for a full retarget window if hashrate drops
 * suddenly).
 */
unsigned int LwmaCalculateNextWorkRequired(const CBlockIndex* pindexLast, const Consensus::Params&);

/** Check whether a block hash satisfies the proof-of-work requirement specified by nBits */
bool CheckProofOfWork(uint256 hash, unsigned int nBits, const Consensus::Params&);
bool CheckProofOfWorkImpl(uint256 hash, unsigned int nBits, const Consensus::Params&);

/**
 * Header-aware proof-of-work check. If block.IsAuxpow() is set, validates the
 * embedded CAuxPow (parent block's proof-of-work plus its merkle commitment
 * to this header's hash) against nBits instead of hashing the header
 * directly — see auxpow.h. Otherwise falls back to the plain hash check
 * above. This needs no block-height/activation context: it only checks
 * whether the *math* is valid, not whether the auxpow *format* is allowed
 * yet at this chain height (that height gate lives in
 * ContextualCheckBlockHeader, validation.cpp).
 *
 * The state-aware overload records the specific rejection reason (e.g. from
 * CAuxPow::CheckAuxPow) into `state`. The convenience overload is for the
 * handful of call sites with no BlockValidationState available (e.g. the
 * anti-DoS headers pre-filter in HasValidProofOfWork) — it still performs
 * the full auxpow verification, just discards the detailed reason.
 */
bool CheckProofOfWork(const CBlockHeader& block, const Consensus::Params&, BlockValidationState& state);
bool CheckProofOfWork(const CBlockHeader& block, const Consensus::Params&);

/**
 * Return false if the proof-of-work requirement specified by new_nbits at a
 * given height is not possible, given the proof-of-work on the prior block as
 * specified by old_nbits.
 *
 * This function only checks that the new value is within a factor of 4 of the
 * old value for blocks at the difficulty adjustment interval, and otherwise
 * requires the values to be the same.
 *
 * Always returns true on networks where min difficulty blocks are allowed,
 * such as regtest/testnet.
 */
bool PermittedDifficultyTransition(const Consensus::Params& params, int64_t height, uint32_t old_nbits, uint32_t new_nbits);

#endif // BITCOIN_POW_H
