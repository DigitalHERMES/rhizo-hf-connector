# Rhizo-connector

  Rhizo-connector is a file exchange solution for HF trx.

  Currently, rhizo-connector sends messages which are placed inside an input
  directory, and writes messages to an output directory, using a HF TNC as
  channel.

  Planned support for the following modems: VARA, Ardop and D-Star TNCs.

## Usage

### Vara

For VARA modem, on gateway side (set VARA to ports 8300 / 8301):

    $ connector -r vara -i l1/ -o l2/ -c PP2PPP -d UU2UUU -s RX -a 127.0.0.1 -p 8300

On client side (ports to 8400/8401):

   $ connector -r vara -i r1/ -o r2/ -c UU2UUU -d PP2PPP -s TX -a 127.0.0.1 -p 8400

### Ardop

For Ardop, run ardopc, for example, in one side (ports 8517/8518):

    $ ardopc 8517 hw:0,0 hw:0,0

In the other site (ports 8515/8516):

   $ ardopc 8515 hw:0,1 hw:0,1

And rhizo-connector:

   $ connector -r ardop -i l1/ -o l2/ -c PP2UIT -d BB2UIT -s RX -a 127.0.0.1 -p 8515
   $ connector -r ardop -i r1/ -o r2/ -c BB2UIT -d PP2UIT -s TX -a 127.0.0.1 -p 8517

### D-Star

TODO

## Author

Rafael Diniz <rafael@rhizomatica.org>
