// Copyright (c) 2014-2024 Namecoin/Dogecoin/Syscoin developers (design)
// Copyright (c) 2026 The S256 developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_AUXPOW_H
#define BITCOIN_AUXPOW_H

#include <primitives/pureheader.h>
#include <primitives/transaction.h>
#include <serialize.h>
#include <uint256.h>

#include <vector>

// Forward declarations only — NOT <consensus/validation.h> (which itself pulls
// in primitives/block.h) and NOT <consensus/params.h>, to avoid a
// block.h -> auxpow.h -> validation.h -> block.h style include cycle, since
// primitives/block.h includes this header. auxpow.cpp includes the real
// headers where these types are actually used.
class BlockValidationState;
namespace Consensus {
struct Params;
} // namespace Consensus

/**
 * VERSION_AUXPOW_BIT flags a CBlockHeader::nVersion as carrying an auxiliary
 * proof-of-work payload instead of (or without ever needing) its own directly
 * satisfied header hash. Bit 8 (0x00000100) is permanently reserved for this;
 * see the reservation note next to VERSIONBITS_NUM_BITS in versionbits.h. It
 * must never be assigned to a future BIP9Deployment.bit.
 */
static const int32_t VERSION_AUXPOW_BIT = (1 << 8);

/** Maximum depth of the chain-merkle branch (vChainMerkleBranch). Bounds an
 * attacker-supplied branch length to something sane; also used to sanity-check
 * the claimed merge-mining tree size against the branch actually supplied. */
static const unsigned int MAX_CHAIN_MERKLE_BRANCH_LENGTH = 30;

/** The 4-byte magic that marks the start of a merge-mining commitment tag
 * inside a parent-chain coinbase's scriptSig, per the conventional
 * Namecoin/Dogecoin-style merged-mining format:
 *   0xfabe6d6d ++ chainMerkleRoot(32) ++ merkleSize(4, LE) ++ merkleNonce(4, LE)
 */
static const unsigned char MERGE_MINING_HEADER[4] = {0xfa, 0xbe, 0x6d, 0x6d};

/**
 * An auxiliary proof-of-work: proof that a block on this chain was merge-mined
 * as an aux chain underneath a foreign parent chain's block, by embedding that
 * parent block's coinbase transaction (whose scriptSig commits to this chain's
 * block hash via a merge-mining tag), a merkle branch proving that coinbase is
 * part of the parent block's own transaction merkle tree, a second ("chain")
 * merkle branch proving this chain's hash is the correct leaf of the (possibly
 * multi-aux-chain) merge-mining commitment tree inside that tag, and the parent
 * block's own 80-byte header (whose hash must meet *this* chain's target).
 *
 * The parent header is deliberately typed as CPureBlockHeader, not
 * CBlockHeader: a parent block can never itself carry an auxpow payload,
 * closing off nested/recursive AuxPoW at the type level.
 */
class CAuxPow
{
public:
    /** Parent chain's coinbase transaction. Its scriptSig carries the
     * merge-mining tag committing to (a merkle root of) aux chain hashes. */
    CTransactionRef coinbaseTx;

    /** Merkle branch proving coinbaseTx is included in the parent block's
     * own transaction merkle tree (hashMerkleRoot of parentBlock). */
    std::vector<uint256> vMerkleBranch;
    int nIndex;

    /** Merkle branch proving this chain's block hash is the correct leaf of
     * the merge-mining commitment tree embedded in the coinbase tag (this
     * lets one parent coinbase merge-mine several aux chains at once). */
    std::vector<uint256> vChainMerkleBranch;
    int nChainIndex;

    /** The parent chain's own block header. Its hash must satisfy the
     * *aux* chain's (this chain's) difficulty target, not the parent's own. */
    CPureBlockHeader parentBlock;

    CAuxPow() = default;

    SERIALIZE_METHODS(CAuxPow, obj)
    {
        // coinbaseTx must be wrapped with an explicit TransactionSerParams
        // (TX_WITH_WITNESS, to preserve full fidelity of the real parent
        // coinbase) rather than serialized bare: CTransaction::Serialize/
        // Unserialize calls s.GetParams<TransactionSerParams>(), which only
        // streams already wrapped in a ParamsStream provide. CBlockHeader is
        // (de)serialized through many bare stream types across the codebase
        // that never needed to carry transaction params before (net_processing
        // header-only paths, disk header reads, etc.) — TX_WITH_WITNESS(...)
        // supplies its own nested ParamsStream locally, so no ambient stream
        // needs to support it.
        READWRITE(TX_WITH_WITNESS(obj.coinbaseTx));
        READWRITE(obj.vMerkleBranch);
        READWRITE(obj.nIndex);
        READWRITE(obj.vChainMerkleBranch);
        READWRITE(obj.nChainIndex);
        READWRITE(obj.parentBlock);
    }

    /**
     * Verify this auxpow proof is valid for the given aux (this chain's)
     * block hash, difficulty bits, and expected chain ID.
     *
     * @param hashAuxBlock  This chain's block hash being proven (the child).
     * @param nBits         This chain's required difficulty for that block.
     * @param nChainId      This chain's configured consensus.nAuxpowChainId.
     * @param params        Consensus params (passed through to CheckProofOfWork).
     * @param state         Populated with a specific rejection reason on failure.
     */
    bool CheckAuxPow(const uint256& hashAuxBlock, unsigned int nBits, int nChainId,
                      const Consensus::Params& params, BlockValidationState& state) const;

    /**
     * The canonical index a chain with the given ID must occupy in a
     * merge-mining commitment tree of the given size, for the given nonce.
     * This mirrors how a real merge-mining proxy deterministically places
     * each aux chain's leaf so that placement can be recomputed and checked
     * rather than merely trusted from the submitted proof. Getting this
     * check right (and never skipping it) is what closes the original
     * Namecoin AuxPoW "index confusion" issue, where a single chain-merkle
     * branch could otherwise be misattributed across aux chains sharing one
     * parent coinbase.
     */
    static int GetExpectedIndex(uint32_t nNonce, int nChainId, unsigned int h);
};

#endif // BITCOIN_AUXPOW_H
