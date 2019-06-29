#ifndef LIBATAXX_POSITION_HPP
#define LIBATAXX_POSITION_HPP

#include <cstdint>
#include <sstream>
#include <string>
#include "constants.hpp"
#include "move.hpp"
#include "other.hpp"
#include "pv.hpp"

namespace libataxx {

enum Side : bool
{
    Black = 0,
    White
};

enum class Result : int
{
    BlackWin = 0,
    WhiteWin,
    Draw,
    None
};

struct Irreversible {
    std::uint64_t captured;
};

class Position {
   public:
    Position() {
        pieces[Side::Black] = sq_to_bb(Square::a7) | sq_to_bb(Square::g1);
        pieces[Side::White] = sq_to_bb(Square::a1) | sq_to_bb(Square::g7);
        gaps = sq_to_bb(Square::c3) | sq_to_bb(Square::c5) |
               sq_to_bb(Square::e3) | sq_to_bb(Square::e5);
        turn_ = Side::Black;
    }

    Position(const std::string &fen) {
        set_fen(fen);
    }

    void set_fen(const std::string &fen) {
        if (fen == "startpos") {
            return set_fen("x5o/7/2-1-2/7/2-1-2/7/o5x x 0 1");
        }

        // Clear board
        pieces[Side::Black] = 0ULL;
        pieces[Side::White] = 0ULL;
        gaps = 0ULL;
        turn_ = Side::Black;

        std::stringstream ss{fen};
        std::string word;
        int sq = static_cast<int>(Square::a7);

        // Position
        if (ss >> word) {
            for (const auto &c : word) {
                switch (c) {
                    case 'x':
                    case 'X':
                    case 'b':
                    case 'B':
                        pieces[Side::Black] ^= sq_to_bb(sq);
                        sq++;
                        break;
                    case 'o':
                    case 'O':
                    case 'w':
                    case 'W':
                        pieces[Side::White] ^= sq_to_bb(sq);
                        sq++;
                        break;
                    case '-':
                        gaps ^= sq_to_bb(sq);
                        sq++;
                        break;
                    case '1':
                    case '2':
                    case '3':
                    case '4':
                    case '5':
                    case '6':
                    case '7':
                        sq += c - '0';
                        break;
                    case '/':
                        sq -= 14;
                        break;
                    default:
                        return;
                }
            }
        }

        // Turn
        if (ss >> word) {
            if (word == "b" || word == "B" || word == "x" || word == "X") {
                turn_ = Side::Black;
            } else {
                turn_ = Side::White;
            }
        }
    }

    int legal_moves(Move *moves) const {
        if (gameover()) {
            return 0;
        }

        const Side us = turn_;
        const Side them = static_cast<Side>(!us);
        const std::uint64_t filled =
            pieces[Side::Black] | pieces[Side::White] | gaps;
        const std::uint64_t empty = Bitboards::All ^ filled;
        int num_moves = 0;

        // Single moves
        std::uint64_t singles = single_moves(pieces[turn_]) & empty;
        while (singles) {
            assert(num_moves < max_moves);
            const Square to = static_cast<Square>(lsbll(singles));
            moves[num_moves] = Move(to);
            assert(moves[num_moves].type() == MoveType::Single);
            num_moves++;
            singles &= singles - 1;
        }

        // Double moves
        std::uint64_t copy = pieces[turn_];
        while (copy) {
            const Square from = static_cast<Square>(lsbll(copy));
            std::uint64_t destinations = double_moves(from) & empty;
            while (destinations) {
                assert(num_moves < max_moves);
                const Square to = static_cast<Square>(lsbll(destinations));
                moves[num_moves] = Move(from, to);
                assert(moves[num_moves].type() == MoveType::Double);
                num_moves++;
                destinations &= destinations - 1;
            }

            copy &= copy - 1;
        }

        if (num_moves == 0) {
            moves[0] = nullmove;
            num_moves++;
        }

        for (int i = 0; i < num_moves; ++i) {
            assert(legal_move(moves[i]));
        }

        assert(count_moves() == num_moves);

        return num_moves;
    }

