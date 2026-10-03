// Copyright (c) 2026 The S256 developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <auxpow.h>

#include <arith_uint256.h>
#include <chainparams.h>
#include <consensus/merkle.h>
#include <consensus/validation.h>
#include <hash.h>
#include <node/miner.h>
#include <pow.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <streams.h>
#include <test/util/setup_common.h>
#include <util/check.h>
#include <util/strencodings.h>
#include <validation.h>
#include <validationinterface.h>

#include <boost/test/unit_test.hpp>

using node::BlockAssembler;

namespace auxpow_tests {

// Builds a syntactically valid parent coinbase + merge-mining tag + parent
// header for a single aux chain (h=0, so GetExpectedIndex is always 0 and
// both merkle branches are empty — the leaf IS the root). Real deployments
// may merge-mine several aux chains under one parent (h>0); h=0 exercises
// exactly the same CheckAuxPow code paths with the simplest possible fixture.
struct AuxPowTestingSetup : public RegTestingSetup {
    // A fresh, otherwise-valid S256 block candidate. `marker` varies the
    // coinbase so repeated calls produce distinct block hashes — needed
    // because CBlockHeader::GetHash() never depends on the auxpow payload,
    // so two attempts with identical pure-header fields collide in the
    // block index even if their attached auxpow differs.
    std::shared_ptr<CBlock> AuxBlockCandidate(int marker)
    {
        BlockAssembler::Options options;
        options.coinbase_output_script = CScript{} << marker << OP_TRUE;
        // Regtest heights here are low (<=16): CScript() << nHeight alone
        // serializes to a single-byte OP_N push for BIP34, one byte short of
        // the consensus-required 2-byte-minimum coinbase scriptSig -- see
        // node/miner.cpp's include_dummy_extranonce option and its comment
        // ("...bad-cb-length") for why this padding is needed at low heights.
        options.include_dummy_extranonce = true;
        auto tmpl = BlockAssembler{m_node.chainman->ActiveChainstate(), m_node.mempool.get(), options}.CreateNewBlock();
        auto block = std::make_shared<CBlock>(tmpl->block);
        // The real createauxblock RPC builds its template via the higher-level
        // interfaces::Mining API (like submitblock/getblocktemplate), which
        // already bakes in the coinbase witness commitment. That interface
        // isn't wired up in this lightweight test harness, so — exactly like
        // validation_block_tests.cpp's own FinalizeBlock() does at this same
        // raw node::BlockAssembler level — it must be added explicitly here,
        // and hashMerkleRoot recomputed afterward since it changes the
        // coinbase's own txid.
        const CBlockIndex* prev_block{WITH_LOCK(::cs_main, return m_node.chainman->m_blockman.LookupBlockIndex(block->hashPrevBlock))};
        m_node.chainman->GenerateCoinbaseCommitment(*block, prev_block);
        block->hashMerkleRoot = BlockMerkleRoot(*block);
        // Mirrors createauxblock (rpc/mining.cpp): the bit must be set before
        // anyone captures GetHash(), since nVersion is a genuine pure-header
        // field and setting the bit later would change the hash out from
        // under an already-committed proof.
        block->nVersion |= VERSION_AUXPOW_BIT;
        return block;
    }

    // Mines (via trivial nonce search — regtest's powLimit is minimal
    // difficulty) a stand-in "parent chain" header meeting nBits, whose
    // single-transaction block consists of just `coinbaseTx`.
    CPureBlockHeader MineParentHeader(const CTransactionRef& coinbaseTx, unsigned int nBits)
    {
        return MineParentHeader(coinbaseTx->GetHash().ToUint256(), nBits);
    }

