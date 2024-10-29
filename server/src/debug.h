/*
 * (c) 2012-2013 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#pragma once

#include <l4/re/util/debug>

class Dbg : public L4Re::Util::Dbg
{
public:
  enum Level
  {
    Info = 1,
    Warn = 2,
    Boot = 4,
    Err  = 8,
  };

  struct Dbg_bits { char const *n; unsigned long bits; };

  explicit
  Dbg(unsigned long mask, char const *subs = 0)
  : L4Re::Util::Dbg(mask, "Cons", subs)
  {}

  static Dbg_bits dbg_bits[];
};
