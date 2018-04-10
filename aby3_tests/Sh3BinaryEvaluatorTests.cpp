#include "aby3/Engines/Lynx/LynxBinaryEngine.h"
#include "aby3/Engines/sh3/Sh3Encryptor.h"
#include "aby3/Engines/Sh3/Sh3BinaryEvaluator.h"
#include "aby3/Circuit/BetaLibrary.h"
#include "cryptoTools/Network/IOService.h"
#include "cryptoTools/Common/Log.h"
#include <random>
#include "cryptoTools/Crypto/PRNG.h"

using namespace oc;
using namespace aby3;
using namespace Sh3;

//
//void createBinarySharing(
//    PRNG& prng,
//    Eigen::Matrix<i64, Eigen::Dynamic, Eigen::Dynamic>& value,
//    Lynx::Engine::Matrix& s0,
//    Lynx::Engine::Matrix& s1,
//    Lynx::Engine::Matrix& s2)
//{
//    s0.mShares[0] = value;
//
//    s1.mShares[0].resizeLike(value);
//    s2.mShares[0].resizeLike(value);
//
//    //if (zeroshare) {
//    //	s1.mShares[0].setZero();
//    //	s2.mShares[0].setZero();
//    //}
//    //else 
//    {
//        prng.get(s1.mShares[0].data(), s1.mShares[0].size());
//        prng.get(s2.mShares[0].data(), s2.mShares[0].size());
//
//        for (u64 i = 0; i < s0.mShares[0].size(); ++i)
//            s0.mShares[0](i) ^= s1.mShares[0](i) ^ s2.mShares[0](i);
//    }
//
//    s0.mShares[1] = s2.mShares[0];
//    s1.mShares[1] = s0.mShares[0];
//    s2.mShares[1] = s1.mShares[0];
//}
Lynx::Matrix toLynx(const Sh3::sb64Matrix& A)
{
    Lynx::Matrix r(A.rows(), A.cols());
    r.mShares[0] = A.mShares[0];
    r.mShares[1] = A.mShares[1];
    return r;
}

void compare(Sh3BinaryEvaluator& a, Lynx::BinaryEngine& b)
{
    if (a.mMem.bitCount() != b.mMem.bitCount())
        throw std::runtime_error(LOCATION);

    if (a.mMem.shareCount() != b.mMem.shareCount())
        throw std::runtime_error(LOCATION);


    if (neq(a.hashState(), b.hashState()))
        ostreamLock(std::cout)
            << "Ha " << a.hashState() << std::endl
            << "Hb " << b.hashState() << std::endl;

    for (u64 s = 0; s < 2; ++s)
    {

        for (u64 i = 0; i < a.mMem.bitCount(); ++i)
        {
            for (u64 j = 0; j < a.mMem.simdWidth(); ++j)
            {
                if (a.mMem.mShares[s](i, j) != b.mMem.mShares[s](i, j))
                {
                    ostreamLock(std::cout) << " share " << s << " bit " << i << " word " << j << std::endl;
                }
            }
        }
    }

    if (a.mPlainWires_DEBUG.size() != b.mPlainWires_DEBUG.size())
        throw std::runtime_error(LOCATION);

    for (u64 s = 0; s < 3; ++s)
    {
        for (u64 i = 0; i < a.mPlainWires_DEBUG.size(); ++i)
        {

            if (a.mPlainWires_DEBUG[i].size() != b.mPlainWires_DEBUG[i].size())
                throw std::runtime_error(LOCATION);

            for (u64 j = 0; j < a.mPlainWires_DEBUG[i].size(); ++j)
            {
                if (a.mPlainWires_DEBUG[i][j].mBits[s] !=
                    b.mPlainWires_DEBUG[i][j].mBits[s])
                {
                    ostreamLock(std::cout) << "*share " << s << " bit " << i << " word " << j << std::endl;
                }
            }
        }
    }
}

