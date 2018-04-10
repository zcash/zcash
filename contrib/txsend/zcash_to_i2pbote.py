#!/usr/bin/env python3
from email.message import EmailMessage
from smtplib import SMTP
import sys

SMTP_ENDPOINT = 'localhost:7661'
SMTP_USERNAME = 'bote'
SMTP_PASSWORD = 'x'

FROM = 'Anonymous'
TO = 'zecDxxuCxz7Bmo7cHT6zYh2FGgY43CsqwLUz-3JFAFplEta7aImKXE4lgN08NbHYc1lZVZt9yNmA60ItKmbbC6@bote'

BEGIN_ZCASH_TX = '-----BEGIN ZCASH TRANSACTION-----'
END_ZCASH_TX = '-----END ZCASH TRANSACTION-----'

def chunks(s, n):
    for i in range(0, len(s), n):
        yield s[i:i + n]

def format_transaction(tx):
    return '\n'.join([BEGIN_ZCASH_TX] + list(chunks(tx, 64)) + [END_ZCASH_TX])

def main():
    if len(sys.argv) < 2:
        print('Usage: zcash_to_i2pbote.py <HEX_TRANSACTION>')
        return
    tx = sys.argv[1]

    msg = EmailMessage()
    msg['Subject'] = 'Zcash transaction'
    msg['From'] = FROM
    msg['To'] = TO
    msg.set_content(format_transaction(tx))

    with SMTP(SMTP_ENDPOINT) as smtp:
        smtp.login(SMTP_USERNAME, SMTP_PASSWORD)
        smtp.send_message(msg)

if __name__ == '__main__':
    main()
