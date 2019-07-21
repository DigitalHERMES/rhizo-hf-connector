# Rhizo-HF-connector

  Rhizo-HF-connector is a file exchange solution compatible with different
  HF modems.

  Rhizo-HF-connector controls a TNC to transmit or receive files,
  though a inotify-driven interface, in which files written to a specified directory
  are automatically transmitted by the TNC, and files received from TNC are
  written to a specified directory.

  Support for the following TNCs are implemented: Ardop (works
  in both normal ardop or experimental ofdm mode) and VARA.

## Usage

| Option | Description |
| --- | --- |
| -r [ardop,vara] | Choose modem/radio type |
| -i input_spool_directory | Input spool directory (Messages to send) |
| -o output_spool_directory | Output spool directory (Received messages) |
| -c callsign | Station Callsign (Eg: PU2HFF) |
| -d remote_callsign | Remote Station Callsign |
| -a tnc_ip_address | IP address of the TNC |
| -p tcp_base_port | TCP base port of the TNC. For VARA and ARDOP ports tcp_base_port and tcp_base_port+1 are used |
| -t timeout | Time to wait before disconnect when idling |
| -f features | Enable/Disable features. Supported features: ofdm, noofdm.|
| -h | Prints this help |

### Ardop

Example of invocation command of Ardop connected to an ICOM IC-7100, using base port 8515:

    $ ardop1ofdm 8515 -c /dev/ttyUSB0 ARDOP ARDOP -k FEFE88E01C0001FD -u FEFE88E01C0000FD

ps: as a note, for an Yaesu FT-991 use " -k 5458313B -u 5458303B ".

With the following ALSA configuration (global-wide ALSA configuration in "/etc/asound.conf"): 

    pcm.ARDOP {type rate slave {pcm "hw:1,0" rate 48000}}

Associated rz-hf-connector command example (If not specified, Ardop's mode defaults to "ofdm"):

    $ rz-hf-connector -r ardop -i /var/spool/outgoing_messages/ -o /var/spool/incoming_messages/ -c BB2UIT -d PP2UIT -a 127.0.0.1 -p 8515 -t 60

### Vara

Example for running rz-hf-connector with VARA modem, on base port 8300:

    $ rz-hf-connector -r vara -i l1/ -o l2/ -c BB2ITU -d UU2ITU -a 127.0.0.1 -p 8300 -t 60

## Author

Rafael Diniz <rafael (AT) rhizomatica (DOT) org>
