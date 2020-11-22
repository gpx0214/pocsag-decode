# pocsag-decode

Decode pocsag number in 1200bps from NFM 32KHz sample rate wav recording. 2 bits error can be corrected by BCH(31,21).

pocsag.cpp read wav file and output decode text to stdout.

[experiment] iir.cpp output new wav file after moving average filter.

## output example
<pre>
block:  0   621.775        186_len        285_sync
 msg = 7cd215d8 000f9a42 SC head
01001011010100010100100111110000 [          ] msg = 4b5149f0 00096a29 address 1234000 01 down
10011011000000110110001100100000 [          ] msg = 9b036320 0013606c payload c6063  6063
10011001101100100001101100010100 [          ] msg = 99b21b14 00133643 payload cc62c   62
10011001110000010000010000010010 [          ] msg = 99c10412 00133820 payload cc140   140
</pre>

# complie 
gcc in msys2 on windows
