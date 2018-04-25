=================
Zcash Payment API
=================

Overview
--------

Zcash extends the Bitcoin Core API with new RPC calls to support private
Zcash payments.

Zcash payments make use of two address formats:

-  taddr - an address for transparent funds (just like a Bitcoin
   address, value stored in UTXOs)
-  zaddr - an address for private funds (value stored in objects called
   notes)

When transferring funds from one taddr to another taddr, you can use
either the existing Bitcoin RPC calls or the new Zcash RPC calls.

When a transfer involves zaddrs, you must use the new Zcash RPC calls.

Compatibility with Bitcoin Core
-------------------------------

Zcash supports all commands in the Bitcoin Core API (as of version
0.11.2). Where applicable, Zcash will extend commands in a
backwards-compatible way to enable additional functionality.

We do not recommend use of accounts which are now deprecated in Bitcoin
Core. Where the account parameter exists in the API, please use “” as
its value, otherwise an error will be returned.

To support multiple users in a single node’s wallet, consider using
getnewaddress or z\_getnewaddress to obtain a new address for each user.
Also consider mapping multiple addresses to each user.

List of Zcash API commands
--------------------------

Optional parameters are denoted in [square brackets].

RPC calls by category:

-  Accounting: z\_getbalance, z\_gettotalbalance
-  Addresses : z\_getnewaddress, z\_listaddresses, z\_validateaddress
-  Keys : z\_exportkey, z\_importkey, z\_exportwallet, z\_importwallet
-  Operation: z\_getoperationresult, z\_getoperationstatus,
   z\_listoperationids
-  Payment : z\_listreceivedbyaddress, z\_sendmany, z\_shieldcoinbase

RPC parameter conventions:

-  taddr : Transparent address
-  zaddr : Private address
-  address : Accepts both private and transparent addresses.
-  amount : JSON format double-precision number with 1 ZC expressed as
   1.00000000.
-  memo : Metadata expressed in hexadecimal format. Limited to 512
   bytes, the current size of the memo field of a private transaction.
   Zero padding is automatic.

Accounting
~~~~~~~~~~

+------+------+------+
| Comm | Para | Desc |
| and  | mete | ript |
|      | rs   | ion  |
+======+======+======+
| z\_g | addr | Retu |
| etba | ess  | rns  |
| lanc | [min | the  |
| e    | conf | bala |
|      | =1]  | nce  |
|      |      | of a |
|      |      | tadd |
|      |      | r    |
|      |      | or   |
|      |      | zadd |
|      |      | r    |
|      |      | belo |
|      |      | ngin |
|      |      | g    |
|      |      | to   |
|      |      | the  |
|      |      | node |
|      |      | ’s   |
|      |      | wall |
|      |      | et.O |
|      |      | ptio |
|      |      | nall |
|      |      | y    |
|      |      | set  |
|      |      | the  |
|      |      | mini |
|      |      | mum  |
|      |      | numb |
|      |      | er   |
|      |      | of   |
|      |      | conf |
|      |      | irma |
|      |      | tion |
|      |      | s    |
|      |      | a    |
|      |      | priv |
|      |      | ate  |
|      |      | or   |
|      |      | tran |
|      |      | spar |
|      |      | ent  |
|      |      | tran |
|      |      | sact |
|      |      | ion  |
|      |      | must |
|      |      | have |
|      |      | in   |
|      |      | orde |
|      |      | r    |
|      |      | to   |
|      |      | be   |
|      |      | incl |
|      |      | uded |
|      |      | in   |
|      |      | the  |
|      |      | bala |
|      |      | nce. |
|      |      | Use  |
|      |      | 0 to |
|      |      | coun |
|      |      | t    |
|      |      | unco |
|      |      | nfir |
|      |      | med  |
|      |      | tran |
|      |      | sact |
|      |      | ions |
|      |      | .    |
+------+------+------+
| z\_g | [min | Retu |
| etto | conf | rn   |
| talb | =1]  | the  |
| alan |      | tota |
| ce   |      | l    |
|      |      | valu |
|      |      | e    |
|      |      | of   |
|      |      | fund |
|      |      | s    |
|      |      | stor |
|      |      | ed   |
|      |      | in   |
|      |      | the  |
|      |      | node |
|      |      | ’s   |
|      |      | wall |
|      |      | et.O |
|      |      | ptio |
|      |      | nall |
|      |      | y    |
|      |      | set  |
|      |      | the  |
|      |      | mini |
|      |      | mum  |
|      |      | numb |
|      |      | er   |
|      |      | of   |
|      |      | conf |
|      |      | irma |
|      |      | tion |
|      |      | s    |
|      |      | a    |
|      |      | priv |
|      |      | ate  |
|      |      | or   |
|      |      | tran |
|      |      | spar |
|      |      | ent  |
|      |      | tran |
|      |      | sact |
|      |      | ion  |
|      |      | must |
|      |      | have |
|      |      | in   |
|      |      | orde |
|      |      | r    |
|      |      | to   |
|      |      | be   |
|      |      | incl |
|      |      | uded |
|      |      | in   |
|      |      | the  |
|      |      | bala |
|      |      | nce. |
|      |      | Use  |
|      |      | 0 to |
|      |      | coun |
|      |      | t    |
|      |      | unco |
|      |      | nfir |
|      |      | med  |
|      |      | tran |
|      |      | sact |
|      |      | ions |
|      |      | .Out |
|      |      | put: |
|      |      | {"tr |
|      |      | ansp |
|      |      | aren |
|      |      | t"   |
|      |      | :    |
|      |      | 1.23 |
|      |      | ,"pr |
|      |      | ivat |
|      |      | e"   |
|      |      | :    |
|      |      | 4.56 |
|      |      | ,"to |
|      |      | tal" |
|      |      | :    |
|      |      | 5.79 |
|      |      | }    |
+------+------+------+

Addresses
~~~~~~~~~

