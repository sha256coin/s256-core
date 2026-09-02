// Copyright (c) 2015-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <arith_uint256.h>
#include <chain.h>
#include <chainparams.h>
#include <pow.h>
#include <test/util/random.h>
#include <test/util/common.h>
#include <test/util/setup_common.h>
#include <uint256.h>
#include <util/chaintype.h>

#include <boost/test/unit_test.hpp>

#include <limits>
#include <memory>
#include <vector>

BOOST_FIXTURE_TEST_SUITE(pow_tests, BasicTestingSetup)

/* Test calculation of next difficulty target with no constraints applying */
BOOST_AUTO_TEST_CASE(get_next_work)
{
    const auto chainParams = CreateChainParams(*m_node.args, ChainType::MAIN);
    int64_t nLastRetargetTime = 1261130161; // Block #30240
    CBlockIndex pindexLast;
    pindexLast.nHeight = 32255;
    pindexLast.nTime = 1262152739;  // Block #32255
    pindexLast.nBits = 0x1d00ffff;

    // Here (and below): expected_nbits is calculated in
    // CalculateNextWorkRequired(); redoing the calculation here would be just
    // reimplementing the same code that is written in pow.cpp. Rather than
    // copy that code, we just hardcode the expected result.
    unsigned int expected_nbits = 0x1d00d86aU;
    BOOST_CHECK_EQUAL(CalculateNextWorkRequired(&pindexLast, nLastRetargetTime, chainParams->GetConsensus()), expected_nbits);
    BOOST_CHECK(PermittedDifficultyTransition(chainParams->GetConsensus(), pindexLast.nHeight+1, pindexLast.nBits, expected_nbits));
}

/* Test the constraint on the upper bound for next work */
BOOST_AUTO_TEST_CASE(get_next_work_pow_limit)
{
    const auto chainParams = CreateChainParams(*m_node.args, ChainType::MAIN);
    int64_t nLastRetargetTime = 1231006505; // Block #0
    CBlockIndex pindexLast;
    pindexLast.nHeight = 2015;
    pindexLast.nTime = 1233061996;  // Block #2015
    pindexLast.nBits = 0x1d00ffff;
    unsigned int expected_nbits = 0x1d00ffffU;
    BOOST_CHECK_EQUAL(CalculateNextWorkRequired(&pindexLast, nLastRetargetTime, chainParams->GetConsensus()), expected_nbits);
    BOOST_CHECK(PermittedDifficultyTransition(chainParams->GetConsensus(), pindexLast.nHeight+1, pindexLast.nBits, expected_nbits));
}

/* Test the constraint on the lower bound for actual time taken */
BOOST_AUTO_TEST_CASE(get_next_work_lower_limit_actual)
{
    // S256: use a local params copy with LwmaStartHeight pushed out of range
    // — this test's height (68543) is a real historical Bitcoin height used
    // purely as a classic-DAA math test vector, unrelated to S256's own
    // chain/activation height, and mainnet's real LwmaStartHeight (17500)
    // would otherwise make PermittedDifficultyTransition bypass the exact
    // check this test exists to verify. See PermittedDifficultyTransition's
    // comment in pow.cpp for why it can't validate LWMA transitions exactly.
    auto consensus = CreateChainParams(*m_node.args, ChainType::MAIN)->GetConsensus();
    consensus.LwmaStartHeight = std::numeric_limits<int>::max();
    int64_t nLastRetargetTime = 1279008237; // Block #66528
    CBlockIndex pindexLast;
    pindexLast.nHeight = 68543;
    pindexLast.nTime = 1279297671;  // Block #68543
    pindexLast.nBits = 0x1c05a3f4;
    unsigned int expected_nbits = 0x1c0168fdU;
    BOOST_CHECK_EQUAL(CalculateNextWorkRequired(&pindexLast, nLastRetargetTime, consensus), expected_nbits);
    BOOST_CHECK(PermittedDifficultyTransition(consensus, pindexLast.nHeight+1, pindexLast.nBits, expected_nbits));
    // Test that reducing nbits further would not be a PermittedDifficultyTransition.
    unsigned int invalid_nbits = expected_nbits-1;
    BOOST_CHECK(!PermittedDifficultyTransition(consensus, pindexLast.nHeight+1, pindexLast.nBits, invalid_nbits));
}

