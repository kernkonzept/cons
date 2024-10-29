/*
 * (c) 2012-2013 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#include "vcon_client.h"

#include <l4/sys/typeinfo_svr>

unsigned Vcon_client::_dfl_obufsz = Vcon_client::Default_obuf_size;

void
Vcon_client::vcon_write(const char *buf, unsigned size) throw()
{ cooked_write(buf, size); }

unsigned
Vcon_client::vcon_read(char *buf, unsigned size) throw()
{
  char const *d = 0;
  const int offset = 0;
  unsigned status = 0;
  unsigned r = rbuf()->get(offset, &d);
  if (r > size)
    r = size;

  if (rbuf()->is_next_break(offset))
    {
      status |= L4_VCON_READ_STAT_BREAK;
      rbuf()->clear_next_break();
    }

  unsigned i = 0;
  for (; i < r; ++i)
    {
      if (rbuf()->is_next_break(offset + i))
        break;
      buf[i] = d[i];
    }

  rbuf()->clear(i);

  if (   i == r
      && (r < size || rbuf()->empty()))
    status |= L4_VCON_READ_STAT_DONE;

  if (!rbuf()->empty())
    i = size + 1;

  return i | status;
}

int
Vcon_client::vcon_set_attr(l4_vcon_attr_t const *a) throw()
{
  _attr = *a;
  return 0;
}

int
Vcon_client::vcon_get_attr(l4_vcon_attr_t *attr) throw()
{
  *attr = _attr;
  return 0;
}

