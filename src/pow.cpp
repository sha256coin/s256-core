// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pow.h>

#include <arith_uint256.h>
#include <auxpow.h>
#include <chain.h>
#include <consensus/validation.h>
#include <primitives/block.h>
#include <uint256.h>
#include <util/check.h>

#include <algorithm>

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    assert(pindexLast != nullptr);

    // S256: from LwmaStartHeight onward, use per-block LWMA instead of the
    // classic fixed-window DAA below — see LwmaCalculateNextWorkRequired for
    // why. A plain height comparison rather than DeploymentActiveAfter, since
    // this function only has pindexLast, not a ChainstateManager/versionbitscache.
    if (pindexLast->nHeight + 1 >= params.LwmaStartHeight) {
        return LwmaCalculateNextWorkRequired(pindexLast, pblock, params);
    }

    unsigned int nProofOfWorkLimit = UintToArith256(params.powLimit).GetCompact();

    // Only change once per difficulty adjustment interval
    if ((pindexLast->nHeight+1) % params.DifficultyAdjustmentInterval() != 0)
    {
        if (params.fPowAllowMinDifficultyBlocks)
        {
            // Special difficulty rule for testnet:
            // If the new block's timestamp is more than 2* 10 minutes
            // then it MUST be a min-difficulty block.
            if (pblock->GetBlockTime() > pindexLast->GetBlockTime() + params.nPowTargetSpacing*2)
                return nProofOfWorkLimit;
            else
            {
                // Return the last non-special-min-difficulty-rules-block
                const CBlockIndex* pindex = pindexLast;
                while (pindex->pprev && pindex->nHeight % params.DifficultyAdjustmentInterval() != 0 && pindex->nBits == nProofOfWorkLimit)
                    pindex = pindex->pprev;
                return pindex->nBits;
            }
        }
        return pindexLast->nBits;
    }

    // Go back by what we want to be 14 days worth of blocks
    int nHeightFirst = pindexLast->nHeight - (params.DifficultyAdjustmentInterval()-1);
    assert(nHeightFirst >= 0);
    const CBlockIndex* pindexFirst = pindexLast->GetAncestor(nHeightFirst);
    assert(pindexFirst);

    return CalculateNextWorkRequired(pindexLast, pindexFirst->GetBlockTime(), params);
}

unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params& params)
{
    if (params.fPowNoRetargeting)
        return pindexLast->nBits;

    // Limit adjustment step
    int64_t nActualTimespan = pindexLast->GetBlockTime() - nFirstBlockTime;
    if (nActualTimespan < params.nPowTargetTimespan/4)
        nActualTimespan = params.nPowTargetTimespan/4;
    if (nActualTimespan > params.nPowTargetTimespan*4)
        nActualTimespan = params.nPowTargetTimespan*4;

    // Retarget
    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    arith_uint256 bnNew;

    // Special difficulty rule for Testnet4
    if (params.enforce_BIP94) {
        // Here we use the first block of the difficulty period. This way
        // the real difficulty is always preserved in the first block as
        // it is not allowed to use the min-difficulty exception.
        int nHeightFirst = pindexLast->nHeight - (params.DifficultyAdjustmentInterval()-1);
        const CBlockIndex* pindexFirst = pindexLast->GetAncestor(nHeightFirst);
        bnNew.SetCompact(pindexFirst->nBits);
    } else {
        bnNew.SetCompact(pindexLast->nBits);
    }

    bnNew *= nActualTimespan;
    bnNew /= params.nPowTargetTimespan;

    if (bnNew > bnPowLimit)
        bnNew = bnPowLimit;

    return bnNew.GetCompact();
}