/* Test the constraint on the upper bound for actual time taken */
BOOST_AUTO_TEST_CASE(get_next_work_upper_limit_actual)
{
    // S256: see get_next_work_lower_limit_actual above for why LwmaStartHeight
    // is pushed out of range here.
    auto consensus = CreateChainParams(*m_node.args, ChainType::MAIN)->GetConsensus();
    consensus.LwmaStartHeight = std::numeric_limits<int>::max();
    int64_t nLastRetargetTime = 1263163443; // NOTE: Not an actual block time
    CBlockIndex pindexLast;
    pindexLast.nHeight = 46367;
    pindexLast.nTime = 1269211443;  // Block #46367
    pindexLast.nBits = 0x1c387f6f;
    unsigned int expected_nbits = 0x1d00e1fdU;
    BOOST_CHECK_EQUAL(CalculateNextWorkRequired(&pindexLast, nLastRetargetTime, consensus), expected_nbits);
    BOOST_CHECK(PermittedDifficultyTransition(consensus, pindexLast.nHeight+1, pindexLast.nBits, expected_nbits));
    // Test that increasing nbits further would not be a PermittedDifficultyTransition.
    unsigned int invalid_nbits = expected_nbits+1;
    BOOST_CHECK(!PermittedDifficultyTransition(consensus, pindexLast.nHeight+1, pindexLast.nBits, invalid_nbits));
}

/* S256: guard the AuxPoW/LWMA hard fork against the failure mode seen on a
 * comparable chain's ASERT-based fork, where the post-fork difficulty
 * algorithm was anchored wrong and the chain opened at powLimit instead of
 * the prevailing pre-fork difficulty. LWMA has no separate anchor value to
 * get wrong -- LwmaCalculateNextWorkRequired (pow.cpp) derives its very
 * first post-fork target directly from the real preceding N=96 blocks'
 * nBits/timestamps -- but nothing previously exercised that bootstrap at
 * mainnet's actual LwmaStartHeight with realistic (classic-DAA) history
 * leading into it: the other boundary tests above deliberately push
 * LwmaStartHeight out of range to test the classic math in isolation
 * instead. This test builds that history and pins down that the fork opens
 * near the prevailing difficulty, not at powLimit.
 */
