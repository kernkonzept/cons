/*
 * Copyright (C) 2025 Kernkonzept GmbH.
 * Author(s): Steffen Liebergeld <steffen.liebergeld@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

/*
 * This implements a frontend using the virtio-console transport.
 *
 * Example lua:
 *
 * local cons_svr = loader:new_channel()
 * local virtio_device = loader:new_channel()
 * loader:start(
 *   {
 *     caps = {
 *       virtio = virtio_device,
 *       cons = cons_svr:svr(),
 *     },
 *    log = {"cons", "blue"},
 *   }, "cons -m guests -V virtio")
 *
 * The `virtio_device` cap needs to be handed to a virtio-driver, e.g. a uvmm
 * virtio-proxy. This will show up in guest Linux as a /dev/hvcX. You may
 * connect to this with a terminal emulator of your choice.
 *
 * Clients may get their log-sessions from the `cons_svr` cap.
 */
#pragma once

#include "frontend.h"
#include "server.h"

#include <l4/re/util/object_registry>
#include <l4/l4virtio/server/virtio-console-device>
#include <l4/l4virtio/l4virtio>
#include <l4/sys/cxx/ipc_epiface>

#include <queue>
#include <string>

class Virtio_console_fe
: public L4virtio::Svr::Console::Device,
  public Frontend,
  public L4::Epiface_t<Virtio_console_fe, L4virtio::Device, Server_object>
{
public:
  Virtio_console_fe(L4Re::Util::Object_registry *r)
  : L4virtio::Svr::Console::Device(0x100)
  {
    init_mem_info(4);
    r->register_irq_obj(irq_iface());
  }

  void rx_data_available(unsigned) override
  {
    unsigned s = 0;
    char buf[64];
    do
      {
        s = port_read(buf, 64);
        _input->input(cxx::String(buf, s));
      } while (s > 0);
  }

  void tx_space_available(unsigned) override
  {
    while (!_output_buffer.empty())
      {
        std::string &output = _output_buffer.front();
        unsigned sent_bytes = port_write(output.data(), output.length());
        if (sent_bytes < output.length())
          {
            output = output.substr(sent_bytes);
            break;
          }
        _output_buffer.pop();
      }
  }

  /*
   * Send client information to the virtio console frontend (e.g. a client
   * printed sth. and we now have to transmit that to the virtio-console
   * frontend, so that it shows up in the users' /dev/hvcX).
   */
  int write(const char* buf, unsigned int sz) override
  {
    if (!_output_buffer.empty())
      {
        _output_buffer.push(std::string(buf, sz));
        return sz;
      }

    unsigned s = port_write(buf, sz);
    if (s < sz)
      _output_buffer.push(std::string(buf + s, sz - s));
    return sz;
  }

  bool check_input() { return false; }

  Server_iface *server_iface() const
  { return L4::Epiface::server_iface(); }

  bool collected() { return false; }
private:
  std::queue<std::string> _output_buffer;
};
