# Komodo Cryptoconditons Terminal User Interfaces (aka TUIs)

These tools creating for demonstration and partial automation of Komodo cryptoconditions modules testing. (RogueCC game, AssetsCC, OraclesCC, GatewaysCC, MarmaraCC, ...)


Developer installation (on Ubuntu 18.04) :

Python3 required for execution:

*  `sudo apt-get install python3.6 python3-pip libgnutls28-dev`

pip packages needed:

* `pip3 install setuptools wheel slick-bitcoinrpc`
* or `pip3 install -r requirements.txt`

Starting: 

# TUI for RogueCC

If you're looking for player 3 in 1 (daemon + game + TUI) multiOS bundle - please check `releases` of this repo.

`python3 rogue_tui.py`

![alt text](https://i.imgur.com/gkcxMGt.png)

# TUI for OraclesCC

Have files uploader/downloader functionality - also there is a AWS branch for AWS certificates uploading demonstration

`python3 oracles_cc_tui.py`

![alt text](https://i.imgur.com/tfHwRqc.png)

# TUI for GatewaysCC

![alt text](https://i.imgur.com/c8DPfpp.png)

`python3 gateways_creation_tui.py`

`python3 gateways_usage_tui.py`

At the moment raw version of manual gateway how-to guide can be found here: https://docs.komodoplatform.com/cc/contracts/gateways/scenarios/tutorial.html I advice to read it before you start use this tool to understand the flow.

# TUI for MarmaraCC

`python3 marmara_tui.py`

![alt text](https://i.imgur.com/uonMWHl.png)

# TUI for AssetsCC (not much finished)

`python3 assets_cc_tui.py`

Before execution be sure than daemon for needed AC up.



