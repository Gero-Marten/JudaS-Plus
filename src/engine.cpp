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

#include "engine.h"

#include <cassert>
#include <deque>
#include <iosfwd>
#include <memory>
#include <ostream>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

#include "evaluate.h"
#include "misc.h"
#include "nnue/network.h"
#include "nnue/nnue_common.h"
#include "perft.h"
#include "position.h"
#include "search.h"
#include "syzygy/tbprobe.h"
#include "types.h"
#include "uci.h"
#include "ucioption.h"
#include "learn/learn.h"
#include "book/book.h"

namespace Judas {

namespace NN = Eval::NNUE;

// Initialize global variables
GameStyle style = Default;          // Default game style

constexpr auto StartFEN  = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
constexpr int  MaxHashMB = Is64Bit ? 33554432 : 2048;

Engine::Engine(std::optional<std::string> path) :
    binaryDirectory(CommandLine::get_binary_directory(
      path.value_or(""), CommandLine::get_working_directory())),
    numaContext(NumaConfig::from_system()),
    states(new std::deque<StateInfo>(1)),
    threads(),
    networks(
      numaContext,
      NN::Networks(
        NN::NetworkBig({EvalFileDefaultNameBig, "None", ""}, NN::EmbeddedNNUEType::BIG),
        NN::NetworkSmall({EvalFileDefaultNameSmall, "None", ""}, NN::EmbeddedNNUEType::SMALL))) {
    pos.set(StartFEN, false, &states->back());

    options["Debug Log File"] << Option("", [](const Option& o) {
        start_logger(o);
        return std::nullopt;
    });

    options["NumaPolicy"] << Option("auto", [this](const Option& o) {
        set_numa_config_from_option(o);
        return numa_config_information_as_string() + "\n"
             + thread_allocation_information_as_string();
    });

    options["Threads"] << Option(1, 1, 1024, [this](const Option&) {
        resize_threads();
        return thread_allocation_information_as_string();
    });

    options["Hash"] << Option(16, 1, MaxHashMB, [this](const Option& o) {
        set_tt_size(o);
        return std::nullopt;
    });

    options["Clear Hash"] << Option([this](const Option&) {
        search_clear();
        return std::nullopt;
    });
    options["Ponder"] << Option(false);
    options["MultiPV"] << Option(1, 1, 500);
    options["Skill Level"] << Option(20, 0, 20);
    options["Move Overhead"] << Option(10, 0, 5000);
    options["Minimum Thinking Time"] << Option(100, 0, 5000);  //minimum thining time
    options["nodestime"] << Option(0, 0, 10000);
    options["UCI_Chess960"] << Option(false);
    options["UCI_LimitStrength"] << Option(false);
    options["UCI_Elo"] << Option(Judas::Search::Skill::LowestElo,
                                 Judas::Search::Skill::LowestElo,
                                 Judas::Search::Skill::HighestElo);
    options["UCI_ShowWDL"] << Option(false);
    options["Book File"]
    << Option(EMPTY, [this](const Option&) {
        init_bookMan(0);
        return std::nullopt;
    });
    options["Book Width"] << Option(1, 1, 20);
    options["Book Depth"] << Option(255, 1, 255);
    options["SyzygyPath"] << Option("", [](const Option& o) {
        Tablebases::init(o);
        return std::nullopt;
    });
    options["SyzygyProbeDepth"] << Option(1, 1, 100);
    options["Syzygy50MoveRule"] << Option(true);
    options["SyzygyProbeLimit"] << Option(7, 0, 7);

    options["Select Style"] << Option(
        "Default var Default var Aggressive var Defensive var Positional", "Default", // Default value
        [](const Option& o) -> std::optional<std::string> {
        std::string selectedStyle = static_cast<std::string>(o);

        if (selectedStyle == "Aggressive") {
            style = Aggressive;
        } else if (selectedStyle == "Defensive") {
            style = Defensive;
        } else if (selectedStyle == "Positional") {
            style = Positional;
        } else {
            style = Default;
        }

        // Send a message to the GUI using the UCI command "info string"
        sync_cout << "info string Style set to: " << selectedStyle << sync_endl;

        return std::optional<std::string>{};
    });

    options["Exploration Mode"] << Option("Off var On var Off", "Off",
        [](const Option& o) -> std::optional<std::string> {
        if (o == "On")
            explorationEnabled = true;
        else
            explorationEnabled = false;

        // Send a message to the GUI using the UCI command "info string"
        sync_cout << "info string Exploration Mode set to: "
                  << (explorationEnabled ? "On" : "Off") << sync_endl;

        return std::nullopt;
    });

	options["EvalFile"] << Option(EvalFileDefaultNameBig, [this](const Option& o) {
        load_big_network(o);
        return std::nullopt;
    });
    options["EvalFileSmall"] << Option(EvalFileDefaultNameSmall, [this](const Option& o) {
        load_small_network(o);
        return std::nullopt;
    });
    options["Read only learning"] << Option(false, [](const Option& o) {
        LD.set_readonly(o);
        return std::nullopt;
    });
    options["Learning Mode"] << Option("Experience var Experience var Self", "Experience",
                                       [this](const Option& o) -> std::optional<std::string> {
                                           if (o == "Experience") {
                                               sync_cout << "info string Learning Mode set to 'Experience'." << sync_endl;
                                               LD.set_learning_mode(get_options(), o);  // Abilita Experience
                                           } else if (o == "Self") {
                                               sync_cout << "info string Learning Mode set to 'Self' (Q-learning)." << sync_endl;
                                               LD.set_learning_mode(get_options(), o);  // Abilita Q-learning
                                           }
                                           return std::optional<std::string>{};
    });
    options["Experience Book"] << Option(true, [this](const Option& opt) {
    bool enabled = opt;
    
    // Send a message to the GUI to notify the enable/disable status
    std::cout << "info string Experience Book " 
              << (enabled ? "enabled" : "disabled") << std::endl;
    
    // Initialize LD only if option is enabled
    if (enabled) {
        LD.init(get_options());
    }
    return std::nullopt;
});

options["Experience Book Max Moves"] << Option(20, 1, 50); // Default: 20, Range: 1-50
options["Experience Book Min Depth"] << Option(6, 1, 40);  // Default: 6, Range: 1-40
options["Experience Book Width"] << Option(3, 1, 10);      // Default: 3, Range: 1-10
options["Experience Book Min Performance"] << Option(30, 10, 100); // Default: 30, Range: 10-100
options["Experience Book Min Quality"] << Option(50, 0, 100, [](const Option& opt) {
    std::cout << "info string Min Quality set to " << opt << std::endl;
    return std::nullopt;
});
options["Experience Book Logging"] << Option(false, [](const Option& opt) {
    bool enabled = opt;
    // Send a message to the GUI to notify the enable/disable status
    std::cout << "info string Experience Book Logging "
              << (enabled ? "enabled" : "disabled") << std::endl;

    return std::nullopt;
});

    options["Concurrent Experience"]
      << Option(false);
    load_networks();
    resize_threads();
}

std::uint64_t Engine::perft(const std::string& fen, Depth depth, bool isChess960) {
    verify_networks();

    return Benchmark::perft(fen, depth, isChess960);
}

void Engine::go(Search::LimitsType& limits) {
    assert(limits.perft == 0);
    verify_networks();

    threads.start_thinking(options, pos, states, limits);
}
void Engine::stop() { threads.stop = true; }

void Engine::search_clear() {
    wait_for_search_finished();

    tt.clear(threads);
    threads.clear();

    // @TODO wont work with multiple instances
    Tablebases::init(options["SyzygyPath"]);  // Free mapped files
}

void Engine::set_on_update_no_moves(std::function<void(const Engine::InfoShort&)>&& f) {
    updateContext.onUpdateNoMoves = std::move(f);
}

void Engine::set_on_update_full(std::function<void(const Engine::InfoFull&)>&& f) {
    updateContext.onUpdateFull = std::move(f);
}

void Engine::set_on_iter(std::function<void(const Engine::InfoIter&)>&& f) {
    updateContext.onIter = std::move(f);
}

void Engine::set_on_bestmove(std::function<void(std::string_view, std::string_view)>&& f) {
    updateContext.onBestmove = std::move(f);
}

void Engine::set_on_verify_networks(std::function<void(std::string_view)>&& f) {
    onVerifyNetworks = std::move(f);
}

void Engine::wait_for_search_finished() { threads.main_thread()->wait_for_search_finished(); }

void Engine::set_position(const std::string& fen, const std::vector<std::string>& moves) {
    // Drop the old state and create a new one
    states = StateListPtr(new std::deque<StateInfo>(1));
    pos.set(fen, options["UCI_Chess960"], &states->back());

    for (const auto& move : moves)
    {
        auto m = UCIEngine::to_move(pos, move);

        if (m == Move::none())
            break;
        if (LD.is_enabled() && LD.learning_mode() != LearningMode::Self && !LD.is_paused())
        {
            PersistedLearningMove persistedLearningMove;

            persistedLearningMove.key                      = pos.key();
            persistedLearningMove.learningMove.depth       = pos.calculate_depth();  // Metodo alternativo
															   
            persistedLearningMove.learningMove.score       = pos.evaluate_position();  // Metodo alternativo
            
            // Calcolo dinamico di "performance"
            persistedLearningMove.learningMove.performance =
                std::clamp(persistedLearningMove.learningMove.depth * 10 +
                           (persistedLearningMove.learningMove.score / 100), 0, 100);

            // Calcolo dinamico di "Quality"
            const int Quality = std::clamp(
                persistedLearningMove.learningMove.depth * 15 +
                (persistedLearningMove.learningMove.score / 50), 0, 100);

            // Aggiunta dei dati di apprendimento
            LD.add_new_learning(persistedLearningMove.key, persistedLearningMove.learningMove);

            // (Opzionale) Log
            if (options["Experience Book Logging"]) {
                std::cout << "info string Added learning move: "
                          << "Depth=" << persistedLearningMove.learningMove.depth
                          << ", Score=" << persistedLearningMove.learningMove.score
                          << ", Performance=" << persistedLearningMove.learningMove.performance
                          << ", Quality=" << Quality << std::endl;
            }
        }
        states->emplace_back();
        pos.do_move(m, states->back());
    }
}

// modifiers

void Engine::set_numa_config_from_option(const std::string& o) {
    if (o == "auto" || o == "system")
    {
        numaContext.set_numa_config(NumaConfig::from_system());
    }
    else if (o == "hardware")
    {
        // Don't respect affinity set in the system.
        numaContext.set_numa_config(NumaConfig::from_system(false));
    }
    else if (o == "none")
    {
        numaContext.set_numa_config(NumaConfig{});
    }
    else
    {
        numaContext.set_numa_config(NumaConfig::from_string(o));
    }

    // Force reallocation of threads in case affinities need to change.
    resize_threads();
    threads.ensure_network_replicated();
}

void Engine::resize_threads() {
    threads.wait_for_search_finished();
    threads.set(numaContext.get_numa_config(), {bookMan, options, threads, tt, networks},
                updateContext);

    // Reallocate the hash with the new threadpool size
    set_tt_size(options["Hash"]);
    threads.ensure_network_replicated();
}
void Engine::init_bookMan(int bookIndex) { bookMan.init(bookIndex, options); }

void Engine::set_tt_size(size_t mb) {
    wait_for_search_finished();
    tt.resize(mb, threads);
}

void Engine::set_ponderhit(bool b) { threads.main_manager()->ponder = b; }

// network related

void Engine::verify_networks() const {
    networks->big.verify(options["EvalFile"], onVerifyNetworks);
    networks->small.verify(options["EvalFileSmall"], onVerifyNetworks);
}

void Engine::load_networks() {
    networks.modify_and_replicate([this](NN::Networks& networks_) {
        networks_.big.load(binaryDirectory, options["EvalFile"]);
        networks_.small.load(binaryDirectory, options["EvalFileSmall"]);
    });
    threads.clear();
    threads.ensure_network_replicated();
}

void Engine::load_big_network(const std::string& file) {
    networks.modify_and_replicate(
      [this, &file](NN::Networks& networks_) { networks_.big.load(binaryDirectory, file); });
    threads.clear();
    threads.ensure_network_replicated();
}

void Engine::load_small_network(const std::string& file) {
    networks.modify_and_replicate(
      [this, &file](NN::Networks& networks_) { networks_.small.load(binaryDirectory, file); });
    threads.clear();
    threads.ensure_network_replicated();
}

void Engine::save_network(const std::pair<std::optional<std::string>, std::string> files[2]) {
    networks.modify_and_replicate([&files](NN::Networks& networks_) {
        networks_.big.save(files[0].first);
        networks_.small.save(files[1].first);
    });
}

// utility functions

void Engine::trace_eval() const {
    StateListPtr trace_states(new std::deque<StateInfo>(1));
    Position     p;
    p.set(pos.fen(), options["UCI_Chess960"], &trace_states->back());

    verify_networks();

    sync_cout << "\n" << Eval::trace(p, *networks) << sync_endl;
}

const OptionsMap& Engine::get_options() const { return options; }
OptionsMap&       Engine::get_options() { return options; }

std::string Engine::fen() const { return pos.fen(); }

void Engine::flip() { pos.flip(); }
void Engine::show_moves_bookMan(const Position& position) {
    bookMan.show_moves(position, options);
}
std::string Engine::visualize() const {
    std::stringstream ss;
    ss << pos;
    return ss.str();
}

int Engine::get_hashfull(int maxAge) const { return tt.hashfull(maxAge); }

std::vector<std::pair<size_t, size_t>> Engine::get_bound_thread_count_by_numa_node() const {
    auto                                   counts = threads.get_bound_thread_count_by_numa_node();
    const NumaConfig&                      cfg    = numaContext.get_numa_config();
    std::vector<std::pair<size_t, size_t>> ratios;
    NumaIndex                              n = 0;
    for (; n < counts.size(); ++n)
        ratios.emplace_back(counts[n], cfg.num_cpus_in_numa_node(n));
    if (!counts.empty())
        for (; n < cfg.num_numa_nodes(); ++n)
            ratios.emplace_back(0, cfg.num_cpus_in_numa_node(n));
    return ratios;
}

std::string Engine::get_numa_config_as_string() const {
    return numaContext.get_numa_config().to_string();
}

std::string Engine::numa_config_information_as_string() const {
    auto cfgStr = get_numa_config_as_string();
    return "Available processors: " + cfgStr;
}

std::string Engine::thread_binding_information_as_string() const {
    auto              boundThreadsByNode = get_bound_thread_count_by_numa_node();
    std::stringstream ss;
    if (boundThreadsByNode.empty())
        return ss.str();

    bool isFirst = true;

    for (auto&& [current, total] : boundThreadsByNode)
    {
        if (!isFirst)
            ss << ":";
        ss << current << "/" << total;
        isFirst = false;
    }

    return ss.str();
}

std::string Engine::thread_allocation_information_as_string() const {
    std::stringstream ss;

    size_t threadsSize = threads.size();
    ss << "Using " << threadsSize << (threadsSize > 1 ? " threads" : " thread");

    auto boundThreadsByNodeStr = thread_binding_information_as_string();
    if (boundThreadsByNodeStr.empty())
        return ss.str();

    ss << " with NUMA node thread binding: ";
    ss << boundThreadsByNodeStr;

    return ss.str();
}

}
