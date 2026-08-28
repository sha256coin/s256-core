// Copyright (c) 2014-2024 Namecoin/Dogecoin/Syscoin developers (design)
// Copyright (c) 2026 The S256 developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <auxpow.h>

#include <consensus/params.h>
#include <consensus/validation.h>
#include <hash.h>
#include <pow.h>
#include <script/script.h>

#include <algorithm>

namespace {

/**
 * Walk a merkle branch up from a leaf hash to a root, combining with each
 * sibling in vMerkleBranch according to the corresponding bit of nIndex (the
 * leaf's position). This is the standard Bitcoin merkle-branch verification
 * algorithm (the same one legacy SPV clients used) — the exact inverse of how
 * consensus/merkle.cpp's MerkleComputation collects a path for a given leaf
 * position, so a genuine branch produced against that leaf position always
 * verifies here.
 */
uint256 CheckMerkleBranch(uint256 hash, const std::vector<uint256>& vMerkleBranch, int nIndex)
{
    if (nIndex == -1) {
        return uint256();
    }
    for (const uint256& otherside : vMerkleBranch) {
        if (nIndex & 1) {
            hash = Hash(otherside, hash);
        } else {
            hash = Hash(hash, otherside);
        }
        nIndex >>= 1;
    }
    return hash;
}

} // namespace

int CAuxPow::GetExpectedIndex(uint32_t nNonce, int nChainId, unsigned int h)
{
    // Choose a pseudo-random slot in the merge-mining tree, deterministic in
    // (nonce, chainId), so a proof's claimed nChainIndex can be independently
    // recomputed and checked rather than merely trusted. This is the classic
    // Namecoin/Dogecoin formula; skipping this check entirely was the root
    // cause of the original AuxPoW "index confusion" issue, where a single
    // chain-merkle branch could otherwise validate against more than one
    // aux-chain slot and be misattributed across chains sharing a parent.
    uint32_t rand = nNonce;
    rand = rand * 1103515245 + 12345;
    rand += static_cast<uint32_t>(nChainId);
    rand = rand * 1103515245 + 12345;

    return static_cast<int>(rand % (1u << h));
}

