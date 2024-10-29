/*
 * (c) 2012-2013 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#pragma once

#include "frontend.h"
#include "client.h"
#include "input_mux.h"
#include "output_mux.h"

class Mux : public Input_mux, public Output_mux
{
public:
  virtual void add_frontend(Frontend *) = 0;
};
