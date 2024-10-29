/*
 * (c) 2012-2013 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#pragma once

#include <l4/cxx/hlist>
#include <l4/sys/cxx/ipc_epiface>

class Server_object
: public L4::Epiface,
  public cxx::H_list_item
{
public:
  virtual bool collected() = 0;
};
