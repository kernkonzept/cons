/*
 * (c) 2012-2013 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#pragma once

#include "vcon_fe_base.h"

class Async_vcon_fe : public Vcon_fe_base
{
public:
  Async_vcon_fe(L4::Cap<L4::Vcon> con, L4Re::Util::Object_registry *r);
  int write(char const *buf, unsigned sz);

private:
  static void *_setup(void *);
  void *setup(void);
  bool _initialized;
};
