/*
 * (c) 2012-2013 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#include "client.h"
#include "controller.h"

#include <l4/re/env.h>
#include <l4/sys/kip.h>

#include <climits>
#include <cstring>
#include <time.h>

template<typename Client>
void
Client_timeout<Client>::expired()
{ _client->timeout_expired(); }

Client::Client(std::string const &tag, int color, int rsz, int wsz, Key key,
               bool line_buffering, unsigned line_buffering_ms,
               L4::Ipc_svr::Server_iface *sif, Controller *ctl)
: _col(color), _tag(tag), _line_buffering(line_buffering),
  _line_buffering_ms(line_buffering_ms), _key(key), _wb(wsz), _rb(rsz),
  _first_unwritten(_wb.head()), _timeout(this), _sif(sif), _ctl(ctl)
{
  _attr.i_flags = L4_VCON_ICRNL;
  _attr.o_flags = L4_VCON_ONLRET | L4_VCON_ONLCR;
  _attr.l_flags = L4_VCON_ECHO;
}

Client::~Client()
{
  if (output_mux())
    output_mux()->disconnect(this);

  if (_ctl)
    _ctl->remove_client(this);
}

bool
Client::collected()
{
  _dead = true;
  if (_keep)
    return false;

  return true;
}

static constexpr int Max_timestamp_len = 25;

void
Client::print_timestamp()
{
  time_t t = time(NULL);
  struct tm *tt = localtime(&t);
  char b[Max_timestamp_len];

  int l = tt ? strftime(b, sizeof(b), "[%Y-%m-%d %T] ", tt)
             : snprintf(b, sizeof(b), "[unknown] ");
  if (l)
    wbuf()->put(b, l);
}

void
Client::do_output(Buf::Index until)
{
  if (!_output)
    return;

  wbuf()->write(_first_unwritten, until, this);
  _first_unwritten = until;

  if (!(_attr.l_flags & L4_VCON_ICANON))
    _output->flush(this);
}

void
Client::cooked_write(const char *buf, long size) throw()
{
  if (size < 0)
    size = strlen(buf);

  Client::Buf *w = wbuf();
  Buf::Index last_nl = _first_unwritten;

  while (size)
    {
      // If doing output, we must be careful not to overwrite parts of the
      // circular write buffer that have not yet been written to the output.
      // Therefore, we do not write everything at once, but divide processing
      // into batches.
      long max_batch_size = _output
        // The range from the write buffer's head to the first unwritten
        // character can be safely written to. Adjust that distance for the
        // possibility that we have to print a timestamp and write an
        // additional \r character for \n.
        ? w->distance(w->head(), _first_unwritten - 1) - Max_timestamp_len - 1
        // Not doing output, so no need to limit the maximum batch size.
        : LONG_MAX;

      long batch_size = 0;
      for (; batch_size < size && batch_size < max_batch_size; batch_size++)
        {
          if (_new_line && timestamp())
            {
              print_timestamp();
              max_batch_size -= Max_timestamp_len;
            }

          char c = *buf++;

          if (_attr.o_flags & L4_VCON_ONLCR && c == '\n')
            {
              w->put('\r');
              max_batch_size--;
            }

          if (_attr.o_flags & L4_VCON_OCRNL && c == '\r')
            c = '\n';

          if (_attr.o_flags & L4_VCON_ONLRET && c == '\r')
            continue;

          w->put(c);

          _new_line = c == '\n';
          if (_new_line)
            last_nl = w->head();
        }

      // Decrement size for characters processed from buf in this batch.
      size -= batch_size;

      if (_output)
        {
          Buf::Index write_until = w->head();
          // If line buffering is enabled only print complete lines, except when an
          // incomplete line spans the entire write buffer.
          if (_line_buffering && batch_size > 0)
            write_until = last_nl;

          // Output characters processed up to and in this batch.
          if (write_until != _first_unwritten)
            do_output(write_until);
        }
    }

  // If line buffering is enabled, and there is an incomplete line pending in
  // the write buffer, enqueue the line buffer timeout.
  if (_output && _line_buffering && w->head() != _first_unwritten && _sif)
    {
      _sif->remove_timeout(&_timeout);
      _sif->add_timeout(&_timeout,
                        l4_kip_clock(l4re_kip()) + _line_buffering_ms * 1000);
    }
}

void
Client::timeout_expired()
{
  do_output(wbuf()->head());
}