+------+------+------+
| Comm | Para | Desc |
| and  | mete | ript |
|      | rs   | ion  |
+======+======+======+
| z\_g |      | Retu |
| etne |      | rn   |
| wadd |      | a    |
| ress |      | new  |
|      |      | zadd |
|      |      | r    |
|      |      | for  |
|      |      | send |
|      |      | ing  |
|      |      | and  |
|      |      | rece |
|      |      | ivin |
|      |      | g    |
|      |      | paym |
|      |      | ents |
|      |      | .    |
|      |      | The  |
|      |      | spen |
|      |      | ding |
|      |      | key  |
|      |      | for  |
|      |      | this |
|      |      | zadd |
|      |      | r    |
|      |      | will |
|      |      | be   |
|      |      | adde |
|      |      | d    |
|      |      | to   |
|      |      | the  |
|      |      | node |
|      |      | ’s   |
|      |      | wall |
|      |      | et.O |
|      |      | utpu |
|      |      | t:zN |
|      |      | 68D8 |
|      |      | hSs3 |
|      |      | ...  |
+------+------+------+
| z\_l |      | Retu |
| ista |      | rns  |
| ddre |      | a    |
| sses |      | list |
|      |      | of   |
|      |      | all  |
|      |      | the  |
|      |      | zadd |
|      |      | rs   |
|      |      | in   |
|      |      | this |
|      |      | node |
|      |      | ’s   |
|      |      | wall |
|      |      | et   |
|      |      | for  |
|      |      | whic |
|      |      | h    |
|      |      | you  |
|      |      | have |
|      |      | a    |
|      |      | spen |
|      |      | ding |
|      |      | key. |
|      |      | Outp |
|      |      | ut:{ |
|      |      | [“z1 |
|      |      | 23…” |
|      |      | ,    |
|      |      | “z45 |
|      |      | 6... |
|      |      | ”,   |
|      |      | “z78 |
|      |      | 9... |
|      |      | ”]   |
|      |      | }    |
+------+------+------+
| z\_v | zadd | Retu |
| alid | r    | rn   |
| atea |      | info |
| ddre |      | rmat |
| ss   |      | ion  |
|      |      | abou |
|      |      | t    |
|      |      | a    |
|      |      | give |
|      |      | n    |
|      |      | zadd |
|      |      | r.Ou |
|      |      | tput |
|      |      | :{"i |
|      |      | sval |
|      |      | id"  |
|      |      | :    |
|      |      | true |
|      |      | ,"ad |
|      |      | dres |
|      |      | s"   |
|      |      | :    |
|      |      | "zcW |
|      |      | smq. |
|      |      | ..", |
|      |      | "pay |
|      |      | ingk |
|      |      | ey"  |
|      |      | :    |
|      |      | "f5b |
|      |      | b3c. |
|      |      | ..", |
|      |      | "tra |
|      |      | nsmi |
|      |      | ssio |
|      |      | nkey |
|      |      | "    |
|      |      | :    |
|      |      | "7a5 |
|      |      | 8c7. |
|      |      | ..", |
|      |      | "ism |
|      |      | ine" |
|      |      | :    |
|      |      | true |
|      |      | }    |
+------+------+------+

Key Management
~~~~~~~~~~~~~~