BOOST_AUTO_TEST_CASE(get_next_work_lwma_activation_transition)
{
    const auto consensus = CreateChainParams(*m_node.args, ChainType::MAIN)->GetConsensus();
    const int64_t T = consensus.nPowTargetSpacing;
    const int activation_height = consensus.LwmaStartHeight;
    BOOST_REQUIRE(activation_height > 200); // needs room for >N blocks of pre-fork history

    // A "real" pre-fork difficulty, well above powLimit's minimum -- an
    // established chain's difficulty, not a fresh/minimum-difficulty one.
    arith_uint256 classic_target = UintToArith256(consensus.powLimit) / 1000;
    const unsigned int classic_nbits = classic_target.GetCompact();
    classic_target.SetCompact(classic_nbits); // round-trip: compact encoding loses precision

    // Synthetic classic-DAA history for the blocks immediately preceding
    // activation, all mined exactly on schedule (steady hashrate) at the
    // same difficulty -- classic DAA only retargets every 2016 blocks, so a
    // constant nBits across this window is the realistic case, and the
    // ordinary situation the fork will actually open into.
    std::vector<std::unique_ptr<CBlockIndex>> chain;
    const int history_len = 150;
    const int first_height = activation_height - history_len;
    int64_t t = 1700000000;
    CBlockIndex* tip = nullptr;
    for (int h = first_height; h < activation_height; ++h) {
        auto pindex = std::make_unique<CBlockIndex>();
        pindex->nHeight = h;
        pindex->nTime = static_cast<uint32_t>(t);
        pindex->nBits = classic_nbits;
        pindex->pprev = tip;
        tip = pindex.get();
        chain.push_back(std::move(pindex));
        t += T;
    }
    BOOST_REQUIRE_EQUAL(tip->nHeight, activation_height - 1);

    const unsigned int powlimit_compact = UintToArith256(consensus.powLimit).GetCompact();

    // The very first post-fork block: the exact call GetNextWorkRequired
    // makes once pindexLast->nHeight + 1 == activation_height. mainnet has
    // fPowAllowMinDifficultyBlocks=false, so the candidate block's own
    // timestamp never actually matters here (the min-difficulty-blocks
    // branch this feeds can't trigger) -- passed on schedule regardless,
    // for a candidate that looks like a real one.
    CBlockHeader candidate;
    candidate.nTime = static_cast<uint32_t>(t);
    unsigned int nbits = LwmaCalculateNextWorkRequired(tip, &candidate, consensus);
    BOOST_CHECK(nbits != powlimit_compact);
    {
        arith_uint256 target;
        target.SetCompact(nbits);
        // Under perfectly on-schedule solvetimes, LWMA's weighted average
        // reproduces the prevailing target almost exactly (mod small
        // compact-rounding error): it must land close to the pre-fork
        // difficulty, nowhere near powLimit.
        BOOST_CHECK(target < classic_target * 2);
        BOOST_CHECK(target > classic_target / 2);
    }

    // Keep mining on schedule for a while longer, past the point where the
    // classic-DAA history has fully rolled out of the N=96 window, and
    // check the difficulty stays put rather than drifting toward powLimit.
    for (int i = 0; i < 20; ++i) {
        auto pindex = std::make_unique<CBlockIndex>();
        pindex->nHeight = tip->nHeight + 1;
        pindex->nTime = static_cast<uint32_t>(t);
        pindex->nBits = nbits;
        pindex->pprev = tip;
        tip = pindex.get();
        chain.push_back(std::move(pindex));
        t += T;

        candidate.nTime = static_cast<uint32_t>(t);
        nbits = LwmaCalculateNextWorkRequired(tip, &candidate, consensus);
        BOOST_CHECK(nbits != powlimit_compact);
        arith_uint256 target;
        target.SetCompact(nbits);
        BOOST_CHECK(target < classic_target * 2);
        BOOST_CHECK(target > classic_target / 2);
    }
}

BOOST_AUTO_TEST_CASE(CheckProofOfWork_test_negative_target)
{
    const auto consensus = CreateChainParams(*m_node.args, ChainType::MAIN)->GetConsensus();
    uint256 hash;
    unsigned int nBits;
    nBits = UintToArith256(consensus.powLimit).GetCompact(true);
    hash = uint256{1};
    BOOST_CHECK(!CheckProofOfWork(hash, nBits, consensus));
}

BOOST_AUTO_TEST_CASE(CheckProofOfWork_test_overflow_target)
{
    const auto consensus = CreateChainParams(*m_node.args, ChainType::MAIN)->GetConsensus();
    uint256 hash;
    unsigned int nBits{~0x00800000U};
    hash = uint256{1};
    BOOST_CHECK(!CheckProofOfWork(hash, nBits, consensus));
}

BOOST_AUTO_TEST_CASE(CheckProofOfWork_test_too_easy_target)
{
    const auto consensus = CreateChainParams(*m_node.args, ChainType::MAIN)->GetConsensus();
    uint256 hash;
    unsigned int nBits;
    arith_uint256 nBits_arith = UintToArith256(consensus.powLimit);
    nBits_arith *= 2;
    nBits = nBits_arith.GetCompact();
    hash = uint256{1};
    BOOST_CHECK(!CheckProofOfWork(hash, nBits, consensus));
}

BOOST_AUTO_TEST_CASE(CheckProofOfWork_test_biger_hash_than_target)
{
    const auto consensus = CreateChainParams(*m_node.args, ChainType::MAIN)->GetConsensus();
    uint256 hash;
    unsigned int nBits;
    arith_uint256 hash_arith = UintToArith256(consensus.powLimit);
    nBits = hash_arith.GetCompact();
    hash_arith *= 2; // hash > nBits
    hash = ArithToUint256(hash_arith);
    BOOST_CHECK(!CheckProofOfWork(hash, nBits, consensus));
}

