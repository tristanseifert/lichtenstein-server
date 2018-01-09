# CommandServer Protocol
Commands can be sent to the lichtenstein server from external programs using JSON-formatted messages through an UNIX domain socket. These messages are parsed and acted upon.

Each message will have at least the `type` field, which corresponds to an integer in the following table. Each message may also have a "txn" field, which is a transaction reference. This field is copied as-is to the output message.

| Type | Note
| ---: | ----
| 0    | Status
| 1    | List Nodes
| 2    | List Groups
| 3    | Add effect mapping
| 4    | Remove effect mapping

All responses have a `status` field that is 0 if the request was successful, a non-zero error code otherwise.

## Error Handling
Errors are indicated with a non-zero status value: possible values are declared in the `CommandServer.h` file.

In this case, the response may contain one or more error-specific fields. However, all errors may have the `error` field populated with a human-readable error string.

## Status
Status messages are responded to by a JSON dictionary containing the following keys:

- `version`: Major/minor version of the server.
- `build`: Build number of the server
- `load`: Array of load averages on the server; 1 minute, 5 minute and 15 minutes
- `mem`: Memory used by the server process

## Add effect mapping
Adds a mapping between the specified group(s) and the specified routine. The request will have two keys:

- `routine`: A dictionary containing the id of the routine (`id`) and optionally, additional parameters (`params`) to be passed to the routine.
- `groups`: An array of IDs of groups.

If either the routine or one or more groups could not be found, an error is returned. Otherwise, the mapping is added.

# Remove effect mapping
Removes any mappings involving the specified group(s). This request has a single key:

- `groups`: An array of IDs of groups.

If the groups are not part of an ubergroup, they're simply removed. Otherwise, they'll be removed from the ubergroup.