+------+------+------+
| Comm | Para | Desc |
| and  | mete | ript |
|      | rs   | ion  |
+======+======+======+
| z\_e | zadd | *Req |
| xpor | r    | uire |
| tkey |      | s    |
|      |      | an   |
|      |      | unlo |
|      |      | cked |
|      |      | wall |
|      |      | et   |
|      |      | or   |
|      |      | an   |
|      |      | unen |
|      |      | cryp |
|      |      | ted  |
|      |      | wall |
|      |      | et.* |
|      |      | \ Re |
|      |      | turn |
|      |      | a    |
|      |      | zkey |
|      |      | for  |
|      |      | a    |
|      |      | give |
|      |      | n    |
|      |      | zadd |
|      |      | r    |
|      |      | belo |
|      |      | ngin |
|      |      | g    |
|      |      | to   |
|      |      | the  |
|      |      | node |
|      |      | ’s   |
|      |      | wall |
|      |      | et.T |
|      |      | he   |
|      |      | key  |
|      |      | will |
|      |      | be   |
|      |      | retu |
|      |      | rned |
|      |      | as a |
|      |      | stri |
|      |      | ng   |
|      |      | form |
|      |      | atte |
|      |      | d    |
|      |      | usin |
|      |      | g    |
|      |      | Base |
|      |      | 58Ch |
|      |      | eck  |
|      |      | as   |
|      |      | desc |
|      |      | ribe |
|      |      | d    |
|      |      | in   |
|      |      | the  |
|      |      | Zcas |
|      |      | h    |
|      |      | prot |
|      |      | ocol |
|      |      | spec |
|      |      | .Out |
|      |      | put: |
|      |      | AKWU |
|      |      | Akyp |
|      |      | wQjh |
|      |      | Z6LL |
|      |      | NaMu |
|      |      | uuLc |
|      |      | mZ6g |
|      |      | t5UF |
|      |      | yo8m |
|      |      | 3jGu |
|      |      | tvAL |
|      |      | mwZK |
|      |      | LdR5 |
+------+------+------+
| z\_i | zkey | *Wal |
| mpor | [res | let  |
| tkey | can= | must |
|      | true | be   |
|      | ]    | unlo |
|      |      | cked |
|      |      | .*\  |
|      |      | Add  |
|      |      | a    |
|      |      | zkey |
|      |      | as   |
|      |      | retu |
|      |      | rned |
|      |      | by   |
|      |      | z\_e |
|      |      | xpor |
|      |      | tkey |
|      |      | to a |
|      |      | node |
|      |      | 's   |
|      |      | wall |
|      |      | et.T |
|      |      | he   |
|      |      | key  |
|      |      | shou |
|      |      | ld   |
|      |      | be   |
|      |      | form |
|      |      | atte |
|      |      | d    |
|      |      | usin |
|      |      | g    |
|      |      | Base |
|      |      | 58Ch |
|      |      | eck  |
|      |      | as   |
|      |      | desc |
|      |      | ribe |
|      |      | d    |
|      |      | in   |
|      |      | the  |
|      |      | Zcas |
|      |      | h    |
|      |      | prot |
|      |      | ocol |
|      |      | spec |
|      |      | .Set |
|      |      | resc |
|      |      | an   |
|      |      | to   |
|      |      | true |
|      |      | (the |
|      |      | defa |
|      |      | ult) |
|      |      | to   |
|      |      | resc |
|      |      | an   |
|      |      | the  |
|      |      | enti |
|      |      | re   |
|      |      | loca |
|      |      | l    |
|      |      | bloc |
|      |      | k    |
|      |      | data |
|      |      | base |
|      |      | for  |
|      |      | tran |
|      |      | sact |
|      |      | ions |
|      |      | affe |
|      |      | ctin |
|      |      | g    |
|      |      | any  |
|      |      | addr |
|      |      | ess  |
|      |      | or   |
|      |      | pubk |
|      |      | ey   |
|      |      | scri |
|      |      | pt   |
|      |      | in   |
|      |      | the  |
|      |      | wall |
|      |      | et   |
|      |      | (inc |
|      |      | ludi |
|      |      | ng   |
|      |      | tran |
|      |      | sact |
|      |      | ions |
|      |      | affe |
|      |      | ctin |
|      |      | g    |
|      |      | the  |
|      |      | newl |
|      |      | y-ad |
|      |      | ded  |
|      |      | addr |
|      |      | ess  |
|      |      | for  |
|      |      | this |
|      |      | spen |
|      |      | ding |
|      |      | key) |
|      |      | .    |
+------+------+------+
| z\_e | file | *Req |
| xpor | name | uire |
| twal |      | s    |
| let  |      | an   |
|      |      | unlo |
|      |      | cked |
|      |      | wall |
|      |      | et   |
|      |      | or   |
|      |      | an   |
|      |      | unen |
|      |      | cryp |
|      |      | ted  |
|      |      | wall |
|      |      | et.* |
|      |      | \ Cr |
|      |      | eate |
|      |      | s    |
|      |      | or   |
|      |      | over |
|      |      | writ |
|      |      | es   |
|      |      | a    |
|      |      | file |
|      |      | with |
|      |      | tadd |
|      |      | r    |
|      |      | priv |
|      |      | ate  |
|      |      | keys |
|      |      | and  |
|      |      | zadd |
|      |      | r    |
|      |      | priv |
|      |      | ate  |
|      |      | keys |
|      |      | in a |
|      |      | huma |
|      |      | n-re |
|      |      | adab |
|      |      | le   |
|      |      | form |
|      |      | at.F |
|      |      | ilen |
|      |      | ame  |
|      |      | is   |
|      |      | the  |
|      |      | file |
|      |      | in   |
|      |      | whic |
|      |      | h    |
|      |      | the  |
|      |      | wall |
|      |      | et   |
|      |      | dump |
|      |      | will |
|      |      | be   |
|      |      | plac |
|      |      | ed.  |
|      |      | May  |
|      |      | be   |
|      |      | pref |
|      |      | aced |
|      |      | by   |
|      |      | an   |
|      |      | abso |
|      |      | lute |
|      |      | file |
|      |      | path |
|      |      | .    |
|      |      | An   |
|      |      | exis |
|      |      | ting |
|      |      | file |
|      |      | with |
|      |      | that |
|      |      | name |
|      |      | will |
|      |      | be   |
|      |      | over |
|      |      | writ |
|      |      | ten. |
|      |      | No   |
|      |      | valu |
|      |      | e    |
|      |      | is   |
|      |      | retu |
|      |      | rned |
|      |      | but  |
|      |      | a    |
|      |      | JSON |
|      |      | -RPC |
|      |      | erro |
|      |      | r    |
|      |      | will |
|      |      | be   |
|      |      | repo |
|      |      | rted |
|      |      | if a |
|      |      | fail |
|      |      | ure  |
|      |      | occu |
|      |      | rred |
|      |      | .    |
+------+------+------+
| z\_i | file | *Req |
| mpor | name | uire |
| twal |      | s    |
| let  |      | an   |
|      |      | unlo |
|      |      | cked |
|      |      | wall |
|      |      | et   |
|      |      | or   |
|      |      | an   |
|      |      | unen |
|      |      | cryp |
|      |      | ted  |
|      |      | wall |
|      |      | et.* |
|      |      | \ Im |
|      |      | port |
|      |      | s    |
|      |      | priv |
|      |      | ate  |
|      |      | keys |
|      |      | from |
|      |      | a    |
|      |      | file |
|      |      | in   |
|      |      | wall |
|      |      | et   |
|      |      | expo |
|      |      | rt   |
|      |      | file |
|      |      | form |
|      |      | at   |
|      |      | (see |
|      |      | z\_e |
|      |      | xpor |
|      |      | twal |
|      |      | let) |
|      |      | .    |
|      |      | Thes |
|      |      | e    |
|      |      | keys |
|      |      | will |
|      |      | be   |
|      |      | adde |
|      |      | d    |
|      |      | to   |
|      |      | the  |
|      |      | keys |
|      |      | curr |
|      |      | entl |
|      |      | y    |
|      |      | in   |
|      |      | the  |
|      |      | wall |
|      |      | et.  |
|      |      | This |
|      |      | call |
|      |      | may  |
|      |      | need |
|      |      | to   |
|      |      | resc |
|      |      | an   |
|      |      | all  |
|      |      | or   |
|      |      | part |
|      |      | s    |
|      |      | of   |
|      |      | the  |
|      |      | bloc |
|      |      | k    |
|      |      | chai |
|      |      | n    |
|      |      | for  |
|      |      | tran |
|      |      | sact |
|      |      | ions |
|      |      | affe |
|      |      | ctin |
|      |      | g    |
|      |      | the  |
|      |      | newl |
|      |      | y-ad |
|      |      | ded  |
|      |      | keys |
|      |      | ,    |
|      |      | whic |
|      |      | h    |
|      |      | may  |
|      |      | take |
|      |      | seve |
|      |      | ral  |
|      |      | minu |
|      |      | tes. |
|      |      | File |
|      |      | name |
|      |      | is   |
|      |      | the  |
|      |      | file |
|      |      | to   |
|      |      | impo |
|      |      | rt.  |
|      |      | The  |
|      |      | path |
|      |      | is   |
|      |      | rela |
|      |      | tive |
|      |      | to   |
|      |      | zcas |
|      |      | hd’s |
|      |      | work |
|      |      | ing  |
|      |      | dire |
|      |      | ctor |
|      |      | y.No |
|      |      | valu |
|      |      | e    |
|      |      | is   |
|      |      | retu |
|      |      | rned |
|      |      | but  |
|      |      | a    |
|      |      | JSON |
|      |      | -RPC |
|      |      | erro |
|      |      | r    |
|      |      | will |
|      |      | be   |
|      |      | repo |
|      |      | rted |
|      |      | if a |
|      |      | fail |
|      |      | ure  |
|      |      | occu |
|      |      | rred |
|      |      | .    |
+------+------+------+

Payment
~~~~~~~