    // Same, for a parent block with the given transaction merkle root.
    CPureBlockHeader MineParentHeader(const uint256& merkleRoot, unsigned int nBits)
    {
        CPureBlockHeader header;
        header.nVersion = 1;
        header.hashPrevBlock = uint256(); // arbitrary — unused by CheckAuxPow
        header.hashMerkleRoot = merkleRoot;
        header.nTime = 1700000000;
        header.nBits = nBits;
        header.nNonce = 0;
        const Consensus::Params& params = Params().GetConsensus();
        while (!CheckProofOfWork(header.GetHash(), nBits, params)) {
            ++header.nNonce;
        }
        return header;
    }

    // Builds a raw scriptSig byte sequence carrying a well-formed
    // merge-mining tag: 0xfabe6d6d ++ chainMerkleRoot(32, reversed) ++ size(4 LE) ++ nonce(4 LE).
    CScript BuildTaggedScriptSig(const uint256& chainMerkleRoot, uint32_t merkleSize, uint32_t merkleNonce, bool includeTag = true)
    {
        std::vector<unsigned char> vch;
        vch.push_back(0x51); // arbitrary prefix byte (e.g. block-height-ish marker), irrelevant to the tag scan
        if (includeTag) {
            vch.insert(vch.end(), std::begin(MERGE_MINING_HEADER), std::end(MERGE_MINING_HEADER));
            vch.insert(vch.end(), std::make_reverse_iterator(chainMerkleRoot.end()), std::make_reverse_iterator(chainMerkleRoot.begin())); // byte-reversed, as in the hex form
            for (int i = 0; i < 4; i++) vch.push_back(static_cast<unsigned char>((merkleSize >> (8 * i)) & 0xff));
            for (int i = 0; i < 4; i++) vch.push_back(static_cast<unsigned char>((merkleNonce >> (8 * i)) & 0xff));
        }
        vch.push_back(0x52); // arbitrary suffix byte
        return CScript(vch.begin(), vch.end());
    }

    // A parent-chain transaction with the given scriptSig: a coinbase by
    // default, or an ordinary transaction spending `prevout`.
    CTransactionRef ParentTx(const CScript& scriptSig,
                             const COutPoint& prevout = COutPoint(Txid::FromUint256(uint256()), 0xffffffff))
    {
        CMutableTransaction tx;
        tx.vin.emplace_back(prevout, scriptSig, 0);
        tx.vout.emplace_back(0, CScript() << OP_TRUE);
        return MakeTransactionRef(std::move(tx));
    }

    // A fully valid CAuxPow proving `hashAuxBlock` against `nBits`, using a
    // freshly built single-tx parent chain with a correctly-formed tag.
    CAuxPow BuildValidAuxPow(const uint256& hashAuxBlock, unsigned int nBits, uint32_t merkleNonce = 0xdeadbeef)
    {
        CMutableTransaction coinbase;
        coinbase.vin.emplace_back(COutPoint(Txid::FromUint256(uint256()), 0xffffffff), CScript(), 0);
        coinbase.vout.emplace_back(0, CScript() << OP_TRUE);
        coinbase.vin[0].scriptSig = BuildTaggedScriptSig(hashAuxBlock, /*merkleSize=*/1, merkleNonce);
        CTransactionRef coinbaseTx = MakeTransactionRef(std::move(coinbase));

        CAuxPow auxpow;
        auxpow.coinbaseTx = coinbaseTx;
        auxpow.vMerkleBranch = {};
        auxpow.nIndex = 0;
        auxpow.vChainMerkleBranch = {};
        auxpow.nChainIndex = 0;
        auxpow.parentBlock = MineParentHeader(coinbaseTx, nBits);
        return auxpow;
    }
};

} // namespace auxpow_tests

namespace {
class DiagStateCatcher final : public CValidationInterface
{
public:
    uint256 hash;
    bool found{false};
    BlockValidationState state;
    explicit DiagStateCatcher(const uint256& hashIn) : hash(hashIn) {}
protected:
    void BlockChecked(const std::shared_ptr<const CBlock>& block, const BlockValidationState& stateIn) override
    {
        if (block->GetHash() != hash) return;
        found = true;
        state = stateIn;
    }
};
} // namespace