BOOST_AUTO_TEST_CASE(CheckProofOfWork_test_zero_target)
{
    const auto consensus = CreateChainParams(*m_node.args, ChainType::MAIN)->GetConsensus();
    uint256 hash;
    unsigned int nBits;
    arith_uint256 hash_arith{0};
    nBits = hash_arith.GetCompact();
    hash = ArithToUint256(hash_arith);
    BOOST_CHECK(!CheckProofOfWork(hash, nBits, consensus));
}

BOOST_AUTO_TEST_CASE(GetBlockProofEquivalentTime_test)
{
    const auto chainParams = CreateChainParams(*m_node.args, ChainType::MAIN);
    std::vector<CBlockIndex> blocks(10000);
    for (int i = 0; i < 10000; i++) {
        blocks[i].pprev = i ? &blocks[i - 1] : nullptr;
        blocks[i].nHeight = i;
        blocks[i].nTime = 1269211443 + i * chainParams->GetConsensus().nPowTargetSpacing;
        blocks[i].nBits = 0x207fffff; /* target 0x7fffff000... */
        blocks[i].nChainWork = i ? blocks[i - 1].nChainWork + GetBlockProof(blocks[i - 1]) : arith_uint256(0);
    }

    for (int j = 0; j < 1000; j++) {
        CBlockIndex *p1 = &blocks[m_rng.randrange(10000)];
        CBlockIndex *p2 = &blocks[m_rng.randrange(10000)];
        CBlockIndex *p3 = &blocks[m_rng.randrange(10000)];

        int64_t tdiff = GetBlockProofEquivalentTime(*p1, *p2, *p3, chainParams->GetConsensus());
        BOOST_CHECK_EQUAL(tdiff, p1->GetBlockTime() - p2->GetBlockTime());
    }
}

void sanity_check_chainparams(const ArgsManager& args, ChainType chain_type)
{
    const auto chainParams = CreateChainParams(args, chain_type);
    const auto consensus = chainParams->GetConsensus();

    // hash genesis is correct
    BOOST_CHECK_EQUAL(consensus.hashGenesisBlock, chainParams->GenesisBlock().GetHash());

    // target timespan is an even multiple of spacing
    BOOST_CHECK_EQUAL(consensus.nPowTargetTimespan % consensus.nPowTargetSpacing, 0);

    // genesis nBits is positive, doesn't overflow and is lower than powLimit
    arith_uint256 pow_compact;
    bool neg, over;
    pow_compact.SetCompact(chainParams->GenesisBlock().nBits, &neg, &over);
    BOOST_CHECK(!neg && pow_compact != 0);
    BOOST_CHECK(!over);
    BOOST_CHECK(UintToArith256(consensus.powLimit) >= pow_compact);

    // check max target * 4*nPowTargetTimespan doesn't overflow -- see pow.cpp:CalculateNextWorkRequired()
    if (!consensus.fPowNoRetargeting) {
        arith_uint256 targ_max{UintToArith256(uint256{"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"})};
        targ_max /= consensus.nPowTargetTimespan*4;
        BOOST_CHECK(UintToArith256(consensus.powLimit) < targ_max);
    }
}

BOOST_AUTO_TEST_CASE(ChainParams_MAIN_sanity)
{
    sanity_check_chainparams(*m_node.args, ChainType::MAIN);
}

BOOST_AUTO_TEST_CASE(ChainParams_REGTEST_sanity)
{
    sanity_check_chainparams(*m_node.args, ChainType::REGTEST);
}

BOOST_AUTO_TEST_CASE(ChainParams_TESTNET_sanity)
{
    sanity_check_chainparams(*m_node.args, ChainType::TESTNET);
}

BOOST_AUTO_TEST_CASE(ChainParams_TESTNET4_sanity)
{
    sanity_check_chainparams(*m_node.args, ChainType::TESTNET4);
}

BOOST_AUTO_TEST_CASE(ChainParams_SIGNET_sanity)
{
    sanity_check_chainparams(*m_node.args, ChainType::SIGNET);
}

BOOST_AUTO_TEST_SUITE_END()
