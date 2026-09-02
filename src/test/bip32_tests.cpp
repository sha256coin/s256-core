// Copyright (c) 2013-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <clientversion.h>
#include <key.h>
#include <key_io.h>
#include <streams.h>
#include <test/util/setup_common.h>
#include <util/strencodings.h>

#include <string>
#include <vector>

namespace {

struct TestDerivation {
    std::string pub;
    std::string prv;
    unsigned int nChild;
};

struct TestVector {
    std::string strHexMaster;
    std::vector<TestDerivation> vDerive;

    explicit TestVector(std::string strHexMasterIn) : strHexMaster(strHexMasterIn) {}

    TestVector& operator()(std::string pub, std::string prv, unsigned int nChild) {
        vDerive.emplace_back();
        TestDerivation &der = vDerive.back();
        der.pub = pub;
        der.prv = prv;
        der.nChild = nChild;
        return *this;
    }
};

TestVector test1 =
  TestVector("000102030405060708090a0b0c0d0e0f")
    ("xq5hkUvViHArUQTUrzVj9Hnzi8A65fPvEcKgkH5GjxfNnSssZ5hrSG7TSE4gpz7PnpDSWBUaK7r8oRPiFaWptwWCf9CaSxiJCwhLm1vgJZegrBR",
     "xq32pFw9Jmdxe2uBeWRG3GFzLzDMXdZRq9bqP49fwaFmE7LtgHNhtioCtRkCyiU6GvoNaeadpApzA83QT4KgbhfZ37CVfFz53RBjzuKwGHAuvG2",
     0x80000000)
    ("xq5hkXBuhzUKZfaU1bdSM3fdbAGCQUxGcgohRtWAi43Vke59vN12rVeYZNR6cFg6Y87yWPm9BMPZY4ToDyUxqyEaj2fJebWZ8FbZGx4EcEPowY2",
     "xq32pJCZJUwRjJ2Ao7YyF28dE2KTrT7nDE5r4faZufdtCJYB3ZftJxLJ1a6ckypRgeBLHhULBU9rYUtHApMQFHwjqBH1j8fDHNtdhuxnhNo3sNY",
     1)
    ("xq5hkZN2un2CxNTXr3ft5JZWBu9rTrwNciXzNDB1JKpJhGsN9uWAaJ4rWwKL2L8fx4qKU7PBxmqfeexGpHDnKJQUQgVEwoMQbtbpugb1uDZi3d7",
     "xq32pLNgWGVK7zuEdZbQyH2VpmD7uq6tDFp8zzFQVwQh8wLPH7B22kkby8zrB31VHZxNjnE1zHkHdBqhJksdNdjicLmTEucwM8fVNiHAEPntte9",
     0x80000002)
    ("xq5hkbyJxJr4fFJwiDTyKtxsYuBzh1ottG7yhkQgYf7Ts9EoZkhHP3uHRNsFaMKWPwGdpdUva9pH2bXFGcN52Px1U8e5XhKYgqfWdNW9zTV9v9C",
     "xq32pNyxYoKApskeVjPWDsRsBmFG8yyQUoQ8LXV5kGhrJohpgxN8qWb2saYmj5E7FoTsFFFQakhD3bfbXxcNxeeujSD7bXBVW4uRyz2NryR2Ptb",
     2)
    ("xq5hkeChnjy1qiyvngwh2AAxSQMJmq6dWrSB9EHyocdwBaoQvrKeSpbecxLBFgJtojRNHwc6iAJE5LRMq2i57hPDBj7LSUsjX2ZLe4R18zcPn1o",
     "xq32pRDMPES81MRdaCsDv8dx5GQaDoG97PiKn1NP1EEKdFGS43zVuHHQ5A1hQPdZiTsJK3P7CfvmxR7m19WqA5CFvpo9zmGb49j98xVYNRiPm7H",
     1000000000)
    ("xq5hkfvUGRaFxrN8K2j98hjgk7MJa4fwLMFCwUgY3u25PfBudyj2yErJt4VEtWDqezW5mdteDWE2cY7xRT76DYt42zGTK2HpKW8yT22vP9S7vPb",
     "xq32pSw7rv3N8Uoq6Yeg2gCgNyQa22qSvtXMaFkwFWcTqKevmBPtRhY4LGAm3FQmdNzNvSnMz2uioTet1KXCh9Cz3nShLG2pB4UrRJ3u7ZoS4mu",
     0);

TestVector test2 =
  TestVector("fffcf9f6f3f0edeae7e4e1dedbd8d5d2cfccc9c6c3c0bdbab7b4b1aeaba8a5a29f9c999693908d8a8784817e7b7875726f6c696663605d5a5754514e4b484542")
    ("xq5hkUvViHArUQTUrc14FSsHAKsrKrYLvEaWRrs12uQCkpzjr4j3cQtPs8EHB6CNW4aQQjiNDnWJCVtxyNF428RMo4CXJtum3SC15PbnaVC37Vm",
     "xq32pFw9Jmdxe2uBe7vb9RLGoBw7mphrWmrf4dwQEWzbCVTkyGPu4sa9KKuoKnF9TSMTphL4ZTHSSE5bYb3ne84FeUXG7g4KBm6Ua9xAdDvQV7y",
     0)
    ("xq5hkYCEyzroxRHZNsbMPEFAE8HNgpHbdD4nz2cNCiReqErhY3B2BgEU1xoXkZAGJ9aqd4CY61W3bajw8XfjuH7n8eroaweRwnLPCsiT1kkBYCH",
     "xq32pKCtaVKv83jGAPWtHCi9rzLe8nT7DkLwcogmQL23GuKifEqse8vDUAV3uHWx6WCJEMZtTThAsPViu6gcCQZJahgf7rUNaFGDhZrizVoWa4X",
     0xFFFFFFFF)
    ("xq5hkZMJEbtLLKTfYjoXogEohTxw5ZWWAhEV5Bgu87LRUyd5zLQUoZenE2b8MwQP3tBwQa85jrMetoAXoCY5gBbtroKS82GVJWbQ5Fz3rWcyYf4",
     "xq32pLMwq6MSVwuNLFj4hehoLL2CXXg1mEWdhxmJKivove677Y5LG2LXgEGeWdzvpkvn8atnEUtovPzqLLRNvnASA18DYahiLkzUVMCFktpci1R",
     1)
    ("xq5hkcAGecsGVxkreMUfsLPNfQuAR6P1Gt9kPdBkyAzknryXfAGe75BV7vyWCjbWqktDzkadaVStXvUMszo6b62r88nLiJULX42c12f16w6Ac9Q",
     "xq32pPAvF7LNfbCZRsQCmJrNJGxRs4YWsRRu2QGAAnb9EXSYnMwVZXsEa8f2MSCYrU8USYFUymyCKwg3f3CXyfgpAUwWgMEXBrxry9czEa1tcgg",
     0xFFFFFFFE)
    ("xq5hkdLJZaod7MiZwJiGbpCv2vbMKprNKrnFRKueVthAddEn7WbaCk7VeTLbqPj238QfxCEGNzWbaXoDXdxVGy9MqVUnzCnq5M81sFjadxLGrgy",
     "xq32pQLxA5GjGzAGipdoVnfufnecmo1svQ4Q46z3hWHZ5HhoEiGRfCoF6f27z8vwUtWkz7YBHbkBiKFdUAHuVcbJrKcgWHmqL4zVJjdWJKDpPfZ",
     2)
    ("xq5hkehLX228dY2ogZuAKtLc1fyKitPUaBhL5VXXug3fGR6NBhw9NP3Fq2HbunoJAhmNUFSVev21UEpD251z74qG9kLNdNcv1iQkYYvmJWw9dXB",
     "xq32pRhz7WVEoAUWU5phDrobeY2bArYzAiyUiGbw7He3i5ZPJubzpqj1HDy84Ybwdubq2RPuaMP1AR6KWiBRDvxXuJMQK9HLPV8D16y4NwuCXcb",
     0);

TestVector test3 =
  TestVector("4b381541583be4423346c643850da4b320e46a87ae3d2a4e6da11eba819cd4acba45d239319ac14f863b8d5ab5a0d0c64d2e8a1e7d1457df2e5a3c51c73235be")
    ("xq5hkUvViHArUQTUqfTDmqN5NK8tCVsSg6jn9CdZSCRm2X5XpJxKGZ8MpPTof5jMKfA3apUE3nPJbAipQfsxrqToDPrfS8oymzdqs77V74RjfV7",
     "xq32pFw9Jmdxe2uBdBNkfoq51BC9eU2xGe1vmyhxdp29UBYYwWdAj1p7Gb9KomyKQTanuPzp8NyYLepJuEf6sZCgRx2WdxcxPCSSrscXHDDhkTW",
      0x80000000)
    ("xq5hkXHhTEzz9mpq3CYczyhaRZtYtrUppx9gYgsWFS7bxpzu5L5mr3uDtrFWxoZs41knnBy4pk93S7vK38pJo5Pyne8c7EksKr8eMHUPmRSTmye",
     "xq32pJJM3jU6KQGXpiU9txAa4RwpLpeLRVRqBTwuT3hzQVTvCXkdJWayM3w37YKfkQuiMYeE9KmQzPSmYnJFSmnSBiE956Y18MiFytaBEXrQBhB",
      0);

TestVector test4 =
  TestVector("3ddd5602285899a946114506157c7997e5444528f3003f6134712147db19b678")
    ("xq5hkUvViHArUQTUsixncZitB4bmpjTcbM9XPLKxow7Lu3vDfCmeUTj82c1uMCz2M6k7AMbnCTcVXwVZDYf6H3UiJFiaCZ74LJbemhniBokMJc8",
     "xq32pFw9Jmdxe2uBfEtKWYBsovf3Ghd8BtRg27QN1YhjLiPEnQSVvvQsUohRVwFSRByyUrxSEZrgGz25YFHPXeTbWmgu9CUzS5L791Ug3GbQ7d2",
     0x80000000)
    ("xq5hkY5c6fHWrAZvWbyvEtefBThYGfLtJeg58dwd9uHKYVzC9jpm2o9rAehFWoFnNj7EaTN99W178sRokUM9ypw7PZmcEsY2D7h2pWLkzBt24dC",
     "xq32pK6Fh9kd1o1dJ7uT8s7epKkoidWPuBxDmR22MWshzATDGwVcVFqbcrNmfVAgU1ALSqysm92ubS8AUuUV928EXQZpzanfdzn78GWUnB5tj3V",
     0x80000001)
    ("xq5hkaDHkegXMtcXBkqZPAZZHLHPSbWxoaNsnZzSveoXCh5dvq9GuHdZPmf8Qk2KDujvAEyW3GwDbKmnixg4SRkG6rhyafP5ArAygvhf3pYScM6",
     "xq32pMDwM99dXX4DyGm6H92YvCLetZgUQ7f2RM4r8GPueMYf42p8MkKJqyLeZT8mXQCAXcNywjrsN5E3BsJCXx5Moo34EvVgk32Ryx4Asmj1SCP",
     0);

const std::vector<std::string> TEST5 = {
    "xpub661MyMwAqRbcEYS8w7XLSVeEsBXy79zSzH1J8vCdxAZningWLdN3zgtU6LBpB85b3D2yc8sfvZU521AAwdZafEz7mnzBBsz4wKY5fTtTQBm",
    "xprv9s21ZrQH143K24Mfq5zL5MhWK9hUhhGbd45hLXo2Pq2oqzMMo63oStZzFGTQQD3dC4H2D5GBj7vWvSQaaBv5cxi9gafk7NF3pnBju6dwKvH",
    "xpub661MyMwAqRbcEYS8w7XLSVeEsBXy79zSzH1J8vCdxAZningWLdN3zgtU6Txnt3siSujt9RCVYsx4qHZGc62TG4McvMGcAUjeuwZdduYEvFn",
    "xprv9s21ZrQH143K24Mfq5zL5MhWK9hUhhGbd45hLXo2Pq2oqzMMo63oStZzFGpWnsj83BHtEy5Zt8CcDr1UiRXuWCmTQLxEK9vbz5gPstX92JQ",
    "xpub661MyMwAqRbcEYS8w7XLSVeEsBXy79zSzH1J8vCdxAZningWLdN3zgtU6N8ZMMXctdiCjxTNq964yKkwrkBJJwpzZS4HS2fxvyYUA4q2Xe4",
    "xprv9s21ZrQH143K24Mfq5zL5MhWK9hUhhGbd45hLXo2Pq2oqzMMo63oStZzFAzHGBP2UuGCqWLTAPLcMtD9y5gkZ6Eq3Rjuahrv17fEQ3Qen6J",
    "xprv9s2SPatNQ9Vc6GTbVMFPFo7jsaZySyzk7L8n2uqKXJen3KUmvQNTuLh3fhZMBoG3G4ZW1N2kZuHEPY53qmbZzCHshoQnNf4GvELZfqTUrcv",
    "xpub661no6RGEX3uJkY4bNnPcw4URcQTrSibUZ4NqJEw5eBkv7ovTwgiT91XX27VbEXGENhYRCf7hyEbWrR3FewATdCEebj6znwMfQkhRYHRLpJ",
    "xprv9s21ZrQH4r4TsiLvyLXqM9P7k1K3EYhA1kkD6xuquB5i39AU8KF42acDyL3qsDbU9NmZn6MsGSUYZEsuoePmjzsB3eFKSUEh3Gu1N3cqVUN",
    "xpub661MyMwAuDcm6CRQ5N4qiHKrJ39Xe1R1NyfouMKTTWcguwVcfrZJaNvhpebzGerh7gucBvzEQWRugZDuDXjNDRmXzSZe4c7mnTK97pTvGS8",
    "DMwo58pR1QLEFihHiXPVykYB6fJmsTeHvyTp7hRThAtCX8CvYzgPcn8XnmdfHGMQzT7ayAmfo4z3gY5KfbrZWZ6St24UVf2Qgo6oujFktLHdHY4",
    "DMwo58pR1QLEFihHiXPVykYB6fJmsTeHvyTp7hRThAtCX8CvYzgPcn8XnmdfHPmHJiEDXkTiJTVV9rHEBUem2mwVbbNfvT2MTcAqj3nesx8uBf9",
    "xprv9s21ZrQH143K24Mfq5zL5MhWK9hUhhGbd45hLXo2Pq2oqzMMo63oStZzF93Y5wvzdUayhgkkFoicQZcP3y52uPPxFnfoLZB21Teqt1VvEHx",
    "xprv9s21ZrQH143K24Mfq5zL5MhWK9hUhhGbd45hLXo2Pq2oqzMMo63oStZzFAzHGBP2UuGCqWLTAPLcMtD5SDKr24z3aiUvKr9bJpdrcLg1y3G",
    "xpub661MyMwAqRbcEYS8w7XLSVeEsBXy79zSzH1J8vCdxAZningWLdN3zgtU6Q5JXayek4PRsn35jii4veMimro1xefsM58PgBMrvdYre8QyULY",
    "xprv9s21ZrQH143K3QTDL4LXw2F7HEK3wJUD2nW2nRk4stbPy6cq3jPPqjiChkVvvNKmPGJxWUtg6LnF5kejMRNNU3TGtRBeJgk33yuGBxrMPHL"
};

void RunTest(const TestVector& test)
{
    std::vector<std::byte> seed{ParseHex<std::byte>(test.strHexMaster)};
    CExtKey key;
    CExtPubKey pubkey;
    key.SetSeed(seed);
    pubkey = key.Neuter();
    for (const TestDerivation &derive : test.vDerive) {
        unsigned char data[74];
        key.Encode(data);
        pubkey.Encode(data);

        // Test private key
        BOOST_CHECK(EncodeExtKey(key) == derive.prv);
        BOOST_CHECK(DecodeExtKey(derive.prv) == key); //ensure a base58 decoded key also matches

        // Test public key
        BOOST_CHECK(EncodeExtPubKey(pubkey) == derive.pub);
        BOOST_CHECK(DecodeExtPubKey(derive.pub) == pubkey); //ensure a base58 decoded pubkey also matches

        // Derive new keys
        CExtKey keyNew;
        BOOST_CHECK(key.Derive(keyNew, derive.nChild));
        CExtPubKey pubkeyNew = keyNew.Neuter();
        if (!(derive.nChild & 0x80000000)) {
            // Compare with public derivation
            CExtPubKey pubkeyNew2;
            BOOST_CHECK(pubkey.Derive(pubkeyNew2, derive.nChild));
            BOOST_CHECK(pubkeyNew == pubkeyNew2);
        }
        key = keyNew;
        pubkey = pubkeyNew;
    }
}

}  // namespace