i64 Sh3_BinaryEngine_test(BetaCircuit* cir, std::function<i64(i64, i64)> binOp)
{

    IOService ios;
    Session s01(ios, "127.0.0.1", SessionMode::Server, "01");
    Session s10(ios, "127.0.0.1", SessionMode::Client, "01");
    Session s02(ios, "127.0.0.1", SessionMode::Server, "02");
    Session s20(ios, "127.0.0.1", SessionMode::Client, "02");
    Session s12(ios, "127.0.0.1", SessionMode::Server, "12");
    Session s21(ios, "127.0.0.1", SessionMode::Client, "12");

    Channel chl01 = s01.addChannel("c");
    Channel chl10 = s10.addChannel("c");
    Channel chl02 = s02.addChannel("c");
    Channel chl20 = s20.addChannel("c");
    Channel chl12 = s12.addChannel("c");
    Channel chl21 = s21.addChannel("c");


    CommPkg comms[3], debugComm[3];
    comms[0] = { chl02, chl01 };
    comms[1] = { chl10, chl12 };
    comms[2] = { chl21, chl20 };

    debugComm[0] = { comms[0].mPrev.getSession().addChannel(), comms[0].mNext.getSession().addChannel() };
    debugComm[1] = { comms[1].mPrev.getSession().addChannel(), comms[1].mNext.getSession().addChannel() };
    debugComm[2] = { comms[2].mPrev.getSession().addChannel(), comms[2].mNext.getSession().addChannel() };

    cir->levelByAndDepth();
    u64 width = 100;
    bool failed = false;

    auto t0 = std::thread([&]() {
        auto i = 0;
        Sh3::i64Matrix a(width, 1), b(width, 1), c(width, 1);
        Sh3::i64Matrix aa(width, 1), bb(width, 1);

        Sh3Runtime rt(i, comms[i]);

        Sh3::sb64Matrix A(width, 1), B(width, 1), C(width, 1);

        Sh3Encryptor enc;
        //enc.init(i, comms[i], toBlock(i));
        enc.init(i, toBlock(i), toBlock(i + 1));
        enc.localBinMatrix(comms[i], a, A);
        enc.localBinMatrix(comms[i], b, B);

        enc.reveal(comms[i], A, aa);
        enc.reveal(comms[i], B, bb);

        if (a != aa || b != bb)
            throw std::runtime_error(LOCATION);

        //{
        //Lynx::BinaryEngine lEng(i, debugComm[i].mPrev, debugComm[i].mNext);
        //lEng.mDebug = true;
        //lEng.setCir(cir, width);
        //lEng.setInput(0, toLynx(A));
        //lEng.setInput(1, toLynx(B));
        //lEng.evaluate();
        //Lynx::Matrix lC(width, 1);
        //lEng.getOutput(0, lC);
        //}

        Sh3BinaryEvaluator eval;
        eval.asyncEvaluate(rt.noDependencies(), cir, { &A, &B }, { &C }).get();


        enc.reveal(comms[i], C, c);

        for (u64 j = 0; j < width; ++j)
        {
            if (c(j) != binOp(a(j), b(j)))
            {
                std::cout << " failed at " << j << " " << c(j) << " != " << binOp(a(j), b(j)) << std::endl;
                failed = true;
            }
        }
    });

    auto routine = [&](int i)
    {
        Sh3::i64Matrix a(width, 1), b(width, 1);

        Sh3Runtime rt(i, comms[i]);

        Sh3::sb64Matrix A(width, 1), B(width, 1), C(width, 1);

        Sh3Encryptor enc;
        enc.init(i, toBlock(i), toBlock((i + 1) % 3));
        enc.remoteBinMatrix(comms[i], A);
        enc.remoteBinMatrix(comms[i], B);

        enc.reveal(comms[i], A, 0);
        enc.reveal(comms[i], B, 0);

        //{
        //Lynx::BinaryEngine lEng(i, debugComm[i].mPrev, debugComm[i].mNext);
        //lEng.mDebug = true;
        //lEng.setCir(cir, width);
        //lEng.setInput(0, toLynx(A));
        //lEng.setInput(1, toLynx(B));
        //lEng.evaluate();
        //Lynx::Matrix lC(width, 1);
        //lEng.getOutput(0, lC);
        //}

        Sh3BinaryEvaluator eval;
        eval.asyncEvaluate(rt.noDependencies(), cir, { &A,&B }, { &C }).get();
        //eval.enableDebug(i);
        //eval.mDebugPrev = debugComm[i].mPrev;
        //eval.mDebugNext = debugComm[i].mNext;
        //eval.setCir(cir, width);
        //eval.setInput(0, A);
        //eval.setInput(1, B);

        //eval.asyncEvaluate(rt.noDependencies()).get();
        ////compare(eval, lEng);

        //eval.getOutput(0, C);

        enc.reveal(comms[i], C, 0);
    };

    auto t1 = std::thread(routine, 1);
    auto t2 = std::thread(routine, 2);

    t0.join();
    t1.join();
    t2.join();

    if (failed)
        throw std::runtime_error(LOCATION);
}

void Sh3_BinaryEngine_and_test()
{

    BetaLibrary lib;
    u64 size = 64, width = 1;

    // and
    {
        auto cir = lib.int_int_bitwiseAnd(size, size, size);
        cir->levelByAndDepth();

        Sh3_BinaryEngine_test(cir, [](i64 a, i64 b) {return a & b; });
    }

    // na_and
    {
        auto* cd = new BetaCircuit;

        BetaBundle a(size);
        BetaBundle b(size);
        BetaBundle c(size);

        cd->addInputBundle(a);
        cd->addInputBundle(b);

        cd->addOutputBundle(c);

        //int_int_bitwiseAnd_build(*cd, a, b, c);
        for (u64 j = 0; j < c.mWires.size(); ++j)
        {
            cd->addGate(
                a.mWires[j],
                b.mWires[j],
                GateType::na_And,
                c.mWires[j]);
        }


        Sh3_BinaryEngine_test(cd, [](i64 a, i64 b) {
            return ~a & b;
        });

    }

}


void Sh3_BinaryEngine_add_test()
{
    BetaLibrary lib;
    u64 size = 64, width = 1;
    auto cir = lib.int_int_add(size, size, size, BetaLibrary::Optimized::Depth);

    Sh3_BinaryEngine_test(cir, [](i64 a, i64 b) {return a + b; });

}


void Sh3_BinaryEngine_add_msb_test()
{
    BetaLibrary lib;
    u64 size = 64, width = 1;
    auto cir = lib.int_int_add_msb(size);

    Sh3_BinaryEngine_test(cir, [size](i64 a, i64 b) {return (a + b) >> (size - 1); });
}