bool CAuxPow::CheckAuxPow(const uint256& hashAuxBlock, unsigned int nBits, int nChainId,
                           const Consensus::Params& params, BlockValidationState& state) const
{
    if (!coinbaseTx) {
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "auxpow-no-coinbase",
                              "auxpow is missing its parent coinbase transaction");
    }
    if (coinbaseTx->vin.empty()) {
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "auxpow-coinbase-no-input",
                              "auxpow parent coinbase transaction has no inputs");
    }

    // --- Chain merkle branch bounds --------------------------------------
    if (vChainMerkleBranch.size() > MAX_CHAIN_MERKLE_BRANCH_LENGTH) {
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "auxpow-chain-merkle-branch-too-long",
                              "auxpow chain merkle branch is longer than allowed");
    }

    // --- Expected-index check: this IS the chain-ID / anti-replay guard -
    // There is no separate "chain ID" field carried inside the serialized
    // proof to check against consensus.nAuxpowChainId — chain identity is
    // instead proven entirely by *where* this chain's hash is required to
    // sit in the merge-mining tree. nChainId here is always this chain's own
    // consensus.nAuxpowChainId (passed in by the caller), so a proof that was
    // actually built for a different aux chain sharing the same parent
    // coinbase will, in general, have been placed at a different slot and
    // will fail this comparison. Skipping this check (accepting any
    // self-consistent branch/index pair without recomputing the expected
    // slot) was the root cause of the original Namecoin AuxPoW "index
    // confusion" issue — never weaken or remove it.
    const int nExpectedIndex = GetExpectedIndex(parentBlock.nNonce, nChainId,
                                                 static_cast<unsigned int>(vChainMerkleBranch.size()));
    if (nChainIndex != nExpectedIndex) {
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "auxpow-wrong-index",
                              "auxpow chain merkle index does not match the expected deterministic slot for this "
                              "chain ID / parent nonce / tree size");
    }

    // --- Locate the merge-mining tag in the parent coinbase --------------
    const CScript& scriptSig = coinbaseTx->vin[0].scriptSig;
    const auto tagBegin = std::search(scriptSig.begin(), scriptSig.end(),
                                       std::begin(MERGE_MINING_HEADER), std::end(MERGE_MINING_HEADER));
    if (tagBegin == scriptSig.end()) {
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "auxpow-no-merge-mining-tag",
                              "auxpow parent coinbase does not contain a merge-mining commitment tag");
    }
    // Reject if the tag magic occurs more than once — an ambiguous/malleable
    // placement was another historical hardening point in real deployments.
    const auto secondTag = std::search(tagBegin + 1, scriptSig.end(),
                                        std::begin(MERGE_MINING_HEADER), std::end(MERGE_MINING_HEADER));
    if (secondTag != scriptSig.end()) {
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "auxpow-multiple-merge-mining-tags",
                              "auxpow parent coinbase contains more than one merge-mining commitment tag");
    }

    constexpr size_t TAG_HEADER_LEN = 4;
    constexpr size_t TAG_ROOT_LEN = 32;
    constexpr size_t TAG_SIZE_LEN = 4;
    constexpr size_t TAG_NONCE_LEN = 4;
    constexpr size_t TAG_TOTAL_LEN = TAG_HEADER_LEN + TAG_ROOT_LEN + TAG_SIZE_LEN + TAG_NONCE_LEN;

    if (static_cast<size_t>(scriptSig.end() - tagBegin) < TAG_TOTAL_LEN) {
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "auxpow-truncated-merge-mining-tag",
                              "auxpow parent coinbase merge-mining tag is truncated");
    }

    auto field = tagBegin + TAG_HEADER_LEN;
    uint256 chainMerkleRoot;
    std::copy(field, field + TAG_ROOT_LEN, chainMerkleRoot.begin());
    field += TAG_ROOT_LEN;

    uint32_t merkleSize = 0;
    for (int i = 3; i >= 0; --i) {
        merkleSize = (merkleSize << 8) | static_cast<uint8_t>(*(field + i));
    }
    field += TAG_SIZE_LEN;

    uint32_t merkleNonce = 0;
    for (int i = 3; i >= 0; --i) {
        merkleNonce = (merkleNonce << 8) | static_cast<uint8_t>(*(field + i));
    }

    if (merkleSize != (1u << vChainMerkleBranch.size())) {
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "auxpow-wrong-merkle-tree-size",
                              "auxpow merge-mining tag's claimed tree size does not match the supplied chain "
                              "merkle branch length");
    }
    if (merkleNonce != parentBlock.nNonce) {
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "auxpow-wrong-merkle-nonce",
                              "auxpow merge-mining tag's nonce does not match the parent block's nonce");
    }

    // --- Chain merkle branch: hashAuxBlock -> chainMerkleRoot ------------
    const uint256 computedChainRoot = CheckMerkleBranch(hashAuxBlock, vChainMerkleBranch, nChainIndex);
    if (computedChainRoot != chainMerkleRoot) {
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "auxpow-chain-merkle-mismatch",
                              "auxpow chain merkle branch does not connect this chain's block hash to the "
                              "merge-mining tag's committed root");
    }

    // --- Coinbase merkle branch: coinbaseTx -> parentBlock.hashMerkleRoot -
    const uint256 coinbaseHash = coinbaseTx->GetHash().ToUint256();
    const uint256 computedParentRoot = CheckMerkleBranch(coinbaseHash, vMerkleBranch, nIndex);
    if (computedParentRoot != parentBlock.hashMerkleRoot) {
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "auxpow-coinbase-merkle-mismatch",
                              "auxpow coinbase merkle branch does not connect to the parent block's merkle root");
    }

    // --- The actual proof-of-work: parent header hash vs. THIS chain's target -
    if (!CheckProofOfWork(parentBlock.GetHash(), nBits, params)) {
        return state.Invalid(BlockValidationResult::BLOCK_INVALID_HEADER, "auxpow-high-hash",
                              "auxpow parent block does not meet this chain's required proof-of-work target");
    }

    return true;
}