BOOST_FIXTURE_TEST_SUITE(auxpow_tests, auxpow_tests::AuxPowTestingSetup)

BOOST_AUTO_TEST_CASE(auxpow_valid_proof_accepted)
{
    // End-to-end: a real S256 block, carrying a real (freshly constructed)
    // auxpow proof, submitted through the actual node acceptance path
    // (ChainstateManager::ProcessNewBlock) — the same call submitauxblock
    // makes — is accepted and becomes the new tip.
    auto block = AuxBlockCandidate(1);
    const uint256 hashAuxBlock = block->GetHash();
    const Consensus::Params& params = Params().GetConsensus();

    auto auxpow = BuildValidAuxPow(hashAuxBlock, block->nBits);
    BlockValidationState checkState;
    BOOST_CHECK(auxpow.CheckAuxPow(hashAuxBlock, block->nBits, params.nAuxpowChainId, params, checkState));
    BOOST_CHECK(checkState.IsValid());

    block->auxpow = std::make_shared<CAuxPow>(auxpow);
    // AuxBlockCandidate already set VERSION_AUXPOW_BIT before hashAuxBlock was
    // captured above, so attaching the auxpow blob itself must not change the
    // hash (GetHash() is inherited unchanged from CPureBlockHeader).
    BOOST_CHECK_EQUAL(block->GetHash().ToString(), hashAuxBlock.ToString());

    bool new_block = false;
    auto sc = std::make_shared<DiagStateCatcher>(hashAuxBlock);
    CHECK_NONFATAL(m_node.chainman->m_options.signals)->RegisterSharedValidationInterface(sc);
    bool accepted = m_node.chainman->ProcessNewBlock(block, /*force_processing=*/true, /*min_pow_checked=*/true, &new_block);
    CHECK_NONFATAL(m_node.chainman->m_options.signals)->UnregisterSharedValidationInterface(sc);
    BOOST_TEST_MESSAGE("ProcessNewBlock accepted=" << accepted << " found=" << sc->found
                        << " reason=" << sc->state.GetRejectReason() << " debug=" << sc->state.GetDebugMessage());
    BOOST_CHECK(accepted);
    BOOST_CHECK(new_block);
    BOOST_CHECK_EQUAL(m_node.chainman->ActiveTip()->GetBlockHash().ToString(), hashAuxBlock.ToString());
}

BOOST_AUTO_TEST_CASE(auxpow_wrong_chain_index_rejected)
{
    // The historical Namecoin "index confusion" CVE case: a self-consistent
    // branch/index pair that doesn't match the deterministically expected
    // slot must be rejected, not merely trusted.
    auto block = AuxBlockCandidate(2);
    const uint256 hashAuxBlock = block->GetHash();
    const Consensus::Params& params = Params().GetConsensus();

    auto auxpow = BuildValidAuxPow(hashAuxBlock, block->nBits);
    auxpow.nChainIndex = 1; // only 0 is ever valid when the branch is empty (h=0)

    BlockValidationState state;
    BOOST_CHECK(!auxpow.CheckAuxPow(hashAuxBlock, block->nBits, params.nAuxpowChainId, params, state));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "auxpow-wrong-index");
}

BOOST_AUTO_TEST_CASE(auxpow_missing_tag_rejected)
{
    auto block = AuxBlockCandidate(3);
    const uint256 hashAuxBlock = block->GetHash();
    const Consensus::Params& params = Params().GetConsensus();

    CMutableTransaction coinbase;
    coinbase.vin.emplace_back(COutPoint(Txid::FromUint256(uint256()), 0xffffffff), CScript(), 0);
    coinbase.vout.emplace_back(0, CScript() << OP_TRUE);
    coinbase.vin[0].scriptSig = BuildTaggedScriptSig(hashAuxBlock, 1, 0xdeadbeef, /*includeTag=*/false);
    CTransactionRef coinbaseTx = MakeTransactionRef(std::move(coinbase));

    CAuxPow auxpow;
    auxpow.coinbaseTx = coinbaseTx;
    auxpow.nIndex = 0;
    auxpow.nChainIndex = 0;
    auxpow.parentBlock = MineParentHeader(coinbaseTx, block->nBits);

    BlockValidationState state;
    BOOST_CHECK(!auxpow.CheckAuxPow(hashAuxBlock, block->nBits, params.nAuxpowChainId, params, state));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "auxpow-no-merge-mining-tag");
}

