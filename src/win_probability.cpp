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

#include "win_probability.h"
#include <cmath>

using namespace Judas::WDLModel;

// 8001 * 62 = 496062
#define WIN_PROBABILITY_SIZE 496062

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

uint8_t win_probabilities[WIN_PROBABILITY_SIZE];

inline size_t index(const Judas::Value v, const int m) {
    assert(v >= -4000 && v <= 4000);
    assert(m >= 17 && m <= 78);

    return (v + 4000) * 62 + m - 17;
}

void Judas::WDLModel::init() {
    for (int valueClamp = -4000; valueClamp <= 4000; ++valueClamp)
    {
        for (int materialClamp = 17; materialClamp <= 78; ++materialClamp)
        {
            win_probabilities[index(valueClamp, materialClamp)] =
              UCIEngine::getWinProbability(valueClamp, materialClamp);
        }
    }
}
uint8_t Judas::WDLModel::get_win_probability_by_material(const Value value,
                                                              const int   materialClamp) {
    const auto valueClamp = std::clamp(static_cast<double>(value), -4000.0, 4000.0);

    return win_probabilities[index(static_cast<int>(valueClamp), materialClamp)];
}

uint8_t Judas::WDLModel::get_win_probability(const Value value, const Position& pos) {
    const int material = pos.count<PAWN>() + 3 * pos.count<KNIGHT>() + 3 * pos.count<BISHOP>()
                       + 5 * pos.count<ROOK>() + 9 * pos.count<QUEEN>();
    // The fitted model only uses data for material counts in [17, 78], and is anchored at count 58.
    const int materialClamp = std::clamp(material, 17, 78);
    return get_win_probability_by_material(value, materialClamp);
}

// Old
inline double win_rate_to_move(const long value, const int full_moves) {
    return round(
      1000
      / (1
         + std::exp((((-1.06249702 * (std::clamp(full_moves, 8, 120) / 32.0) + 7.42016937)
                        * (std::clamp(full_moves, 8, 120) / 32.0)
                      + 0.89425629)
                       * (std::clamp(full_moves, 8, 120) / 32.0)
                     + 348.60356174 - static_cast<double>(value))
                    / (((-5.33122190 * (std::clamp(full_moves, 8, 120) / 32.0) + 39.57831533)
                          * (std::clamp(full_moves, 8, 120) / 32.0)
                        + -90.84473771)
                         * (std::clamp(full_moves, 8, 120) / 32.0)
                       + 123.40620748))));
}

inline double win_rate_opponent(const long value, const int full_moves) {
    return round(
      1000
      / (1
         + std::exp((((-1.06249702 * (std::clamp(full_moves, 8, 120) / 32.0) + 7.42016937)
                        * (std::clamp(full_moves, 8, 120) / 32.0)
                      + 0.89425629)
                       * (std::clamp(full_moves, 8, 120) / 32.0)
                     + 348.60356174 - std::clamp(static_cast<double>(-value), -4000.0, 4000.0))
                    / (((-5.33122190 * (std::clamp(full_moves, 8, 120) / 32.0) + 39.57831533)
                          * (std::clamp(full_moves, 8, 120) / 32.0)
                        - 90.84473771)
                         * (std::clamp(full_moves, 8, 120) / 32.0)
                       + 123.40620748))));
}

uint8_t Judas::WDLModel::get_win_probability(const Value value, const int plies) {
    const auto full_moves = plies / 2 + 1;

    const double win      = win_rate_to_move(value, full_moves);
    const double opponent = win_rate_opponent(value, full_moves);
    const double draw     = 1000 - win - opponent;

    const double win_probability = round(win + draw / 2.0) / 10.0;

    return static_cast<int>(round(win_probability));
}

#undef WIN_PROBABILITY_SIZE

#undef MAX
#undef MIN