/*
 * (c) 2012-2013 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#pragma once

#include "vcon_fe_base.h"

#include <l4/re/util/object_registry>

class Vcon_fe : public Vcon_fe_base
{
public:
  Vcon_fe(L4::Cap<L4::Vcon> con, L4Re::Util::Object_registry *r);
  int write(char const *buf, unsigned sz)
  { return do_write(buf, sz); }
};
