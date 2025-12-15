/*
 * (c) 2012-2013 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#include "vcon_client.h"

#include <l4/sys/typeinfo_svr>
#include <l4/cxx/minmax>

unsigned Vcon_client::_dfl_obufsz = Vcon_client::Default_obuf_size;

void
Vcon_client::vcon_write(const char *buf, unsigned size) noexcept
{ cooked_write(buf, size); }

unsigned
Vcon_client::vcon_read(char *buf, unsigned const size) noexcept
{
  char const *str = 0;
  int const offset = 0;
  unsigned status = 0;
  unsigned strsz = rbuf()->get(offset, &str);
  unsigned copysz = cxx::min(strsz, size);

  if (rbuf()->is_next_break(offset))
    {
      status |= L4_VCON_READ_STAT_BREAK;
      rbuf()->clear_next_break();
    }

  unsigned i = 0;
  bool break_signal = false;
  for (; i < copysz; ++i)
    {
      if (rbuf()->is_next_break(offset + i))
        {
          break_signal = true;
          break;
        }
      buf[i] = str[i];
    }

  rbuf()->clear(i);

  // in case the rbuf wrapped around, there might be more data to read.
  if (   !rbuf()->empty() // rbuf wrapped, thus not empty
      && !break_signal    // no break signal
      && copysz < size)   // still room in the caller's buffer
    {
      // precondition: tail ptr incremented via clear();
      strsz = rbuf()->get(offset, &str);
      unsigned buf_left_sz = size - i;
      copysz = std::min(strsz, buf_left_sz);

      unsigned j = 0;
      for (; j < copysz; ++j)
        {
          if (rbuf()->is_next_break(offset + j))
            {
              break_signal = true;
              break;
            }
          buf[i+j] = str[j];
        }

      rbuf()->clear(j);
      i += j; // sum up all copied bytes
    }

  if (!break_signal)
    {
      if (rbuf()->empty())
        status |= L4_VCON_READ_STAT_DONE;
      else
        i = size + 1;
    }
  // else: break signal encountered: report bytes read

  return i | status;
}

int
Vcon_client::vcon_set_attr(l4_vcon_attr_t const *a) noexcept
{
  _attr = *a;
  return 0;
}

int
Vcon_client::vcon_get_attr(l4_vcon_attr_t *attr) noexcept
{
  *attr = _attr;
  return 0;
}

