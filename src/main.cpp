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

#include <iostream>
#include <exception>
#include <iomanip>

#include "bitboard.h"
#include "misc.h"
#include "position.h"
#include "types.h"
#include "uci.h"
#include "tune.h"
#include "win_probability.h"
#include "learn/learn.h"

using namespace Judas;

int main(int argc, char* argv[]) {
    try {
        std::cout << "==========================================" << std::endl;
        std::cout << engine_info() << std::endl;
        std::cout << "Compiled: " << __DATE__ << " " << __TIME__ << std::endl;
        std::cout << "==========================================" << std::endl;

        WDLModel::init();
        Bitboards::init();
        Position::init();

        UCIEngine uci(argc, argv);
        LD.init(uci.engine_options());
        Tune::init(uci.engine_options());

        // Probing the experience file
        if ((bool)uci.engine_options()["Experience Book"]) {
            std::cout << "\n*** Probing Experience Book ***\n" << std::endl;
            try {
                // View the contents of the experience file
                const auto& expTable = LD.get_table();
                size_t entryCount = 0;

                for (const auto& [key, move] : expTable) {
                    entryCount++;
                    std::cout << "Entry " << entryCount << ": Key=" << key
                              << ", Score=" << move->score
                              << ", Depth=" << move->depth
                              << ", Performance=" << static_cast<int>(move->performance)
                              << std::endl;

                    if (entryCount >= 3) { // Limit to 10 entries to avoid too much output
                        std::cout << "...and more entries in the table..." << std::endl;
                        break;
                    }
                }

                std::cout << "\nTotal entries in experience book: " << entryCount << "\n" << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Error accessing experience book: " << e.what() << std::endl;
            }
        } else {
            std::cout << "\nExperience book is disabled.\n" << std::endl;
        }

        // Entering UCI mode
        uci.loop();

    } catch (const std::exception& e) {
        std::cerr << "Error during initialization: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown error during initialization." << std::endl;
        return 1;
    }

    return 0;
}