    [[nodiscard]] int score() const {
        return popcountll(pieces[Side::Black]) -
               popcountll(pieces[Side::White]);
    }

    [[nodiscard]] const std::string fen() const {
        std::string fen = "";
        int empty = 0;

        // Board
        int sq = 42;
        while (sq >= 0) {
            const std::uint64_t bb = sq_to_bb(sq);

            if (bb & pieces[Side::Black]) {
                if (empty) {
                    fen += std::to_string(empty);
                    empty = 0;
                }
                fen += "x";
            } else if (bb & pieces[Side::White]) {
                if (empty) {
                    fen += std::to_string(empty);
                    empty = 0;
                }
                fen += "o";
            } else if (bb & gaps) {
                if (empty) {
                    fen += std::to_string(empty);
                    empty = 0;
                }
                fen += "-";
            } else {
                empty += 1;
            }

            if (sq_to_file(sq) == 6) {
                sq -= 14;
                if (empty) {
                    fen += std::to_string(empty);
                    empty = 0;
                }
                if (sq >= -1) {
                    fen += "/";
                }
            }
            sq++;
        }

        // Turn
        if (turn_ == Side::Black) {
            fen += " x";
        } else {
            fen += " o";
        }

        return fen;
    }

    void print() const {
        int sq = 42;
        while (sq >= 0) {
            const std::uint64_t bb = sq_to_bb(sq);

            if (bb & pieces[Side::Black]) {
                std::cout << "x";
            } else if (bb & pieces[Side::White]) {
                std::cout << "o";
            } else if (bb & gaps) {
                std::cout << " ";
            } else {
                std::cout << "-";
            }

            if (sq_to_file(sq) == 6) {
                sq -= 14;
                std::cout << std::endl;
            }
            sq++;
        }
        std::cout << "Turn: " << (turn_ == Side::Black ? "b" : "w")
                  << std::endl;
        std::cout << "FEN: " << fen() << std::endl;
    }

    [[nodiscard]] bool legal_move(const Move &move) const {
        if (move == nullmove) {
            return !can_move() && !gameover();
        }

        const Square from = move.from();
        const Square to = move.to();
        const std::uint64_t filled =
            pieces[Side::Black] | pieces[Side::White] | gaps;

        // Make sure the destination square is empty
        if (sq_to_bb(to) & filled) {
            return false;
        }

        // Single moves
        if (move.type() == MoveType::Single) {
            return single_moves(sq_to_bb(to)) & pieces[turn_];
        }
        // Double moves
        else {
            return double_moves(to) & pieces[turn_] & sq_to_bb(from);
        }
    }

    [[nodiscard]] bool legal_pv(const PV &pv) const {
        Position npos = *this;
        for (const auto &move : pv) {
            if (!npos.legal_move(move)) {
                return false;
            }
            npos.makemove(move);
        }
        return true;
    }

    void makemove(const Move &move) {
        if (!legal_move(move)) {
            return;
        }

        if (move == nullmove) {
            history_.push_back(move);
            irreversible_.push_back(Irreversible{.captured = 0ULL});
            assert(history_.size() == irreversible_.size());
            turn_ = static_cast<Side>(!turn_);
            return;
        }

        const Side us = turn_;
        const Side them = static_cast<Side>(!us);
        const int to = move.to();
        const int from = move.from();
        const std::uint64_t to_bb = sq_to_bb(to);
        const std::uint64_t from_bb = sq_to_bb(from);
        const std::uint64_t neighbours = single_moves(to_bb);
        const std::uint64_t captured = neighbours & pieces[them];

        // Remove and replace our stone
        pieces[us] ^= from_bb | to_bb;

        // Flip any captured stones
        pieces[them] ^= captured;
        pieces[us] ^= captured;

        turn_ = them;

        history_.push_back(move);
        irreversible_.push_back(Irreversible{.captured = captured});
        assert(history_.size() == irreversible_.size());
    }

