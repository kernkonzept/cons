/*
 * (c) 2012-2013 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>,
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

// TODO: compressed output (maybe 'dump' command, or cat -z or so)
// TODO: store buffer compressed
// TODO: port libxz (http://tukaani.org/xz/)
// TODO: macro to inject some text into the read buffer

#include "mux_impl.h"
#include "frontend.h"
#include "client.h"
#include "controller.h"
#include "debug.h"
#include "vcon_client.h"
#include "vcon_fe.h"
#include "virtio_client.h"
#include "async_vcon_fe.h"
#include "registry.h"
#include "server.h"
#include "virtio_console_fe.h"

#include <l4/re/util/icu_svr>
#include <l4/re/util/vcon_svr>
#include <l4/re/util/object_registry>
#include <l4/re/util/br_manager>
#include <l4/re/error_helper>

#include <l4/sys/typeinfo_svr>
#include <l4/cxx/iostream>
#include <l4/sys/cxx/ipc_epiface>

#include <terminate_handler-l4>

#include <algorithm>
#include <set>
#include <getopt.h>

#include <l4/bid_config.h>

#ifdef CONFIG_CONS_USE_ASYNC_FE
typedef Async_vcon_fe Fe;
#else
typedef Vcon_fe Fe;
#endif

using L4Re::chksys;

struct Config_opts
{
  // Show output of all consoles.
  bool default_show_all;
  // By default, keep a console when a client disconnects.
  bool default_keep;
  // By default, merge client write requests to a single request containing an
  // entire line (until a newline is detected).
  bool default_line_buffering = true;
  // In line-buffered mode, timeout for write the client output even if no
  // newline was detected.
  unsigned default_line_buffering_ms = 50;
  // By default, show time stamps for all consoles.
  bool default_timestamp;
  // Currently unused.
  std::string auto_connect_console;
};

static Config_opts config;

static L4::Server<L4Re::Util::Br_manager_timeout_hooks> server;
static Registry registry(&server);

class My_mux : public Mux_i, public cxx::H_list_item
{
public:
  My_mux(Controller *ctl, char const *name) : Mux_i(ctl, name) {}

  void add_auto_connect_console(std::string const &name)
  {
    _auto_connect_consoles.insert(name);
  }

  bool is_auto_connect_console(std::string const &name)
  {
    return _auto_connect_consoles.find(name) != _auto_connect_consoles.end();
  }

private:
  std::set<std::string> _auto_connect_consoles;
};

class Cons_svr : public L4::Epiface_t<Cons_svr, L4::Factory, Server_object>
{
public:
  explicit Cons_svr(const char *name);

  bool collected() { return false; }

  template< typename CLI >
  int create(std::string const &tag, int color, CLI **, size_t bufsz,
             Client::Key key, bool line_buffering, unsigned line_buffering_ms);
  int op_create(L4::Factory::Rights, L4::Ipc::Cap<void> &obj,
                l4_mword_t proto, L4::Ipc::Varg_list_ref args);

  Controller *ctl() { return &_ctl; }
  void add(My_mux *m)
  {  _muxe.add(m); }

  int sys_msg(char const *fmt, ...) __attribute__((format(printf, 2, 3)));

private:
  typedef cxx::H_list<My_mux> Mux_list;
  typedef Mux_list::Iterator Mux_iter;
  Mux_list _muxe;
  Controller _ctl;
  Dbg _info;
  Dbg _err;
};

Cons_svr::Cons_svr(const char* name)
: _info(Dbg::Info, name), _err(Dbg::Err, name)
{
  Dbg::set_level(~0U);
}

int
Cons_svr::sys_msg(char const *fmt, ...)
{
  int r = 0;
  for (Mux_iter i = _muxe.begin(); i != _muxe.end(); ++i)
    {
      va_list args;
      va_start(args, fmt);
      r = i->vsys_msg(fmt, args);
      va_end(args);
    }
  return r;
}

template< typename CLI >
int
Cons_svr::create(std::string const &tag, int color, CLI **vout, size_t bufsz,
                 Client::Key key, bool line_buffering, unsigned line_buffering_ms)
{
  typedef Controller::Client_iter Client_iter;
  Client_iter c = std::find_if(_ctl.clients.begin(),
                               Client_iter(_ctl.clients.end()),
                               Client::Equal_key(key));
  if (c != _ctl.clients.end())
    sys_msg("WARNING: multiple clients with key '%c'\n", key.v());

  std::string name = tag.length() > 0 ? tag : "<noname>";

  auto it =
    std::find_if(_ctl.clients.rbegin(), _ctl.clients.rend(),
                 Client::Equal_tag(cxx::String(name.data(), name.length())));

  CLI *v = new CLI(name, color, bufsz, key, line_buffering, line_buffering_ms,
                   &registry, &server, &_ctl);
  if (!v)
    return -L4_ENOMEM;

  if (!registry.register_obj(v))
    {
      delete v;
      return -L4_ENOMEM;
    }

  if (it != _ctl.clients.rend())
    {
      sys_msg("WARNING: multiple clients with tag '%s'\n", name.c_str());
      v->idx = (*it)->idx + 1;
      _ctl.clients.push_back(v);
    }
  else
    _ctl.clients.push_back(v);

  sys_msg("Created vcon channel: %s [%lx]\n",
          v->tag().c_str(), v->obj_cap().cap());

  *vout = v;
  return 0;
}

int
Cons_svr::op_create(L4::Factory::Rights, L4::Ipc::Cap<void> &obj,
                    l4_mword_t proto, L4::Ipc::Varg_list_ref args)
{
  switch (proto)
    {
    case (l4_mword_t)L4_PROTO_LOG:
    case 1:
        {
          // copied from moe/server/src/alloc.cc

          L4::Ipc::Varg tag = args.pop_front();

          if (!tag.is_of<char const *>())
            return -L4_EINVAL;

          L4::Ipc::Varg col = args.pop_front();

          int color;
          if (col.is_of<char const *>())
            {
              cxx::String cs(col.value<char const*>(), col.length() - 1);

              int c = 7, bright = 0;
              if (!cs.empty())
                {
                  switch (cs[0])
                    {
                    case 'N': bright = 1; /* FALLTHRU */ case 'n': c = 0; break;
                    case 'R': bright = 1; /* FALLTHRU */ case 'r': c = 1; break;
                    case 'G': bright = 1; /* FALLTHRU */ case 'g': c = 2; break;
                    case 'Y': bright = 1; /* FALLTHRU */ case 'y': c = 3; break;
                    case 'B': bright = 1; /* FALLTHRU */ case 'b': c = 4; break;
                    case 'M': bright = 1; /* FALLTHRU */ case 'm': c = 5; break;
                    case 'C': bright = 1; /* FALLTHRU */ case 'c': c = 6; break;
                    case 'W': bright = 1; /* FALLTHRU */ case 'w': c = 7; break;
                    default: c = 0;
                    }
                }

              color = (bright << 3) | c;

            }
          else if (col.is_of_int())
            color = col.value<l4_mword_t>();
          else
            color = 7;

          // length() > 0 for a valid string parameter
          std::string ts(tag.value<char const*>(), tag.length() - 1);

          bool show = config.default_show_all;
          bool keep = config.default_keep;
          bool line_buffering = config.default_line_buffering;
          unsigned line_buffering_ms = config.default_line_buffering_ms;
          bool timestamp = config.default_timestamp;
          Client::Key key;
          size_t bufsz = 0;

          for (L4::Ipc::Varg opts: args)
            {
              if (opts.is_of<char const *>())
                {
                  cxx::String cs(opts.value<char const *>(), opts.length() - 1);

                  if (cs == "hide")
                    show = false;
                  else if (cs == "show")
                    show = true;
                  else if (cs == "keep")
                    keep = true;
                  else if (cs == "no-keep")
                    keep = false;
                  else if (cs == "line-buffering")
                    line_buffering = true;
                  else if (cs == "no-line-buffering")
                    line_buffering = false;
                  else if (cxx::String::Index t = cs.starts_with("line-buffered-ms="))
                    cs.substr(t).from_dec(&line_buffering_ms);
                  else if (cs == "timestamp")
                    timestamp = true;
                  else if (cs == "no-timestamp")
                    timestamp = false;
                  else if (cxx::String::Index k = cs.starts_with("key="))
                    key = *k;
                  else if (cxx::String::Index v = cs.starts_with("bufsz="))
                    cs.substr(v).from_dec(&bufsz);
                }
            }

          Client *v;
          L4::Cap<void> v_cap;
          if (proto == 1)
            {
              Virtio_cons *_v;
              if (int r = create(ts, color, &_v, bufsz, key,
                                 line_buffering, line_buffering_ms))
                return r;
              v = _v;
              v_cap = _v->obj_cap();
            }
          else
            {
              Vcon_client *_v;
              if (int r = create(ts, color, &_v, bufsz, key,
                                 line_buffering, line_buffering_ms))
                return r;
              v = _v;
              v_cap = _v->obj_cap();
            }

          if (show && _muxe.front())
            _muxe.front()->show(v);

          if (keep)
            v->keep(v);

          v->timestamp(timestamp);

          for (Mux_iter i = _muxe.begin(); i != _muxe.end(); ++i)
            {
              if (i->is_auto_connect_console(ts))
                {
                  i->connect(v);
                  break;
                }
            }

          obj = L4::Ipc::make_cap(v_cap, L4_CAP_FPAGE_RWSD);
          return L4_EOK;
        }
      break;
    default:
      return -L4_ENOSYS;
    }
}


