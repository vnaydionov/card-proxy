
ABOUT

This is a HTTP proxy-server that stores securely the payment card data
sent by a user from a WEB-form or a mobile application.
On receiving of the card data the proxy passes to the upstream only an opaque
token parameter.

Then the proxy can be used by a payment application backend to address some
external payment gateway.  In this case the outgoing authorization request
containing a token but no payment card data would expand to contain
full card data.

The point is to take out payment card data handling out of payment application
backend to minimize the scope of applicable PCI/DSS restrictions.


CRYPTOGRAPHY DETAILS

Sensitive card data is stored encrypted using AES algorithm with a 256 bit key
(DEK=Data Encryption Key).  A DEK can be used for encryption only few times.
For this reason new DEKs are generated as needed.
Each of DEKs is encrypted using AES algorithm with a 256 bit key (KEK=
Key Encryption Key).
The important thing about KEK is that it is not stored as a whole in single
place, but rather it is reconstructed on each request from three
different places:
  1) in memory of special daemon process (key keeper)
  2) in servant configuration file
  3) in servant database table


SOFTWARE REQUIREMENTS

Linux: Ubuntu Trusty 14.04 with GNU g++ 4.8
C++ REST SDK: ver2.0
YB.ORM: 0.4.7