    void undomove() {
        assert(history_.size() > 0);
        assert(history_.size() == irreversible_.size());
        const Move move = history_.back();
        const Irreversible irreversible = irreversible_.back();

        if (move == nullmove) {
            turn_ = static_cast<Side>(!turn_);
            history_.pop_back();
            irreversible_.pop_back();
            return;
        }

        const Side us = static_cast<Side>(!turn_);
        const Side them = turn_;
        const int to = move.to();
        const int from = move.from();
        const std::uint64_t to_bb = sq_to_bb(to);
        const std::uint64_t from_bb = sq_to_bb(from);
        const std::uint64_t neighbours = single_moves(to_bb);
        const std::uint64_t captured = irreversible.captured;

        // Remove and replace our stone
        pieces[us] ^= from_bb | to_bb;

        // Flip any captured stones
        pieces[them] ^= captured;
        pieces[us] ^= captured;

        turn_ = us;

        history_.pop_back();
        irreversible_.pop_back();
    }

    [[nodiscard]] Result result() const {
        if (!gameover()) {
            return Result::None;
        }
        const int s = score();
        if (s > 0) {
            return Result::BlackWin;
        } else if (s < 0) {
            return Result::WhiteWin;
        } else {
            return Result::Draw;
        }
    }

    [[nodiscard]] bool gameover() const {
        const std::uint64_t both = pieces[Side::Black] | pieces[Side::White];
        const std::uint64_t empty = Bitboards::All ^ (both | gaps);
        const std::uint64_t moves = single_moves(single_moves(both));

        if (!pieces[Side::Black]) {
            return true;
        }
        if (!pieces[Side::White]) {
            return true;
        }
        if (moves & empty) {
            return false;
        }
        return true;
    }

    [[nodiscard]] std::uint64_t perft(const int depth) {
        if (depth == 1) {
            return count_moves();
        } else if (depth <= 0) {
            return 1ULL;
        }

        std::uint64_t nodes = 0ULL;

        libataxx::Move moves[max_moves];
        const int num_moves = legal_moves(moves);
        for (int i = 0; i < num_moves; ++i) {
            makemove(moves[i]);
            nodes += perft(depth - 1);
            undomove();
        }

        return nodes;
    }

    [[nodiscard]] const PV &history() const {
        return history_;
    }

    int count_moves() const {
        if (gameover()) {
            return 0;
        }

        const std::uint64_t filled =
            pieces[Side::Black] | pieces[Side::White] | gaps;
        const std::uint64_t empty = Bitboards::All ^ filled;
        int num_moves = 0;

        // Single moves
        const std::uint64_t singles = single_moves(pieces[turn_]) & empty;
        num_moves += popcountll(singles);

        // Double moves
        std::uint64_t copy = pieces[turn_];
        while (copy) {
            const Square from = static_cast<Square>(lsbll(copy));
            const std::uint64_t destinations = double_moves(from) & empty;
            num_moves += popcountll(destinations);
            copy &= copy - 1;
        }

        // Nullmove
        if (num_moves == 0) {
            num_moves++;
        }

        assert(num_moves < max_moves);
        return num_moves;
    }

    Side turn() const {
        return turn_;
    }

    [[nodiscard]] bool can_move() const {
        const std::uint64_t both = pieces[Side::Black] | pieces[Side::White];
        const std::uint64_t empty = Bitboards::All ^ (both | gaps);
        const std::uint64_t moves = single_moves(single_moves(pieces[turn_]));
        return moves & empty;
    }

    std::uint64_t hash() const {
        std::uint64_t key = 0ULL;

        if (turn_ == Side::Black) {
            key ^= zobrist::turn;
        }

        std::uint64_t copy = pieces[Side::Black];
        while (copy) {
            int sq = lsbll(copy);
            key ^= zobrist::piece[Side::Black][sq];
            copy &= copy - 1;
        }

        copy = pieces[Side::White];
        while (copy) {
            int sq = lsbll(copy);
            key ^= zobrist::piece[Side::White][sq];
            copy &= copy - 1;
        }

        return key;
    }

   private:
    std::uint64_t pieces[2];
    std::uint64_t gaps : 49;
    int halfmoves_;
    Side turn_;
    PV history_;
    std::vector<Irreversible> irreversible_;
};

}  // namespace libataxx

#endif
