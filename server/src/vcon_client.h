/*
 * (c) 2012-2013 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#pragma once

#include "client.h"
#include "controller.h"
#include "server.h"

#include <l4/re/util/icu_svr>
#include <l4/re/util/vcon_svr>
#include <l4/re/util/object_registry>

class Vcon_client
: public L4::Epiface_t<Vcon_client, L4::Vcon, Server_object>,
  public L4Re::Util::Icu_cap_array_svr<Vcon_client>,
  public L4Re::Util::Vcon_svr<Vcon_client>,
  public Client
{
public:
  typedef L4Re::Util::Icu_cap_array_svr<Vcon_client> Icu_svr;
  typedef L4Re::Util::Vcon_svr<Vcon_client> My_vcon_svr;

  Vcon_client(std::string const &name, int color, size_t bufsz, Key key,
              bool line_buffering, unsigned line_buffering_ms,
              L4Re::Util::Object_registry *, L4::Ipc_svr::Server_iface *sif,
              Controller *ctl)
  : Icu_svr(1, &_irq),
    Client(name, color, 512, bufsz < 512 ? _dfl_obufsz : bufsz, key,
           line_buffering, line_buffering_ms, sif, ctl)
  {}

  void vcon_write(const char *buffer, unsigned size) noexcept;
  unsigned vcon_read(char *buffer, unsigned size) noexcept;

  int vcon_set_attr(l4_vcon_attr_t const *a) noexcept;
  int vcon_get_attr(l4_vcon_attr_t *attr) noexcept;

  const l4_vcon_attr_t *attr() const { return &_attr; }

  void trigger() const { _irq.trigger(); }

  bool collected() { return Client::collected(); }

  static void default_obuf_size(unsigned bufsz)
  {
    _dfl_obufsz = cxx::max(512U, cxx::min(16U << 20, bufsz));
  }

private:
  enum { Default_obuf_size = 40960 };
  static unsigned _dfl_obufsz;
  Icu_svr::Irq _irq;
};