// S256: per-block LWMA (Linearly Weighted Moving Average) difficulty
// adjustment, active from LwmaStartHeight onward — a well-known community
// algorithm (N=96 window) chosen over the classic fixed-window DAA below.
// Unlike the classic DAA (which only retargets every
// DifficultyAdjustmentInterval() blocks), this recomputes the target every
// block from a weighted average of the last N blocks' (clamped) solve
// times, so difficulty tracks hashrate changes within a handful of blocks
// instead of being stuck at a stale value for up to a full retarget window.
unsigned int LwmaCalculateNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader* pblock, const Consensus::Params& params)
{
    // S256: mirror the classic DAA's own fPowNoRetargeting check (see
    // CalculateNextWorkRequired above). Regtest sets fPowNoRetargeting=true precisely so
    // difficulty doesn't react to synthetic test timestamps; test fixtures
    // routinely mine many blocks with compressed (e.g. ~1s) gaps that look
    // "impossibly fast" against the real nPowTargetSpacing. Without this
    // check, LWMA (which retargets every block, unlike the classic DAA)
    // ratchets the target tighter each such block with no bound, and mining
    // past the N-block window becomes computationally infeasible.
    if (params.fPowNoRetargeting) return pindexLast->nBits;

    const int64_t T = params.nPowTargetSpacing;
    const int64_t N = 96;
    const int64_t k = N * (N + 1) * T / 2;
    const int64_t height = pindexLast->nHeight;
    const arith_uint256 powLimit = UintToArith256(params.powLimit);

    // S256: classic DAA's testnet min-difficulty-blocks rule (see
    // GetNextWorkRequired above), reimplemented here because LWMA otherwise
    // never consults fPowAllowMinDifficultyBlocks at all. Without this,
    // testnet (where LWMA is active from height 1) is exactly as hard to
    // mine as mainnet, since both share the same powLimit — defeating the
    // whole point of the flag. If the new block arrives more than 2x the
    // target spacing after its parent, it may use powLimit directly, same
    // as the classic rule. Unlike the classic rule, this doesn't walk back
    // to find the "last non-min-difficulty block" to re-anchor from: LWMA's
    // own N-block weighted window already absorbs an occasional easy block
    // and re-adjusts within it, so that extra bookkeeping isn't needed here.
    if (params.fPowAllowMinDifficultyBlocks &&
        pblock->GetBlockTime() > pindexLast->GetBlockTime() + T * 2) {
        return powLimit.GetCompact();
    }

    if (height < N) { return powLimit.GetCompact(); }

    arith_uint256 sumTarget, nextTarget;
    int64_t thisTimestamp, previousTimestamp;
    int64_t t = 0, j = 0;

    const CBlockIndex* blockPreviousTimestamp = pindexLast->GetAncestor(height - N);
    previousTimestamp = blockPreviousTimestamp->GetBlockTime();

    // Loop through N most recent blocks.
    for (int64_t i = height - N + 1; i <= height; i++) {
        const CBlockIndex* block = pindexLast->GetAncestor(i);
        thisTimestamp = (block->GetBlockTime() > previousTimestamp) ?
                            block->GetBlockTime() : previousTimestamp + 1;

        int64_t solvetime = std::min(6 * T, thisTimestamp - previousTimestamp);
        previousTimestamp = thisTimestamp;

        j++;
        t += solvetime * j; // Weighted solvetime sum.
        arith_uint256 target;
        target.SetCompact(block->nBits);
        sumTarget += target / (k * N);
    }
    nextTarget = t * sumTarget;

    if (nextTarget > powLimit) { nextTarget = powLimit; }

    return nextTarget.GetCompact();
}