+------+------+------+
| Comm | Para | Desc |
| and  | mete | ript |
|      | rs   | ion  |
+======+======+======+
| z\_l | zadd | Retu |
| istr | r    | rn   |
| ecei | [min | a    |
| vedb | conf | list |
| yadd | =1]  | of   |
| ress |      | amou |
|      |      | nts  |
|      |      | rece |
|      |      | ived |
|      |      | by a |
|      |      | zadd |
|      |      | r    |
|      |      | belo |
|      |      | ngin |
|      |      | g    |
|      |      | to   |
|      |      | the  |
|      |      | node |
|      |      | ’s   |
|      |      | wall |
|      |      | et.O |
|      |      | ptio |
|      |      | nall |
|      |      | y    |
|      |      | set  |
|      |      | the  |
|      |      | mini |
|      |      | mum  |
|      |      | numb |
|      |      | er   |
|      |      | of   |
|      |      | conf |
|      |      | irma |
|      |      | tion |
|      |      | s    |
|      |      | whic |
|      |      | h    |
|      |      | a    |
|      |      | rece |
|      |      | ived |
|      |      | amou |
|      |      | nt   |
|      |      | must |
|      |      | have |
|      |      | in   |
|      |      | orde |
|      |      | r    |
|      |      | to   |
|      |      | be   |
|      |      | incl |
|      |      | uded |
|      |      | in   |
|      |      | the  |
|      |      | resu |
|      |      | lt.  |
|      |      | Use  |
|      |      | 0 to |
|      |      | coun |
|      |      | t    |
|      |      | unco |
|      |      | nfir |
|      |      | med  |
|      |      | tran |
|      |      | sact |
|      |      | ions |
|      |      | .Out |
|      |      | put: |
|      |      | [{“t |
|      |      | xid” |
|      |      | :    |
|      |      | “4a0 |
|      |      | f…”, |
|      |      | “amo |
|      |      | unt” |
|      |      | :    |
|      |      | 0.54 |
|      |      | ,“me |
|      |      | mo”: |
|      |      | ”F0F |
|      |      | F…”, |
|      |      | },   |
|      |      | {... |
|      |      | },   |
|      |      | {... |
|      |      | }]   |
+------+------+------+
| z\_s | from | *Thi |
| endm | addr | s    |
| any  | ess  | is   |
|      | amou | an   |
|      | nts  | Asyn |
|      | [min | chro |
|      | conf | nous |
|      | =1]  | RPC  |
|      | [fee | call |
|      | =0.0 | *\ S |
|      | 001] | end  |
|      |      | fund |
|      |      | s    |
|      |      | from |
|      |      | an   |
|      |      | addr |
|      |      | ess  |
|      |      | to   |
|      |      | mult |
|      |      | iple |
|      |      | outp |
|      |      | uts. |
|      |      | The  |
|      |      | addr |
|      |      | ess  |
|      |      | can  |
|      |      | be   |
|      |      | eith |
|      |      | er   |
|      |      | a    |
|      |      | tadd |
|      |      | r    |
|      |      | or a |
|      |      | zadd |
|      |      | r.Am |
|      |      | ount |
|      |      | s    |
|      |      | is a |
|      |      | list |
|      |      | cont |
|      |      | aini |
|      |      | ng   |
|      |      | key/ |
|      |      | valu |
|      |      | e    |
|      |      | pair |
|      |      | s    |
|      |      | corr |
|      |      | espo |
|      |      | ndin |
|      |      | g    |
|      |      | to   |
|      |      | the  |
|      |      | addr |
|      |      | esse |
|      |      | s    |
|      |      | and  |
|      |      | amou |
|      |      | nt   |
|      |      | to   |
|      |      | pay. |
|      |      | Each |
|      |      | outp |
|      |      | ut   |
|      |      | addr |
|      |      | ess  |
|      |      | can  |
|      |      | be   |
|      |      | in   |
|      |      | tadd |
|      |      | r    |
|      |      | or   |
|      |      | zadd |
|      |      | r    |
|      |      | form |
|      |      | at.W |
|      |      | hen  |
|      |      | send |
|      |      | ing  |
|      |      | to a |
|      |      | zadd |
|      |      | r,   |
|      |      | you  |
|      |      | also |
|      |      | have |
|      |      | the  |
|      |      | opti |
|      |      | on   |
|      |      | of   |
|      |      | atta |
|      |      | chin |
|      |      | g    |
|      |      | a    |
|      |      | memo |
|      |      | in   |
|      |      | hexa |
|      |      | deci |
|      |      | mal  |
|      |      | form |
|      |      | at.\ |
|      |      |  **N |
|      |      | OTE: |
|      |      | **\  |
|      |      | When |
|      |      | send |
|      |      | ing  |
|      |      | coin |
|      |      | base |
|      |      | fund |
|      |      | s    |
|      |      | to a |
|      |      | zadd |
|      |      | r,   |
|      |      | the  |
|      |      | node |
|      |      | 's   |
|      |      | wall |
|      |      | et   |
|      |      | does |
|      |      | not  |
|      |      | allo |
|      |      | w    |
|      |      | any  |
|      |      | chan |
|      |      | ge.  |
|      |      | Put  |
|      |      | anot |
|      |      | her  |
|      |      | way, |
|      |      | spen |
|      |      | ding |
|      |      | a    |
|      |      | part |
|      |      | ial  |
|      |      | amou |
|      |      | nt   |
|      |      | of a |
|      |      | coin |
|      |      | base |
|      |      | utxo |
|      |      | is   |
|      |      | not  |
|      |      | allo |
|      |      | wed. |
|      |      | This |
|      |      | is   |
|      |      | not  |
|      |      | a    |
|      |      | cons |
|      |      | ensu |
|      |      | s    |
|      |      | rule |
|      |      | but  |
|      |      | a    |
|      |      | loca |
|      |      | l    |
|      |      | wall |
|      |      | et   |
|      |      | rule |
|      |      | due  |
|      |      | to   |
|      |      | the  |
|      |      | curr |
|      |      | ent  |
|      |      | impl |
|      |      | emen |
|      |      | tati |
|      |      | on   |
|      |      | of   |
|      |      | z\_s |
|      |      | endm |
|      |      | any. |
|      |      | In   |
|      |      | futu |
|      |      | re,  |
|      |      | this |
|      |      | rule |
|      |      | may  |
|      |      | be   |
|      |      | remo |
|      |      | ved. |
|      |      | Exam |
|      |      | ple  |
|      |      | of   |
|      |      | Outp |
|      |      | uts  |
|      |      | para |
|      |      | mete |
|      |      | r:[{ |
|      |      | “add |
|      |      | ress |
|      |      | ”:”t |
|      |      | 123… |
|      |      | ”,   |
|      |      | “amo |
|      |      | unt” |
|      |      | :0.0 |
|      |      | 05}, |
|      |      | ,{“a |
|      |      | ddre |
|      |      | ss”: |
|      |      | ”z01 |
|      |      | 0…”, |
|      |      | ”amo |
|      |      | unt” |
|      |      | :0.0 |
|      |      | 3,   |
|      |      | “mem |
|      |      | o”:” |
|      |      | f508 |
|      |      | af…” |
|      |      | }]Op |
|      |      | tion |
|      |      | ally |
|      |      | set  |
|      |      | the  |
|      |      | mini |
|      |      | mum  |
|      |      | numb |
|      |      | er   |
|      |      | of   |
|      |      | conf |
|      |      | irma |
|      |      | tion |
|      |      | s    |
|      |      | whic |
|      |      | h    |
|      |      | a    |
|      |      | priv |
|      |      | ate  |
|      |      | or   |
|      |      | tran |
|      |      | spar |
|      |      | ent  |
|      |      | tran |
|      |      | sact |
|      |      | ion  |
|      |      | must |
|      |      | have |
|      |      | in   |
|      |      | orde |
|      |      | r    |
|      |      | to   |
|      |      | be   |
|      |      | used |
|      |      | as   |
|      |      | an   |
|      |      | inpu |
|      |      | t.   |
|      |      | When |
|      |      | send |
|      |      | ing  |
|      |      | from |
|      |      | a    |
|      |      | zadd |
|      |      | r,   |
|      |      | minc |
|      |      | onf  |
|      |      | must |
|      |      | be   |
|      |      | grea |
|      |      | ter  |
|      |      | than |
|      |      | zero |
|      |      | .Opt |
|      |      | iona |
|      |      | lly  |
|      |      | set  |
|      |      | a    |
|      |      | tran |
|      |      | sact |
|      |      | ion  |
|      |      | fee, |
|      |      | whic |
|      |      | h    |
|      |      | by   |
|      |      | defa |
|      |      | ult  |
|      |      | is   |
|      |      | 0.00 |
|      |      | 01   |
|      |      | ZEC. |
|      |      | Any  |
|      |      | tran |
|      |      | spar |
|      |      | ent  |
|      |      | chan |
|      |      | ge   |
|      |      | will |
|      |      | be   |
|      |      | sent |
|      |      | to a |
|      |      | new  |
|      |      | tran |
|      |      | spar |
|      |      | ent  |
|      |      | addr |
|      |      | ess. |
|      |      | Any  |
|      |      | priv |
|      |      | ate  |
|      |      | chan |
|      |      | ge   |
|      |      | will |
|      |      | be   |
|      |      | sent |
|      |      | back |
|      |      | to   |
|      |      | the  |
|      |      | zadd |
|      |      | r    |
|      |      | bein |
|      |      | g    |
|      |      | used |
|      |      | as   |
|      |      | the  |
|      |      | sour |
|      |      | ce   |
|      |      | of   |
|      |      | fund |
|      |      | s.Re |
|      |      | turn |
|      |      | s    |
|      |      | an   |
|      |      | oper |
|      |      | atio |
|      |      | nid. |
|      |      | You  |
|      |      | use  |
|      |      | the  |
|      |      | oper |
|      |      | atio |
|      |      | nid  |
|      |      | valu |
|      |      | e    |
|      |      | with |
|      |      | z\_g |
|      |      | etop |
|      |      | erat |
|      |      | ions |
|      |      | tatu |
|      |      | s    |
|      |      | and  |
|      |      | z\_g |
|      |      | etop |
|      |      | erat |
|      |      | ionr |
|      |      | esul |
|      |      | t    |
|      |      | to   |
|      |      | obta |
|      |      | in   |
|      |      | the  |
|      |      | resu |
|      |      | lt   |
|      |      | of   |
|      |      | send |
|      |      | ing  |
|      |      | fund |
|      |      | s,   |
|      |      | whic |
|      |      | h    |
|      |      | if   |
|      |      | succ |
|      |      | essf |
|      |      | ul,  |
|      |      | will |
|      |      | be a |
|      |      | txid |
|      |      | .    |
+------+------+------+
| z\_s | from | *Thi |
| hiel | addr | s    |
| dcoi | ess  | is   |
| nbas | toad | an   |
| e    | dres | Asyn |
|      | s    | chro |
|      | [fee | nous |
|      | =0.0 | RPC  |
|      | 001] | call |
|      | [lim | *\ S |
|      | it=5 | hiel |
|      | 0]   | d    |
|      |      | tran |
|      |      | spar |
|      |      | ent  |
|      |      | coin |
|      |      | base |
|      |      | fund |
|      |      | s    |
|      |      | by   |
|      |      | send |
|      |      | ing  |
|      |      | to a |
|      |      | shie |
|      |      | lded |
|      |      | z    |
|      |      | addr |
|      |      | ess. |
|      |      | Utxo |
|      |      | s    |
|      |      | sele |
|      |      | cted |
|      |      | for  |
|      |      | shie |
|      |      | ldin |
|      |      | g    |
|      |      | will |
|      |      | be   |
|      |      | lock |
|      |      | ed.  |
|      |      | If   |
|      |      | ther |
|      |      | e    |
|      |      | is   |
|      |      | an   |
|      |      | erro |
|      |      | r,   |
|      |      | they |
|      |      | are  |
|      |      | unlo |
|      |      | cked |
|      |      | .    |
|      |      | The  |
|      |      | RPC  |
|      |      | call |
|      |      | ``li |
|      |      | stlo |
|      |      | ckun |
|      |      | spen |
|      |      | t``  |
|      |      | can  |
|      |      | be   |
|      |      | used |
|      |      | to   |
|      |      | retu |
|      |      | rn   |
|      |      | a    |
|      |      | list |
|      |      | of   |
|      |      | lock |
|      |      | ed   |
|      |      | utxo |
|      |      | s.Th |
|      |      | e    |
|      |      | numb |
|      |      | er   |
|      |      | of   |
|      |      | coin |
|      |      | base |
|      |      | utxo |
|      |      | s    |
|      |      | sele |
|      |      | cted |
|      |      | for  |
|      |      | shie |
|      |      | ldin |
|      |      | g    |
|      |      | can  |
|      |      | be   |
|      |      | set  |
|      |      | with |
|      |      | the  |
|      |      | limi |
|      |      | t    |
|      |      | para |
|      |      | mete |
|      |      | r,   |
|      |      | whic |
|      |      | h    |
|      |      | has  |
|      |      | a    |
|      |      | defa |
|      |      | ult  |
|      |      | valu |
|      |      | e    |
|      |      | of   |
|      |      | 50.  |
|      |      | If   |
|      |      | the  |
|      |      | para |
|      |      | mete |
|      |      | r    |
|      |      | is   |
|      |      | set  |
|      |      | to   |
|      |      | 0,   |
|      |      | the  |
|      |      | numb |
|      |      | er   |
|      |      | of   |
|      |      | utxo |
|      |      | s    |
|      |      | sele |
|      |      | cted |
|      |      | is   |
|      |      | limi |
|      |      | ted  |
|      |      | by   |
|      |      | the  |
|      |      | ``-m |
|      |      | empo |
|      |      | oltx |
|      |      | inpu |
|      |      | tlim |
|      |      | it`` |
|      |      | opti |
|      |      | on.  |
|      |      | Any  |
|      |      | limi |
|      |      | t    |
|      |      | is   |
|      |      | cons |
|      |      | trai |
|      |      | ned  |
|      |      | by a |
|      |      | cons |
|      |      | ensu |
|      |      | s    |
|      |      | rule |
|      |      | defi |
|      |      | ning |
|      |      | a    |
|      |      | maxi |
|      |      | mum  |
|      |      | tran |
|      |      | sact |
|      |      | ion  |
|      |      | size |
|      |      | of   |
|      |      | 1000 |
|      |      | 00   |
|      |      | byte |
|      |      | s.   |
|      |      | The  |
|      |      | from |
|      |      | addr |
|      |      | ess  |
|      |      | is a |
|      |      | tadd |
|      |      | r    |
|      |      | or   |
|      |      | "\*" |
|      |      | for  |
|      |      | all  |
|      |      | tadd |
|      |      | rs   |
|      |      | belo |
|      |      | ngin |
|      |      | g    |
|      |      | to   |
|      |      | the  |
|      |      | wall |
|      |      | et.  |
|      |      | The  |
|      |      | to   |
|      |      | addr |
|      |      | ess  |
|      |      | is a |
|      |      | zadd |
|      |      | r.   |
|      |      | The  |
|      |      | defa |
|      |      | ult  |
|      |      | fee  |
|      |      | is   |
|      |      | 0.00 |
|      |      | 01.R |
|      |      | etur |
|      |      | ns   |
|      |      | an   |
|      |      | obje |
|      |      | ct   |
|      |      | cont |
|      |      | aini |
|      |      | ng   |
|      |      | an   |
|      |      | oper |
|      |      | atio |
|      |      | nid  |
|      |      | whic |
|      |      | h    |
|      |      | can  |
|      |      | be   |
|      |      | used |
|      |      | with |
|      |      | z\_g |
|      |      | etop |
|      |      | erat |
|      |      | ions |
|      |      | tatu |
|      |      | s    |
|      |      | and  |
|      |      | z\_g |
|      |      | etop |
|      |      | erat |
|      |      | ionr |
|      |      | esul |
|      |      | t,   |
|      |      | alon |
|      |      | g    |
|      |      | with |
|      |      | key- |
|      |      | valu |
|      |      | e    |
|      |      | pair |
|      |      | s    |
|      |      | rega |
|      |      | rdin |
|      |      | g    |
|      |      | how  |
|      |      | many |
|      |      | utxo |
|      |      | s    |
|      |      | are  |
|      |      | bein |
|      |      | g    |
|      |      | shie |
|      |      | lded |
|      |      | in   |
|      |      | this |
|      |      | tran |
|      |      | sact |
|      |      | ion  |
|      |      | and  |
|      |      | what |
|      |      | rema |
|      |      | ins  |
|      |      | to   |
|      |      | be   |
|      |      | shie |
|      |      | lded |
|      |      | .    |
+------+------+------+

