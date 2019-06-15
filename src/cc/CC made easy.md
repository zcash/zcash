/******************************************************************************
 * Copyright © 2014-2019 The SuperNET Developers.                             *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * SuperNET software, including this file may be copied, modified, propagated *
 * or distributed except according to the terms contained in the LICENSE file *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/

How to write utxo based CryptoConditions contracts for KMD chains
by jl777

This is not the only smart contracts methodology that is possible to build on top of OP_CHECKCRYPTOCONDITION, just the first one. All the credit for getting OP_CHECKCRYPTOCONDITION working in the Komodo codebase goes to @libscott. I am just hooking into the code that he made and tried to make it just a little easier to make new contracts.

There is probably some fancy marketing name to use, but for now, I will just call it "CC contract" for short, knowing that it is not 100% technically accurate as the CryptoConditions aspect is not really the main attribute. However, the KMD contracts were built to make the CryptoConditions codebase that was integrated into it to be more accessible.

Since CC contracts run native C/C++ code, it is turing complete and that means that any contract that is possible to do on any other platform will be possible to create via CC contract.

utxo based contracts are a bit harder to start writing than for balance based contracts. However, they are much more secure as they leverage the existing bitcoin utxo system. That makes it much harder to have bugs that issue a zillion new coins from a bug, since all the CC contract operations needs to also obey the existing bitcoin utxo protocol.

This document will be heavily example based so it will utilize many of the existing reference CC contracts. After understanding this document, you should be in a good position to start creating either a new CC contract to be integrated into komodod or to make rpc based dapps directly.

Chapter 0 - Bitcoin Protocol Basics
There are many aspects of the bitcoin protocol that isnt needed to understand the CC contracts dependence on it. Such details will not be discussed. The primary aspect is the utxo, unspent transaction output. Just a fancy name for txid/vout, so when you sendtoaddress some coins, it creates a txid and the first output is vout.0, combine it and txid/0 is a specific utxo.

Of course, to understand even this level of detail requires that you understand what a txid is, but there are plenty of reference materials on that. It is basically the 64 char long set of letters and numbers that you get when you send funds.

Implicit with the utxo is that it prevents double spends. Once you spend a utxo, you cant spend it again. This is quite an important characteristic and while advanced readers will point out chain reorgs can allow a double spend, we will not confuse the issue with such details. The important thing is that given a blockchain at a specific height's blockhash, you can know if a txid/vout has been spent or not.

There are also the transactions that are in memory waiting to be mined, the mempool. And it is possible for the utxo to be spent by a tx in the mempool. However since it isnt confirmed yet, it is still unspent at the current height, even if we are pretty sure it will be spent in the next block.

A useful example is to think about a queue of people lined up to get into an event. They need to have a valid ticket and also to get into the queue. After some time passes, they get their ticket stamped and allowed into the event.

In the utxo case, the ticket is the spending transaction and the event is the confirmed blockchain. The queue is the mempool.


Chapter 1 - OP_CHECKCRYPTOCONDITION
In the prior chapter the utxo was explained. However, the specific mechanism used to send a payment was not explained. Contrary to what most people might think, on the blockchain there are not entries that say "pay X amount to address". Instead what exists is a bitcoin script that must be satisfied in order for the funds to be able to be spent.

Originally, there was the pay to pubkey script:
<pubkey> <checksig>

About as simple of a payment script that you can get. Basically the pubkey's signature is checked and if it is valid, you get to spend it. One problem satoshi realized was that with Quantum Computers such payment scripts are vulnerable! So, he made a way to have a cold address, ie. an address whose pubkey isnt known. At least it isnt known until it is spent, so it is only Quantum resistant prior to the first spend. This line of reasoning is why we have one time use addresses and a new change address for each transaction. Maybe in some ways, this is too forward thinking as it makes things a lot more confusing to use and easier to lose track of all the required private keys.

However, it is here to stay and its script is:
<hash the pubkey> <pubkey> <verify hash matches> <checksig>

With this, the blockchain has what maps to "pay to address", just that the address is actually a base58 encoded (prefix + pubkeyhash). Hey, if it wasnt complicated, it would be easy!

In order to spend a p2pkh (pay to pubkey hash) utxo, you need to divulge the pubkey in addition to having a valid signature. After the first spend from an address, its security is degraded to p2pk (pay to pubkey) as its pubkey is now known. The net result is that each reused address takes 25 extra bytes on the blockchain, and that is why for addresses that are expected to be reused, I just use the p2pk script.

Originally, bitcoin allowed any type of script opcodes to be used directly. The problem was some of them caused problems and satoshi decided to disable them and only allow standard forms of payments. Thus the p2pk and p2pkh became 99%+ of bitcoin transactions. However, going from having a fully scriptable language that can create countless payment scripts (and bugs!), to having just 2... well it was a "short term" limitation. It did last for some years but eventually a compromise p2sh script was allowed to be standard. This is a pay to script hash, so it can have a standard format as the normal p2pkh, but have infinitely more flexibility.

<hash the script> <script> <verify hash matches>

Wait, something is wrong! If it was just that, then anybody that found out what the required script (called redeemscript) was, they could just spend it. I forgot to say that the redeemscript is then used to determine if the payment can be spent or not. So you can have a normal p2pk or p2pkh redeemscript inside a p2sh script.

OK, I know that just got really confusing. Let us have a more clear example:

redeemscript <- pay to pubkey
p2sh becomes the hash of the redeem script + the compares

So to spend it, you need to divulge the redeemscript, which in turn requires you to divulge the pubkey. Put it all together and the p2sh mechanism verifies you not only had the correct redeemscript by comparing its hash, but that when the redeemscript is run, it is satisfied. In this case, that the pubkey's signature was valid.

If you are still following, there is some good news! OP_CHECKCRYPTOCONDITION scripts are actually simpler than p2sh scripts in some sense as there isnt this extra level of script inside a scripthash. @libscott implemented the addition of OP_CHECKCRYPTOCONDITION to the set of bitcoin opcodes and what it does is makes sure that a CryptoConditions script is properly signed.

Which gets us to the CryptoConditions specification, which is a monster of a IETF (Internet standards) draft and has hundred(s) of pages of specification. I am sure you are happy to know that you dont really need to know about it much at all! Just know that you can create all sorts of cryptoconditions and its binary encoding can be used in a bitcoin utxo. If the standard CC contracts dont have the power you need, it is always possible to expand on it. So far, most all the CC contracts only need the power of a 1of1 CC script, which is 1 signature combined with custom constraints. The realtime payment channels CC is the only one of the reference CC contracts so far that didnt fit into this model, it needed a 1of2 CC script.

The best part is that all these opcode level things are not needed at all. I just wanted to explain it for those that need to know all the details of everything.

Chapter 2 - CC contract basics
Each CC contract has an eval code, this is just an arbitrary number that is associated with a specific CC contract. The details about a specific CC contract are all determined by the validation logic, that is ultimately what implements a CC contract.

However, unlike the normal bitcoin payments, where it is validated with only information in the transaction, a CC contract has the power to do pretty much anything. It has full access to the blockchain and even the mempool, though using mempool information is inherently more risky and needs to be done carefully or for exclusions, rather than inclusions.

However, this is the CC contract basics chapter, so let us ignore mempool issues and deal with just the basics. Fundamentally there is no structure for OP_CHECKCRYPTOCONDITION serialized scripts, but if you are like me, you want to avoid having to read and understand a 1000 page IETF standard. What we really want to do is have a logical way to make a new contract and have it be able to be coded and debugged in an efficient way.

That means to just follow a known working template and only changing the things where the existing templates are not sufficient, ie. the core differentiator of your CC contract.

In the ~/komodo/src/cc/eval.h file all the eval codes are defined, currently:

#define FOREACH_EVAL(EVAL)             \
EVAL(EVAL_IMPORTPAYOUT, 0xe1)  \
EVAL(EVAL_IMPORTCOIN,   0xe2)  \
EVAL(EVAL_ASSETS,   0xe3)  \
EVAL(EVAL_FAUCET, 0xe4) \
EVAL(EVAL_REWARDS, 0xe5) \
EVAL(EVAL_DICE, 0xe6) \
EVAL(EVAL_FSM, 0xe7) \
EVAL(EVAL_AUCTION, 0xe8) \
EVAL(EVAL_LOTTO, 0xe9) \
EVAL(EVAL_HEIR, 0xea) \
EVAL(EVAL_CHANNELS, 0xeb) \
EVAL(EVAL_ORACLES, 0xec) \
EVAL(EVAL_PRICES, 0xed) \
EVAL(EVAL_PEGS, 0xee) \
EVAL(EVAL_TRIGGERS, 0xef) \
EVAL(EVAL_PAYMENTS, 0xf0) \
EVAL(EVAL_GATEWAYS, 0xf1)

Ultimately, we will probably end up with all 256 eval codes used, for now there is plenty of room. I imagined that similar to my coins repo, we can end up with a much larger than 256 number of CC contracts and you select the 256 that you want active for your blockchain. That does mean any specific chain will be limited to "only" having 256 contracts. Since there seems to be so few actually useful contracts so far, this limit seems to be sufficient. I am told that the evalcode can be of any length, but the current CC contracts assumes it is one byte.

The simplest CC script would be one that requires a signature from a pubkey along with a CC validation. This is the equivalent of the pay to pubkey bitcoin script and is what most of the initial CC contracts use. Only the channels one needed more than this and it will be explained in its chapter.

We end up with CC scripts of the form (evalcode) + (pubkey) + (other stuff), dont worry about the other stuff, it is automatically handled with some handy internal functions. The important thing to note is that each CC contract of this form needs a single pubkey and eval code and from that we get the CC script. Using the standard bitcoin's "hash and make an address from it" method, this means that the same pubkey will generate a different address for each different CC contract!

This is an important point, so I will say it in a different way. In bitcoin there used to be uncompressed pubkeys which had both the right and left half combined, into a giant 64 byte pubkey. But since you can derive one from the other, compressed pubkeys became the standard, that is why you have bitcoin pubkeys of 33 bytes instead of 65 bytes. There is a 02, 03 or 04 prefix, to mean odd or even or big pubkey. This means there are two different pubkeys for each privkey, the compressed and uncompressed. And in fact you can have two different bitcoin protocol addresses that are spendable by the same privkey. If you use some paper wallet generators, you might have noticed this.

CC contracts are like that, where each pubkey gets a different address for each evalcode. It is the same pubkey, just different address due to the actual script having a different evalcode, it ends up with a different hash and thus a different address. Now funds send to a specific CC address is only accessible by that CC contract and must follow the rules of that contract.

I also added another very useful feature where the convention is for each CC contract to have a special address that is known to all, including its private key. Before you panic about publishing the private key, remember that to spend a CC output, you need to properly sign it AND satisfy all the rules. By everyone having the privkey for the CC contract, everybody can do the "properly sign" part, but they still need to follow the rest of the rules.

From a user's perspective, there is the global CC address for a CC contract and some contracts also use the user pubkey's CC address. Having a pair of new addresses for each contract can get a bit confusing at first, but eventually we will get easy to use GUI that will make it all easy to use.


Chapter 3 - CC vins and vouts
You might want to review the bitcoin basics and other materials to refresh about how bitcoin outputs become inputs. It is a bit complicated, but ultimately it is about one specific amount of coins that are spent, once spent it is combined with the other coins that are also spent in that transaction and then various outputs are created.

vin0 + vin1 + vin2 -> vout0 + vout1

That is a 3 input, 2 output transaction. The value from the three inputs are combined and then split into vout0 and vout1, each of the vouts gets a spend script that must be satisfied to be able to be spent. Which means for all three of out vins, all the requirements (as specified in the output that created them) are satisfied.

Yes, I know this is a bit too complicated without a nice chart, so we will hope that a nice chart is added here:

[nice chart goes here]

Out of all the aspects of the CC contracts, the flexibility that different vins and vouts created was the biggest surprise. When I started writing the first of these a month ago, I had no idea the power inherent in the smart utxo contracts. I was just happy to have a way to lock funds and release them upon some specific conditions.

After the assets/tokens CC contract, I realized that it was just a tip of the iceberg. I knew it was Turing complete, but after all these years of restricted bitcoin script, to have the full power of any arbitrary algorithm, it was eye opening. Years of writing blockchain code and having really bad consequences with every bug naturally makes you gun shy about doing aggressive things at the consensus level. And that is the way it should be, if not very careful, some really bad things can and do happen. The foundation of building on top of the existing (well tested and reliable) utxo system is what makes the CC contracts less likely for the monster bugs. That being said, lack of validation can easily allow an improperly coded CC contract to have its funds drained.

The CC contract breaks out of the standard limitations of a bitcoin transaction. Already, what I wrote explains the reason, but it was not obvious even to me at first, so likely you might have missed it too. If you are wondering what on earth I am talking about, THAT is what I am talking about!

To recap, we have now a new standard bitcoin output type called a CC output. Further, there can be up to 256 different types of CC outputs active on any given blockchain. We also know that to spend any output, you need to satisfy its spending script, which in our case is the signature and whatever constraints the CC validation imposes. We also have the convention of a globally shared keypair, which gives us a general CC address that can have funds sent to it, along with a user pubkey specific CC address.

Let us go back to the 3+2 transaction example:

vin0 + vin1 + vin2 -> vout0 + vout1

Given the prior paragraph, try to imagine the possibilities the simple 3+2 transaction can be. Each vin could be a normal vin, from the global contract address, the user's CC address and the vouts can also have this range. Theoretically, there can be 257 * 257 * 257 * 257 * 257 forms of a 3+2 transaction!

In reality, we really dont want that much degrees of freedom as it will ensure a large degree of bugs! So we need to reduce things to a more manageable level where there are at most 3 types for each, and preferably just 1 type. That will make the job of validating it much simpler and simple is better as long as we dont sacrifice the power. We dont.

Ultimately the CC contract is all about how it constrains its inputs, but before it can constrain them, they need to be created as outputs. More about this in the CC validation chapter.

Chapter 4 - CC rpc extensions
Currently, CC contracts need to be integrated at the source level. This limits who is able to create and add new CC contracts, which at first is good, but eventually will be a too strict limitation. The runtime bindings chapter will touch on how to break out of the source based limitation, but there is another key interface level, the RPC.

By convention, each CC contract adds an associated set of rpc calls to the komodo-cli. This not only simplifies the creation of the CC contract transactions, it further will allow dapps to be created just via rpc calls. That will require there being enough foundational CC contracts already in place. As we find new usecases that cannot be implemented via rpc, then a new CC contract is made that can handle that (and more) and the power of the rpc level increases. This is a long term process.

The typical rpc calls that are added <CC>address, <CClist>, <CCinfo> return the various special CC addresses, the list of CC contract instances and info about each CC contract instance. Along with an rpc that creates a CC instance and of course the calls to invoke a CC instance.

The role of the rpc calls are to create properly signed rawtransactions that are ready for broadcasting. This then allows using only the rpc calls to not only invoke but to create a specific instance of a CC. The faucet contract is special in that it only has a single instance, so some of these rpc calls are skipped.

So, there is no MUSTHAVE rpc calls, just a sane convention to follow so it fits into the general pattern.

One thing that I forgot to describe was how to create a special CC address and even though this is not really an rpc issue, it is kind of separate from the core CC functions, so I will show how to do it here:

const char *FaucetCCaddr = "R9zHrofhRbub7ER77B7NrVch3A63R39GuC";
const char *FaucetNormaladdr = "RKQV4oYs4rvxAWx1J43VnT73rSTVtUeckk";
char FaucetCChexstr[67] = { "03682b255c40d0cde8faee381a1a50bbb89980ff24539cb8518e294d3a63cefe12" };
uint8_t FaucetCCpriv[32] = { 0xd4, 0x4f, 0xf2, 0x31, 0x71, 0x7d, 0x28, 0x02, 0x4b, 0xc7, 0xdd, 0x71, 0xa0, 0x39, 0xc4, 0xbe, 0x1a, 0xfe, 0xeb, 0xc2, 0x46, 0xda, 0x76, 0xf8, 0x07, 0x53, 0x3d, 0x96, 0xb4, 0xca, 0xa0, 0xe9 };

Above are the specifics for the faucet CC, but each one has the equivalent in CCcustom.cpp. At the bottom of the file is a big switch statement where these values are copied into an in memory data structure for each CC type. This allows all the CC codebase to access these special addresses in a standard way.

In order to get the above values, follow these steps:
A. use getnewaddress to get a new address and put that in the <CC>Normaladdr = ""; line
B. use validateaddress <newaddress from A> to get the pubkey, which is put into the <CC>hexstr[67] = ""; line
C. stop the daemon and start with -pubkey=<pubkey from B> and do a <CC>address rpc call. In the console you will get a printout of the hex for the privkey, assuming the if ( 0 ) in Myprivkey() is enabled (CCutils.cpp)
D. update the CCaddress and privkey and dont forget to change the -pubkey= parameter

The first rpc command to add is <CC>address and to do that, add a line to rpcserver.h and update the commands array in rpcserver.cpp

In the rpcwallet.cpp file you will find the actual rpc functions, find one of the <CC>address ones, copy paste, change the eval code to your eval code and customize the function. Oh, and dont forget to add an entry into eval.h

Now you have made your own CC contract, but it wont link as you still need to implement the actual functions of it. This will be covered in the following chapters.


Chapter 5 - CC validation
CC validation is what its all about, not the "hokey pokey"!

Each CC must have its own validation function and when the blockchain is validating a transaction, it will call the CC validation code. It is totally up to the CC validation whether to validate it or not.

Any set of rules that you can think of and implement can be part of the validation. Make sure that there is no ambiguity! Make sure that all transactions that should be rejected are in fact rejected.

Also, make sure any rpc calls that create a CC transaction dont create anything that doesnt validate.

Really, that is all that needs to be said about validation that is generic, as it is just a concept and gets a dedicated function to determine if a transaction is valid or not.

For most of the initial CC contracts, I made a function code for various functions of the CC contract and add that along with the creation txid. That enables the validation of the transactions much easier, as the required data is right there in the opreturn.

You do need to be careful not to cause a deadlock as the CC validation code is called while already locked in the main loop of the bitcoin protocol. As long as the provided CC contracts are used as models, you should keep out of deadlock troubles.


Chapter 6 - faucet example
Finally, we are ready for the first actual example of a CC contract. The faucet. This is a very simple contract and it ran into some interesting bugs in the first incarnation.

The code in ~/komodo/src/cc/faucet.cpp is the ultimate documentation for it with all the details, so I will just address the conceptual issues here.

The idea is that people send funds to the faucet by locking it in faucet's global CC address and anybody is allowed to create a faucetget transaction that spends it.

There are only 7 functions in faucet.cpp, a bit over 200 lines including comments. The first three are for validation, the last four for the rpc calls to use.

int64_t IsFaucetvout(struct CCcontract_info *cp,const CTransaction& tx,int32_t v)

bool FaucetExactAmounts(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx,int32_t minage,uint64_t txfee)

bool FaucetValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx)

int64_t AddFaucetInputs(struct CCcontract_infoCC_info *cp,CMutableTransaction &mtx,CPubKey pk,int64_t total,int32_t maxinputs)

std::string FaucetGet(uint64_t txfee)

std::string FaucetFund(uint64_t txfee,int64_t funds)

UniValue FaucetInfo()

Functions in rpcwallet implement:

faucetaddress fully implemented in rpcwallet.cpp
faucetfund calls FaucetFund
faucetget calls FaucetGet
faucetinfo calls FaucetInfo

Now you might not be a programmer, but I hope you are able to understand the above sequence. user types in a cli call, komodo-cli processes it by calling the rpc function, which in turn calls the function inside faucet.cpp

No magic, just simple conversion of a user command line call that runs code inside the komodod. Both the faucetfund and faucetget create properly signed rawtransaction that is ready to be broadcast to the network using the standard sendrawtransaction rpc. It doesnt automatically do this to allow the GUI to have a confirmation step with all the details before doing an irrevocable CC contract transaction.

faucetfund allows anybody to add funds to the faucet
faucetget allows anybody to get 0.1 coins from the faucet as long as they dont violate the rules.

And we come to what it is all about. The rules of the faucet. Initially it was much less strict and that allowed it to be drained slowly, but automatically and it prevented most from being able to use the faucet.

To make it much harder to leech, it was made so each faucetget returned only 0.1 coins (down from 1.0) so it was worth 90% less. It was also made so that it had to be to a fresh address with less than 3 transactions. Finally each txid was constrained to start and end with 00! This is a cool trick to force usage of precious CPU time (20 to 60 seconds depending on system) to generate a valid txid. Like PoW mining for the txid and I expect other CC contracts to use a similar mechanism if they want to rate limit usage.

Combined, it became such a pain to get 0.1 coins, the faucet leeching problem was solved. It might not seem like too much trouble to change an address to get another 0.1 coins, but the way things are setup you need to launch the komodod -pubkey=<your pubkey> to change the pubkey that is active for a node. That means to change the pubkey being used, the komodod needs to be restarted and this creates a lot of issues for any automation trying to do this. Combined with the PoW required, only when 0.1 coins becomes worth a significant effort will faucet leeching return. In that case, the PoW requirement can be increased and coin amount decreased, likely with a faucet2 CC contract as I dont expect many such variations to be needed.

Chapter 7 - rewards example
The next CC contract in complexity is the rewards CC contract. This is designed to capture what most people like about masternodes, without anything else, ie. the rewards!

The idea is to allow people to lock funds for some amount of time and get an extra reward. We also want to support having more than one rewards plan at a time and to allow customization of plan details. One twist that makes it a bit unexpected is that anybody should be able to unlock the funds that were locked, as long as it ends up in the locking address. The reason for this is that SPV servers want to be supported and while locking can be done via normal sendrawtransaction, it requires a native node to do the unlocking. By allowing anybody to be able to unlock, then there can be a special node that unlocks all locked funds when they are ready. This way, from the user's point of view, they lock the funds and after it is matured, it reappears in their wallet.

The above requirements leads us to using the global CC address for the rewards contract to lock the funds in. That allows anybody to properly sign the unlock, but of course that is not enough, we need to make sure they are following all the unlock requirements. Primarily that the funds go back to the locking address.

The four aspects of the rewards plan that are customizable are:
APR, minseconds, maxseconds, mindeposit

This allows each plan to set a different APR (up to 25%, anything above is becoming silly), the minimum time funds must be locked, the maximum time they are earning rewards and the minimum that can be deposited.

So the tx that creates the rewards plan will have these attributes and it is put into the OP_RETURN data. All the other calls will reference the plan creation txid and inherit these parameters from the creation tx. This means it is an important validation to do, to make sure the funding txid is a valid funding txid.

Since it is possible that the initial funding will be used up, there needs to be a way for more funding to be added to the rewards plan.

Having multiple possible rewards plans means it is useful to have rpc calls to get information about them. Hence: rewardslist returns the list of rewards creation txids and rewardsinfo <txid> returns the details about a specific rewards plan.

A locking transaction sends funds to the rewards CC address, along with a normal (small) tx to the address that the unlock should go to. This allows the validation of the proper unlocking. Also, it is important to make sure only locking transactions are able to be unlocked. Additionally, the minimum time needs to elapse before unlocking is allowed.

All of these things are done in rewards.cpp, with the validation code being about 200 lines and a total of 700 lines or so. Bigger than faucet, but most of the code is the non-consensus code to create the proper transactions. In order to simplify the validation, specific vin and vout positions are designated to have specific required values:

createfunding
vins.*: normal inputs
vout.0: CC vout for funding
vout.1: normal marker vout for easy searching
vout.2: normal change
vout.n-1: opreturn 'F' sbits APR minseconds maxseconds mindeposit

addfunding
vins.*: normal inputs
vout.0: CC vout for funding
vout.1: normal change
vout.n-1: opreturn 'A' sbits fundingtxid

lock
vins.*: normal inputs
vout.0: CC vout for locked funds
vout.1: normal output to unlock address
vout.2: change
vout.n-1: opreturn 'L' sbits fundingtxid

unlock
vin.0: locked funds CC vout.0 from lock
vin.1+: funding CC vout.0 from 'F' and 'A' and 'U'
vout.0: funding CC change
vout.1: normal output to unlock address
vout.n-1: opreturn 'U' sbits fundingtxid

It is recommended to create such a vin/vout allocation for each CC contract to make sure that the rpc calls that create the transaction and the validation code have a specific set of constraints that can be checked for.

Chapter 8 - assets example
In some respects the assets CC is the most complex, it was actually the first one that I coded. It is however using a simple model, even for the DEX functions, so while it is quite involved, it does not have the challenge/response complexity of dice.

There are two major aspects to creating tokens. First is to create and track it, down to every specific satoshi. The second is solving how to implement DEX functions of trading assets.

The model used is "colored coins". This means that the token creating txid issues the assets as denoted by all the satoshis, so locking 1 COIN issues 100 million tokens. This multiplication will allow creation of plenty of assets. We want to preserve all the tokens created across all allowed operations. The way this is achieved is that all operations attaches the token creation txid in its OP_RETURN, along with the specified operation.

Ownership of tokens are represented by the colored satoshis in the CC address for the user's pubkey. This allows using the standard utxo system to automatically track ownership of the tokens. This automatic inheritance is one of the big advantages of utxo CC contracts that compensates for the slightly more work needed to implement a CC contract.

So now we have the standard CC addresss, list and info commands that provide the CC addresses, list of all tokens and info on specific tokens and the ability to create and transfer tokens. Any amount of tokens can be created from 1 to very large numbers and using standard addressbalance, addressutxo type of commands, the details of all assets owned can be determined for a specific pubkey.

Now we can solve the DEX part of the tokenization, which turns out to be much simpler than initially imagined. We start with bidding for a specific token. Funds for the bid are locked into the global CC address, along with the desired token and price. This creates a bid utxo that is able to be listed via an orderbook rpc call. To fill the bid, a specific bid utxo is spent with the appropriate number of assets and change and updated price for the unfilled amount. if the entire amount is filled, then it wont appear in the orderbook anymore.

asks work by locking assets along with the required price. Partial fills can be supported and the rpc calls can mask the utxo-ness of the funds/assets needed by automatically gathering the required amount of funds to fill the specific amount.

With calls to cancel the pending bid or ask, we get a complete set of rpc calls that can support a COIN-centric DEX.

In the future, it is expected that a token swap rpc can be supported to allow directly swapping one token for another, but at first it is expected that there wont be sufficient volumes for such token to token swaps, so it was left out of the initial implementation.

With just these rpc calls and associated validation, we get the ability to issue tokens and trade them on a DEX!

create
vin.0: normal input
vout.0: issuance assetoshis to CC
vout.1: tag sent to normal address of AssetsCCaddress
vout.2: normal output for change (if any)
vout.n-1: opreturn [EVAL_ASSETS] ['c'] [origpubkey] "<assetname>" "<description>"

transfer
vin.0: normal input
vin.1 .. vin.n-1: valid CC outputs
vout.0 to n-2: assetoshis output to CC
vout.n-2: normal output for change (if any)
vout.n-1: opreturn [EVAL_ASSETS] ['t'] [assetid]

buyoffer:
vins.*: normal inputs (bid + change)
vout.0: amount of bid to unspendable
vout.1: normal output for change (if any)
vout.n-1: opreturn [EVAL_ASSETS] ['b'] [assetid] [amount of asset required] [origpubkey]

cancelbuy:
vin.0: normal input
vin.1: unspendable.(vout.0 from buyoffer) buyTx.vout[0]
vout.0: vin.1 value to original pubkey buyTx.vout[0].nValue -> [origpubkey]
vout.1: normal output for change (if any)
vout.n-1: opreturn [EVAL_ASSETS] ['o'] [assetid]

fillbuy:
vin.0: normal input
vin.1: unspendable.(vout.0 from buyoffer) buyTx.vout[0]
vin.2+: valid CC output satisfies buyoffer (*tx.vin[2])->nValue
vout.0: remaining amount of bid to unspendable
vout.1: vin.1 value to signer of vin.2
vout.2: vin.2 assetoshis to original pubkey
vout.3: CC output for assetoshis change (if any)
vout.4: normal output for change (if any)
vout.n-1: opreturn [EVAL_ASSETS] ['B'] [assetid] [remaining asset required] [origpubkey]

selloffer:
vin.0: normal input
vin.1+: valid CC output for sale
vout.0: vin.1 assetoshis output to CC to unspendable
vout.1: CC output for change (if any)
vout.2: normal output for change (if any)
vout.n-1: opreturn [EVAL_ASSETS] ['s'] [assetid] [amount of native coin required] [origpubkey]

cancel:
vin.0: normal input
vin.1: unspendable.(vout.0 from exchange or selloffer) sellTx/exchangeTx.vout[0] inputTx
vout.0: vin.1 assetoshis to original pubkey CC sellTx/exchangeTx.vout[0].nValue -> [origpubkey]
vout.1: normal output for change (if any)
vout.n-1: opreturn [EVAL_ASSETS] ['x'] [assetid]

fillsell:
vin.0: normal input
vin.1: unspendable.(vout.0 assetoshis from selloffer) sellTx.vout[0]
vin.2+: normal output that satisfies selloffer (*tx.vin[2])->nValue
vout.0: remaining assetoshis -> unspendable
vout.1: vin.1 assetoshis to signer of vin.2 sellTx.vout[0].nValue -> any
vout.2: vin.2 value to original pubkey [origpubkey]
vout.3: CC asset for change (if any)
vout.4: CC asset2 for change (if any) 'E' only
vout.5: normal output for change (if any)
vout.n-1: opreturn [EVAL_ASSETS] ['S'] [assetid] [amount of coin still required] [origpubkey]

Chapter 9 - dice example
The dice CC contract is actually more complex in the sequences required than the assets/tokens CC. The reason is the need for realtime response by the dealer node, but also having a way to resolve bets if the dealer node is not online. The dice CC contract shows how to build in such a challenge/response mechanism, which likely will be very useful for many other realtime interactive CC contracts.

First, let us describe the issues that the dice CC contract needs to solve. Foremost is that it needs to be random and fair. It should also have realtime response and a fallback timeout in case the realtime response doesnt happen. As with the rewards CC contract, multiple dice plans are supported. Each plan can be customized as to the following:  minbet, maxbet, maxodds, timeoutblocks

This allows each plan to control the risk exposure and also advertises to everyone when dicebets expire and a timeout win can be claimed. In event the dealer node does not process a dicebet in time, in order to prevent dealer nodes from simply not responding to dicebets that they lose, a timeout must go to the dicebet player. A short timeframe means that the dealer would need to be running multiple redundant nodes to make sure they can respond in time. If the timeout is set to long, then many players would prefer to use a different dice plan with a shorter timeout.

Now to describe how to ensure a proper random number that is fair. The method chosen was for the dealer node to create transactions with hash of their entropy in the OP_RETURN. Then the dicebet player would select a specific entropy tx and include their (unhashed) entropy to their OP_RETURN. This allows the dealer node to immediately determine if the dicebet won or lost. If the dicebet included the hash of the bettor entropy, then another step would be needed. However, doing so would allow some timeouts to end with a refund, rather than an automatic win for the dicebet player.

One additional technique used to keep all required data on the blockchain is the dealer entropy value calculation. The vin0 txid is used as one of the privkeys to calculate a shared secret and then hashed to remove links to the original privkey. This method allows recreating the dealer's entropy value (by the dealer node) given the blockchain itself, which means there is no need for any local storage.

This allows the dealer node to recreate the unhashed entropy value used and so when the dicebet transaction is seen (in the mempool!), the dealer node can immediately determine if it is a winner or a loser. This is done by creating a dealer hash vs. a bettor hash via:

dealer hash: SHA256(dealer entropy + bettor entropy)
bettor hash: SHA256(bettor entropy + dealer entropy)

The same values are used, but in different order. The resulting hashes are compared arithmetically for 1:1 bets and the standard industry use is used for the higher odds: https://dicesites.com/provably-fair

The dealer creates a dice plan and then also needs to create entropy transactions. Each win or loss that creates change also creates entropy transactions by the dealer, but timeout transactions wont as it needs to be created by the dealer node to prevent cheating. The dealer tx are locked into the global dice CC address, as is the dicebet transaction, which selects a specific entropy tx to "roll" against. Then the dicefinish process by the dealer will spend the dicebet outputs either all to itself for a loss, or the winning amount to th dice bettor's address. For dicebets that are not dicefinish'ed by the dealer, anybody is able to do a timeout completion.

createfunding:
vins.*: normal inputs
vout.0: CC vout for funding
vout.1: owner vout
vout.2: dice marker address vout for easy searching
vout.3: normal change
vout.n-1: opreturn 'F' sbits minbet maxbet maxodds timeoutblocks

addfunding (entropy):
vins.*: normal inputs
vout.0: CC vout for locked entropy funds
vout.1: tag to owner address for entropy funds
vout.2: normal change
vout.n-1: opreturn 'E' sbits fundingtxid hentropy

bet:
vin.0: entropy txid from house (must validate vin0 of 'E')
vins.1+: normal inputs
vout.0: CC vout for locked entropy
vout.1: CC vout for locked bet
vout.2: tag for bettor's address (txfee + odds)
vout.3: change
vout.n-1: opreturn 'B' sbits fundingtxid entropy

loser:
vin.0: normal input
vin.1: betTx CC vout.0 entropy from bet
vin.2: betTx CC vout.1 bet amount from bet
vin.3+: funding CC vout.0 from 'F', 'E', 'W', 'L' or 'T'
vout.0: funding CC to entropy owner
vout.1: tag to owner address for entropy funds
vout.2: change to fundingpk
vout.n-1: opreturn 'L' sbits fundingtxid hentropy proof

winner:
same as loser, but vout.2 is winnings
vout.3: change to fundingpk
vout.n-1: opreturn 'W' sbits fundingtxid hentropy proof

timeout:
same as winner, just without hentropy or proof

WARNING: there is an attack vector that precludes betting any large amounts, it goes as follows:
1. do dicebet to get the house entropy revealed
2. calculate bettor entropy that would win against the house entropy
3. reorg the chain and make a big bet using the winning entropy calculated in 2.

In order to mitigate this, the disclosure of the house entropy needs to be delayed beyond a reasonable reorg depth (notarization). It is recommended for production dice game with significant amounts of money to use such a delayed disclosure method.


Chapter 10 - channels example
It might be hard to believe, but channels CC implements an instant payment mechanism that is secured by dPoW in a way that is backward compatible with the existing wallets, explorers, etc. and channels CC does not require both nodes to be online. Its usecases are all the usecases for Lightning Network, it is just more secure, less expensive and backward compatible! The one aspect which some might consider a downside (and others another benefit) is that all payments are onchain. This means it would increase blockchain size, but the idea is for channels CC to be used on blockchains with relatively lower value coins, so a txfee of 0.0001 is not anything significant.

Warning: very confusing blockchain reorganization issues described below. Will be confusing to most people

From a distance, the blockchain is a chain of blocks. One block after the next, each referencing all the prior blocks. Each block containing a group of transactions. Prior to getting into a block, the transactions are broadcast to the network and if it is valid, it enters the memory pool. Each miner then constructs a valid block from these memory pool transactions and when a transaction gets mined (confirmed), it is removed from the memory pool.

That is the simple version!

The reality is quite a bit more complex, but the critical aspect is that the blockchain can (and is) reorganized as part of the expected protocol. This can happen even when there is no 51% attack happening and it is important to understand this process in detail, so here goes.

What happens if two miners find a valid block at the same time? In this case the "same time" means within the time it takes for a block to propagate to the network. When a miner finds a new block, it is broadcast to the network and nodes update and start waiting for the next block. When there are two different (and valid) blocks propagating at the same time, some nodes update with one of the blocks and some the other, lets call it blockA and blockB. Now the nodes will know about both blockA and blockB, but some will consider blockA to be the chaintip and others will consider blockB to be the chaintip.

This is where it gets confusing. Which is the correct chaintip (latest block?). It turns out that both blockA and blockB are valid at this moment in time. So there are actuall two blockchains. We have what is called a small fork! Now dont worry, the protocol will help us converge to a single chain, but in order to do that, we need the next block.

Some miners will be mining from blockA and others from blockB. In most all cases, when the next block is found, it wont be at the "same time" again. So we will end up with a chain that is blockA+blockA2 or blockB+blockB2. Here comes the small reorg! Let's assuming blockA2 was found before blockB2, so that means all nodes that had blockB as the chaintip now see a longer chain blockA+blockA2, which trumps blockB. When that happens, it reorgs the chain so it is on blockA+blockA2. To do this properly, all the transactions that were in blockB are put back into the mempool and blockA is added, then blockA2.

Of course, when blockB2 arrives, the nodes see it but blockB+blockB2 is the same length as blockA+blockA2, so no reorg happens. Since we postulated that blockAs arrived "before" blockB2, that means all nodes are on the same chaintip, including all the miners and the next block found would be blockA3, without any complications.

Believe it or not, this sort of thing is happening all the time, one all blockchains. The chaintip is a volatile thing and that is why more than one confirmation is needed to avoid the small reorgs invalidating blockhash. However, it is possible for more than just the blockhash to change. When the reorg happens, all the transactions in the block are put back into the mempool and then the new blocks are processed in order. So what happens if one of the inputs to a transaction that happened in blockB, gets spent in blockA2? Based on random utxo allocation by wallets this is not impossible if an address has a lot of activity, but if it is part of a 51% attack, then this remote chance of an utxo being spent becomes a certainity! In fact, that is what a 51% attack is.

The attack can go much deeper than just one block. For chains that use the longest chain rule, it can go quite deep indeed. So as all the reorged transactions are put back into the mempool, we feel good that it will get confirmed again. Unfortunately, there is no enforcement of a miner needing to mine any specific transaction in the mempool. And the 51% attacker is intent on mining the transaction that spends an already spent utxo in the reorganized chain. it is called a double spend, but in the reorganized chain, it is spent only once. So it is a bit of a misnomer.

The important thing to understand is that if any transaction has inputs that are signed by a node, it is possible when the chain reorganizes for that transaction to become invalid. This is why dPoW is important as it doesnt strictly use the longest chain rule, but rather the longest notarized chain rule. Once a block is notarized, then it will refuse to reorganize that block (or any block before). So the risk is still there, but only until a notarization. Please see more detailed information about dPoW <here>.

Given the above, if you are wondering how can it be possible to have a mempool payment be secured by dPoW. Since it is explained how the reorgs can make valid transactions disappear, it seems unlikely any such solution is possible. However, the CC is very powerful and it can make unlikely things possible.

The following describes how.

We know that any payment that is utxo based can be invalidated via 51% attack, or even an unlikely but not impossible random utxo allocation from a busy wallet. Which means the payment cant be via a utxo. Since the CC system is utxo based, you might think that it means CC cant solve this. However, CC is very powerful and can implement payments that are not utxo based. But before this non-utxo payment method is explained, first we need to solve the mechanics of payment.

At a high level, we want to lock funds into a channel, have this lock notarized so it cant be reorganized. Then payments can unlock funds. Additionally, if we are restricting the payment to just one destination, we also need a way for the sender to reclaim the unused funds. So there needs a way for a close channel notification, which when notarized allows the sender to reclaim all funds. After the channel close is notarized, then the only action possible should be a reclaim of sender funds.

We need to assume that any payment, channel close, reclaim can be reorganized until it is notarized so great care needs to be made that a payment that is made will always be valid. With some allowances for blocks after a channelclose is notarized, we can protect the payments using the logic of "stop accepting payments after a channelclose is seen". It might be that a full notarization of wait time after the channelclose is notarized is needed to provide sufficient time for all the payments to be reprocessed.

Now we can finally describe the requirements for the CC. The locked funds need to be able to be spent by either the sender or receiver, the former only after sufficient time after a channelclose and the latter only after a payment is seen (not just confirmed, but just seeing it should be enough). The protection from reorgs is that the payment itself reveals a secret that is needed for the payment and only the secret would be needed, so it wont matter what utxo is used. To lock funds into a CC address that can handle this we need a 1of2 CC address, which can accept a signature from either of two pubkeys. The additional CC constraints would be enforced to make sure payments are made until the channel is closed.

A hashchain has the nice property of being able to encode a lot of secrets with a single hash. You can hash the hash, over and over and the final hash is the public value. By revealing the next to last hash, it can be verified that it hashes to the final hash. There is a restriction that a hashchain needs to be of reasonable maximum depth, say 1000. That means each iteration of the hashchain that is revealed is worth 1/1000th the total channelfunds. In fact, if the 500th hash value is revealed, half the channelfunds are released. this allows 1/1000th resolution that can be released with a single hash value.

Now we can make the payment based on the hashvalue revealed at a specified depth before the prior released hashchain value. Both the sender and receiver can make a payment to the destination by attaching a hashchain secret. This means even if the sender's payment is reorganized, if the destination has the revealed secret, a replacement payment can be made that is valid. If the destination account isnt monitoring the blockchain, then it wont see the revealed secret, but in this case there shouldnt be value released for the payments that are reorganized. So it would be a case of no harm, no foul. In any event, all the payments end up verifiable on the blockchain to provide verifiability.

Payments at the speed of the mempool, protected by dPoW!

RPC calls
channelsopen:
 Used to open channel between two pub keys (sender and receiver). Parameters: destination_pubkey, total_number_of_payments, payment_denomination.
 Example - channelsopen 03a8fe537de2ace0d9c210b0ff945085c9192c9abf56ea22f22ce7998f289bb7bb 10 10000000
channelspayment:
 Sending payment to receiver. Condition is that the channel open tx is confirmed/notarised. Parameters: open_tx_id, payment_amount, [secret] (optional, used when receiver needs to make a payment which secret has already been revealed by sender).
 Example - channelspayment b9c141facc8cb71306d0de8e525b3de1450e93e17fc8799c8fda5ed52fd14440 20000000
channelsclose:
 Marking channel as closed. This RPC only creates a tx which says that the channel is closed and will be used in refund RPC to withdraw funds from closed channel. This also notifies receiver that channel fund could be withdrawn, but the payment RPC is still available until all funds are withdrawn. Parameters: open_tx_id.
 Example - channelsclose b9c141facc8cb71306d0de8e525b3de1450e93e17fc8799c8fda5ed52fd14440
channelsrefund:
 Withdrawing funds back to senders address. Refund can be issued only when channel close tx is confirmed/notarised. Parameters: open_tx_id, close_tx_id
 Example - channelsrefund b9c141facc8cb71306d0de8e525b3de1450e93e17fc8799c8fda5ed52fd14440 bb0ea34f846247642684c7c541c435b06ee79e47893640e5d2e51023841677fd
channelsinfo:
 Getting info about channels in which the issuer is involved, either as sender or receiver. Call without parameters give the list of available channels.  Parameters: [open_tx_id] (optional - used to get info about specific channel)

VIN/VOUT allocation
Open:
 vin.0: normal input
 vout.0: CC vout for channel funding on CC1of2 pubkey
 vout.1: CC vout marker to senders pubKey
 vout.2: CC vout marker to receiver pubkey
 vout.n-2: normal change
 vout.n-1: opreturn - 'O' zerotxid senderspubkey receiverspubkey totalnumberofpayments paymentamount hashchain

Payment
 vin.0: normal input
 vin.1: CC input from channel funding
 vin.2: CC input from src marker
 vout.0: CC vout change to channel funding on CC1of2 pubkey
 vout.1: CC vout marker to senders pubKey
 vout.2: CC vout marker to receiver pubkey
 vout.3: normal output of payment amount to receiver pubkey
 vout.n-2: normal change
 vout.n-1: opreturn - 'P' opentxid senderspubkey receiverspubkey depth numpayments secret

Close:
 vin.0: normal input
 vin.1: CC input from channel funding
 vin.2: CC input from src marker
 vout.0: CC vout for channel funding
 vout.1: CC vout marker to senders pubKey
 vout.2: CC vout marker to receiver pubkey
 vout.n-2: normal change
 vout.n-1: opreturn - 'C' opentxid senderspubkey receiverspubkey 0 0 0

Refund:
 vin.0: normal input
 vin.1: CC input from channel funding
 vin.2: CC input from src marker
 vout.0: CC vout marker to senders pubKey
 vout.1: CC vout marker to receiver pubKey
 vout.2: normal output of CC input to senders pubkey
 vout.n-2: normal change
 vout.n-1: opreturn - 'R' opentxid senderspubkey receiverspubkey numpayments payment closetxid

Chapter 11 - oracles example
Oracles CC is an example where it ended up being simpler than I first expected, but at the same time a lot more powerful. It is one of the smaller CC, but it enables creation of an arbitrary number of data markets, in a performant way.

In order to gain the performance, some clever usage of special addresses was needed. It was a bit tricky to generate a special address to keep track of the latest data.

Let's back up to the beginning. Just what is an oracle? In this context it is something that puts data that is not on the blockchain, onto the blockchain. Since everything other than the transactions and blocks are not in the blockchain, there is a very large universe of data that can be oracle-ized. It can be literally anything, from the obvious like prices to specific results relative to an arbitrary description.

The most difficult issue about oracles is that they need to be trusted to various degree to provide accurate and timely data. The danger is that if a trusted node is used to write data to the blockchain, it creates a trust point and a single point of attack. Ultimately there is nothing that can ensure only valid data is written to the blockchain, so what is done is to reinforce good behavior via pay per datapoint. However, for critical data, higher level processing is needed that combines multiple data providers into a validated signal.

At the oracles CC level, it is enough that there is financial incentive to provide good data. Also it is needed to allow multiple vendors for each data that is required and to enable efficient ways to update and query the data.

The following are the rpc calls:
oraclescreate name description format
oracleslist
oraclesinfo oracletxid
oraclesregister oracletxid datafee
oraclessubscribe oracletxid publisher amount
oraclesdata oracletxid hexstr
oraclessamples oracletxid batonutxo num

The first step is to create a specific data description with oraclescreate, which also defines the format of the binary data. This creates an oracletxid, which is used in the other rpc calls. name and description are just arbitrary strings, with name preferably being a short name used to access the data. The format is a string comprised of a single character per data element:

's' -> <256 char string
'S' -> <65536 char string
'd' -> <256 binary data
'D' -> <65536 binary data
'c' -> 1 byte signed little endian number, 'C' unsigned
't' -> 2 byte signed little endian number, 'T' unsigned
'i' -> 4 byte signed little endian number, 'I' unsigned
'l' -> 8 byte signed little endian number, 'L' unsigned
'h' -> 32 byte hash

For example, if the datapoint is comprised of a 4byte timestamp and an 8byte number the format string would be: "IL"

oracleslist displays a list of all the oraclestxid and oraclesinfo displays information about the specific oracletxid. Each oracletxid deterministically generates a marker address and a small amount is sent to that address to mark a transaction's relation to the oracltxid.

{
"result": "success",
"txid": "4895f631316a649e216153aee7a574bd281686265dc4e8d37597f72353facac3",
"name": "BTCUSD",
"description": "coindeskpricedata",
"format": "L",
"marker": "RVqJCSrdBm1gYJZS1h7dgtHioA5TEYzNRk",
"registered": [
{
"publisher": "02ebc786cb83de8dc3922ab83c21f3f8a2f3216940c3bf9da43ce39e2a3a882c92",
"baton": "RKY4zmHJZ5mNtf6tfKE5VMsKoV71Euej3i",
"batontxid": "4de10b01242ce1a5e29d5fbb03098b4519976879e05ad0458ef7174ed9127f18",
"lifetime": "1.50000000",
"funds": "0.01000000",
"datafee": "0.01000000"
}
]
}

A data publisher needs to register a datafee and their pubkey for a specific oracletxid. datafee needs to be at least as big as a txfee. Using oraclesregister the current datafee can be updated so a publisher can adapt to market conditions. Once registered, subscribers can prepay for some number of datapoints to a specific publisher using the oraclessubscribe rpc. At first, it is likely that the publisher would pay themselves to enable the posting of initial data points so the potential subscribers can evaluate the quality and consistency of the data.

The one final rpc is oraclessamples, which returns the most recent samples of data from a specific publisher. In order to have a performant solution to track all the potential data streams from all the publishers for all the oracletxid, a baton utxo is used. This is an output sent to a specific address and expected to have just a single utxo at any given time to allow for direct lookup. oraclessamples requires a starting txid to use and with each datapoint having the prior batontxid, there is a reverse linked list to traverse the most recent data.

In order to implement this, the following vin/vout contraints are used:

create:
vins.*: normal inputs
vout.0: txfee tag to oracle normal address
vout.1: change, if any
vout.n-1: opreturn with name and description and format for data

register:
vins.*: normal inputs
vout.0: txfee tag to normal marker address
vout.1: baton CC utxo
vout.2: change, if any
vout.n-1: opreturn with oracletxid, pubkey and price per data point

subscribe:
vins.*: normal inputs
vout.0: subscription fee to publishers CC address
vout.1: change, if any
vout.n-1: opreturn with oracletxid, registered provider's pubkey, amount

data:
vin.0: normal input
vin.1: baton CC utxo (most of the time)
vin.2+: subscription or data vout.0
vout.0: change to publishers CC address
vout.1: baton CC utxo
vout.2: payment for dataprovider
vout.3: change, if any
vout.n-1: opreturn with oracletxid, prevbatontxid and data in proper format

The oraclesdata transaction is the most complex as it needs to find and spend the baton utxo, use the correct datafee and spend funds from the locked subscription funds. With the above, the oracles CC is complete and allows the creations of massively parallel data streams from multiple vendors that uses free market feedback via payments, ie. poorly performing providers wont get renewals.

I expect that at first, the data providers will just be dapp developers deploying a working system including the required data, but its structure allows open market competition. Of course, specific dapps could restrict themselves to using only publishers from a whitelist of pubkeys. The potential usecases for oracles CC is quite varied and limited only by the imagination.

Chapter 12 - limitless possibilities
As can be seen, CC contracts can do a wide range of things and since they are Turing complete, we know that this is true. However, what is more important is the added security gained from using a utxo based system. While in some ways it is more complex to have to deal with utxo, as can be seen by the above examples, it is either solved and made invisible at the rpc level, or actually used as part of the solution.

Being utxo based, automatically builds in a rate limit to how many tx per block a specific CC contract can do. The state advancing by one transaction at a time is another means that rate limits. Since more utxo can be made available to increase capacity, it actually offers a way for managing load.

I believe I have made one of the first operational utxo smart contracts, CC or otherwise and hope that there will be many more developers joining forces to create more foundational CC contracts. Feel free to contact me for feedback on the type of CC contract you want to make. I have not documented all my notes and it could well be I already sort of know how to implement what your want your CC contract to do. Just only so many I can actually make time to code and debug.

Our testing cycle went a lot faster than expected as the bugs found were few and far between. Considering the scope of the assets CC and the realtime response aspects of dice CC, this was quite unexpected. I can only attribute it to the fact that CC validation is just the final validation on top of all the standard bitcoin protocol validations. Not having to worry about double spends is sure a nice luxury, though dont get too complacent about chain rewrites! It is possible to wait for information to be divulged and then reorg the chain to take advantage of this knowledge in a chain which is rewound.

Yes, blockchains are complicated.

Chapter 13 - different languages
The current codebase is integrated into the komodod codebase, which is C/C++. However, it is possible to use different languages and integrate into the C/C++ as zcash has shown by using the rust language for some parts of the zcashd.

I think any language that is compiled and can create a linkable library while being able to call and be called by C/C++ functions can be used. If you are able to make such a language binding for a simple CC contract like faucet, this will be good for a 777 KMD bounty. Of course, you need to be the first to submit a properly working pull request.


Chapter 14 - runtime bindings
Once build time linking works, then it is one step away from being able to do runtime linking, ie. dynamically linked libraries. There will be some work required to prevent duplication of eval codes and making sure it is a valid version of the CC contract plugin, but these are issues that have been solved before and I dont see any reason they cant be solved for CC contracts.

This would open up the door for quite an interesting ecosystem of CC plugins that blockchains can subscribe to.

Chapter 15 - rpc based dapps
Ultimately, I expect there to be so many new rpc calls (one set from each CC contract), that virtually any dapp can be made with rpc calls. We are just at the beginning now, but it is just a matter of time when we get there.

For now, we just need to keep listening to what the market wants as far as dapps go. Then make a new CC contract that enables doing as many of those as possible.

Repeat...

Imagine the scope that will exist after a year or two of continuous new CC contracts being created, along with all the rpc based dapps. I have seen some automatic GUI generators and it could be that for most cases, there can be a special GUI that not only create the dapp's GUI, but also all the rpc calls that are needed to make it work the way it is customized.

This codebase and tools in between the GUI and the rpc level will be a very good area for new initiatives.

##########

Conclusion
I hope this document has helped you understand what a Komodo utxo based CC contract is and how it is different from the other smart contracts. If you are now able to dive into the cc directory and start making your own CC contract, then I am very happy!


gateways CC
gateways CC is the first CC that combines multiple CC into a single one. In order to achieve its goals, both the assets CC and the oracles CC is used, in addition to a dapp that issues regular transactions. This general approach can be used to solve quite a few different use cases, so it is important to understand how a multi-CC solution is put together. There are some tricky issues that only arise when using more than one CC at a time.

Before the implementation details, first lets understand what the gateways CC does. At a high level it is similar to the old multigateway (from NXT AE 2014), but with improvements. The basic idea is to tokenize other crypto coins (like BTC) and then use the assets CC to transact/swap against the tokenized crypto. By enforcing a 1:1 peg between a specific token and BTC and an automated deposit/withdraw mechanism, it is possible to transact in the virtual BTC without the delay or expensive txfees. Then anybody that ends up having any of the BTC token would be able to withdraw actual BTC by redeeming the token.

BTC -> deposit to special address -> receive token that represents BTC onchain
do onchain transactions with the BTC token
anybody who obtains the BTC token can redeem the token and get actual BTC in the withdraw address

By bringing the operations onchain, it avoids the need for crosschain complications for each trade. The crosschain does still have to happen on the deposit and withdraw, but that is all. There is just one aspect that makes it not fully decentralized, which is the reliance on MofN multisig. However, with N trusted community members and a reasonable M value, it is not expected to be a big barrier. Since all operations are automated, the only trust needed is that M of the N multisig signers are running their nodes with the gateways dapp active and also that M of them wont collude to steal the funds locked in the multisig. In three years of operations, the MGW multigateway didnt have any incident of multisig signer misbehavior and it was only 2of3 multisig.

How can the gatewaysCC work? First, it needs a dedicated token that can be used to represent the external crypto coin. In order to avoid any issues with misplaced tokens, it is simplest to require that 100% of all the tokens are all locked in the gatewaysCC address. We want to make it so that the only way the tokens can be released from the locked address is when a verified deposit is made. So, we also need a deposit address, which means there needs to be a set of pubkeys that control the deposit address. It turns out, we also need to post merkleroot data from the external coin so that the information is onchain to be able to validate the external deposit. Since we are already trusting the deposit address signers to safekeep the external coins via MofN multisig, trusting them to post the merkleroots doesnt increase the trust footprint needed.

Now we have all the ingredients needed, a dedicated token, a set of multisig pubkeys and an oracle for merkleroots.

gatewaysbind tokenid oracletxid coin tokensupply M N pubkey(s)

With a gatewaysbind, a new gateway is defined. the pubkeys are for the custodians of the multisig funds and they also need to be posting merkleroots to the chain, so the oracle needs to be setup and funded and each of the signers needs to run the oraclefeed dapp. That posts each new merkleroot via oraclesdata and also responds to withdraw requests.

The MofN pubkeys generates a deposit address and when funds are sent to that address along with a small amount to the claim address. With the txid from this external coin, along with the txproof and the rawtransaction, all is submitted with a gatewaysdeposit. This adds a special baton output which is a gateways CC output to invoke gateways validation and also prevents double claims by using the unspent status of the baton.

gatewaysdeposit bindtxid height coin cointxid claimvout deposithex proof destpub amount
gatewaysclaim bindtxid coin deposittxid destpub amount

Once the gatewaysdeposit is validated, it can be claimed and now the token is sent to the claim address. A 1:1 pegging of the external crypto to the token is established. And as long as one of the deposit address signers is running the oraclefeed, then the deposit/claim process is fully automatic and under the control of the depositor. Nothing needs to be signed by any other party! Also by using the utxo from the deposittxid, double claims are prevented.

On the withdraw side, the tokens are sent back to the address where the tokens are locked and this needs to create a redemption right that can only be used once.

gatewayswithdraw bindtxid coin withdrawpub amount


And with a bit more magic in the oraclefeed, this is achieved. To be continued...






