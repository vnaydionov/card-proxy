
<a href="https://scan.coverity.com/projects/vnaydionov-card-proxy">
  <img alt="Coverity Scan Build Status"
       src="https://scan.coverity.com/projects/7872/badge.svg"/>
</a>

ABOUT

This is a HTTP proxy-server that stores securely the payment card data
sent by a user from a WEB-form or a mobile application.
On receiving of the card data the proxy passes to the upstream only an opaque
token parameter, i.e. it performs tokenization.

Then the proxy can be used by a payment application backend to address some
external payment gateway.  In this case the outgoing authorization request
containing a token but no payment card data would expand to contain
full card data, so it does de-tokenization.

The point is to take out payment card data handling out of payment application
backend to minimize the scope of applicable PCI/DSS restrictions.


CRYPTOGRAPHY DETAILS

Sensitive card data is stored encrypted using AES algorithm with a 256 bit key
(DEK=Data Encryption Key).  A DEK can be used for encryption only few times.
For this reason new DEKs are generated as needed.
Each of DEKs is encrypted using AES algorithm with a 256 bit key (KEK=
Key Encryption Key).

The important thing about KEK is that it is not stored as a whole in single
place, but rather it is reconstructed on each request from parts placed in three
different places:
* in memory of special daemon process (key keeper)
* in servant configuration file
* in servant database table


SOFTWARE REQUIREMENTS

* Linux: Ubuntu 12.04 with GNU g++
* LibCURL
* LibJSON-C
* YB.ORM: 0.4.7