BOOST_AUTO_TEST_CASE(auxpow_wrong_chain_merkle_root_rejected)
{
    // Tag commits to a different hash than the one actually being proven.
    auto block = AuxBlockCandidate(4);
    const uint256 hashAuxBlock = block->GetHash();
    const uint256 wrongHash = uint256{uint8_t{0x42}};
    const Consensus::Params& params = Params().GetConsensus();

    auto auxpow = BuildValidAuxPow(wrongHash, block->nBits); // tag commits to wrongHash, not hashAuxBlock

    BlockValidationState state;
    BOOST_CHECK(!auxpow.CheckAuxPow(hashAuxBlock, block->nBits, params.nAuxpowChainId, params, state));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "auxpow-chain-merkle-mismatch");
}

BOOST_AUTO_TEST_CASE(auxpow_insufficient_parent_pow_rejected)
{
    // The parent header genuinely satisfies an easy target, but is checked
    // here against a target far too strict for it to plausibly meet.
    auto block = AuxBlockCandidate(5);
    const uint256 hashAuxBlock = block->GetHash();
    const Consensus::Params& params = Params().GetConsensus();

    auto auxpow = BuildValidAuxPow(hashAuxBlock, block->nBits);

    unsigned int veryStrictBits = arith_uint256(1).GetCompact();

    BlockValidationState state;
    BOOST_CHECK(!auxpow.CheckAuxPow(hashAuxBlock, veryStrictBits, params.nAuxpowChainId, params, state));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "auxpow-high-hash");
}

BOOST_AUTO_TEST_CASE(auxpow_wrong_coinbase_merkle_branch_rejected)
{
    // parentBlock.hashMerkleRoot doesn't actually match the coinbase's own
    // hash (simulated here by lying about which block the coinbase mines
    // into, via a mismatched hashMerkleRoot on an otherwise-valid proof).
    auto block = AuxBlockCandidate(6);
    const uint256 hashAuxBlock = block->GetHash();
    const Consensus::Params& params = Params().GetConsensus();

    auto auxpow = BuildValidAuxPow(hashAuxBlock, block->nBits);
    auxpow.parentBlock.hashMerkleRoot = uint256{uint8_t{0x99}};
    // hashMerkleRoot participates in the parent header's own hash, so it must
    // be re-mined to keep meeting nBits under the corrupted root — otherwise
    // this would spuriously fail on auxpow-high-hash instead of the merkle
    // check this test actually targets.
    auxpow.parentBlock.nNonce = 0;
    while (!CheckProofOfWork(auxpow.parentBlock.GetHash(), block->nBits, params)) {
        ++auxpow.parentBlock.nNonce;
    }

    BlockValidationState state;
    BOOST_CHECK(!auxpow.CheckAuxPow(hashAuxBlock, block->nBits, params.nAuxpowChainId, params, state));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "auxpow-coinbase-merkle-mismatch");
}

BOOST_AUTO_TEST_CASE(auxpow_oversized_chain_branch_rejected)
{
    auto block = AuxBlockCandidate(7);
    const uint256 hashAuxBlock = block->GetHash();
    const Consensus::Params& params = Params().GetConsensus();

    auto auxpow = BuildValidAuxPow(hashAuxBlock, block->nBits);
    auxpow.vChainMerkleBranch.assign(MAX_CHAIN_MERKLE_BRANCH_LENGTH + 1, uint256());

    BlockValidationState state;
    BOOST_CHECK(!auxpow.CheckAuxPow(hashAuxBlock, block->nBits, params.nAuxpowChainId, params, state));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "auxpow-chain-merkle-branch-too-long");
}

