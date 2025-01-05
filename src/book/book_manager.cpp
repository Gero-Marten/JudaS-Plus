#include "../uci.h"
#include "polyglot/polyglot.h"
#include "ctg/ctg.h"
#include "book_manager.h"

namespace Judas {
BookManager::BookManager() {
    for (int i = 0; i < NumberOfBooks; ++i)
        books[i] = nullptr;
}

BookManager::~BookManager() {
    for (int i = 0; i < NumberOfBooks; ++i)
        delete books[i];
}

void BookManager::init(const OptionsMap& options) {
    for (size_t i = 0; i < NumberOfBooks; ++i)
        init(i, options);
}

void BookManager::init(int index, const OptionsMap& options) {
    assert(index < NumberOfBooks);

// Close previous book if any
delete books[0];  // Supponendo che stai usando solo il primo libro
books[0] = nullptr;

std::string filename = std::string(options["Book File"]);

    //Load new book
    if (Util::is_empty_filename(filename))
        return;

    //Create book object for the given book type
    std::string fn   = Util::map_path(filename);
    Book::Book* book = Book::Book::create_book(fn);
    if (book == nullptr)
    {
        sync_cout << "info string Unknown book type: " << filename << sync_endl;
        return;
    }

    //Open/Initialize the book
    if (!book->open(fn))
    {
        delete book;
        return;
    }

    books[index] = book;
}

Move BookManager::probe(const Position& pos, const OptionsMap& options) const {
    int moveNumber = 1 + pos.game_ply() / 2;
    Move bookMove = Move::none();

    // Check if the book has been uploaded
    if (books[0] != nullptr
        && int(options["Book Depth"]) >= moveNumber)
    {
        // Set the value of "Only Green" to true by default
        bool onlyGreen = true;

        // Searches the book with the "Only Green" option set to true
        bookMove = books[0]->probe(
            pos, size_t(int(options["Book Width"])),
            onlyGreen);
    }

    return bookMove;
}
void BookManager::show_moves(const Position& pos, const OptionsMap& options) const {
    std::cout << pos << std::endl << std::endl;

if (books[0] != nullptr) {
    std::cout << "Book (" << books[0]->type() << "): "
              << std::string(options["Book File"])
              << std::endl;
    books[0]->show_moves(pos);
} else {
    std::cout << "No book loaded." << std::endl;
}
    }
}
