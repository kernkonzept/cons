/*
 * (c) 2012-2013 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#pragma once

#include <l4/cxx/hlist>
#include "input_mux.h"

class Frontend : public cxx::H_list_item
{
public:
  virtual int write(char const *buffer, unsigned size) = 0;
  void input_mux(Input_mux *m) { _input = m; }

  virtual ~Frontend() = 0;

  virtual bool check_input() = 0;

protected:
  Input_mux *_input;
};

inline Frontend::~Frontend() {}
