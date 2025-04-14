# Cons, the Console Multiplexer {#l4re_servers_cons}

`cons` is an interactive multiplexer for console in- and output. It buffers the
output from different %L4 clients and allows to switch between them to redirect
input.

## Multiplexers and Frontends

cons is able to connect multiple clients with multiple in/output
servers.

Clients are handled by a _multiplexer_. Each multiplexer publishes
a server capability that allows to create new client connections. The
default multiplexer is normally known under the `cons` capability.

Actual in/output is handled by separate frontends. From the
point-of-view of cons, a frontend consists of an IPC channel to
a server that speaks an appropriate server protocol. By default
the L4.Env.log capability is used.

For clients cons implements the L4::Vcon and the Virtio console interface.
The supported frontends is limited to L4::Vcon only.

## Starting the service

The cons server can be started with Lua like this:

    local log_server = L4.default_loader:new_channel()
    L4.default_loader:start({
      caps = {
        cons = log_server:svr(),
        fe = L4.Env.log,
      },
    },
    "rom/cons -m l4re -f fe")

First an IPC gate (`log_server`) is created which is used between the cons
server and a client to request a new session. The server side is assigned to the
mandatory `cons` capability of cons. The `fe` capability points to a L4::Vcon
capable output.

## Command Line Options

cons accepts the following command line switches:

* `-a`, `--show-all`

  Initially show output from all clients.

* `-B <size>`, `--defaultbufsize <size>`

  Default buffer size per client in bytes. Default: 40960

* `-c <client>`, `--autoconnect <client>`

  Automatically connect to the client with the given name.
  That means that output of this client will be visible and
  input will be routed to it.

* `-f <cap>`, `--frontend <cap>`

  Set the frontend for the current multiplexer. Output for the multiplexer
  is then sent to the capability with the given name. The server connected
  to the capability needs to understand the L4::Vcon protocol.

* `-k`, `--keep`

  Keep the console buffer when a client disconnects.

* `-l`, `--no-line-buffering`

  By default, merge the client output to entire lines. If the client writes
  characters without a final newline, the following client output is merged
  with the current line content. Specifying this switch disables the line
  buffered mode by default.

* `--line-buffering-ms <timeout>`

  Timeout in milliseconds before buffered client output is written even
  without a newline. Default value is 50.

* `-m <prompt name>`, `--mux <prompt name>`

  Add a new multiplexer named \<prompt name\>. This is necessary if output
  should be sent to different frontends.

* `-n`, `--defaultname`

  Default multiplexer capability to use. Default: `cons`.

* `-t`, `--timestamp`

  Prefix the output with timestamps.

## Connecting a client

     create(backend_type, ["client_name"], ["color"], ["option"] [,"option"] ...)

* `backend_type`

   The type of backend that should be created for the client. The type is a
   positive integer and currently the following types are supported:
   * `L4.Proto.Log`: L4::Vcon client
   * `1`: Virtio console client

cons accepts the following per-client options:

* `bufsz=n`

  Use a buffer of `n` bytes for this client, deviating from the default
  buffer size.

* `keep` / `no-keep`

  The console buffer is kept / thrown away when the client disconnects.

* `key=<key>`

  Assign `<key>` as keyboard shortcut to this client.

* `line-buffering` / `no-linux-buffering`

  Line buffering is enabled / disabled for this client.

* `show` / `hide`

  Output from this client is initially shown / hidden.

* `timestamp` / `no-timestamp`

  Do / do not prefix the output of this client with timestamps.
