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

#ifndef LEARN_H_INCLUDED
#define LEARN_H_INCLUDED

#include <unordered_map>
#include "../types.h"
#include "../ucioption.h"
#include "../position.h"

enum class LearningMode {
    Off      = 1,
    Standard = 2,
    Self     = 3,
};

struct LearningMove {
    Judas::Depth depth       = 0;
    Judas::Value score       = Judas::VALUE_NONE;
    Judas::Move  move        = Judas::Move::none();
    int               performance = 100;
};

struct PersistedLearningMove {
    Judas::Key key{};
    LearningMove    learningMove;
};

struct QLearningMove {
    PersistedLearningMove persistedLearningMove;
    int                   materialClamp;
};

class LearningData {
    bool         isPaused;
    bool         isReadOnly;
    bool         needPersisting;
    LearningMode learningMode;

    std::unordered_multimap<Judas::Key, LearningMove*> HT;
    std::vector<void*>                                      mainDataBuffers;
    std::vector<void*>                                      newMovesDataBuffers;
    bool                                                    load(const std::string& filename);
    void insert_or_update(PersistedLearningMove* plm, bool qLearning);

   public:
    LearningData();
    ~LearningData();

    void               pause();
    void               resume();
    [[nodiscard]] bool is_paused() const { return isPaused; };

    void quick_reset_exp();
  void set_learning_mode(Judas::OptionsMap& options, const std::string& lm);
    [[nodiscard]] LearningMode learning_mode() const;
    [[nodiscard]] bool         is_enabled() const { return learningMode != LearningMode::Off; }

    void               set_readonly(bool ro) { isReadOnly = ro; }
    [[nodiscard]] bool is_readonly() const { return isReadOnly; }

    void clear();
    void init(Judas::OptionsMap& o);
    void persist(const Judas::OptionsMap& o);

    void add_new_learning(Judas::Key key, const LearningMove& lm);

    int probeByMaxDepthAndScore(Judas::Key key, const LearningMove*& learningMove);
    const LearningMove* probe_move(Judas::Key key, Judas::Move move);
    std::vector<LearningMove*> probe(Judas::Key key);
    static void                sortLearningMoves(std::vector<LearningMove*>& learningMoves);
    static void                show_exp(const Judas::Position& pos);

    // Added method to access table
    const auto& get_table() const { return HT; }
};

extern LearningData LD;

#endif  // #ifndef LEARN_H_INCLUDED