Operations
~~~~~~~~~~

Asynchronous calls return an OperationStatus object which is a JSON
object with the following defined key-value pairs:

-  operationid : unique identifier for the async operation. Use this
   value with z\_getoperationstatus or z\_getoperationresult to poll and
   query the operation and obtain its result.
-  status : current status of operation
-  queued : operation is pending execution
-  executing : operation is currently being executed
-  cancelled
-  failed.
-  success
-  result : result object if the status is ‘success’. The exact form of
   the result object is dependent on the call itself.
-  error: error object if the status is ‘failed’. The error object has
   the following key-value pairs:
-  code : number
-  message: error message

Depending on the type of asynchronous call, there may be other key-value
pairs. For example, a z\_sendmany operation will also include the
following in an OperationStatus object:

-  method : name of operation e.g. z\_sendmany
-  params : an object containing the parameters to z\_sendmany

Currently, as soon as you retrieve the operation status for an operation
which has finished, that is it has either succeeded, failed, or been
cancelled, the operation and any associated information is removed.

It is currently not possible to cancel operations.

+------+------+------+
| Comm | Para | Desc |
| and  | mete | ript |
|      | rs   | ion  |
+======+======+======+
| z\_g | [ope | Retu |
| etop | rati | rn   |
| erat | onid | Oper |
| ionr | s]   | atio |
| esul |      | nSta |
| t    |      | tus  |
|      |      | JSON |
|      |      | obje |
|      |      | cts  |
|      |      | for  |
|      |      | all  |
|      |      | comp |
|      |      | lete |
|      |      | d    |
|      |      | oper |
|      |      | atio |
|      |      | ns   |
|      |      | the  |
|      |      | node |
|      |      | is   |
|      |      | curr |
|      |      | entl |
|      |      | y    |
|      |      | awar |
|      |      | e    |
|      |      | of,  |
|      |      | and  |
|      |      | then |
|      |      | remo |
|      |      | ve   |
|      |      | the  |
|      |      | oper |
|      |      | atio |
|      |      | n    |
|      |      | from |
|      |      | memo |
|      |      | ry.O |
|      |      | pera |
|      |      | tion |
|      |      | ids  |
|      |      | is   |
|      |      | an   |
|      |      | opti |
|      |      | onal |
|      |      | arra |
|      |      | y    |
|      |      | to   |
|      |      | filt |
|      |      | er   |
|      |      | whic |
|      |      | h    |
|      |      | oper |
|      |      | atio |
|      |      | ns   |
|      |      | you  |
|      |      | want |
|      |      | to   |
|      |      | rece |
|      |      | ive  |
|      |      | stat |
|      |      | us   |
|      |      | obje |
|      |      | cts  |
|      |      | for. |
|      |      | Outp |
|      |      | ut   |
|      |      | is a |
|      |      | list |
|      |      | of   |
|      |      | oper |
|      |      | atio |
|      |      | n    |
|      |      | stat |
|      |      | us   |
|      |      | obje |
|      |      | cts, |
|      |      | wher |
|      |      | e    |
|      |      | the  |
|      |      | stat |
|      |      | us   |
|      |      | is   |
|      |      | eith |
|      |      | er   |
|      |      | "fai |
|      |      | led" |
|      |      | ,    |
|      |      | "can |
|      |      | cell |
|      |      | ed"  |
|      |      | or   |
|      |      | "suc |
|      |      | cess |
|      |      | ".[{ |
|      |      | “ope |
|      |      | rati |
|      |      | onid |
|      |      | ”:   |
|      |      | “opi |
|      |      | d-11 |
|      |      | ee…” |
|      |      | ,“st |
|      |      | atus |
|      |      | ”:   |
|      |      | “can |
|      |      | cell |
|      |      | ed”} |
|      |      | ,{“o |
|      |      | pera |
|      |      | tion |
|      |      | id”: |
|      |      | “opi |
|      |      | d-98 |
|      |      | 76”, |
|      |      | “sta |
|      |      | tus” |
|      |      | :    |
|      |      | ”fai |
|      |      | led” |
|      |      | },{“ |
|      |      | oper |
|      |      | atio |
|      |      | nid” |
|      |      | :    |
|      |      | “opi |
|      |      | d-0e |
|      |      | 0e”, |
|      |      | “sta |
|      |      | tus” |
|      |      | :”su |
|      |      | cces |
|      |      | s”,“ |
|      |      | exec |
|      |      | utio |
|      |      | n\_t |
|      |      | ime” |
|      |      | :”25 |
|      |      | ”,“r |
|      |      | esul |
|      |      | t”:  |
|      |      | {“tx |
|      |      | id”: |
|      |      | ”af3 |
|      |      | 8876 |
|      |      | 54…” |
|      |      | ,... |
|      |      | }},] |
|      |      | Exam |
|      |      | ples |
|      |      | :zca |
|      |      | sh-c |
|      |      | li   |
|      |      | z\_g |
|      |      | etop |
|      |      | erat |
|      |      | ionr |
|      |      | esul |
|      |      | t    |
|      |      | '["o |
|      |      | pid- |
|      |      | 8120 |
|      |      | fa20 |
|      |      | -5ee |
|      |      | 7-45 |
|      |      | 87-9 |
|      |      | 57b- |
|      |      | f257 |
|      |      | 9c2d |
|      |      | 882b |
|      |      | "]'  |
|      |      | zcas |
|      |      | h-cl |
|      |      | i    |
|      |      | z\_g |
|      |      | etop |
|      |      | erat |
|      |      | ionr |
|      |      | esul |
|      |      | t    |
+------+------+------+
| z\_g | [ope | Retu |
| etop | rati | rn   |
| erat | onid | Oper |
| ions | s]   | atio |
| tatu |      | nSta |
| s    |      | tus  |
|      |      | JSON |
|      |      | obje |
|      |      | cts  |
|      |      | for  |
|      |      | all  |
|      |      | oper |
|      |      | atio |
|      |      | ns   |
|      |      | the  |
|      |      | node |
|      |      | is   |
|      |      | curr |
|      |      | entl |
|      |      | y    |
|      |      | awar |
|      |      | e    |
|      |      | of.O |
|      |      | pera |
|      |      | tion |
|      |      | ids  |
|      |      | is   |
|      |      | an   |
|      |      | opti |
|      |      | onal |
|      |      | arra |
|      |      | y    |
|      |      | to   |
|      |      | filt |
|      |      | er   |
|      |      | whic |
|      |      | h    |
|      |      | oper |
|      |      | atio |
|      |      | ns   |
|      |      | you  |
|      |      | want |
|      |      | to   |
|      |      | rece |
|      |      | ive  |
|      |      | stat |
|      |      | us   |
|      |      | obje |
|      |      | cts  |
|      |      | for. |
|      |      | Outp |
|      |      | ut   |
|      |      | is a |
|      |      | list |
|      |      | of   |
|      |      | oper |
|      |      | atio |
|      |      | n    |
|      |      | stat |
|      |      | us   |
|      |      | obje |
|      |      | cts. |
|      |      | [{“o |
|      |      | pera |
|      |      | tion |
|      |      | id”: |
|      |      | “opi |
|      |      | d-12 |
|      |      | ee…” |
|      |      | ,“st |
|      |      | atus |
|      |      | ”:   |
|      |      | “que |
|      |      | ued” |
|      |      | },{“ |
|      |      | oper |
|      |      | atio |
|      |      | nid” |
|      |      | :    |
|      |      | “opd |
|      |      | -098 |
|      |      | a…”, |
|      |      | “sta |
|      |      | tus” |
|      |      | :    |
|      |      | ”exe |
|      |      | cuti |
|      |      | ng”} |
|      |      | ,{“o |
|      |      | pera |
|      |      | tion |
|      |      | id”: |
|      |      | “opi |
|      |      | d-98 |
|      |      | 76”, |
|      |      | “sta |
|      |      | tus” |
|      |      | :    |
|      |      | ”fai |
|      |      | led” |
|      |      | }]Wh |
|      |      | en   |
|      |      | the  |
|      |      | oper |
|      |      | atio |
|      |      | n    |
|      |      | succ |
|      |      | eeds |
|      |      | ,    |
|      |      | the  |
|      |      | stat |
|      |      | us   |
|      |      | obje |
|      |      | ct   |
|      |      | will |
|      |      | also |
|      |      | incl |
|      |      | ude  |
|      |      | the  |
|      |      | resu |
|      |      | lt.{ |
|      |      | “ope |
|      |      | rati |
|      |      | onid |
|      |      | ”:   |
|      |      | “opi |
|      |      | d-0e |
|      |      | 0e”, |
|      |      | “sta |
|      |      | tus” |
|      |      | :”su |
|      |      | cces |
|      |      | s”,“ |
|      |      | exec |
|      |      | utio |
|      |      | n\_t |
|      |      | ime” |
|      |      | :”25 |
|      |      | ”,“r |
|      |      | esul |
|      |      | t”:  |
|      |      | {“tx |
|      |      | id”: |
|      |      | ”af3 |
|      |      | 8876 |
|      |      | 54…” |
|      |      | ,... |
|      |      | }}   |
+------+------+------+
| z\_l | [sta | Retu |
| isto | te]  | rn   |
| pera |      | a    |
| tion |      | list |
| ids  |      | of   |
|      |      | oper |
|      |      | atio |
|      |      | nids |
|      |      | for  |
|      |      | all  |
|      |      | oper |
|      |      | atio |
|      |      | ns   |
|      |      | whic |
|      |      | h    |
|      |      | the  |
|      |      | node |
|      |      | is   |
|      |      | curr |
|      |      | entl |
|      |      | y    |
|      |      | awar |
|      |      | e    |
|      |      | of.S |
|      |      | tate |
|      |      | is   |
|      |      | an   |
|      |      | opti |
|      |      | onal |
|      |      | stri |
|      |      | ng   |
|      |      | para |
|      |      | mete |
|      |      | r    |
|      |      | to   |
|      |      | filt |
|      |      | er   |
|      |      | the  |
|      |      | oper |
|      |      | atio |
|      |      | ns   |
|      |      | you  |
|      |      | want |
|      |      | list |
|      |      | ed   |
|      |      | by   |
|      |      | thei |
|      |      | r    |
|      |      | stat |
|      |      | e.   |
|      |      | Acce |
|      |      | ptab |
|      |      | le   |
|      |      | para |
|      |      | mete |
|      |      | r    |
|      |      | valu |
|      |      | es   |
|      |      | are  |
|      |      | ‘que |
|      |      | ued’ |
|      |      | ,    |
|      |      | ‘exe |
|      |      | cuti |
|      |      | ng’, |
|      |      | ‘suc |
|      |      | cess |
|      |      | ’,   |
|      |      | ‘fai |
|      |      | led’ |
|      |      | ,    |
|      |      | ‘can |
|      |      | cell |
|      |      | ed’. |
|      |      | [“op |
|      |      | id-0 |
|      |      | e0e… |
|      |      | ”,   |
|      |      | “opi |
|      |      | d-1a |
|      |      | f4…” |
|      |      | ,    |
|      |      | … ]  |
+------+------+------+

