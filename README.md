<h1 align="center">JudaS ++</h1>

# JudaS ++  Overview


JudaS ++ is a free and strong UCI chess engine derived from Stockfish 
that analyzes chess positions and computes the optimal moves.

JudaS ++ does not include a graphical user interface (GUI) that is required 
to display a chessboard and to make it easy to input moves. These GUIs are 
developed independently from JudaS and are available online.

## UCI options

  * #### Self-Learning

JudaS implements a persisted learning algorithm, managing a file named experience.bin.

It is a collection of one or more positions stored with the following format (similar to in memory Brainlearn Transposition Table):

- _best move_
- _board signature (hash key)_
- _best move depth_
- _best move score_
- _best move performance_ , a new parameter you can calculate with any learning application supporting this specification.
This file is loaded in an hashtable at the engine load and updated each time the engine receive quit or stop uci command.
When BrainLearn starts a new game or when we have max 8 pieces on the chessboard, the learning is activated and the hash table updated each time the engine has a best score
at a depth >= 4 PLIES, according to JudaS aspiration window.

  * #### CTG/BIN Book File
    The file name of the first book file which could be a polyglot (BIN) or Chessbase (CTG) book. To disable this book, use: ```<empty>```
    If the book (CTG or BIN) is in a different directory than the engine executable, then configure the full path of the book file, example:
    ```C:\Path\To\My\Book.ctg``` or ```/home/username/path/to/book/bin```

  * #### Book Width
    The number of moves to consider from the book for the same position. To play best book move, set this option to 1. If a value ```n``` (greater than 1 is configured, the engine will pick **randomly** one of the top ```n``` moves available in the book for the given position

  * #### Book Depth
    The maximum number of moves to play from the book

  * #### Exploration Mode

The Exploration Mode introduces variability in the engine's move selection process. It applies small, random bonuses to secondary moves, encouraging the exploration of less obvious lines and enhancing the engine's creativity.


When enabled, the engine adjusts the priority of secondary moves by adding a random "exploration bonus."
The primary (best) move remains unaffected, ensuring reliability in critical positions.
This mode is particularly useful for analysis, training, or playing more unpredictable games.
How to Enable
Through the UCI option Exploration Mode in the GUI.
Possible values: On (enabled) or Off (disabled).
Default: Off.
Benefits
Encourages creative and diverse play.
Useful for discovering alternative strategies or move sequences.
Fully optional and can be turned on or off as needed.
  * #### Playing Styles

The Playing Styles feature allows users to customize the engine's behavior by selecting different styles of play. Each style affects how the engine evaluates positions and prioritizes moves, giving the engine a distinct personality during games or analysis.

  * #### Available Styles
  * #### Default: Balanced and neutral behavior.
  * #### Aggressive: Focuses on attacking moves, especially those targeting the opponent's king, and rewards advanced pawns.
  * #### Defensive: Prioritizes solid setups, penalizes isolated pawns, and rewards castling.
  * #### Positional: Rewards long-term strategic advantages, such as bishop pairs, rook activity on the seventh rank, and good pawn structure.
