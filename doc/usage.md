# Cons, the Console Multiplexer {#l4re_servers_cons}

`cons` is an interactive multiplexer for console input and output. It buffers the
output from different %L4 clients and allows to switch between them to redirect
input.

## Multiplexers and Frontends

cons is able to connect multiple clients to multiple console I/O servers. All
clients are connected to all configured multiplexers

Multiplexers and frontends come in pairs where the actual I/O is handled by the
frontend. From the point-of-view of cons, a frontend consists of an IPC channel
to a server that speaks an appropriate server protocol. By default the
L4.Env.log capability is used. Only frontends that speak the L4::Vcon protocol
are supported.

A multiplexer handles and routes the I/O of each client to and from its
respective frontend.

Each client's console settings (e.g. color, visibility) apply to all
multiplexers and cannot be changed individually. A user can connect to all
clients through all multiplexers. It is not possible to assign individual
clients to distinct multiplexers.

## Client sessions

For clients cons implements the L4::Vcon and the Virtio console interface. A
client session can be requested through a `create` call to cons' factory. cons
binds its factory capability to the `cons` capability. See the example below on
how this can be set up.

## Starting the service

The cons server can be started with Lua like this:

    local log_server = L4.default_loader:new_channel()
    L4.default_loader:start({
      caps = {
        cons = log_server:svr(),
      },
      log = L4.Env.log,
    },
    "rom/cons")

First an IPC gate (`log_server`) is created which is used between the cons
server and a client to request a new session. The server side is assigned to the
mandatory `cons` capability of cons. This example explicitly assigns the
kernel's log capability (`L4.Env.log`) to cons' `log` capability in order to
allow cons to provide input and output for its clients. The default log factory
(usually provided by `moe`) doesn't provide input capabilities.

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
  is then sent to the capability with the given name `<cap>`. The server
  connected to the capability needs to understand the L4::Vcon protocol.

* `-V <cap>`, `--virtio-device-frontend <cap>`

  Set a virtio-console frontend for the current multiplexer. This behaves
  identical to the `--frontend` option except that this frontend implements
  a virtio-console device that --for example-- can be connected to a
  virtio proxy in uvmm.

  Example lua:

        -- use 'cons_svr' to create client sessions
        local cons_svr = loader:new_channel()
        -- connect 'virtio_device' with a virtio driver (e.g. Uvmm)
        local virtio_device = loader:new_channel()
        loader:start(
          {
            caps = {
              virtio = virtio_device,
              cons = cons_svr:svr(),
            },
           log = {"cons", "blue"},
          }, "cons -m guests -V virtio")

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

  Add a new multiplexer named `<prompt name>`. This is necessary if output
  should be sent to different frontends. This option must be used in conjunction
  with the `-f` frontend option

* `-n`, `--defaultname`

  Default name for the multiplexer prompt. Default: `cons`.

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