Asynchronous RPC call Error Codes
---------------------------------

Zcash error codes are defined in
https://github.com/zcash/zcash/blob/master/src/rpcprotocol.h

z\_sendmany error codes
~~~~~~~~~~~~~~~~~~~~~~~

+---------------------------+------------------------------------------------+
| RPC\_INVALID\_PARAMETER   | *Invalid, missing or duplicate parameter*      |
| (-8)                      |                                                |
+===========================+================================================+
| "Minconf cannot be zero   | Cannot accept minimum confirmation value of    |
| when sending from zaddr"  | zero when sending from zaddr.                  |
+---------------------------+------------------------------------------------+
| "Minconf cannot be        | Cannot accept negative minimum confirmation    |
| negative"                 | number.                                        |
+---------------------------+------------------------------------------------+
| "Minimum number of        | Cannot accept negative minimum confirmation    |
| confirmations cannot be   | number.                                        |
| less than 0"              |                                                |
+---------------------------+------------------------------------------------+
| "From address parameter   | Missing an address to send funds from.         |
| missing"                  |                                                |
+---------------------------+------------------------------------------------+
| "No recipients"           | Missing recipient addresses.                   |
+---------------------------+------------------------------------------------+
| "Memo must be in          | Encrypted memo field data must be in           |
| hexadecimal format"       | hexadecimal format.                            |
+---------------------------+------------------------------------------------+
| "Memo size of \_\_ is too | Encrypted memo field data exceeds maximum size |
| big, maximum allowed is   | of 512 bytes.                                  |
| \_\_ "                    |                                                |
+---------------------------+------------------------------------------------+
| "From address does not    | Sender address spending key not found.         |
| belong to this node,      |                                                |
| zaddr spending key not    |                                                |
| found."                   |                                                |
+---------------------------+------------------------------------------------+
| "Invalid parameter,       | Expected object.                               |
| expected object"          |                                                |
+---------------------------+------------------------------------------------+
| "Invalid parameter,       | Encrypted memo field data exceeds maximum size |
| unknown key: **" \|       | of 512 bytes.                                  |
| Unknown key. "Invalid     |                                                |
| parameter, expected valid |                                                |
| size" \| Invalid size.    |                                                |
| "Invalid parameter,       |                                                |
| expected hex txid" \|     |                                                |
| Invalid txid. "Invalid    |                                                |
| parameter, vout must be   |                                                |
| positive" \| Invalid      |                                                |
| vout. "Invalid parameter, |                                                |
| duplicated address" \|    |                                                |
| Address is duplicated.    |                                                |
| "Invalid parameter,       |                                                |
| amounts array is empty"   |                                                |
| \| Amounts array is       |                                                |
| empty. "Invalid           |                                                |
| parameter, unknown key"   |                                                |
| \| Key not found.         |                                                |
| "Invalid parameter,       |                                                |
| unknown address format"   |                                                |
| \| Unknown address        |                                                |
| format. "Invalid          |                                                |
| parameter, size of memo"  |                                                |
| \| Invalid memo field     |                                                |
| size. "Invalid parameter, |                                                |
| amount must be positive"  |                                                |
| \| Invalid or negative    |                                                |
| amount. "Invalid          |                                                |
| parameter, too many zaddr |                                                |
| outputs" \| z\_address    |                                                |
| outputs exceed maximum    |                                                |
| allowed. "Invalid         |                                                |
| parameter, expected memo  |                                                |
| data in hexadecimal       |                                                |
| format" \| Encrypted memo |                                                |
| field is not in           |                                                |
| hexadecimal format.       |                                                |
| "Invalid parameter, size  |                                                |
| of memo is larger than    |                                                |
| maximum allowed ** "      |                                                |
+---------------------------+------------------------------------------------+

