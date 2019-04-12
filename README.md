What is Qtum pow-pos-develop?
-------------

Qtum pows(Qtum pow-pos-develop) is a qtum amateurs' spontaneous project. It's aim to hard fork qtum, developing qtum's consensus mpos to pow+pos. The pow+pos consensus is porting from [DECRED](https://github.com/decred/dcrd). Using this consensus is aiming to develop QTUM performance and understand blockchain.
 **wahet is qtum or how to install QTUM**, click [this](https://github.com/qtumproject/qtum).
 
Know Qtum pows roadmap
-------------
 
At now qtum pows finish 
1. [stake api](https://github.com/901d/qtum-develop/tree/pow-pos-develop/src/stake), [stake api unit test](https://github.com/901d/qtum-develop/blob/pow-pos-develop/src/test/stake_tests.cpp) & [stake db api unit test](https://github.com/901d/qtum-develop/blob/pow-pos-develop/src/test/stakedb_tests.cpp). Stake is about how to manager ticket pruchase tx(sstx), vote block tx(ssgen) and ticket revoke tx(ssrtx), produce prng seed, produce winner tickets...
2. Create pruchase tx(sstx), vote block tx(ssgen) and ticket revoke tx(ssrtx) api...
3. Block struct add svtx member used to store sstx, ssgen & ssrx, wallet add stx class like normal tx class.
4. Test above function [unit test](https://github.com/901d/qtum-develop/blob/pow-pos-develop/src/wallet/test/wallet_stx_tests.cpp)
**how to run unit test**, click [this](https://github.com/901d/qtum-develop/tree/pow-pos-develop/src/test/README.md).