BOOST_AUTO_TEST_CASE(auxpow_tagged_tx_not_coinbase_rejected)
{
    // The merge-mining tag in an ordinary transaction at parent position 1,
    // behind an untagged coinbase. With the proof's nIndex = 1 this used to be
    // accepted, letting anyone mine S256 blocks with another chain's work for
    // the price of a transaction fee.
    auto block = AuxBlockCandidate(10);
    const uint256 hashAuxBlock = block->GetHash();
    const Consensus::Params& params = Params().GetConsensus();

    const auto parentCoinbase = ParentTx(BuildTaggedScriptSig(uint256(), 1, 0, /*includeTag=*/false));
    const auto tagged = ParentTx(BuildTaggedScriptSig(hashAuxBlock, 1, 0),
                                 COutPoint(Txid::FromUint256(uint256{uint8_t{1}}), 3));
    CAuxPow auxpow;
    auxpow.coinbaseTx = tagged;
    auxpow.vMerkleBranch = {parentCoinbase->GetHash().ToUint256()};
    auxpow.nIndex = 1;
    auxpow.nChainIndex = 0;
    auxpow.parentBlock = MineParentHeader(Hash(parentCoinbase->GetHash().ToUint256(), tagged->GetHash().ToUint256()),
                                          block->nBits);

    BlockValidationState state;
    BOOST_CHECK(!auxpow.CheckAuxPow(hashAuxBlock, block->nBits, params.nAuxpowChainId, params, state));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "auxpow-coinbase-not-first");

    // Claiming position 0 instead does not connect to the parent's merkle root.
    auxpow.nIndex = 0;
    state = BlockValidationState{};
    BOOST_CHECK(!auxpow.CheckAuxPow(hashAuxBlock, block->nBits, params.nAuxpowChainId, params, state));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "auxpow-coinbase-merkle-mismatch");

    // End to end, as submitauxblock submits it.
    auxpow.nIndex = 1;
    block->auxpow = std::make_shared<CAuxPow>(auxpow);
    const uint256 tip_before = WITH_LOCK(::cs_main, return m_node.chainman->ActiveTip()->GetBlockHash());
    BOOST_CHECK(!m_node.chainman->ProcessNewBlock(block, /*force_processing=*/true, /*min_pow_checked=*/true, /*new_block=*/nullptr));
    BOOST_CHECK(WITH_LOCK(::cs_main, return m_node.chainman->ActiveTip()->GetBlockHash()) == tip_before);
}

BOOST_AUTO_TEST_CASE(auxpow_coinbase_with_merkle_branch_accepted)
{
    // The normal case for a real parent block: the tagged coinbase at
    // position 0 with a non-empty branch to the parent's merkle root.
    auto block = AuxBlockCandidate(11);
    const uint256 hashAuxBlock = block->GetHash();
    const Consensus::Params& params = Params().GetConsensus();

    const auto coinbase = ParentTx(BuildTaggedScriptSig(hashAuxBlock, 1, 0));
    const auto other = ParentTx(CScript() << OP_TRUE, COutPoint(Txid::FromUint256(uint256{uint8_t{1}}), 0));
    CAuxPow auxpow;
    auxpow.coinbaseTx = coinbase;
    auxpow.vMerkleBranch = {other->GetHash().ToUint256()};
    auxpow.nIndex = 0;
    auxpow.nChainIndex = 0;
    auxpow.parentBlock = MineParentHeader(Hash(coinbase->GetHash().ToUint256(), other->GetHash().ToUint256()),
                                          block->nBits);

    BlockValidationState state;
    BOOST_CHECK(auxpow.CheckAuxPow(hashAuxBlock, block->nBits, params.nAuxpowChainId, params, state));
    BOOST_CHECK(state.IsValid());
}