+-----------------------------------+------------------------------+
| RPC\_INVALID\_ADDRESS\_OR\_KEY    | *Invalid address or key*     |
| (-5)                              |                              |
+===================================+==============================+
| "Invalid from address, no         | z\_address spending key not  |
| spending key found for zaddr"     | found.                       |
+-----------------------------------+------------------------------+
| "Invalid output address, not a    | Transparent output address   |
| valid taddr."                     | is invalid.                  |
+-----------------------------------+------------------------------+
| "Invalid from address, should be  | Sender address is invalid.   |
| a taddr or zaddr."                |                              |
+-----------------------------------+------------------------------+
| "From address does not belong to  | Sender address spending key  |
| this node, zaddr spending key not | not found.                   |
| found."                           |                              |
+-----------------------------------+------------------------------+

RPC\_WALLET\_INSUFFICIENT\_FUNDS (-6) \| *Not enough funds in wallet or
account* -----------------------------------\|
------------------------------------------ "Insufficient funds, no UTXOs
found for taddr from address." \| Insufficient funds for sending
address. "Could not find any non-coinbase UTXOs to spend. Coinbase UTXOs
can only be sent to a single zaddr recipient." \| Must send Coinbase
UTXO to a single z\_address. "Could not find any non-coinbase UTXOs to
spend." \| No available non-coinbase UTXOs. "Insufficient funds, no
unspent notes found for zaddr from address." \| Insufficient funds for
sending address. "Insufficient transparent funds, have **, need ** plus
fee **" \| Insufficient funds from transparent address. "Insufficient
protected funds, have **, need \_\_ plus fee \_\_" \| Insufficient funds
from shielded address.

