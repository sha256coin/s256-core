// Copyright (c) 2026-present The SHA256Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <chainparamsbase.h>
#include <common/args.h>
#include <consensus/params.h>
#include <test/util/setup_common.h>
#include <uint256.h>
#include <util/chaintype.h>

#include <string>
#include <vector>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(checkpoint_tests, BasicTestingSetup)

static const uint256 CHECKPOINT_HASH{"d465034b116df75304b9aea548d0706110b13ed7c7abe87f746abb35e0726416"};

static void SetupArgs(ArgsManager& args, const std::vector<std::string>& extra)
{
    SetupChainParamsBaseOptions(args);
    std::vector<const char*> argv{"testprog"};
    for (const auto& a : extra) argv.push_back(a.c_str());
    std::string error;
    BOOST_REQUIRE_MESSAGE(args.ParseParameters(argv.size(), argv.data(), error), error);
}

// Mainnet: the checkpoint, defaultAssumeValid and nMinimumChainWork all describe block 18,000,
// and the other networks have none.
BOOST_AUTO_TEST_CASE(mainnet_values_agree)
{
    const auto main_params = CChainParams::Main();
    const auto& checkpoints = main_params->Checkpoints();
    BOOST_REQUIRE_EQUAL(checkpoints.size(), 1U);
    BOOST_CHECK_EQUAL(checkpoints.begin()->first, 18000);
    BOOST_CHECK_EQUAL(checkpoints.begin()->second.ToString(), CHECKPOINT_HASH.ToString());
    BOOST_CHECK_EQUAL(main_params->GetConsensus().defaultAssumeValid.ToString(), CHECKPOINT_HASH.ToString());
    BOOST_CHECK_EQUAL(main_params->GetConsensus().nMinimumChainWork.ToString(),
                      "0000000000000000000000000000000000000000000001b0c0a03d9c1e5cbeaa");

    BOOST_CHECK(CChainParams::TestNet()->Checkpoints().empty());
    BOOST_CHECK(CChainParams::TestNet4()->Checkpoints().empty());
    BOOST_CHECK(CChainParams::SigNet({})->Checkpoints().empty());
    BOOST_CHECK(CChainParams::RegTest({})->Checkpoints().empty());
}

// -testcheckpoint works on regtest only: every other network refuses to start with it.
BOOST_AUTO_TEST_CASE(testcheckpoint_is_regtest_only)
{
    const std::string option{"-testcheckpoint=5:" + CHECKPOINT_HASH.ToString()};
    ArgsManager args;
    SetupArgs(args, {option});

    const auto regtest = CreateChainParams(args, ChainType::REGTEST);
    BOOST_REQUIRE_EQUAL(regtest->Checkpoints().size(), 1U);
    BOOST_CHECK_EQUAL(regtest->Checkpoints().at(5).ToString(), CHECKPOINT_HASH.ToString());

    for (const ChainType chain : {ChainType::MAIN, ChainType::TESTNET, ChainType::TESTNET4, ChainType::SIGNET}) {
        BOOST_CHECK_EXCEPTION(CreateChainParams(args, chain), std::runtime_error, [](const std::runtime_error& e) {
            return std::string{e.what()} == "-testcheckpoint is only available on regtest";
        });
    }
    // Without the option, mainnet still starts with its own checkpoint
    ArgsManager no_args;
    SetupArgs(no_args, {});
    BOOST_CHECK_EQUAL(CreateChainParams(no_args, ChainType::MAIN)->Checkpoints().size(), 1U);
}

BOOST_AUTO_TEST_CASE(testcheckpoint_format)
{
    for (const std::string& bad : std::vector<std::string>{"-testcheckpoint=5", "-testcheckpoint=x:" + CHECKPOINT_HASH.ToString(),
                                   "-testcheckpoint=0:" + CHECKPOINT_HASH.ToString(), "-testcheckpoint=5:abcd",
                                   "-testcheckpoint=-1:" + CHECKPOINT_HASH.ToString()}) {
        ArgsManager args;
        SetupArgs(args, {bad});
        BOOST_CHECK_EXCEPTION(CreateChainParams(args, ChainType::REGTEST), std::runtime_error, [](const std::runtime_error& e) {
            return std::string{e.what()}.starts_with("Invalid format (");
        });
    }
}

BOOST_AUTO_TEST_SUITE_END()
