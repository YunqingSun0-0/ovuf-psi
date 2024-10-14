* This project relies on emp and relic library.
* To install emp, refer to https://github.com/emp-toolkit/emp-tool, https://github.com/emp-toolkit/emp-ot
* To install relic, refer to https://github.com/relic-toolkit/relic. We use the preset of BLS128 curve.

## Test
* Private Set Generation
* Server and client locally generate a private set with a specific size of elements, each element is 256-bit:

`./bin/test_gen_private_set $size$ >> xxx.txt`;
Example: `./bin/test_gen_private_set 100 >> setA.txt`

* Key Generation
* Server locally generate secret key and public key pairs and store them in file:

`./bin/test_gen_key "keyfile/sk.key" "keyfile/pk.key"`

* Server sends public key to client:

  Server: `./bin/test_send_pk 1 "port" "keyfile/pk.key"`
  Client: `./bin/test_send_pk 2 "port" "keyfile/pk.key"`

* Set Encoding
* Server use the generated sk to encode its private set "setA.txt" to "encodeA.encode":

`./bin/test_read_from_set "keyfile/sk.key" "setA.txt" "encodeA.encode" "THREAD_NUM"`

* Client interact with server to encode its private set "setB.txt" to "encodeB.encode"

  Server: `./bin/test_psi 1 12345 "THREAD_NUM" "keyfile/sk.key"`
  Client: `./bin/test_psi 2 12345 "THREAD_NUM" "keyfile/pk.key" "setB.txt" "encodeB.encode"`

* Compute Intersection
* Client locally compare "encodeB.encode" and "encodeA.encode" to find the intersection:

`./bin/test_look_up "encodeA.encode" "encodeB.encode"`