RPC\_WALLET\_ERROR (-4) \| *Unspecified problem with wallet*
----------------------\| ------------------------------------- "Could
not find previous JoinSplit anchor" \| Try restarting node with
``-reindex``. "Error decrypting output note of previous JoinSplit: \_\_"
\| "Could not find witness for note commitment" \| Try restarting node
with ``-rescan``. "Witness for note commitment is null" \| Missing
witness for note commitment. "Witness for spendable note does not have
same anchor as change input" \| Invalid anchor for spendable note
witness. "Not enough funds to pay miners fee" \| Retry with sufficient
funds. "Missing hex data for raw transaction" \| Raw transaction data is
null. "Missing hex data for signed transaction" \| Hex value for signed
transaction is null. "Send raw transaction did not return an error or a
txid." \|

+-------------------------------------------------+--------------------------+
| RPC\_WALLET\_ENCRYPTION\_FAILED (-16)           | *Failed to encrypt the   |
|                                                 | wallet*                  |
+=================================================+==========================+
| "Failed to sign transaction"                    | Transaction was not      |
|                                                 | signed, sign transaction |
|                                                 | and retry.               |
+-------------------------------------------------+--------------------------+

+---------------------------------------------+------------------------------+
| RPC\_WALLET\_KEYPOOL\_RAN\_OUT (-12)        | *Keypool ran out, call       |
|                                             | keypoolrefill first*         |
+=============================================+==============================+
| "Could not generate a taddr to use as a     | Call keypoolrefill and       |
| change address"                             | retry.                       |
+---------------------------------------------+------------------------------+