int main(int argc, char const *argv[])
{
  printf("Console Server\n");

  enum
  {
    OPT_SHOW_ALL = 'a',
    OPT_MUX = 'm',
    OPT_FE = 'f',
    OPT_VFE = 'V',
    OPT_KEEP = 'k',
    OPT_NO_LINE_BUFFERING = 'l',
    OPT_LINE_BUFFERING_MS = 1,
    OPT_TIMESTAMP = 't',
    OPT_AUTOCONNECT = 'c',
    OPT_DEFAULT_NAME = 'n',
    OPT_DEFAULT_BUFSIZE = 'B',
  };

  static option opts[] =
  {
    { "show-all",          no_argument,       0, OPT_SHOW_ALL },
    { "mux",               required_argument, 0, OPT_MUX },
    { "frontend",          required_argument, 0, OPT_FE },
    { "virtio-device-frontend", required_argument, 0, OPT_VFE },
    { "keep",              no_argument,       0, OPT_KEEP },
    { "no-line-buffering", no_argument,       0, OPT_NO_LINE_BUFFERING },
    { "line-buffering-ms", required_argument, 0, OPT_LINE_BUFFERING_MS },
    { "timestamp",         no_argument,       0, OPT_TIMESTAMP },
    { "autoconnect",       required_argument, 0, OPT_AUTOCONNECT },
    { "defaultname",       required_argument, 0, OPT_DEFAULT_NAME },
    { "defaultbufsize",    required_argument, 0, OPT_DEFAULT_BUFSIZE },
    { 0, 0, 0, 0 },
  };

  Cons_svr *cons = new Cons_svr("cons");

  if (!registry.register_obj(cons, "cons"))
    {
      printf("Registering 'cons' server failed!\n");
      return 1;
    }

  My_mux *current_mux = 0;
  Fe *current_fe = 0;
  typedef std::set<std::string> Str_vector;
  Str_vector ac_consoles;
  const char *default_name = "cons";

  while (1)
    {
      int optidx = 0;
      int c = getopt_long(argc, const_cast<char *const*>(argv),
                          "am:f:klc:n:B:tV:", opts, &optidx);
      if (c == -1)
        break;

      switch (c)
        {
        case OPT_SHOW_ALL:
          config.default_show_all = true;
          break;
        case OPT_MUX:
          current_mux = new My_mux(cons->ctl(), optarg);
          cons->add(current_mux);
          if (!ac_consoles.empty())
            printf("WARNING: Ignoring all previous auto-connect-consoles.\n");
          break;
        case OPT_FE:
          if (!current_mux)
            {
              printf("ERROR: need to instantiate a muxer (--mux) before\n"
                     "using the --frontend option.\n");
              break;
            }

            {
              L4::Cap<L4::Vcon> cap
                = L4Re::Env::env()->get_cap<L4::Vcon>(optarg);

              if (!cap)
                {
                  printf("ERROR: Frontend capability '%s' invalid.\n", optarg);
                  break;
                }

              current_fe = new Fe(cap, &registry);
              current_mux->add_frontend(current_fe);
            }
          break;
        case OPT_VFE:
          if (!current_mux)
            {
              printf("ERROR: need to instantiate a muxer (--mux) before\n"
                     "using the --virtio-device-frontend option.\n");
              break;
            }
          {
            auto fe = new Virtio_console_fe(&registry);
            auto cap = registry.register_obj(fe, optarg);

            L4Re::chkcap(cap, "Could not register virtio_console "
                              "device frontend\n");
            current_mux->add_frontend(fe);
          }
          break;
        case OPT_KEEP:
          config.default_keep = true;
          break;
        case OPT_TIMESTAMP:
          config.default_timestamp = true;
          break;
        case OPT_AUTOCONNECT:
          if (current_mux)
            current_mux->add_auto_connect_console(optarg);
          else
            ac_consoles.insert(optarg);
          break;
        case OPT_DEFAULT_NAME:
          default_name = optarg;
          break;
        case OPT_DEFAULT_BUFSIZE:
          Vcon_client::default_obuf_size(atoi(optarg));
          break;
        case OPT_NO_LINE_BUFFERING:
          config.default_line_buffering = false;
          break;
        case OPT_LINE_BUFFERING_MS:
          config.default_line_buffering_ms = atoi(optarg);
          break;
        }
    }

  // now check if we had any explicit options
  if (!current_mux)
    {
      current_mux = new My_mux(cons->ctl(), default_name);
      cons->add(current_mux);
      current_fe = new Fe(L4Re::Env::env()->log(), &registry);
      current_mux->add_frontend(current_fe);
      for (Str_vector::const_iterator i = ac_consoles.begin();
           i != ac_consoles.end(); ++i)
        current_mux->add_auto_connect_console(*i);
    }

  ac_consoles.clear();

  server.loop<L4::Runtime_error>(&registry);
  return 0;
}
