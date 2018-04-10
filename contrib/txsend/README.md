## -txsend scripts

These scripts can be used with the `-txsend` config option to send transactions
via an external service instead of directly through the Zcash P2P network.

### `zcash_to_i2pbote.py`

Sends transactions as [I2P-Bote](https://i2pbote.xyz) messages to a remote
listener, for them to broadcast to the Zcash network.

#### Usage

- Install [I2P](https://geti2p.net/en/download).
- Install [I2P-Bote](https://i2pbote.xyz/install/).
- Enable IMAP in [I2P-Bote's settings](http://127.0.0.1:7657/i2pbote/settings.jsp).
- Add the following line to your `zcash.conf`:

    txsend="/path/to/zcash_to_i2pbote.py '%s'"

#### Notes

- The script assumes I2P-Bote is running on the same machine as `zcashd`. Change
  `SMTP_ENDPOINT` in the script if you have a different local setup.
- The number of relay hops, and the random delay per hop, can be configured in
  I2P-Bote's settings.
- The default remote listener is operated by str4d.