BOOST_AUTO_TEST_CASE(auxpow_one_parent_proves_one_s256_block)
{
    // Two competing S256 blocks committed in the same merge-mining tree
    // (size 4). Only the block at the slot expected for S256's chain ID and
    // the tag's nonce can use the parent's work.
    auto blockA = AuxBlockCandidate(12);
    auto blockB = AuxBlockCandidate(13);
    const uint256 hashA = blockA->GetHash();
    const uint256 hashB = blockB->GetHash();
    BOOST_REQUIRE(hashA != hashB);
    const Consensus::Params& params = Params().GetConsensus();

    constexpr uint32_t nonce{7};
    const int expected = CAuxPow::GetExpectedIndex(nonce, params.nAuxpowChainId, 2);
    for (int slotB = 0; slotB < 4; ++slotB) {
        if (slotB == expected) continue;
        std::vector<uint256> leaves(4, uint256{uint8_t{0x77}});
        leaves[expected] = hashA;
        leaves[slotB] = hashB;
        const uint256 n01 = Hash(leaves[0], leaves[1]);
        const uint256 n23 = Hash(leaves[2], leaves[3]);
        const auto branch = [&](int i) {
            return std::vector<uint256>{leaves[i ^ 1], i < 2 ? n23 : n01};
        };
        const auto coinbase = ParentTx(BuildTaggedScriptSig(Hash(n01, n23), 4, nonce));

        CAuxPow auxpow;
        auxpow.coinbaseTx = coinbase;
        auxpow.nIndex = 0;
        auxpow.parentBlock = MineParentHeader(coinbase, blockA->nBits);

        auxpow.vChainMerkleBranch = branch(slotB);
        auxpow.nChainIndex = slotB;
        BlockValidationState state;
        BOOST_CHECK(!auxpow.CheckAuxPow(hashB, blockB->nBits, params.nAuxpowChainId, params, state));
        BOOST_CHECK_EQUAL(state.GetRejectReason(), "auxpow-wrong-index");

        // B can't borrow A's slot either.
        auxpow.vChainMerkleBranch = branch(expected);
        auxpow.nChainIndex = expected;
        state = BlockValidationState{};
        BOOST_CHECK(!auxpow.CheckAuxPow(hashB, blockB->nBits, params.nAuxpowChainId, params, state));
        BOOST_CHECK_EQUAL(state.GetRejectReason(), "auxpow-chain-merkle-mismatch");

        state = BlockValidationState{};
        BOOST_CHECK(auxpow.CheckAuxPow(hashA, blockA->nBits, params.nAuxpowChainId, params, state));
    }
}

BOOST_AUTO_TEST_CASE(auxpow_duplicate_tag_rejected)
{
    // Two merge-mining tags in one parent coinbase, one per S256 block.
    // Neither may use the parent's work.
    auto blockA = AuxBlockCandidate(14);
    auto blockB = AuxBlockCandidate(15);
    const Consensus::Params& params = Params().GetConsensus();

    CScript scriptSig = BuildTaggedScriptSig(blockA->GetHash(), 1, 0);
    const CScript second = BuildTaggedScriptSig(blockB->GetHash(), 1, 0);
    scriptSig.insert(scriptSig.end(), second.begin(), second.end());
    const auto coinbase = ParentTx(scriptSig);

    CAuxPow auxpow;
    auxpow.coinbaseTx = coinbase;
    auxpow.nIndex = 0;
    auxpow.nChainIndex = 0;
    auxpow.parentBlock = MineParentHeader(coinbase, blockA->nBits);
    for (const auto& block : {blockA, blockB}) {
        BlockValidationState state;
        BOOST_CHECK(!auxpow.CheckAuxPow(block->GetHash(), block->nBits, params.nAuxpowChainId, params, state));
        BOOST_CHECK_EQUAL(state.GetRejectReason(), "auxpow-multiple-merge-mining-tags");
    }
}

