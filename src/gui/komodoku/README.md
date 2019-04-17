About
-----
Komodo SudokuCC GUI

Just solve Sudoku and earn SUDOKU coins!

![alt text](https://i.imgur.com/std99XW.png)

To run you need up and running SUDOKU chain daemon built from latest https://github.com/jl777/komodo/tree/FSM and started with valid for your wallet pubkey in `-pubkey=` param.

SUDOKU chain params: 
```./komodod -ac_name=SUDOKU -ac_supply=1000000 -pubkey=<yourpubkey> -addnode=5.9.102.210 -gen -genproclimit=1 -ac_cclib=sudoku -ac_perc=10000000 -ac_reward=100000000 -ac_cc=60000 -ac_script=2ea22c80203d1579313abe7d8ea85f48c65ea66fc512c878c0d0e6f6d54036669de940febf8103120c008203000401cc &```

1) install dependencies:

```
$ sudo apt-get install python-pygame libgnutls28-dev
$ pip install requests wheel slick-bitcoinrpc pygame
```

2) and then start:

```
$ git clone https://github.com/tonymorony/Komodoku
$ cd Komodoku
$ python Sudoku.py
```
