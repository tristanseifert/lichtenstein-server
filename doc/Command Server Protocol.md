# CommandServer Protocol
Commands can be sent to the lichtenstein server from external programs using JSON-formatted messages through an UNIX domain socket. These messages are parsed and acted upon.

Each message will have at least the `type` field, which corresponds to an integer in the following table. Each message may also have a "txn" field, which is a transaction reference. This field is copied as-is to the output message.

| Type | Note
| ---: | ----
| 0    | Status

All responses have a `status` field that is 0 if the request was successful, a non-zero error code otherwise.

## Status
Status messages are responded to by a JSON dictionary containing the following keys:

- `version`: Major/minor version of the server.
- `build`: Build number of the server
- `load`: Array of load averages on the server; 1 minute, 5 minute and 15 minutes
- `mem`: Memory used by the server process
