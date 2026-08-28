// Copyright (c) 2026 The S256 developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <auxpow.h>

#include <arith_uint256.h>
#include <chainparams.h>
#include <consensus/merkle.h>
#include <consensus/validation.h>
#include <node/miner.h>
#include <pow.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <streams.h>
#include <test/util/setup_common.h>
#include <util/check.h>
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
        CPureBlockHeader header;
        header.nVersion = 1;
        header.hashPrevBlock = uint256(); // arbitrary — unused by CheckAuxPow
        header.hashMerkleRoot = coinbaseTx->GetHash().ToUint256();
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
    // merge-mining tag: 0xfabe6d6d ++ chainMerkleRoot(32) ++ size(4 LE) ++ nonce(4 LE).
    CScript BuildTaggedScriptSig(const uint256& chainMerkleRoot, uint32_t merkleSize, uint32_t merkleNonce, bool includeTag = true)
    {
        std::vector<unsigned char> vch;
        vch.push_back(0x51); // arbitrary prefix byte (e.g. block-height-ish marker), irrelevant to the tag scan
        if (includeTag) {
            vch.insert(vch.end(), std::begin(MERGE_MINING_HEADER), std::end(MERGE_MINING_HEADER));
            vch.insert(vch.end(), chainMerkleRoot.begin(), chainMerkleRoot.end());
            for (int i = 0; i < 4; i++) vch.push_back(static_cast<unsigned char>((merkleSize >> (8 * i)) & 0xff));
            for (int i = 0; i < 4; i++) vch.push_back(static_cast<unsigned char>((merkleNonce >> (8 * i)) & 0xff));
        }
        vch.push_back(0x52); // arbitrary suffix byte
        return CScript(vch.begin(), vch.end());
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
    BOOST_CHECK_EQUAL(block->GetHash(), hashAuxBlock);

    bool new_block = false;
    auto sc = std::make_shared<DiagStateCatcher>(hashAuxBlock);
    CHECK_NONFATAL(m_node.chainman->m_options.signals)->RegisterSharedValidationInterface(sc);
    bool accepted = m_node.chainman->ProcessNewBlock(block, /*force_processing=*/true, /*min_pow_checked=*/true, &new_block);
    CHECK_NONFATAL(m_node.chainman->m_options.signals)->UnregisterSharedValidationInterface(sc);
    BOOST_TEST_MESSAGE("ProcessNewBlock accepted=" << accepted << " found=" << sc->found
                        << " reason=" << sc->state.GetRejectReason() << " debug=" << sc->state.GetDebugMessage());
    BOOST_CHECK(accepted);
    BOOST_CHECK(new_block);
    BOOST_CHECK_EQUAL(m_node.chainman->ActiveTip()->GetBlockHash(), hashAuxBlock);
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

    BOOST_CHECK_EQUAL(block->GetHash(), hashBefore);

    DataStream stream;
    stream << TX_WITH_WITNESS(*block);
    CBlock roundTripped;
    stream >> TX_WITH_WITNESS(roundTripped);
    BOOST_CHECK_EQUAL(roundTripped.GetHash(), hashBefore);
    BOOST_CHECK(roundTripped.IsAuxpow());
    BOOST_CHECK(roundTripped.auxpow != nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
