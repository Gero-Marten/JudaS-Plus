/*
  JudaS, a UCI chess playing engine derived from Stockfish
  Copyright (C) 2004-2025 The Stockfish developers (see AUTHORS file)

  JudaS is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  JudaS is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef WIN_PROBABILITY_H
#define WIN_PROBABILITY_H
#include <cstdint>
#include "uci.h"

namespace Judas::WDLModel {

void    init();
uint8_t get_win_probability_by_material(const Value value, const int materialClamp);
uint8_t get_win_probability(Value value, const Position& pos);
uint8_t get_win_probability(Value value, int plies);

}
#endif  //WIN_PROBABILITY_H
