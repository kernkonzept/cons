/*
 * (c) 2012-2013 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#pragma once

#include <l4/cxx/string>

class Input_mux
{
public:
  virtual void input(cxx::String const &buf) = 0;
  virtual ~Input_mux() = 0;
};

inline Input_mux::~Input_mux() {}
