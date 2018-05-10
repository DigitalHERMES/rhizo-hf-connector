# Rhizo-HF-connector

  Rhizo-HF-connector is a file exchange solution compatible with different
  HF modems.

  Currently, Rhizo-HF-connector sends messages which are placed inside an input
  directory, and writes received messages to an output directory, using a HF
  modem as channel.

  Planned support for the following modems: VARA, Ardop and D-Star (DV low
  speed data).

## Usage

| Option | Description |
| ---------------------------------- | --- |
| -r [ardop,dstar,vara] | Choose modem/radio type |
| -i input_spool_directory | Input spool directory (Messages to send) |
| -o output_spool_directory | Output spool directory (Received messages) |
| -c callsign | Station Callsign (Eg: PU2HFF) |
| -d remote_callsign  | Remote Station Callsign |
| -a tnc_ip_address    | IP address of the TNC |
| -p tcp_base_port      | TCP base port of the TNC. For VARA and ARDOP ports tcp_base_port and tcp_base_port+1 are used |
| -t timeout       |            Time to wait before disconnect when idling |
| -h |   Prints this help |

### Vara

For VARA modem, on gateway side (set VARA to ports 8300 / 8301):

    $ rz-hf-connector -r vara -i l1/ -o l2/ -c PP2PPP -d UU2UUU -a 127.0.0.1 -p 8300 -t 60

On client side (ports to 8400/8401):

    $ rz-hf-connector -r vara -i r1/ -o r2/ -c UU2UUU -d PP2PPP -a 127.0.0.1 -p 8400 -t 60

### Ardop

For Ardop, run ardopc, for example, in one side (ports 8517/8518):

    $ ardopc 8517 hw:0,0 hw:0,0

In the other site (ports 8515/8516):

    $ ardopc 8515 hw:0,1 hw:0,1

And rhizo-connector:

    $ rz-hf-connector -r ardop -i l1/ -o l2/ -c PP2UIT -d BB2UIT -a 127.0.0.1 -p 8515 -t 60

    $ rz-hf-connector -r ardop -i r1/ -o r2/ -c BB2UIT -d PP2UIT -a 127.0.0.1 -p 8517 -t 60


### D-Star

TODO

## Author

Rafael Diniz <rafael (AT) rhizomatica (DOT) org>