// Check that on difficulty adjustments, the new difficulty does not increase
// or decrease beyond the permitted limits.
bool PermittedDifficultyTransition(const Consensus::Params& params, int64_t height, uint32_t old_nbits, uint32_t new_nbits)
{
    if (params.fPowAllowMinDifficultyBlocks) return true;

    // S256: under LWMA, every block can retarget (not just at classic
    // DifficultyAdjustmentInterval() boundaries), and this function's
    // signature (old_nbits/new_nbits/height only, no ancestor chain access)
    // structurally cannot recompute LWMA's exact expected target to verify
    // it here. This is only ever a coarse anti-DoS pre-check (used by
    // headerssync.cpp's low-work presync path) — the real, authoritative
    // check is ContextualCheckBlockHeader's exact GetNextWorkRequired
    // recomputation later in the normal header-acceptance path, which is
    // unaffected by this and always enforced. So: don't reject here for
    // LWMA-active heights rather than apply the classic (and, for LWMA,
    // simply wrong) boundary-based bound.
    if (height >= params.LwmaStartHeight) return true;

    if (height % params.DifficultyAdjustmentInterval() == 0) {
        int64_t smallest_timespan = params.nPowTargetTimespan/4;
        int64_t largest_timespan = params.nPowTargetTimespan*4;

        const arith_uint256 pow_limit = UintToArith256(params.powLimit);
        arith_uint256 observed_new_target;
        observed_new_target.SetCompact(new_nbits);

        // Calculate the largest difficulty value possible:
        arith_uint256 largest_difficulty_target;
        largest_difficulty_target.SetCompact(old_nbits);
        largest_difficulty_target *= largest_timespan;
        largest_difficulty_target /= params.nPowTargetTimespan;

        if (largest_difficulty_target > pow_limit) {
            largest_difficulty_target = pow_limit;
        }

        // Round and then compare this new calculated value to what is
        // observed.
        arith_uint256 maximum_new_target;
        maximum_new_target.SetCompact(largest_difficulty_target.GetCompact());
        if (maximum_new_target < observed_new_target) return false;

        // Calculate the smallest difficulty value possible:
        arith_uint256 smallest_difficulty_target;
        smallest_difficulty_target.SetCompact(old_nbits);
        smallest_difficulty_target *= smallest_timespan;
        smallest_difficulty_target /= params.nPowTargetTimespan;

        if (smallest_difficulty_target > pow_limit) {
            smallest_difficulty_target = pow_limit;
        }

        // Round and then compare this new calculated value to what is
        // observed.
        arith_uint256 minimum_new_target;
        minimum_new_target.SetCompact(smallest_difficulty_target.GetCompact());
        if (minimum_new_target > observed_new_target) return false;
    } else if (old_nbits != new_nbits) {
        return false;
    }
    return true;
}

// Bypasses the actual proof of work check during fuzz testing with a simplified validation checking whether
// the most significant bit of the last byte of the hash is set.
bool CheckProofOfWork(uint256 hash, unsigned int nBits, const Consensus::Params& params)
{
    if (EnableFuzzDeterminism()) return (hash.data()[31] & 0x80) == 0;
    return CheckProofOfWorkImpl(hash, nBits, params);
}

bool CheckProofOfWork(const CBlockHeader& block, const Consensus::Params& params, BlockValidationState& state)
{
    if (block.IsAuxpow()) {
        if (!block.auxpow) {
            return state.Invalid(BlockValidationResult::BLOCK_INVALID_HEADER, "auxpow-missing",
                                  "VERSION_AUXPOW_BIT is set but no auxpow payload is present");
        }
        return block.auxpow->CheckAuxPow(block.GetHash(), block.nBits, params.nAuxpowChainId, params, state);
    }
    if (!CheckProofOfWork(block.GetHash(), block.nBits, params)) {
        return state.Invalid(BlockValidationResult::BLOCK_INVALID_HEADER, "high-hash", "proof of work failed");
    }
    return true;
}

bool CheckProofOfWork(const CBlockHeader& block, const Consensus::Params& params)
{
    BlockValidationState state;
    return CheckProofOfWork(block, params, state);
}

std::optional<arith_uint256> DeriveTarget(unsigned int nBits, const uint256 pow_limit)
{
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(pow_limit))
        return {};

    return bnTarget;
}

bool CheckProofOfWorkImpl(uint256 hash, unsigned int nBits, const Consensus::Params& params)
{
    auto bnTarget{DeriveTarget(nBits, params.powLimit)};
    if (!bnTarget) return false;

    // Check proof of work matches claimed amount
    if (UintToArith256(hash) > bnTarget)
        return false;

    return true;
}