BOOST_AUTO_TEST_CASE(auxpow_tag_root_byte_order)
{
    // The tag carries the root in the order of its hex form (Namecoin's and
    // Dogecoin's): for one aux chain, the block hash's GetHex() decoded as is.
    auto block = AuxBlockCandidate(16);
    const uint256 hashAuxBlock = block->GetHash();
    const Consensus::Params& params = Params().GetConsensus();

    const auto tagged = [&](const std::vector<unsigned char>& root) {
        std::vector<unsigned char> vch(std::begin(MERGE_MINING_HEADER), std::end(MERGE_MINING_HEADER));
        vch.insert(vch.end(), root.begin(), root.end());
        vch.insert(vch.end(), {1, 0, 0, 0, 0, 0, 0, 0}); // size 1, nonce 0
        const auto coinbase = ParentTx(CScript() << vch);
        CAuxPow auxpow;
        auxpow.coinbaseTx = coinbase;
        auxpow.nIndex = 0;
        auxpow.nChainIndex = 0;
        auxpow.parentBlock = MineParentHeader(coinbase, block->nBits);
        return auxpow;
    };

    BlockValidationState state;
    BOOST_CHECK(tagged(ParseHex(hashAuxBlock.GetHex())).CheckAuxPow(hashAuxBlock, block->nBits, params.nAuxpowChainId, params, state));
    BOOST_CHECK(state.IsValid());

    const std::vector<unsigned char> internal(hashAuxBlock.begin(), hashAuxBlock.end());
    state = BlockValidationState{};
    BOOST_CHECK(!tagged(internal).CheckAuxPow(hashAuxBlock, block->nBits, params.nAuxpowChainId, params, state));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "auxpow-chain-merkle-mismatch");
}

BOOST_AUTO_TEST_CASE(auxpow_header_hash_independent_of_auxpow)
{
    // The Phase-1 correctness fix this whole design depends on: GetHash()
    // must be identical whether or not the auxpow payload is attached, for
    // the same pure-header fields (nVersion, with VERSION_AUXPOW_BIT already
    // set, included), and must survive a serialize round-trip.
    auto block = AuxBlockCandidate(8);
    const uint256 hashBefore = block->GetHash();

    auto auxpow = BuildValidAuxPow(hashBefore, block->nBits);
    block->auxpow = std::make_shared<CAuxPow>(auxpow);

    BOOST_CHECK_EQUAL(block->GetHash().ToString(), hashBefore.ToString());

    DataStream stream;
    stream << TX_WITH_WITNESS(*block);
    CBlock roundTripped;
    stream >> TX_WITH_WITNESS(roundTripped);
    BOOST_CHECK_EQUAL(roundTripped.GetHash().ToString(), hashBefore.ToString());
    BOOST_CHECK(roundTripped.IsAuxpow());
    BOOST_CHECK(roundTripped.auxpow != nullptr);
}

BOOST_AUTO_TEST_CASE(auxpow_bit_without_auxpow_not_serializable)
{
    // What CBlockIndex::GetPureHeader() gives for a merge-mined block: the
    // auxpow bit but no auxpow. Serializing it must fail cleanly, not crash.
    CBlockHeader header;
    header.nVersion |= VERSION_AUXPOW_BIT;
    BOOST_REQUIRE(!header.auxpow);
    DataStream stream;
    BOOST_CHECK_THROW(stream << header, std::ios_base::failure);
    BOOST_CHECK_THROW(GetSerializeSize(header), std::ios_base::failure);

    // Without the bit nothing is missing.
    header.nVersion &= ~VERSION_AUXPOW_BIT;
    stream.clear();
    stream << header;
    BOOST_CHECK_EQUAL(stream.size(), 80U);
}

BOOST_AUTO_TEST_SUITE_END()