BOOST_FIXTURE_TEST_SUITE(bip32_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(bip32_test1) {
    RunTest(test1);
}

BOOST_AUTO_TEST_CASE(bip32_test2) {
    RunTest(test2);
}

BOOST_AUTO_TEST_CASE(bip32_test3) {
    RunTest(test3);
}

BOOST_AUTO_TEST_CASE(bip32_test4) {
    RunTest(test4);
}

BOOST_AUTO_TEST_CASE(bip32_test5) {
    for (const auto& str : TEST5) {
        auto dec_extkey = DecodeExtKey(str);
        auto dec_extpubkey = DecodeExtPubKey(str);
        BOOST_CHECK_MESSAGE(!dec_extkey.key.IsValid(), "Decoding '" + str + "' as xprv should fail");
        BOOST_CHECK_MESSAGE(!dec_extpubkey.pubkey.IsValid(), "Decoding '" + str + "' as xpub should fail");
    }
}

BOOST_AUTO_TEST_CASE(bip32_max_depth) {
    CExtKey key_parent{DecodeExtKey(test1.vDerive[0].prv)}, key_child;
    CExtPubKey pubkey_parent{DecodeExtPubKey(test1.vDerive[0].pub)}, pubkey_child;

    // We can derive up to the 255th depth..
    for (auto i = 0; i++ < 255;) {
        BOOST_CHECK(key_parent.Derive(key_child, 0));
        std::swap(key_parent, key_child);
        BOOST_CHECK(pubkey_parent.Derive(pubkey_child, 0));
        std::swap(pubkey_parent, pubkey_child);
    }

    // But trying to derive a non-existent 256th depth will fail!
    BOOST_CHECK(key_parent.nDepth == 255 && pubkey_parent.nDepth == 255);
    BOOST_CHECK(!key_parent.Derive(key_child, 0));
    BOOST_CHECK(!pubkey_parent.Derive(pubkey_child, 0));
}

BOOST_AUTO_TEST_SUITE_END()
