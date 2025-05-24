
#pragma GCC optimize("O3")
#pragma GCC optimize("inline")
#pragma GCC optimize("omit-frame-pointer")

/*
        Content of 'srcs/chessengine/ChessEngine.hpp'
*/

#ifndef CHESSENGINE_HPP
#define CHESSENGINE_HPP

using namespace std;

#include <bits/stdc++.h>
#include <iostream>
#include <locale>
#include <stdio.h>
#include <string.h>
#include <vector>

#define CHESS960_RULES 1

#define USE_VISUAL_BOARD 0
#define PRINT_DEBUG_DATA 0 & USE_VISUAL_BOARD
#define PRINT_TURNS      0 & USE_VISUAL_BOARD

#define EMPTY_CELL '-'

// Bit masks representing chess board columns and lines, using UCI notation
#define BITMASK_ALL_CELLS   0xFFFFFFFFFFFFFFFFUL
#define BITMASK_WHITE_CELLS 0xAA55AA55AA55AA55UL
#define BITMASK_BLACK_CELLS 0x55AA55AA55AA55AAUL

#define BITMASK_LINE_81 0xFF000000000000FFUL
#define BITMASK_LINE_7  0x000000000000FF00UL
#define BITMASK_LINE_65 0x00000000FFFF0000UL
#define BITMASK_LINE_43 0x0000FFFF00000000UL
#define BITMASK_LINE_2  0x00FF000000000000UL

#define BITMASK_CASTLE_BLACK_LEFT_KING  0x0000000000000004UL
#define BITMASK_CASTLE_BLACK_LEFT_ROOK  0x0000000000000008UL
#define BITMASK_CASTLE_BLACK_RIGHT_KING 0x0000000000000040UL
#define BITMASK_CASTLE_BLACK_RIGHT_ROOK 0x0000000000000020UL
#define BITMASK_CASTLE_WHITE_LEFT_KING  0x0400000000000000UL
#define BITMASK_CASTLE_WHITE_LEFT_ROOK  0x0800000000000000UL
#define BITMASK_CASTLE_WHITE_RIGHT_KING 0x4000000000000000UL
#define BITMASK_CASTLE_WHITE_RIGHT_ROOK 0x2000000000000000UL

/* ENUMS */

#define GAME_CONTINUE -2
#define BLACK_WIN     -1
#define DRAW          0
#define WHITE_WIN     1

// enum for Move type
enum castle_info_e
{
    NOINFO,
    NOTCASTLE,
    WHITELEFT,
    WHITERIGHT,
    BLACKLEFT,
    BLACKRIGHT
};

// enum for sliding pieces rays direction
enum ray_dir_e
{
    NORTH,
    NORTHEAST,
    EAST,
    SOUTHEAST,
    SOUTH,
    SOUTHWEST,
    WEST,
    NORTHWEST
};

/* PIECES FUNCTIONS */

inline wchar_t convert_piece_to_unicode(char piece)
{
    switch (piece)
    {
    case 'P':
        return L'♟';
    case 'N':
        return L'♞';
    case 'B':
        return L'♝';
    case 'R':
        return L'♜';
    case 'Q':
        return L'♛';
    case 'K':
        return L'♚';
    case 'p':
        return L'♙';
    case 'n':
        return L'♘';
    case 'b':
        return L'♗';
    case 'r':
        return L'♖';
    case 'q':
        return L'♕';
    case 'k':
        return L'♔';
    default:
        return piece;
    }
}

/* NOTATION FUNCTIONS */

inline int column_name_to_index(char column_name)
{
    // 'a' -> 0
    return tolower(column_name) - 'a';
}

inline char column_index_to_name(int column_id)
{
    // 7 -> 'h'
    return column_id + 'a';
}

inline int line_number_to_index(char line_number)
{
    // '8' -> 0
    return 8 - (line_number - '0');
}

inline char line_index_to_number(int line_index)
{
    // 7 -> '1'
    return (8 - line_index) + '0';
}

inline void algebraic_to_coord(string algebraic, int *x, int *y)
{
    // "a8" -> 0, 0
    const char *algebraic_char = algebraic.c_str();
    *x = column_name_to_index(algebraic_char[0]);
    *y = line_number_to_index(algebraic_char[1]);
}

inline string coord_to_algebraic(int x, int y)
{
    // 7, 7 -> "h1"
    char algebraic[3];
    algebraic[0] = (char)column_index_to_name(x);
    algebraic[1] = (char)line_index_to_number(y);
    algebraic[2] = '\0';

    return string(algebraic);
}

inline uint64_t algebraic_to_bitboard(string algebraic)
{
    // TODO: Hardcode all possibilities for faster execution
    // "a8" -> ...001011...
    int x, y;
    algebraic_to_coord(algebraic, &x, &y);
    return 1UL << (y * 8 + x);
}

inline string bitboard_to_algebraic(uint64_t bitboard)
{
    // TODO: Hardcode all possibilities for faster execution
    // ...001011... -> "a8"
    uint64_t mask = 1UL;
    for (int i = 0; i < 64; i++)
    {
        if (bitboard & mask)
            return coord_to_algebraic(i % 8, i / 8);

        mask <<= 1;
    }

    return "N/A";
}

/* BITWISE OPERATIONS */

inline uint64_t _count_bits(uint64_t bitboard)
{
    // __builtin_popcountll() returns the number of set bits
    return __builtin_popcountll(bitboard);
}

inline uint64_t _count_trailing_zeros(uint64_t bitboard)
{
    // __builtin_ctzll() returns the number of trailing zeros in the bitboard
    // (Zeros on the right)
    return __builtin_ctzll(bitboard);
}

inline uint64_t _count_leading_zeros(uint64_t bitboard)
{
    // __builtin_clzll() returns the number of leading zeros in the bitboard
    // (Zeros on the left)
    return 63 - __builtin_clzll(bitboard);
}

inline uint64_t _get_least_significant_bit(uint64_t bitboard)
{
    return 1UL << _count_trailing_zeros(bitboard);
}

inline uint64_t _get_most_significant_bit(uint64_t bitboard)
{
    return 1UL << _count_leading_zeros(bitboard);
}

#endif

/*
        Content of 'srcs/chessengine/Move.hpp'
*/

#ifndef MOVE_HPP
#define MOVE_HPP

using namespace std;

class Move
{

    public:
        // UCI information
        string   uci;
        uint64_t src;
        uint64_t dst;
        char     promotion; // Piece created by the promotion (always lowercase, because its UCI
                            // representation is lowercase)

        // Extra information
        char          piece;       // Piece moved
        castle_info_e castle_info; // Castle information, for optimization and UCI generation

        Move(string _uci);
        Move(
            char          _piece,
            uint64_t      src,
            uint64_t      dst,
            char          _promotion = 0,
            castle_info_e _castle_info = NOTCASTLE
        );

        void   log();
        string to_uci();
        string to_uci(bool chess960_rules, bool castling);

        bool operator==(Move *other);

        static bool compare_move_vector(vector<Move> movelst1, vector<Move> movelst2);

    private:
        static bool _is_move_in_movelst(Move *move, vector<Move> movelst);
};

#endif

/*
        Content of 'srcs/chessengine/Board.hpp'
*/

#ifndef BOARD_HPP
#define BOARD_HPP

// --- BASIC BOARD IMPLEMENTATION---

#pragma region Board

// Because of the Fifty-Move rule, a game cannot exceed 50 moves without a capture
// So we can assume that a position cannot be repeated at more than 50 moves away
#define FEN_HISTORY_SIZE 50

class Board
{
        /*
        Board represent all FEN data :
        rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w AHah - 0 1

        White = Upper case = First indexes
        Black = Lower case = Last indexes
        */

        bool chess960_rule;
        bool codingame_rule;

        // FEN data: Player turn
        bool white_turn;

        // FEN data: Castling info
        uint64_t white_castles; // Positions where the castle is available
        uint64_t black_castles;

        // FEN data: en passant and turns
        uint64_t en_passant; // En passant position is created after a pawn move of 2 squares. 0
                             // means no en passant available
        uint64_t next_turn_en_passant; // En passant position is created after a pawn move of 2
                                       // squares. 0 means no en passant available
        int half_turn_rule; // Number of half-turn since the last capture or pawn move (Fifty-Move
                            // rule)

    public:
        int game_turn; // Game turn, incremented after each black move

        // FEN data: Pieces
        uint64_t white_pawns;
        uint64_t white_knights;
        uint64_t white_bishops;
        uint64_t white_rooks;
        uint64_t white_queens;
        uint64_t white_king;
        uint64_t black_pawns;
        uint64_t black_knights;
        uint64_t black_bishops;
        uint64_t black_rooks;
        uint64_t black_queens;
        uint64_t black_king;

        Board(
            string _fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w AHah - 0 1",
            bool   chess960_rule = true,
            bool   codingame_rule = true
        );
        Board(
            string _board,
            string _color,
            string _castling,
            string _en_passant,
            int    _half_turn_rule,
            int    _full_move,
            bool   chess960_rule = true,
            bool   codingame_rule = true
        );

        inline bool is_white_turn()
        {
            return white_turn;
        }
        char          get_cell(int x, int y);
        float         get_game_state();
        bool          get_check_state();
        uint64_t      get_castling_rights();
        static string get_name();

        string create_fen(bool with_turns = true);
        Board *clone();

        vector<Move> get_available_moves();
        void         apply_move(Move move);

        void log(bool raw = false);

        bool operator==(Board *test_board);
        bool moves_computed;

    private:
#if USE_VISUAL_BOARD == 1
        VisualBoard visual_board;
#endif

        // Getters data
        bool         check_state;
        bool         double_check;
        bool         engine_data_updated;
        vector<Move> available_moves;
        float        game_state;
        bool         game_state_computed;

        // Engine variables
        uint64_t white_pieces_mask;     // All white pieces on the board
        uint64_t black_pieces_mask;     // All black pieces on the board
        uint64_t not_white_pieces_mask; // All cells that are not a white piece
        uint64_t not_black_pieces_mask; // All cells that are not a black piece

        uint64_t all_pieces_mask;  // All pieces on the board
        uint64_t empty_cells_mask; // All empty cells on the board

        uint64_t ally_king;
        uint64_t ally_pieces;

        uint64_t enemy_pawns;
        uint64_t enemy_knights;
        uint64_t enemy_bishops;
        uint64_t enemy_rooks;
        uint64_t enemy_queens;
        uint64_t enemy_king;
        uint64_t enemy_pieces;
        uint64_t enemy_pieces_sliding_diag; // Queens and Bishops
        uint64_t enemy_pieces_sliding_line; // Queens and Rooks

        uint64_t capturable_by_white_pawns_mask;
        uint64_t capturable_by_black_pawns_mask;

        uint64_t uncheck_mask;        // Full set of bits to 1 means there is no check
        uint64_t pawn_uncheck_mask;   // Uncheck mask only available for pawns
        uint64_t attacked_cells_mask; // Squares attacked by the opponent
        uint64_t pin_masks[64];       // Each cell can have a pinned mask

        // FEN history is used to check the Threefold Repetition rule
        // Each FEN is saved in the history after each move
        string fen_history[FEN_HISTORY_SIZE];
        int    fen_history_index;

        // // Function pointer to apply castle depending on the chess960 rule
        // bool    (Board::*_handle_castle)(int, int, int, int);

        // - Parsing -
        void _main_parsing(
            string _board,
            string _color,
            string _castling,
            string _en_passant,
            int    _half_turn_rule,
            int    _game_turn,
            bool   _chess960_rule,
            bool   _codingame_rule
        );
        void _initialize_bitboards();
        void _parse_board(string fen_board);
        void _parse_castling(string castling_fen);

        // - Accessibility / Getters -
        char _get_cell(uint64_t mask);
        void _create_fen_for_standard_castling(char *fen, int *fen_i);
        void _create_fen_for_chess960_castling(char *fen, int *fen_i);

        // - Move application -
        void _apply_regular_white_move(uint64_t src, uint64_t dst, uint64_t *piece_mask);
        void _apply_regular_black_move(uint64_t src, uint64_t dst, uint64_t *piece_mask);
        void _move_white_pawn(uint64_t src, uint64_t dst, char promotion);
        void _move_black_pawn(uint64_t src, uint64_t dst, char promotion);
        void _move_white_king(uint64_t src, uint64_t dst, castle_info_e castle_info);
        void _move_black_king(uint64_t src, uint64_t dst, castle_info_e castle_info);
        void _capture_white_pieces(uint64_t dst);
        void _capture_black_pieces(uint64_t dst);

        // - Engine updates -
        void _update_engine_at_turn_end();
        void _update_engine_at_turn_start();
        void _update_check_and_pins();
        void _update_pawn_check(int king_lkt_i);
        void _update_attacked_cells_mask();
        void _update_fen_history();

        // - Piece attacks -
        void _find_white_pawns_attacks(uint64_t src);
        void _find_white_knights_attacks(uint64_t src);
        void _find_white_bishops_attacks(uint64_t src);
        void _find_white_rooks_attacks(uint64_t src);
        void _find_white_queens_attacks(uint64_t src);
        void _find_white_king_attacks();
        void _find_black_pawns_attacks(uint64_t src);
        void _find_black_knights_attacks(uint64_t src);
        void _find_black_bishops_attacks(uint64_t src);
        void _find_black_rooks_attacks(uint64_t src);
        void _find_black_queens_attacks(uint64_t src);
        void _find_black_king_attacks();

        // - Move creation -
        void _find_moves();
        void _find_white_pawns_moves(uint64_t src);
        void _find_white_knights_moves(uint64_t src);
        void _find_white_bishops_moves(uint64_t src);
        void _find_white_rooks_moves(uint64_t src);
        void _find_white_queens_moves(uint64_t src);
        void _find_white_king_moves();
        void _find_white_castle_moves(uint64_t dst);
        void _find_black_pawns_moves(uint64_t src);
        void _find_black_knights_moves(uint64_t src);
        void _find_black_bishops_moves(uint64_t src);
        void _find_black_rooks_moves(uint64_t src);
        void _find_black_queens_moves(uint64_t src);
        void _find_black_king_moves();
        void _find_black_castle_moves(uint64_t dst);

        void _add_regular_move_or_promotion(char piece, uint64_t src, uint64_t dst);
        void _create_piece_moves(char piece, uint64_t src, uint64_t legal_moves);

        // - Bit operations -

        void _apply_function_on_all_pieces(uint64_t bitboard, std::function<void(uint64_t)> func);
        uint64_t _get_diagonal_rays(uint64_t src, uint64_t piece_to_ignore = 0UL);
        uint64_t _get_line_rays(uint64_t src, uint64_t piece_to_ignore = 0UL);
        uint64_t
        _compute_sliding_piece_positive_ray(uint64_t src, ray_dir_e dir, uint64_t piece_to_ignore);
        uint64_t
        _compute_sliding_piece_negative_ray(uint64_t src, ray_dir_e dir, uint64_t piece_to_ignore);
        void _compute_sliding_piece_positive_ray_checks_and_pins(
            uint64_t king_pos, ray_dir_e dir, uint64_t potential_attacker
        );
        void _compute_sliding_piece_negative_ray_checks_and_pins(
            uint64_t king_pos, ray_dir_e dir, uint64_t potential_attacker
        );
        bool     _is_sliding_piece_positive_diagonal_ray_behind(uint64_t pawn_pos, ray_dir_e dir);
        bool     _is_sliding_piece_negative_diagonal_ray_behind(uint64_t pawn_pos, ray_dir_e dir);
        uint64_t _compute_castling_positive_path(uint64_t src, uint64_t dst);
        uint64_t _compute_castling_negative_path(uint64_t src, uint64_t dst);

        // - End game -
        float _compute_game_state();
        bool  _threefold_repetition_rule();
        bool  _insufficient_material_rule();

        // STATIC LOOKUP TABLES

        static bool     lookup_tables_initialized;
        static uint64_t pawn_captures_lookup[64][2]; // 0: white, 1: black
        static uint64_t knight_lookup[64];
        static uint64_t sliding_lookup[64][8]; // 0: N, 1: NE, 2: E, 3: SE, 4: S, 5: SW, 6: W, 7: NW
        static uint64_t king_lookup[64];

        static void _initialize_lookup_tables();
        static void _create_pawn_captures_lookup_table(int y, int x, uint64_t position, int lkt_i);
        static void _create_knight_lookup_table(int y, int x, uint64_t position, int lkt_i);
        static void _create_sliding_lookup_table(int y, int x, uint64_t position, int lkt_i);
        static void _create_king_lookup_table(int y, int x, uint64_t position, int lkt_i);
};

#endif

/*
        Content of 'srcs/heuristics/AbstractHeuristic.hpp'
*/

#ifndef ABSTRACTHEURISTIC_HPP
#define ABSTRACTHEURISTIC_HPP

class AbstractHeuristic
{

    public:
        virtual float  evaluate(Board *board) = 0;
        virtual string get_name() = 0;
};

#endif

/*
        Content of 'srcs/heuristics/PiecesHeuristic.hpp'
*/

#ifndef PIECESHEURISTIC_HPP
#define PIECESHEURISTIC_HPP

class PiecesHeuristic : public AbstractHeuristic
{

    public:
        float  evaluate(Board *board) override;
        string get_name() override;

    private:
        int _material_evaluation(Board *board, int *white_material, int *black_material);
        int _piece_positions_evaluation(
            Board *board, float white_eg_coefficient, float black_eg_coefficient
        );

        int _lookup_bonuses_for_all_pieces(int *bonus_table, uint64_t bitboard);
        int _lookup_bonuses_for_all_pieces(
            int *sg_bonus_table, int *eg_bonus_table, float eg_coef, uint64_t bitboard
        );

        // From AlphaZero paper
        // https://en.wikipedia.org/wiki/Chess_piece_relative_value
        typedef enum t_piece_values
        {
            PAWN_VALUE = 100,
            KNIGHT_VALUE = 305,
            BISHOP_VALUE = 333,
            ROOK_VALUE = 563,
            QUEEN_VALUE = 950
        } e_piece_values;

        // Completely arbitrary estimation (but fast)
        const int material_start_game =
            10 * PAWN_VALUE + 2 * KNIGHT_VALUE + 2 * BISHOP_VALUE + 2 * ROOK_VALUE + QUEEN_VALUE;
        const int material_end_game = QUEEN_VALUE + ROOK_VALUE + 3 * PAWN_VALUE;
        const int material_start_end_game_diff = material_start_game - material_end_game;

        // clang-format off
        // Pieces bonuses depending on their position
        // https://www.chessprogramming.org/Simplified_Evaluation_Function
        // https://github.com/amir650/BlackWidow-Chess/blob/master/src/com/chess/engine/classic/Alliance.java
        int white_pawn_sg_bonus_table[64] = {
             0,  0,  0,  0,  0,  0,  0,  0,
            50, 50, 50, 50, 50, 50, 50, 50,
            10, 10, 20, 30, 30, 20, 10, 10,
             5,  5, 10, 25, 25, 10,  5,  5,
             0,  0,  0, 20, 20,  0,  0,  0,
             5, -5,-10,  0,  0,-10, -5,  5,
             5, 10, 10,-20,-20, 10, 10,  5,
             0,  0,  0,  0,  0,  0,  0,  0
        };

        int white_pawn_eg_bonus_table[64] = {
             0,  0,  0,  0,  0,  0,  0,  0,
            80, 80, 80, 80, 80, 80, 80, 80,
            50, 50, 50, 50, 50, 50, 50, 50,
            30, 30, 30, 30, 30, 30, 30, 30,
            20, 20, 20, 20, 20, 20, 20, 20,
            10, 10, 10, 10, 10, 10, 10, 10,
            10, 10, 10, 10, 10, 10, 10, 10,
             0,  0,  0,  0,  0,  0,  0,  0,
        };
        
        int black_pawn_sg_bonus_table[64] = {
            0,  0,  0,  0,  0,  0,  0,  0,
            5, 10, 10,-20,-20, 10, 10,  5,
            5, -5,-10,  0,  0,-10, -5,  5,
            0,  0,  0, 20, 20,  0,  0,  0,
            5,  5, 10, 25, 25, 10,  5,  5,
            10, 10, 20, 30, 30, 20, 10, 10,
            50, 50, 50, 50, 50, 50, 50, 50,
            0,  0,  0,  0,  0,  0,  0,  0
        };

        int black_pawn_eg_bonus_table[64] = {
             0,  0,  0,  0,  0,  0,  0,  0,
            10, 10, 10, 10, 10, 10, 10, 10,
            10, 10, 10, 10, 10, 10, 10, 10,
            20, 20, 20, 20, 20, 20, 20, 20,
            30, 30, 30, 30, 30, 30, 30, 30,
            50, 50, 50, 50, 50, 50, 50, 50,
            80, 80, 80, 80, 80, 80, 80, 80,
             0,  0,  0,  0,  0,  0,  0,  0
        };

        int white_knight_bonus_table[64] = {
            -50,-40,-30,-30,-30,-30,-40,-50,
            -40,-20,  0,  5,  5,  0,-20,-40,
            -30,  5, 10, 15, 15, 10,  5,-30,
            -30,  5, 15, 20, 20, 15,  5,-30,
            -30,  5, 15, 20, 20, 15,  5,-30,
            -30,  5, 10, 15, 15, 10,  5,-30,
            -40,-20,  0,  0,  0,  0,-20,-40,
            -50,-40,-30,-30,-30,-30,-40,-50
        };

        int black_knight_bonus_table[64] = {
            -50,-40,-30,-30,-30,-30,-40,-50,
            -40,-20,  0,  0,  0,  0,-20,-40,
            -30,  5, 10, 15, 15, 10,  5,-30,
            -30,  5, 15, 20, 20, 15,  5,-30,
            -30,  5, 15, 20, 20, 15,  5,-30,
            -30,  5, 10, 15, 15, 10,  5,-30,
            -40,-20,  0,  5,  5,  0,-20,-40,
            -50,-40,-30,-30,-30,-30,-40,-50,
        };

        int white_bishop_bonus_table[64] = {
            -20,-10,-10,-10,-10,-10,-10,-20,
            -10,  0,  0,  0,  0,  0,  0,-10,
            -10,  0,  5, 10, 10,  5,  0,-10,
            -10,  5,  5, 10, 10,  5,  5,-10,
            -10,  0, 10, 15, 15, 10,  0,-10,
            -10, 10, 10, 10, 10, 10, 10,-10,
            -10,  5,  0,  0,  0,  0,  5,-10,
            -20,-10,-10,-10,-10,-10,-10,-20
        };

        int black_bishop_bonus_table[64] = {
            -20,-10,-10,-10,-10,-10,-10,-20,
            -10,  5,  0,  0,  0,  0,  5,-10,
            -10, 10, 10, 10, 10, 10, 10,-10,
            -10,  0, 10, 15, 15, 10,  0,-10,
            -10,  5, 10, 15, 15, 10,  5,-10,
            -10,  0, 10, 10, 10, 10,  0,-10,
            -10,  0,  0,  0,  0,  0,  0,-10,
            -20,-10,-10,-10,-10,-10,-10,-20
        };

        int white_rook_bonus_table[64] = {
             0,  0,  0,  0,  0,  0,  0,  0,
             5, 20, 20, 20, 20, 20, 20,  5,
            -5,  0,  0,  0,  0,  0,  0, -5,
            -5,  0,  0,  0,  0,  0,  0, -5,
            -5,  0,  0,  0,  0,  0,  0, -5,
            -5,  0,  0,  0,  0,  0,  0, -5,
            -5,  0,  0,  0,  0,  0,  0, -5,
             0,  0,  0,  5,  5,  0,  0,  0
        };

        int black_rook_bonus_table[64] = {
             0,  0,  0,  5,  5,  0,  0,  0,
            -5,  0,  0,  0,  0,  0,  0, -5,
            -5,  0,  0,  0,  0,  0,  0, -5,
            -5,  0,  0,  0,  0,  0,  0, -5,
            -5,  0,  0,  0,  0,  0,  0, -5,
            -5,  0,  0,  0,  0,  0,  0, -5,
             5, 20, 20, 20, 20, 20, 20,  5,
             0,  0,  0,  0,  0,  0,  0,  0,
        };

        int white_queen_bonus_table[64] = {
            -20,-10,-10, -5, -5,-10,-10,-20,
            -10,  0,  0,  0,  0,  0,  0,-10,
            -10,  0,  5,  5,  5,  5,  0,-10,
            -5,   0,  5, 10, 10,  5,  0, -5,
            -5,   0,  5, 10, 10,  5,  0, -5,
            -10,  0,  5,  5,  5,  5,  0,-10,
            -10,  0,  0,  0,  0,  0,  0,-10,
            -20,-10,-10, -5, -5,-10,-10,-20
        };

        int black_queen_bonus_table[64] = {
            -20,-10,-10, -5, -5,-10,-10,-20,
            -10,  0,  5,  0,  0,  0,  0,-10,
            -10,  5,  5,  5,  5,  5,  0,-10,
            -5,   0,  5, 10, 10,  5,  0, -5,
            -5,   0,  5, 10, 10,  5,  0, -5,
            -10,  0,  5,  5,  5,  5,  0,-10,
            -10,  0,  0,  0,  0,  0,  0,-10,
            -20,-10,-10, -5, -5,-10,-10,-20
        };

        int white_king_sg_bonus_table[64] = {
            -60,-60,-60,-60,-60,-60,-60,-60,
            -50,-50,-50,-50,-50,-50,-50,-50,
            -40,-40,-40,-40,-40,-40,-40,-40,
            -30,-30,-30,-30,-30,-30,-30,-30,
            -20,-20,-20,-20,-20,-20,-20,-20,
            -10,-10,-10,-10,-10,-10,-10,-10,
              0,  0,  0,  0,  0,  0,  0,  0,
             20, 30,  5,  0,  0,  5, 30, 20
        };

        int white_king_eg_bonus_table[64] = {
            -50,-40,-30,-20,-20,-30,-40,-50,
            -30,-20,-10,  0,  0,-10,-20,-30,
            -30,-10, 20, 30, 30, 20,-10,-30,
            -30,-10, 30, 40, 40, 30,-10,-30,
            -30,-10, 30, 40, 40, 30,-10,-30,
            -30,-10, 20, 30, 30, 20,-10,-30,
            -30,-30,  0,  0,  0,  0,-30,-30,
            -50,-30,-30,-30,-30,-30,-30,-50
        };

        int black_king_sg_bonus_table[64] = {
             20, 30,  5,  0,  0,  5, 30, 20,
              0,  0,  0,  0,  0,  0,  0,  0,
            -10,-10,-10,-10,-10,-10,-10,-10,
            -20,-20,-20,-20,-20,-20,-20,-20,
            -30,-30,-30,-30,-30,-30,-30,-30,
            -40,-40,-40,-40,-40,-40,-40,-40,
            -50,-50,-50,-50,-50,-50,-50,-50,
            -60,-60,-60,-60,-60,-60,-60,-60,
        };

        int black_king_eg_bonus_table[64] = {
            -50,-30,-30,-30,-30,-30,-30,-50
            -30,-30,  0,  0,  0,  0,-30,-30,
            -30,-10, 20, 30, 30, 20,-10,-30,
            -30,-10, 30, 40, 40, 30,-10,-30,
            -30,-10, 30, 40, 40, 30,-10,-30,
            -30,-10, 20, 30, 30, 20,-10,-30,
            -30,-20,-10,  0,  0,-10,-20,-30,
            -50,-40,-30,-20,-20,-30,-40,-50,
        };
        // clang-format on
};

#endif

/*
        Content of 'srcs/agents/AbstractAgent.hpp'
*/

#ifndef ABSTRACTAGENT_HPP
#define ABSTRACTAGENT_HPP

#include <tuple>
#include <vector>

class AbstractAgent
{

    public:
        virtual void get_qualities(Board *board, vector<Move> moves, vector<float> *qualities) = 0;
        virtual string get_name() = 0;

        virtual vector<string> get_stats()
        {
            return vector<string>{};
        };
};

#endif

/*
        Content of 'srcs/agents/MinMaxAlphaBetaAgent.hpp'
*/

#ifndef MINMAXITERDEEPAGENT_HPP
#define MINMAXITERDEEPAGENT_HPP

class MinMaxAlphaBetaAgent : public AbstractAgent
{

    public:
        MinMaxAlphaBetaAgent(AbstractHeuristic *heuristic, int ms_constraint);
        virtual void
        get_qualities(Board *board, vector<Move> moves, vector<float> *qualities) override;
        virtual string get_name() override;
        vector<string> get_stats() override;

    private:
        AbstractHeuristic *_heuristic;

        int     _ms_constraint;
        float   _ms_turn_stop;
        clock_t _start_time;

        int _depth_reached;
        int _nodes_explored;

        float minmax(Board *board, int max_depth, int depth, float alpha, float beta);
        float max_node(
            Board *board, vector<Move> *moves, int max_depth, int depth, float alpha, float beta
        );
        float min_node(
            Board *board, vector<Move> *moves, int max_depth, int depth, float alpha, float beta
        );

        bool  is_time_up();
        float elapsed_time();
};

#endif

/*
        Content of 'srcs/players/AbstractPlayer.hpp'
*/

#ifndef ABSTRACTPLAYER_HPP
#define ABSTRACTPLAYER_HPP

#include <vector>

class AbstractPlayer
{

    public:
        virtual Move   choose_from(Board *board, vector<Move> moves) = 0;
        virtual string get_name() = 0;
};

#endif

/*
        Content of 'srcs/players/BotPlayer.hpp'
*/

#ifndef BOTPLAYER_HPP
#define BOTPLAYER_HPP

class BotPlayer : public AbstractPlayer
{

    public:
        BotPlayer(AbstractAgent *agent);
        vector<string> get_stats();

        virtual Move   choose_from(Board *board, vector<Move> moves) override;
        virtual string get_name() override;

    private:
        AbstractAgent *_agent;

        float max_float(float a, float b);
        float min_float(float a, float b);
};

#endif

/*
        Content of 'srcs/gameengine/GameEngine.hpp'
*/

#ifndef GAMEENGINE_HPP
#define GAMEENGINE_HPP

class GameEngine
{

    public:
        GameEngine(BotPlayer *player);
        void infinite_game_loop();

    private:
        // Inputs from CG protocol - First turn
        bool crazy_house;
        int  max_moves;

        // Inputs from CG protocol - Other turns
        bool _lastmove;
        bool _fen;
        bool _moves;
        bool _draw;
        bool _game;
        bool _score;

        BotPlayer   *_player;
        Move        *_cg_last_move;
        Board       *_cg_board;
        Board       *_board;
        vector<Move> _possible_moves;
        int          _possible_moves_count;

        void _parse_first_turn();
        void _parse_turn();
};

#endif

/*
        Content of 'srcs/gameengine/GameEngine.cpp'
*/

// --- PUBLIC METHODS ---

GameEngine::GameEngine(BotPlayer *player)
{
    this->_player = player;
    this->_cg_last_move = NULL;
    this->_cg_board = NULL;
    this->_board = NULL;
}

void GameEngine::infinite_game_loop()
{
    this->_parse_first_turn();
    cout << "lastmove fen" << endl;

    while (1)
    {
        _parse_turn();

        // First turns
        if (this->_cg_board->game_turn == 1)
        {
            this->_board = this->_cg_board->clone();
            // cerr << "\nGameEngine: Initial board:" << endl;
            // this->_board->log();
        }
        else
            this->_board->apply_move(*this->_cg_last_move);

        vector<Move> moves = this->_board->get_available_moves();

        Move           move = this->_player->choose_from(this->_board, moves);
        vector<string> stats = this->_player->get_stats();

        cout << move.to_uci();

        for (string stat : stats)
            cout << " " << stat;
        cout << endl;

        this->_board->apply_move(move);

        float game_state = this->_board->get_game_state();
        if (game_state != GAME_CONTINUE)
        {
            cerr << "\nGameEngine: Game is over : " << game_state << endl;
            this->_board->log(true);
            // if (this->_board)
            //     delete this->_board;
        }
    }
}

// --- PRIVATE METHODS ---

void GameEngine::_parse_first_turn()
{
    int    constants_count;
    string name;
    string value;
    bool   crazy_house = false;
    int    max_moves = 0;

    cin >> constants_count;
    cin.ignore();

    for (int i = 0; i < constants_count; i++)
    {
        cin >> name >> value;
        cin.ignore();

        if (name == "crazyHouse")
            crazy_house = value == "1";
        else if (name == "maxMoves")
            max_moves = stoi(value);
    }

    this->crazy_house = crazy_house;
    this->max_moves = max_moves;
}

void GameEngine::_parse_turn()
{
    // Parse last move
    string move;
    cin >> move;

    this->_cg_last_move = move == "none" ? NULL : new Move(move);

    // Parse FEN
    string board;
    string color;
    string castling;
    string en_passant;
    string half_move_clock_str;
    string full_move_str;

    cin >> board >> color >> castling >> en_passant >> half_move_clock_str >> full_move_str;

    int half_move_clock = stoi(half_move_clock_str);
    int full_move = stoi(full_move_str);

    if (this->_cg_board)
        delete this->_cg_board;
    this->_cg_board = new Board(board, color, castling, en_passant, half_move_clock, full_move);

    cin.ignore();
}

/*
        Content of 'srcs/chessengine/Board.cpp'
*/

bool     Board::lookup_tables_initialized = false;
uint64_t Board::pawn_captures_lookup[64][2];
uint64_t Board::knight_lookup[64];
uint64_t Board::sliding_lookup[64][8];
uint64_t Board::king_lookup[64];

// --- PUBLIC METHODS ---

Board::Board(string _fen, bool _chess960_rule, bool _codingame_rule)
{
    stringstream ss(_fen);
    string       board;
    string       color;
    string       castling;
    string       en_passant;
    string       half_turn_rule;
    string       game_turn;

    getline(ss, board, ' ');
    getline(ss, color, ' ');
    getline(ss, castling, ' ');
    getline(ss, en_passant, ' ');
    getline(ss, half_turn_rule, ' ');
    getline(ss, game_turn, ' ');

    _main_parsing(
        board, color, castling, en_passant, stoi(half_turn_rule), stoi(game_turn), _chess960_rule,
        _codingame_rule
    );
}

Board::Board(
    string _board,
    string _color,
    string _castling,
    string _en_passant,
    int    _half_turn_rule,
    int    _game_turn,
    bool   _chess960_rule,
    bool   _codingame_rule
)
{
    _main_parsing(
        _board, _color, _castling, _en_passant, _half_turn_rule, _game_turn, _chess960_rule,
        _codingame_rule
    );
}

void Board::log(bool raw)
{
    uint64_t rook;

    // Find all individual rooks in white_castles
    string   white_castles_pos[2] = {"N/A", "N/A"};
    int      white_castles_pos_i = 0;
    uint64_t castle_tmp = white_castles;
    while (castle_tmp)
    {
        rook = _get_least_significant_bit(castle_tmp);
        white_castles_pos[white_castles_pos_i++] = bitboard_to_algebraic(rook);

        // Remove the actual rook from castle_tmp, so we can find the next one
        castle_tmp ^= rook;
    }

    // Find all individual rooks in black_castles
    string black_castles_pos[2] = {"N/A", "N/A"};
    int    black_castles_pos_i = 0;
    castle_tmp = black_castles;
    while (castle_tmp)
    {
        rook = _get_least_significant_bit(castle_tmp);
        black_castles_pos[black_castles_pos_i++] = bitboard_to_algebraic(rook);

        // Remove the actual rook from castle_tmp, so we can find the next one
        castle_tmp ^= rook;
    }

    cerr << "Board: FEN: " << create_fen() << endl;
    cerr << "Board: Turn: " << (white_turn ? "White" : "Black") << endl;
    cerr << "Board: White castling: " << white_castles_pos[0] << " " << white_castles_pos[1]
         << endl;
    cerr << "Board: Black castling: " << black_castles_pos[0] << " " << black_castles_pos[1]
         << endl;
    cerr << "Board: En passant: " << (en_passant ? bitboard_to_algebraic(en_passant) : "N/A")
         << endl;
    cerr << "Board: half_turn_rule: " << to_string(half_turn_rule) << endl;
    cerr << "Board: game_turn: " << to_string(game_turn) << endl;

#if USE_VISUAL_BOARD == 1
    if (raw)
        this->visual_board.printRawBoard();
    else
        this->visual_board.printBoard();
#else
    raw = raw;
#endif
}

void Board::apply_move(Move move)
{
    if (!this->engine_data_updated)
        _update_engine_at_turn_start();

    // TODO: Sort them by probability to optimize the if-else chain
    char piece = move.piece == EMPTY_CELL ? _get_cell(move.src) : move.piece;
    if (piece == 'P')
        _move_white_pawn(move.src, move.dst, move.promotion);
    else if (piece == 'N')
        _apply_regular_white_move(move.src, move.dst, &white_knights);
    else if (piece == 'B')
        _apply_regular_white_move(move.src, move.dst, &white_bishops);
    else if (piece == 'R')
    {
        _apply_regular_white_move(move.src, move.dst, &white_rooks);

        // Disable castling for this rook
        white_castles &= ~move.src;
    }
    else if (piece == 'Q')
        _apply_regular_white_move(move.src, move.dst, &white_queens);
    else if (piece == 'K')
        _move_white_king(move.src, move.dst, move.castle_info);
    else if (piece == 'p')
        _move_black_pawn(move.src, move.dst, move.promotion);
    else if (piece == 'n')
        _apply_regular_black_move(move.src, move.dst, &black_knights);
    else if (piece == 'b')
        _apply_regular_black_move(move.src, move.dst, &black_bishops);
    else if (piece == 'r')
    {
        _apply_regular_black_move(move.src, move.dst, &black_rooks);

        // Disable castling for this rook
        black_castles &= ~move.src;
    }
    else if (piece == 'q')
        _apply_regular_black_move(move.src, move.dst, &black_queens);
    else if (piece == 'k')
        _move_black_king(move.src, move.dst, move.castle_info);

#if USE_VISUAL_BOARD == 1
    this->visual_board.resetBoard();
    this->visual_board.updateBoard('P', white_pawns);
    this->visual_board.updateBoard('N', white_knights);
    this->visual_board.updateBoard('B', white_bishops);
    this->visual_board.updateBoard('R', white_rooks);
    this->visual_board.updateBoard('Q', white_queens);
    this->visual_board.updateBoard('K', white_king);
    this->visual_board.updateBoard('p', black_pawns);
    this->visual_board.updateBoard('n', black_knights);
    this->visual_board.updateBoard('b', black_bishops);
    this->visual_board.updateBoard('r', black_rooks);
    this->visual_board.updateBoard('q', black_queens);
    this->visual_board.updateBoard('k', black_king);

    if (PRINT_TURNS)
        this->visual_board.printBoard();
#endif

    _update_engine_at_turn_end();
}

float Board::get_game_state()
{
    if (!this->game_state_computed)
    {
        if (!this->engine_data_updated)
            _update_engine_at_turn_start();

        this->game_state = _compute_game_state();
    }

    return this->game_state;
}

bool Board::get_check_state()
{
    if (!this->engine_data_updated)
        _update_engine_at_turn_start();

    return this->check_state;
}

char Board::get_cell(int x, int y)
{
    uint64_t pos_mask = 1UL << (y * 8 + x);

    return _get_cell(pos_mask);
}

uint64_t Board::get_castling_rights()
{
    return white_castles | black_castles;
}

vector<Move> Board::get_available_moves()
{
    if (!this->moves_computed)
    {
        if (!this->engine_data_updated)
            _update_engine_at_turn_start();

        _find_moves();
    }

    return this->available_moves;
}

string Board::get_name()
{
    return "BitBoard";
}

string Board::create_fen(bool with_turns)
{
    char fen[85];
    int  fen_i = 0;

    bzero(fen, 85);

    // Write pieces
    int empty_cells_count = 0;
    for (int y = 0; y < 8; y++)
    {
        for (int x = 0; x < 8; x++)
        {
            // TODO: Don't use get_cell() but a mask we shift at each for loop beginning
            //          And create get_cell(uint64_t mask) method, used inside get_cell(int, int)
            if (get_cell(x, y) == EMPTY_CELL)
            {
                empty_cells_count++;
                continue;
            }

            if (empty_cells_count > 0)
            {
                fen[fen_i++] = '0' + empty_cells_count;
                empty_cells_count = 0;
            }

            fen[fen_i++] = get_cell(x, y);
        }

        if (empty_cells_count > 0)
        {
            fen[fen_i++] = '0' + empty_cells_count;
            empty_cells_count = 0;
        }

        if (y != 7)
            fen[fen_i++] = '/';
    }
    fen[fen_i++] = ' ';

    // Write turn
    fen[fen_i++] = white_turn ? 'w' : 'b';
    fen[fen_i++] = ' ';

    // Write castling
    if (white_castles || black_castles)
    {
        if (this->chess960_rule)
            _create_fen_for_chess960_castling(fen, &fen_i);
        else
            _create_fen_for_standard_castling(fen, &fen_i);
    }
    else
        fen[fen_i++] = '-';
    fen[fen_i++] = ' ';

    // Write en passant
    if (en_passant)
    {
        string en_passant_str = bitboard_to_algebraic(en_passant);
        fen[fen_i++] = en_passant_str[0];
        fen[fen_i++] = en_passant_str[1];
    }
    else
        fen[fen_i++] = '-';
    fen[fen_i++] = ' ';

    string fen_string = string(fen, fen_i);

    if (!with_turns)
        return fen_string;

    // Write half turn rule
    fen_string += to_string(half_turn_rule);
    fen_string += string(" ");

    // Write game turn
    fen_string += to_string(game_turn);

    return fen_string;
}

Board *Board::clone()
{
    // TODO: Create a new constructor, taking an instance in param, which then copy all the needed
    // data instead of creating and parsing FEN
    Board *cloned_board = new Board(create_fen(), this->chess960_rule, this->codingame_rule);

    // Copy history
    for (int i = 0; i < FEN_HISTORY_SIZE; i++)
    {
        if (this->fen_history[i].empty())
            break;
        cloned_board->fen_history[i] = this->fen_history[i];
    }

    return cloned_board;
}

// --- PRIVATE METHODS ---

// - Parsing -

void Board::_main_parsing(
    string _board,
    string _color,
    string _castling,
    string _en_passant,
    int    _half_turn_rule,
    int    _game_turn,
    bool   _chess960_rule,
    bool   _codingame_rule
)
{
#if USE_VISUAL_BOARD == 1
    this->visual_board = VisualBoard();
#endif

    if (Board::lookup_tables_initialized == false)
        Board::_initialize_lookup_tables();

    // Initialize private variables
    chess960_rule = _chess960_rule;
    codingame_rule = _codingame_rule;

    _initialize_bitboards();

    // Parse FEN data
    _parse_board(_board);
    white_turn = _color == "w";
    _parse_castling(_castling);
    en_passant = _en_passant != "-" ? algebraic_to_bitboard(_en_passant) : 0;
    next_turn_en_passant = 0UL;
    half_turn_rule = _half_turn_rule;
    game_turn = _game_turn;

    // Initialize internal data
    moves_computed = false;
    game_state_computed = false;
    engine_data_updated = false;

    fen_history_index = 0;
    for (int i = 0; i < FEN_HISTORY_SIZE; i++)
        fen_history[i] = string();
    _update_fen_history();
}

void Board::_initialize_bitboards()
{
    white_pawns = 0UL;
    white_knights = 0UL;
    white_bishops = 0UL;
    white_rooks = 0UL;
    white_queens = 0UL;
    white_king = 0UL;
    black_pawns = 0UL;
    black_knights = 0UL;
    black_bishops = 0UL;
    black_rooks = 0UL;
    black_queens = 0UL;
    black_king = 0UL;

    white_castles = 0UL;
    black_castles = 0UL;
    en_passant = 0UL;

    white_pieces_mask = 0UL;
    black_pieces_mask = 0UL;
    not_white_pieces_mask = 0UL;
    not_black_pieces_mask = 0UL;
    all_pieces_mask = 0UL;
    empty_cells_mask = 0UL;

    check_state = false;
    double_check = false;
    uncheck_mask = BITMASK_ALL_CELLS;
    attacked_cells_mask = BITMASK_ALL_CELLS;
}

void Board::_parse_board(string fen_board)
{
    char pos_index = 0;

    // Parse _board string into boards
    for (size_t i = 0; i < fen_board.length(); i++)
    {
        char piece = fen_board[i];

        if (isdigit(piece))
        {
            pos_index += atoi(&fen_board[i]);
        }
        else if (piece != '/')
        {
            white_pawns |= (uint64_t)(piece == 'P') << pos_index;
            white_knights |= (uint64_t)(piece == 'N') << pos_index;
            white_bishops |= (uint64_t)(piece == 'B') << pos_index;
            white_rooks |= (uint64_t)(piece == 'R') << pos_index;
            white_queens |= (uint64_t)(piece == 'Q') << pos_index;
            white_king |= (uint64_t)(piece == 'K') << pos_index;
            black_pawns |= (uint64_t)(piece == 'p') << pos_index;
            black_knights |= (uint64_t)(piece == 'n') << pos_index;
            black_bishops |= (uint64_t)(piece == 'b') << pos_index;
            black_rooks |= (uint64_t)(piece == 'r') << pos_index;
            black_queens |= (uint64_t)(piece == 'q') << pos_index;
            black_king |= (uint64_t)(piece == 'k') << pos_index;

            pos_index++;
        }
    }

#if USE_VISUAL_BOARD == 1
    this->visual_board.resetBoard();
    this->visual_board.updateBoard('P', white_pawns);
    this->visual_board.updateBoard('N', white_knights);
    this->visual_board.updateBoard('B', white_bishops);
    this->visual_board.updateBoard('R', white_rooks);
    this->visual_board.updateBoard('Q', white_queens);
    this->visual_board.updateBoard('K', white_king);
    this->visual_board.updateBoard('p', black_pawns);
    this->visual_board.updateBoard('n', black_knights);
    this->visual_board.updateBoard('b', black_bishops);
    this->visual_board.updateBoard('r', black_rooks);
    this->visual_board.updateBoard('q', black_queens);
    this->visual_board.updateBoard('k', black_king);
#endif
}

void Board::_parse_castling(string castling_fen)
{
    // '-' means that no castling are available
    if (castling_fen == "-")
        return;

    // Parse castling fen 'AHah' into bitboards
    for (size_t i = 0; i < castling_fen.length(); i++)
    {
        if (isupper(castling_fen[i]))
        {
            if (chess960_rule)
                white_castles |= algebraic_to_bitboard(string(1, castling_fen[i]) + '1');
            else
                white_castles |=
                    castling_fen[i] == 'K' ? 0x8000000000000000UL : 0x0100000000000000UL; // Rooks
        }
        else
        {
            if (chess960_rule)
                black_castles |= algebraic_to_bitboard(string(1, castling_fen[i]) + '8');
            else
                black_castles |= castling_fen[i] == 'k' ? 0b10000000UL : 0b00000001UL; // Rooks
        }
    }
}

// - Accessibility / Getters -

char Board::_get_cell(uint64_t mask)
{
    if (white_pawns & mask)
        return 'P';
    if (white_knights & mask)
        return 'N';
    if (white_bishops & mask)
        return 'B';
    if (white_rooks & mask)
        return 'R';
    if (white_queens & mask)
        return 'Q';
    if (white_king & mask)
        return 'K';
    if (black_pawns & mask)
        return 'p';
    if (black_knights & mask)
        return 'n';
    if (black_bishops & mask)
        return 'b';
    if (black_rooks & mask)
        return 'r';
    if (black_queens & mask)
        return 'q';
    if (black_king & mask)
        return 'k';

    return EMPTY_CELL;
}

void Board::_create_fen_for_standard_castling(char *fen, int *fen_i)
{
    // Loop over the castling boards from the right to the left
    uint64_t white_mask = 1UL << 63;
    for (int x = 7; x >= 0; x--)
    {
        if (white_castles & white_mask)
        {
            fen[*fen_i] = x == 0 ? 'Q' : 'K';
            (*fen_i)++;
        }
        white_mask = 1UL << (55 + x);
    }

    uint64_t black_mask = 1UL << 7;
    for (int x = 7; x >= 0; x--)
    {
        if (black_castles & black_mask)
        {
            fen[*fen_i] = x == 0 ? 'q' : 'k';
            (*fen_i)++;
        }
        black_mask >>= 1;
    }
}

void Board::_create_fen_for_chess960_castling(char *fen, int *fen_i)
{
    // Loop over the castling boards from the left to the right
    uint64_t white_mask = 1UL << 56;
    for (int x = 0; x < 8; x++)
    {
        if (white_castles & white_mask)
        {
            fen[*fen_i] = toupper(column_index_to_name(x));
            (*fen_i)++;
        }
        white_mask <<= 1;
    }

    uint64_t black_mask = 1UL;
    for (int x = 0; x < 8; x++)
    {
        if (black_castles & black_mask)
        {
            fen[*fen_i] = column_index_to_name(x);
            (*fen_i)++;
        }
        black_mask <<= 1;
    }
}

// - Move application -

void Board::_apply_regular_white_move(uint64_t src, uint64_t dst, uint64_t *piece_mask)
{
    _capture_black_pieces(dst);

    // Remove the piece from its actual cell, and add the piece to the destination cell
    *piece_mask &= ~src;
    *piece_mask |= dst;
}

void Board::_apply_regular_black_move(uint64_t src, uint64_t dst, uint64_t *piece_mask)
{
    _capture_white_pieces(dst);

    // Remove the piece from its actual cell, and add the piece to the destination cell
    *piece_mask &= ~src;
    *piece_mask |= dst;
}

void Board::_move_white_pawn(uint64_t src, uint64_t dst, char promotion)
{
    // Fifty-Move rule: Reset half turn counter if a pawn is moved (-1 because it will be
    // incremented at the end of the turn)
    half_turn_rule = -1;

    // If the pawn move is an en passant, capture the opponent pawn
    if (dst == en_passant)
        black_pawns &= (~en_passant) << 8;

    // If the pawn moves 2 squares, we generate an en-passant
    if ((src & 0x00FF000000000000UL) && (dst & 0x000000FF00000000UL))
        next_turn_en_passant = src >> 8;

    _capture_black_pieces(dst);

    // Remove the piece from its actual cell
    white_pawns &= ~src;

    // TODO: Sort them by probability to optimize the if-else chain
    // Add the piece to the destination cell
    char final_piece = promotion ? toupper(promotion) : 'P';
    if (final_piece == 'P')
        white_pawns |= dst;
    else if (final_piece == 'N')
        white_knights |= dst;
    else if (final_piece == 'B')
        white_bishops |= dst;
    else if (final_piece == 'R')
        white_rooks |= dst;
    else if (final_piece == 'Q')
        white_queens |= dst;
}

void Board::_move_black_pawn(uint64_t src, uint64_t dst, char promotion)
{
    // Fifty-Move rule: Reset half turn counter if a pawn is moved (-1 because it will be
    // incremented at the end of the turn)
    half_turn_rule = -1;

    // If the pawn move is an en passant, capture the opponent pawn
    if (dst == en_passant)
        white_pawns &= (~en_passant) >> 8;

    // If the pawn moves 2 squares, we generate an en-passant
    if ((src & 0x000000000000FF00UL) && (dst & 0x00000000FF000000UL))
        next_turn_en_passant = src << 8;

    _capture_white_pieces(dst);

    // Remove the piece from its actual cell
    black_pawns &= ~src;

    // TODO: Sort them by probability to optimize the if-else chain
    // Add the piece to the destination cell
    char final_piece = promotion ? promotion : 'p';
    if (final_piece == 'p')
        black_pawns |= dst;
    else if (final_piece == 'n')
        black_knights |= dst;
    else if (final_piece == 'b')
        black_bishops |= dst;
    else if (final_piece == 'r')
        black_rooks |= dst;
    else if (final_piece == 'q')
        black_queens |= dst;
}

void Board::_move_white_king(uint64_t src, uint64_t dst, castle_info_e castle_info)
{
    if (castle_info == NOINFO)
    {
        if ((src & white_king) && (dst & white_rooks))
        {
            castle_info = dst < src ? WHITELEFT : WHITERIGHT;
        }
        else
            castle_info = NOTCASTLE;
    }

    if (castle_info == NOTCASTLE)
    {
        _capture_black_pieces(dst);

        // Remove the piece from its actual cell, and add the piece to the destination cell
        white_king = dst;
    }
    else if (castle_info == WHITELEFT)
    {
        white_rooks &= ~dst;
        white_king = BITMASK_CASTLE_WHITE_LEFT_KING;
        white_rooks |= BITMASK_CASTLE_WHITE_LEFT_ROOK;
    }
    else if (castle_info == WHITERIGHT)
    {
        white_rooks &= ~dst;
        white_king = BITMASK_CASTLE_WHITE_RIGHT_KING;
        white_rooks |= BITMASK_CASTLE_WHITE_RIGHT_ROOK;
    }

    white_castles = 0UL;
}

void Board::_move_black_king(uint64_t src, uint64_t dst, castle_info_e castle_info)
{
    // When move is created from UCI, there is no castle information, so we need to compute it
    if (castle_info == NOINFO)
    {
        if ((src & black_king) && (dst & black_rooks))
        {
            castle_info = dst < src ? BLACKLEFT : BLACKRIGHT;
        }
        else
            castle_info = NOTCASTLE;
    }

    if (castle_info == NOTCASTLE)
    {
        _capture_white_pieces(dst);

        // Remove the piece from its actual cell, and add the piece to the destination cell
        black_king = dst;
    }
    else if (castle_info == BLACKLEFT)
    {
        black_rooks &= ~dst;
        black_king = BITMASK_CASTLE_BLACK_LEFT_KING;
        black_rooks |= BITMASK_CASTLE_BLACK_LEFT_ROOK;
    }
    else if (castle_info == BLACKRIGHT)
    {
        black_rooks &= ~dst;
        black_king = BITMASK_CASTLE_BLACK_RIGHT_KING;
        black_rooks |= BITMASK_CASTLE_BLACK_RIGHT_ROOK;
    }

    black_castles = 0UL;
}

void Board::_capture_white_pieces(uint64_t dst)
{
    // Detect if a piece is captured
    if (all_pieces_mask & dst)
    {
        // Fifty-Move rule: Reset half turn counter if a piece is captured (-1 because it will be
        // incremented at the end of the turn)
        half_turn_rule = -1;

        uint64_t not_dst_mask = ~dst;

        // Detect if a potential castle cell is captured. If so, remove the castle possibility, and
        // the rook
        if (dst & white_castles)
        {
            white_castles &= not_dst_mask;
            white_rooks &= not_dst_mask;
            return;
        }

        // Remove the captured piece
        white_pawns &= not_dst_mask;
        white_knights &= not_dst_mask;
        white_bishops &= not_dst_mask;
        white_rooks &= not_dst_mask;
        white_queens &= not_dst_mask;
    }
}

void Board::_capture_black_pieces(uint64_t dst)
{
    // Detect if a piece is captured
    if (all_pieces_mask & dst)
    {
        // Fifty-Move rule: Reset half turn counter if a piece is captured (-1 because it will be
        // incremented at the end of the turn)
        half_turn_rule = -1;

        uint64_t not_dst_mask = ~dst;

        // Detect if a potential castle cell is captured. If so, remove the castle possibility, and
        // the rook
        if (dst & black_castles)
        {
            black_castles &= not_dst_mask;
            black_rooks &= not_dst_mask;
            return;
        }

        // Remove the captured piece
        black_pawns &= not_dst_mask;
        black_knights &= not_dst_mask;
        black_bishops &= not_dst_mask;
        black_rooks &= not_dst_mask;
        black_queens &= not_dst_mask;
    }
}

// - Engine updates -

void Board::_update_engine_at_turn_start()
{
    white_pieces_mask =
        white_pawns | white_knights | white_bishops | white_rooks | white_queens | white_king;
    black_pieces_mask =
        black_pawns | black_knights | black_bishops | black_rooks | black_queens | black_king;
    not_white_pieces_mask = ~white_pieces_mask;
    not_black_pieces_mask = ~black_pieces_mask;

    all_pieces_mask = white_pieces_mask | black_pieces_mask;
    empty_cells_mask = ~all_pieces_mask;

    if (white_turn)
    {
        ally_king = white_king;
        ally_pieces = white_pieces_mask;

        enemy_pawns = black_pawns;
        enemy_knights = black_knights;
        enemy_bishops = black_bishops;
        enemy_rooks = black_rooks;
        enemy_queens = black_queens;
    }
    else
    {
        ally_king = black_king;
        ally_pieces = black_pieces_mask;

        enemy_pawns = white_pawns;
        enemy_knights = white_knights;
        enemy_bishops = white_bishops;
        enemy_rooks = white_rooks;
        enemy_queens = white_queens;
    }
    enemy_pieces_sliding_diag = enemy_bishops | enemy_queens;
    enemy_pieces_sliding_line = enemy_rooks | enemy_queens;

    capturable_by_white_pawns_mask = black_pieces_mask | en_passant;
    capturable_by_black_pawns_mask = white_pieces_mask | en_passant;

    check_state = false;
    double_check = false;
    uncheck_mask = 0UL;
    pawn_uncheck_mask = 0UL;
    std::fill(std::begin(pin_masks), std::end(pin_masks), BITMASK_ALL_CELLS);
    attacked_cells_mask = 0UL;

    _update_check_and_pins();
    _update_attacked_cells_mask();

    // pawn_uncheck_mask is only set for a the specific case, so we must add generic uncheck_mask to
    // it
    pawn_uncheck_mask |= uncheck_mask;

#if USE_VISUAL_BOARD == 1
    if (PRINT_DEBUG_DATA)
    {
        this->visual_board.printSpecificBoard('A', attacked_cells_mask, "Attacked cells mask");
        this->visual_board.printSpecificBoard('C', uncheck_mask, "Uncheck mask");
    }
#endif

    engine_data_updated = true;
}

void Board::_update_check_and_pins()
{
    if (ally_king == 0UL)
    {
        check_state = false;
        uncheck_mask = BITMASK_ALL_CELLS;
        return;
    }

    // If our king can take an opponent piece using its movements, then the opponent piece is
    // checking our king.
    int king_lkt_i = _count_trailing_zeros(ally_king);

    // Compute pawn attacks
    _update_pawn_check(king_lkt_i);

    // Compute knight attacks
    uint64_t knight_attacks = knight_lookup[king_lkt_i] & enemy_knights;
    if (knight_attacks)
    {
        check_state = true;
        uncheck_mask |= knight_attacks;
    }

    // Compute diagonale attacks
    _compute_sliding_piece_negative_ray_checks_and_pins(
        ally_king, NORTHEAST, enemy_pieces_sliding_diag
    );
    _compute_sliding_piece_positive_ray_checks_and_pins(
        ally_king, SOUTHEAST, enemy_pieces_sliding_diag
    );
    _compute_sliding_piece_positive_ray_checks_and_pins(
        ally_king, SOUTHWEST, enemy_pieces_sliding_diag
    );
    _compute_sliding_piece_negative_ray_checks_and_pins(
        ally_king, NORTHWEST, enemy_pieces_sliding_diag
    );

    // Compute line attacks
    _compute_sliding_piece_negative_ray_checks_and_pins(
        ally_king, NORTH, enemy_pieces_sliding_line
    );
    _compute_sliding_piece_positive_ray_checks_and_pins(ally_king, EAST, enemy_pieces_sliding_line);
    _compute_sliding_piece_positive_ray_checks_and_pins(
        ally_king, SOUTH, enemy_pieces_sliding_line
    );
    _compute_sliding_piece_negative_ray_checks_and_pins(ally_king, WEST, enemy_pieces_sliding_line);

    // If no checks found, then there are no move restrictions
    if (uncheck_mask == 0UL)
    {
        check_state = false;
        uncheck_mask = BITMASK_ALL_CELLS;
    }
}

void Board::_update_pawn_check(int king_lkt_i)
{
    int lkt_color = white_turn ? 0 : 1;

    // Compute pawn attacks
    uint64_t attacking_pawn = pawn_captures_lookup[king_lkt_i][lkt_color] & enemy_pawns;
    if (attacking_pawn)
    {
        check_state = true;
        uncheck_mask |= attacking_pawn;

        // Attacking pawn could be captured by taking the en-passant
        if (en_passant)
        {
            if (white_turn)
            {
                if (attacking_pawn == (en_passant << 8))
                {
                    // But only if no opponent diagonal sliding piece is hidden behind it
                    if (!_is_sliding_piece_negative_diagonal_ray_behind(
                            attacking_pawn, ally_king == attacking_pawn - 9 ? NORTHWEST : NORTHEAST
                        ))
                        pawn_uncheck_mask = en_passant;
                }
            }
            else
            {
                if (attacking_pawn == (en_passant >> 8))
                {
                    // But only if no opponent diagonal sliding piece is hidden behind it
                    if (!_is_sliding_piece_positive_diagonal_ray_behind(
                            attacking_pawn, ally_king == attacking_pawn + 7 ? SOUTHWEST : SOUTHEAST
                        ))
                        pawn_uncheck_mask = en_passant;
                }
            }
        }
    }
}

void Board::_update_attacked_cells_mask()
{
    if (white_turn)
    {
        _apply_function_on_all_pieces(
            black_pawns, [this](uint64_t param) { _find_black_pawns_attacks(param); }
        );
        _apply_function_on_all_pieces(
            black_knights, [this](uint64_t param) { _find_black_knights_attacks(param); }
        );
        _apply_function_on_all_pieces(
            black_bishops, [this](uint64_t param) { _find_black_bishops_attacks(param); }
        );
        _apply_function_on_all_pieces(
            black_rooks, [this](uint64_t param) { _find_black_rooks_attacks(param); }
        );
        _apply_function_on_all_pieces(
            black_queens, [this](uint64_t param) { _find_black_queens_attacks(param); }
        );
        _find_black_king_attacks();
    }
    else
    {
        _apply_function_on_all_pieces(
            white_pawns, [this](uint64_t param) { _find_white_pawns_attacks(param); }
        );
        _apply_function_on_all_pieces(
            white_knights, [this](uint64_t param) { _find_white_knights_attacks(param); }
        );
        _apply_function_on_all_pieces(
            white_bishops, [this](uint64_t param) { _find_white_bishops_attacks(param); }
        );
        _apply_function_on_all_pieces(
            white_rooks, [this](uint64_t param) { _find_white_rooks_attacks(param); }
        );
        _apply_function_on_all_pieces(
            white_queens, [this](uint64_t param) { _find_white_queens_attacks(param); }
        );
        _find_white_king_attacks();
    }
}

void Board::_update_engine_at_turn_end()
{
    moves_computed = false;
    game_state_computed = false;
    engine_data_updated = false;

    en_passant = next_turn_en_passant;
    next_turn_en_passant = 0UL;

    half_turn_rule += 1;

    // Only increment game turn after black turn
    if (!white_turn)
        game_turn += 1;
    white_turn = !white_turn;

    _update_fen_history();
}

void Board::_update_fen_history()
{
    // Removing half turn rule and game turn (For future Threefold rule comparisons)
    string fen = create_fen(false);

    // Loop the history
    if (fen_history_index == FEN_HISTORY_SIZE)
        fen_history_index = 0;

    fen_history[fen_history_index++] = fen;
}

// - Piece attacks -

void Board::_find_white_pawns_attacks(uint64_t src)
{
    int src_lkt_i = _count_trailing_zeros(src);
    attacked_cells_mask |= pawn_captures_lookup[src_lkt_i][0];
}

void Board::_find_white_knights_attacks(uint64_t src)
{
    int src_lkt_i = _count_trailing_zeros(src);
    attacked_cells_mask |= knight_lookup[src_lkt_i];
}

void Board::_find_white_bishops_attacks(uint64_t src)
{
    attacked_cells_mask |= _get_diagonal_rays(src, ally_king);
}

void Board::_find_white_rooks_attacks(uint64_t src)
{
    attacked_cells_mask |= _get_line_rays(src, ally_king);
}

void Board::_find_white_queens_attacks(uint64_t src)
{
    attacked_cells_mask |= _get_diagonal_rays(src, ally_king) | _get_line_rays(src, ally_king);
}

void Board::_find_white_king_attacks()
{
    // TODO: This condition is only needed for testing purposes, when no king is on the board
    if (white_king)
    {
        int src_lkt_i = _count_trailing_zeros(white_king);
        attacked_cells_mask |= king_lookup[src_lkt_i];
    }
}

void Board::_find_black_pawns_attacks(uint64_t src)
{
    int src_lkt_i = _count_trailing_zeros(src);
    attacked_cells_mask |= pawn_captures_lookup[src_lkt_i][1];
}

void Board::_find_black_knights_attacks(uint64_t src)
{
    int src_lkt_i = _count_trailing_zeros(src);
    attacked_cells_mask |= knight_lookup[src_lkt_i];
}

void Board::_find_black_bishops_attacks(uint64_t src)
{
    attacked_cells_mask |= _get_diagonal_rays(src, ally_king);
}

void Board::_find_black_rooks_attacks(uint64_t src)
{
    attacked_cells_mask |= _get_line_rays(src, ally_king);
}

void Board::_find_black_queens_attacks(uint64_t src)
{
    attacked_cells_mask |= _get_diagonal_rays(src, ally_king) | _get_line_rays(src, ally_king);
}

void Board::_find_black_king_attacks()
{
    // TODO: This condition is only needed for testing purposes, when no king is on the board
    if (black_king)
    {
        int src_lkt_i = _count_trailing_zeros(black_king);
        attacked_cells_mask |= king_lookup[src_lkt_i];
    }
}

// - Move creation -

void Board::_find_moves()
{
    // Reset move list
    this->available_moves.clear();

    if (white_turn)
    {
        if (!double_check)
        {
            _apply_function_on_all_pieces(
                white_pawns, [this](uint64_t param) { _find_white_pawns_moves(param); }
            );
            _apply_function_on_all_pieces(
                white_queens, [this](uint64_t param) { _find_white_queens_moves(param); }
            );
            _apply_function_on_all_pieces(
                white_rooks, [this](uint64_t param) { _find_white_rooks_moves(param); }
            );
            _apply_function_on_all_pieces(
                white_bishops, [this](uint64_t param) { _find_white_bishops_moves(param); }
            );
            _apply_function_on_all_pieces(
                white_knights, [this](uint64_t param) { _find_white_knights_moves(param); }
            );
            _apply_function_on_all_pieces(
                white_castles, [this](uint64_t param) { _find_white_castle_moves(param); }
            );
        }
        _find_white_king_moves();
    }
    else
    {
        if (!double_check)
        {
            _apply_function_on_all_pieces(
                black_pawns, [this](uint64_t param) { _find_black_pawns_moves(param); }
            );
            _apply_function_on_all_pieces(
                black_queens, [this](uint64_t param) { _find_black_queens_moves(param); }
            );
            _apply_function_on_all_pieces(
                black_rooks, [this](uint64_t param) { _find_black_rooks_moves(param); }
            );
            _apply_function_on_all_pieces(
                black_bishops, [this](uint64_t param) { _find_black_bishops_moves(param); }
            );
            _apply_function_on_all_pieces(
                black_knights, [this](uint64_t param) { _find_black_knights_moves(param); }
            );
            _apply_function_on_all_pieces(
                black_castles, [this](uint64_t param) { _find_black_castle_moves(param); }
            );
        }
        _find_black_king_moves();
    }

    this->moves_computed = true;
}

void Board::_find_white_pawns_moves(uint64_t src)
{
    // Generate pawn moves: (pawn position shifted) & ~ColumnA & enemy_pieces & check_mask &
    int src_lkt_i = _count_trailing_zeros(src);

    uint64_t capture_moves = pawn_captures_lookup[src_lkt_i][0] & capturable_by_white_pawns_mask;
    uint64_t advance_move = (src >> 8) & empty_cells_mask;
    uint64_t legal_moves =
        (capture_moves | advance_move) & pawn_uncheck_mask & pin_masks[src_lkt_i];

    // When pawn is on the 7th rank, it can move two squares forward
    // We just need to check if the squares in front of it are empty
    if (src & BITMASK_LINE_2 && (src >> 8) & empty_cells_mask)
        legal_moves |= (src >> 16) & empty_cells_mask & pawn_uncheck_mask & pin_masks[src_lkt_i];

    // Find all individual bits in legal_moves
    uint64_t dst;
    while (legal_moves)
    {
        dst = _get_least_significant_bit(legal_moves);
        _add_regular_move_or_promotion('P', src, dst);

        // Remove the actual bit from legal_moves, so we can find the next one
        legal_moves ^= dst;
    }
}

void Board::_find_white_knights_moves(uint64_t src)
{
    int      src_lkt_i = _count_trailing_zeros(src);
    uint64_t legal_moves =
        knight_lookup[src_lkt_i] & not_white_pieces_mask & uncheck_mask & pin_masks[src_lkt_i];

    _create_piece_moves('N', src, legal_moves);
}

void Board::_find_white_bishops_moves(uint64_t src)
{
    int      src_lkt_i = _count_trailing_zeros(src);
    uint64_t legal_moves =
        not_white_pieces_mask & _get_diagonal_rays(src) & uncheck_mask & pin_masks[src_lkt_i];

    _create_piece_moves('B', src, legal_moves);
}

void Board::_find_white_rooks_moves(uint64_t src)
{
    int      src_lkt_i = _count_trailing_zeros(src);
    uint64_t legal_moves =
        not_white_pieces_mask & _get_line_rays(src) & uncheck_mask & pin_masks[src_lkt_i];

    _create_piece_moves('R', src, legal_moves);
}

void Board::_find_white_queens_moves(uint64_t src)
{
    int      src_lkt_i = _count_trailing_zeros(src);
    uint64_t legal_moves = not_white_pieces_mask & (_get_diagonal_rays(src) | _get_line_rays(src)) &
                           uncheck_mask & pin_masks[src_lkt_i];

    _create_piece_moves('Q', src, legal_moves);
}

void Board::_find_white_king_moves()
{
    if (white_king)
    {
        int      src_lkt_i = _count_trailing_zeros(white_king);
        uint64_t legal_moves =
            king_lookup[src_lkt_i] & not_white_pieces_mask & ~attacked_cells_mask;

        _create_piece_moves('K', white_king, legal_moves);
    }
}

void Board::_find_white_castle_moves(uint64_t rook)
{
    if (white_king && !check_state)
    {
        uint64_t      rook_path;
        uint64_t      king_path;
        castle_info_e castle_info;
        if (rook < white_king)
        {
            castle_info = WHITELEFT;

            if (white_king < BITMASK_CASTLE_WHITE_LEFT_KING)
                king_path =
                    _compute_castling_positive_path(white_king, BITMASK_CASTLE_WHITE_LEFT_KING);
            else
                king_path =
                    _compute_castling_negative_path(white_king, BITMASK_CASTLE_WHITE_LEFT_KING);
            if (rook < BITMASK_CASTLE_WHITE_LEFT_ROOK)
                rook_path = _compute_castling_positive_path(rook, BITMASK_CASTLE_WHITE_LEFT_ROOK);
            else
                rook_path = _compute_castling_negative_path(rook, BITMASK_CASTLE_WHITE_LEFT_ROOK);
        }
        else
        {
            castle_info = WHITERIGHT;

            if (white_king < BITMASK_CASTLE_WHITE_RIGHT_KING)
                king_path =
                    _compute_castling_positive_path(white_king, BITMASK_CASTLE_WHITE_RIGHT_KING);
            else
                king_path =
                    _compute_castling_negative_path(white_king, BITMASK_CASTLE_WHITE_RIGHT_KING);
            if (rook < BITMASK_CASTLE_WHITE_RIGHT_ROOK)
                rook_path = _compute_castling_positive_path(rook, BITMASK_CASTLE_WHITE_RIGHT_ROOK);
            else
                rook_path = _compute_castling_negative_path(rook, BITMASK_CASTLE_WHITE_RIGHT_ROOK);
        }

        // Castle is legal if :
        // - No pieces are blocking both king and rook paths
        // - No cells are attacked in the king path
        // - Rook isn't pinned
        if (((king_path | rook_path) & (all_pieces_mask ^ white_king ^ rook)) == 0UL &&
            (king_path & attacked_cells_mask) == 0UL &&
            pin_masks[_count_trailing_zeros(rook)] == BITMASK_ALL_CELLS)
        {
            this->available_moves.push_back(Move('K', white_king, rook, 0, castle_info));
        }
    }
}

void Board::_find_black_pawns_moves(uint64_t src)
{
    int src_lkt_i = _count_trailing_zeros(src);

    uint64_t capture_moves = pawn_captures_lookup[src_lkt_i][1] & capturable_by_black_pawns_mask;
    uint64_t advance_move = (src << 8) & empty_cells_mask;
    uint64_t legal_moves =
        (capture_moves | advance_move) & pawn_uncheck_mask & pin_masks[src_lkt_i];

    // When pawn is on the 7th rank, it can move two squares forward
    // We just need to check if the squares in front of it are empty
    if (src & BITMASK_LINE_7 && (src << 8) & empty_cells_mask)
        legal_moves |= (src << 16) & empty_cells_mask & pawn_uncheck_mask & pin_masks[src_lkt_i];

    // Find all individual bits in legal_moves
    uint64_t dst;
    while (legal_moves)
    {
        dst = _get_least_significant_bit(legal_moves);
        _add_regular_move_or_promotion('p', src, dst);

        // Remove the actual bit from legal_moves, so we can find the next one
        legal_moves ^= dst;
    }
}

void Board::_find_black_knights_moves(uint64_t src)
{
    int      src_lkt_i = _count_trailing_zeros(src);
    uint64_t legal_moves =
        knight_lookup[src_lkt_i] & not_black_pieces_mask & uncheck_mask & pin_masks[src_lkt_i];

    _create_piece_moves('n', src, legal_moves);
}

void Board::_find_black_bishops_moves(uint64_t src)
{
    int      src_lkt_i = _count_trailing_zeros(src);
    uint64_t legal_moves =
        not_black_pieces_mask & _get_diagonal_rays(src) & uncheck_mask & pin_masks[src_lkt_i];

    _create_piece_moves('b', src, legal_moves);
}

void Board::_find_black_rooks_moves(uint64_t src)
{
    int      src_lkt_i = _count_trailing_zeros(src);
    uint64_t legal_moves =
        not_black_pieces_mask & _get_line_rays(src) & uncheck_mask & pin_masks[src_lkt_i];

    _create_piece_moves('r', src, legal_moves);
}

void Board::_find_black_queens_moves(uint64_t src)
{
    int      src_lkt_i = _count_trailing_zeros(src);
    uint64_t legal_moves = not_black_pieces_mask & (_get_diagonal_rays(src) | _get_line_rays(src)) &
                           uncheck_mask & pin_masks[src_lkt_i];

    _create_piece_moves('q', src, legal_moves);
}

void Board::_find_black_king_moves()
{
    if (black_king)
    {
        int      src_lkt_i = _count_trailing_zeros(black_king);
        uint64_t legal_moves =
            king_lookup[src_lkt_i] & not_black_pieces_mask & ~attacked_cells_mask;

        _create_piece_moves('k', black_king, legal_moves);
    }
}

void Board::_find_black_castle_moves(uint64_t rook)
{
    if (black_king && !check_state)
    {
        uint64_t      rook_path;
        uint64_t      king_path;
        castle_info_e castle_info;
        if (rook < black_king)
        {
            castle_info = BLACKLEFT;

            if (black_king < BITMASK_CASTLE_BLACK_LEFT_KING)
                king_path =
                    _compute_castling_positive_path(black_king, BITMASK_CASTLE_BLACK_LEFT_KING);
            else
                king_path =
                    _compute_castling_negative_path(black_king, BITMASK_CASTLE_BLACK_LEFT_KING);
            if (rook < BITMASK_CASTLE_BLACK_LEFT_ROOK)
                rook_path = _compute_castling_positive_path(rook, BITMASK_CASTLE_BLACK_LEFT_ROOK);
            else
                rook_path = _compute_castling_negative_path(rook, BITMASK_CASTLE_BLACK_LEFT_ROOK);
        }
        else
        {
            castle_info = BLACKRIGHT;

            if (black_king < BITMASK_CASTLE_BLACK_RIGHT_KING)
                king_path =
                    _compute_castling_positive_path(black_king, BITMASK_CASTLE_BLACK_RIGHT_KING);
            else
                king_path =
                    _compute_castling_negative_path(black_king, BITMASK_CASTLE_BLACK_RIGHT_KING);
            if (rook < BITMASK_CASTLE_BLACK_RIGHT_ROOK)
                rook_path = _compute_castling_positive_path(rook, BITMASK_CASTLE_BLACK_RIGHT_ROOK);
            else
                rook_path = _compute_castling_negative_path(rook, BITMASK_CASTLE_BLACK_RIGHT_ROOK);
        }

        // Castle is legal if :
        // - No pieces are blocking both king and rook paths
        // - No cells are attacked in the king path
        // - Rook isn't pinned
        if (((king_path | rook_path) & (all_pieces_mask ^ black_king ^ rook)) == 0UL &&
            (king_path & attacked_cells_mask) == 0UL &&
            (pin_masks[_count_trailing_zeros(rook)] == BITMASK_ALL_CELLS))
        {
            this->available_moves.push_back(Move('k', black_king, rook, 0, castle_info));
        }
    }
}

void Board::_add_regular_move_or_promotion(char piece, uint64_t src, uint64_t dst)
{
    if (dst & BITMASK_LINE_81)
    {
        // Promotions (As UCI representation is always lowercase, finale piece case doesn't matter)
        this->available_moves.push_back(Move(piece, src, dst, 'n'));
        this->available_moves.push_back(Move(piece, src, dst, 'b'));
        this->available_moves.push_back(Move(piece, src, dst, 'r'));
        this->available_moves.push_back(Move(piece, src, dst, 'q'));
    }
    else
        this->available_moves.push_back(Move(piece, src, dst));
}

void Board::_create_piece_moves(char piece, uint64_t src, uint64_t legal_moves)
{
    // Find all individual bits in legal_moves
    uint64_t dst;
    while (legal_moves)
    {
        dst = _get_least_significant_bit(legal_moves);
        this->available_moves.push_back(Move(piece, src, dst));

        // Remove the actual bit from legal_moves, so we can find the next one
        legal_moves ^= dst;
    }
}

// - Bit operations -

void Board::_apply_function_on_all_pieces(uint64_t bitboard, std::function<void(uint64_t)> func)
{
    // Find all individual bits in bitboard
    uint64_t piece;
    while (bitboard)
    {
        piece = _get_least_significant_bit(bitboard);
        func(piece);

        // Remove the actual bit from bitboard, so we can find the next one
        bitboard ^= piece;
    }
}

uint64_t Board::_get_diagonal_rays(uint64_t src, uint64_t piece_to_ignore)
{
    return _compute_sliding_piece_positive_ray(src, SOUTHEAST, piece_to_ignore) |
           _compute_sliding_piece_positive_ray(src, SOUTHWEST, piece_to_ignore) |
           _compute_sliding_piece_negative_ray(src, NORTHWEST, piece_to_ignore) |
           _compute_sliding_piece_negative_ray(src, NORTHEAST, piece_to_ignore);
}

uint64_t Board::_get_line_rays(uint64_t src, uint64_t piece_to_ignore)
{
    return _compute_sliding_piece_positive_ray(src, EAST, piece_to_ignore) |
           _compute_sliding_piece_positive_ray(src, SOUTH, piece_to_ignore) |
           _compute_sliding_piece_negative_ray(src, WEST, piece_to_ignore) |
           _compute_sliding_piece_negative_ray(src, NORTH, piece_to_ignore);
}

uint64_t
Board::_compute_sliding_piece_positive_ray(uint64_t src, ray_dir_e dir, uint64_t piece_to_ignore)
{
    int      src_lkt_i = _count_trailing_zeros(src);
    uint64_t attacks = sliding_lookup[src_lkt_i][dir];

    // Find all pieces in the ray (Ignoring ally king when searching attacked cells)
    uint64_t blockers = attacks & (all_pieces_mask ^ piece_to_ignore);
    if (blockers)
    {
        // Find the first blocker in the ray (the one with the smallest index)
        int src_lkt_i = _count_trailing_zeros(blockers);

        // Remove all squares behind the blocker from the attacks
        attacks ^= sliding_lookup[src_lkt_i][dir];
    }

    // Blocker should be in the ray, so ~color_pieces_mask removes it if it's an ally
    return attacks;
}

uint64_t
Board::_compute_sliding_piece_negative_ray(uint64_t src, ray_dir_e dir, uint64_t piece_to_ignore)
{
    int      src_lkt_i = _count_trailing_zeros(src);
    uint64_t attacks = sliding_lookup[src_lkt_i][dir];

    // Find all pieces in the ray (Ignoring ally king when searching attacked cells)
    uint64_t blockers = attacks & (all_pieces_mask ^ piece_to_ignore);
    if (blockers)
    {
        // Find the first blocker in the ray (the one with the biggest index)
        uint64_t blocker = _get_most_significant_bit(blockers);
        int      src_lkt_i = _count_trailing_zeros(blocker);

        // Remove all squares behind the blocker from the attacks
        attacks ^= sliding_lookup[src_lkt_i][dir];
    }

    // Blocker should be in the ray, so ~color_pieces_mask removes it if it's an ally
    return attacks;
}

void Board::_compute_sliding_piece_positive_ray_checks_and_pins(
    uint64_t king_pos, ray_dir_e dir, uint64_t potential_attacker
)
{
    int      king_lkt_i = _count_trailing_zeros(king_pos);
    uint64_t attacks = sliding_lookup[king_lkt_i][dir];

    // Find all pieces in the ray
    uint64_t blockers = attacks & all_pieces_mask;

    if (blockers)
    {
        // Find the first blocker in the ray (the one with the smallest index)
        uint64_t blocker = _get_least_significant_bit(blockers);
        int      blocker_lkt_i = _count_trailing_zeros(blocker);

        // If the blocker is a potential attacker, there is check
        if (blocker & potential_attacker)
        {
            if (check_state)
                double_check = true;
            check_state = true;

            // Remove all squares behind the blocker
            uncheck_mask |= attacks ^ sliding_lookup[blocker_lkt_i][dir];
        }

        // If the blocker is an ally, it might be pinned
        else if (blocker & ally_pieces)
        {
            // Remove this blocker from the mask, and look for further pieces in the ray
            blockers ^= blocker;
            if (blockers)
            {
                blocker = _get_least_significant_bit(blockers);

                // If the new blocker is a potential attacker, there is a pin
                if (blocker & potential_attacker)
                {
                    // Remove all squares behind the blocker
                    int      second_blocker_lkt_i = _count_trailing_zeros(blocker);
                    uint64_t pin_ray = attacks ^ sliding_lookup[second_blocker_lkt_i][dir];

                    // Save this pin ray on the ally blocker cell
                    pin_masks[blocker_lkt_i] = pin_ray;
                }
            }
        }
    }
}

void Board::_compute_sliding_piece_negative_ray_checks_and_pins(
    uint64_t king_pos, ray_dir_e dir, uint64_t potential_attacker
)
{
    int      king_lkt_i = _count_trailing_zeros(king_pos);
    uint64_t attacks = sliding_lookup[king_lkt_i][dir];

    // Find all pieces in the ray
    uint64_t blockers = attacks & all_pieces_mask;

    if (blockers)
    {
        // Find the first blocker in the ray (the one with the greatest index)
        uint64_t blocker = _get_most_significant_bit(blockers);
        int      blocker_lkt_i = _count_trailing_zeros(blocker);

        // If the blocker is a potential attacker, there is check
        if (blocker & potential_attacker)
        {
            if (check_state)
                double_check = true;
            check_state = true;

            // Remove all squares behind the blocker
            uncheck_mask |= attacks ^ sliding_lookup[blocker_lkt_i][dir];
        }

        // If the blocker is an ally, it might be pinned
        else if (blocker & ally_pieces)
        {
            // Remove this blocker from the mask, and look for further pieces in the ray
            blockers ^= blocker;
            if (blockers)
            {
                blocker = _get_most_significant_bit(blockers);

                // If the new blocker is a potential attacker, there is a pin
                if (blocker & potential_attacker)
                {
                    // Remove all squares behind the blocker
                    int      second_blocker_lkt_i = _count_trailing_zeros(blocker);
                    uint64_t pin_ray = attacks ^ sliding_lookup[second_blocker_lkt_i][dir];

                    // Save this pin ray on the ally blocker cell
                    pin_masks[blocker_lkt_i] = pin_ray;
                }
            }
        }
    }
}

bool Board::_is_sliding_piece_positive_diagonal_ray_behind(uint64_t pawn_pos, ray_dir_e dir)
{
    int      pawn_lkt_i = _count_trailing_zeros(pawn_pos);
    uint64_t attacks = sliding_lookup[pawn_lkt_i][dir];

    // Find all pieces in the ray
    uint64_t blockers = attacks & all_pieces_mask;
    if (blockers)
    {
        // Find the first blocker in the ray (the one with the smallest index)
        uint64_t blocker = _get_least_significant_bit(blockers);

        if (blocker & enemy_pieces_sliding_diag)
            return true;
    }

    return false;
}

bool Board::_is_sliding_piece_negative_diagonal_ray_behind(uint64_t pawn_pos, ray_dir_e dir)
{
    int      pawn_lkt_i = _count_trailing_zeros(pawn_pos);
    uint64_t attacks = sliding_lookup[pawn_lkt_i][dir];

    // Find all pieces in the ray
    uint64_t blockers = attacks & all_pieces_mask;
    if (blockers)
    {
        // Find the first blocker in the ray (the one with the greatest index)
        uint64_t blocker = _get_most_significant_bit(blockers);

        if (blocker & enemy_pieces_sliding_diag)
            return true;
    }

    return false;
}

uint64_t Board::_compute_castling_positive_path(uint64_t src, uint64_t dst)
{
    int src_lkt_i = _count_trailing_zeros(src);
    int dst_lkt_i = _count_trailing_zeros(dst);

    return sliding_lookup[src_lkt_i][EAST] ^ sliding_lookup[dst_lkt_i][EAST];
}

uint64_t Board::_compute_castling_negative_path(uint64_t src, uint64_t dst)
{
    int src_lkt_i = _count_trailing_zeros(src);
    int dst_lkt_i = _count_trailing_zeros(dst);

    return sliding_lookup[src_lkt_i][WEST] ^ sliding_lookup[dst_lkt_i][WEST];
}

// - End game -

float Board::_compute_game_state()
{
    // Fifty-Move rule + Game turn limit + 2 other rules to detect a draw
    if (half_turn_rule >= 99 || _threefold_repetition_rule() || _insufficient_material_rule())
        return DRAW;

    // Convert this to PRE PROCESSING if ?
    if (codingame_rule)
    {
        if (game_turn > 125)
            return DRAW;
    }

    vector<Move> moves = get_available_moves();

    // If no moves are available, it's either a Checkmate or a Stalemate
    if (moves.size() == 0)
    {
        if (get_check_state())
            return white_turn ? BLACK_WIN : WHITE_WIN;
        return DRAW;
    }

    return GAME_CONTINUE;
}

bool Board::_threefold_repetition_rule()
{
    int    actual_fen_index = fen_history_index - 1;
    string actual_fen = fen_history[actual_fen_index];

    // Check if the actual FEN is already 2 times in the history
    // Loop over all FEN_HISTORY_SIZE last moves, skipping the actual one
    bool fen_found = false;
    int  history_index = -1;
    while (++history_index < FEN_HISTORY_SIZE)
        if (history_index != actual_fen_index && fen_history[history_index] == actual_fen)
        {
            if (fen_found)
                return true;
            fen_found = true;
        }

    return false;
}

bool Board::_insufficient_material_rule()
{
    if (white_pawns | black_pawns | white_rooks | black_rooks | white_queens | black_queens)
        return false;

    int white_knights_count = _count_bits(white_knights);
    int black_knights_count = _count_bits(black_knights);
    int knights_count = white_knights_count + black_knights_count;

    // If there is multiple knights, it's not a material insufficient
    if (knights_count > 1)
        return false;

    int white_bishops_count = _count_bits(white_bishops);
    int black_bishops_count = _count_bits(black_bishops);
    int bishops_count = white_bishops_count + black_bishops_count;

    // If there is a knight and another piece, it's not a material insufficient
    if (knights_count == 1 && bishops_count > 0)
        return false;

    // If there is more than one bishop, on opposite colors, it's not a material insufficient
    uint64_t all_bishops = white_bishops | black_bishops;
    if ((all_bishops & BITMASK_WHITE_CELLS) && (all_bishops & BITMASK_BLACK_CELLS))
        return false;

    return true;
}

// --- OPERATORS ---

bool Board::operator==(Board *test_board_abstracted)
{
    // TODO: move this in the Board class, as inlined method
    return this->create_fen() == test_board_abstracted->create_fen();
}

// --- STATIC LOOKUP METHODS ---

void Board::_initialize_lookup_tables()
{
    memset(pawn_captures_lookup, 0, sizeof(uint64_t) * 64 * 2);
    memset(knight_lookup, 0, sizeof(uint64_t) * 64);
    memset(sliding_lookup, 0, sizeof(uint64_t) * 64 * 8);
    memset(king_lookup, 0, sizeof(uint64_t) * 64);

    int      lkt_i = 0;
    uint64_t pos_mask = 1UL;
    for (int y = 0; y < 8; y++)
    {
        for (int x = 0; x < 8; x++)
        {
            // Create the lookup table for the current position
            _create_pawn_captures_lookup_table(y, x, pos_mask, lkt_i);
            _create_knight_lookup_table(y, x, pos_mask, lkt_i);
            _create_sliding_lookup_table(y, x, pos_mask, lkt_i);
            _create_king_lookup_table(y, x, pos_mask, lkt_i);
            lkt_i++;
            pos_mask <<= 1;
        }
    }

    Board::lookup_tables_initialized = true;
}

void Board::_create_pawn_captures_lookup_table(int y, int x, uint64_t position, int lkt_i)
{
    // We need pawn attacks on lines 0 and 7, because we are using them to find checks (when kings
    // are on those lines)

    // - White pawns -
    uint64_t pawn_mask = 0UL;

    // Left capture
    if (x > 0 && y > 0)
        pawn_mask |= (position >> 9);
    // Right capture
    if (x < 7 && y > 0)
        pawn_mask |= (position >> 7);

    pawn_captures_lookup[lkt_i][0] = pawn_mask;

    // - Black pawns -
    pawn_mask = 0UL;

    // Left capture
    if (x > 0 && y < 7)
        pawn_mask |= position << 7;
    // Right capture
    if (x < 7 && y < 7)
        pawn_mask |= position << 9;

    pawn_captures_lookup[lkt_i][1] = pawn_mask;
}

void Board::_create_knight_lookup_table(int y, int x, uint64_t position, int lkt_i)
{
    uint64_t knight_mask = 0UL;

    if (y > 0)
    {
        // 2 left 1 down
        if (x > 1)
            knight_mask |= position >> 10;
        // 2 right 1 down
        if (x < 6)
            knight_mask |= position >> 6;
    }
    if (y > 1)
    {
        // 2 up 1 left
        if (x > 0)
            knight_mask |= position >> 17;
        // 2 up 1 right
        if (x < 7)
            knight_mask |= position >> 15;
    }
    if (y < 6)
    {
        // 2 down 1 left
        if (x > 0)
            knight_mask |= position << 15;
        // 2 down 1 right
        if (x < 7)
            knight_mask |= position << 17;
    }
    if (y < 7)
    {
        // 2 left 1 up
        if (x > 1)
            knight_mask |= position << 6;
        // 2 right 1 up
        if (x < 6)
            knight_mask |= position << 10;
    }

    knight_lookup[lkt_i] = knight_mask;
}

void Board::_create_sliding_lookup_table(int y, int x, uint64_t position, int lkt_i)
{
    // North
    uint64_t sliding_pos = position;
    for (int j = y - 1; j >= 0; j--)
    {
        sliding_pos >>= 8;
        sliding_lookup[lkt_i][NORTH] |= sliding_pos;
    }

    // North-East
    sliding_pos = position;
    for (int j = y - 1, i = x + 1; j >= 0 && i < 8; j--, i++)
    {
        sliding_pos >>= 7;
        sliding_lookup[lkt_i][NORTHEAST] |= sliding_pos;
    }

    // East
    sliding_pos = position;
    for (int i = x + 1; i < 8; i++)
    {
        sliding_pos <<= 1;
        sliding_lookup[lkt_i][EAST] |= sliding_pos;
    }

    // South-East
    sliding_pos = position;
    for (int j = y + 1, i = x + 1; j < 8 && i < 8; j++, i++)
    {
        sliding_pos <<= 9;
        sliding_lookup[lkt_i][SOUTHEAST] |= sliding_pos;
    }

    // South
    sliding_pos = position;
    for (int j = y + 1; j < 8; j++)
    {
        sliding_pos <<= 8;
        sliding_lookup[lkt_i][SOUTH] |= sliding_pos;
    }

    // South-West
    sliding_pos = position;
    for (int j = y + 1, i = x - 1; j < 8 && i >= 0; j++, i--)
    {
        sliding_pos <<= 7;
        sliding_lookup[lkt_i][SOUTHWEST] |= sliding_pos;
    }

    // West
    sliding_pos = position;
    for (int i = x - 1; i >= 0; i--)
    {
        sliding_pos >>= 1;
        sliding_lookup[lkt_i][WEST] |= sliding_pos;
    }

    // North-West
    sliding_pos = position;
    for (int j = y - 1, i = x - 1; j >= 0 && i >= 0; j--, i--)
    {
        sliding_pos >>= 9;
        sliding_lookup[lkt_i][NORTHWEST] |= sliding_pos;
    }
}

void Board::_create_king_lookup_table(int y, int x, uint64_t position, int lkt_i)
{
    uint64_t king_mask = 0UL;
    if (x > 0)
    {
        // 1 up left
        if (y > 0)
            king_mask |= position >> 9;
        // 1 left
        king_mask |= position >> 1;
        // 1 down left
        if (y < 7)
            king_mask |= position << 7;
    }
    if (x < 7)
    {
        // 1 up right
        if (y > 0)
            king_mask |= position >> 7;
        // 1 right
        king_mask |= position << 1;
        // 1 down right
        if (y < 7)
            king_mask |= position << 9;
    }
    // 1 up
    if (y > 0)
        king_mask |= position >> 8;
    // 1 down
    if (y < 7)
        king_mask |= position << 8;

    king_lookup[lkt_i] = king_mask;
}

/*
        Content of 'srcs/chessengine/Move.cpp'
*/

// --- PUBLIC METHODS ---

Move::Move(string _uci)
{
    // Normal move, castles:   e2e4,
    // Promotion:              e7e8q
    this->uci = _uci;

    // First 4 characters represent the move
    this->src = algebraic_to_bitboard(_uci.substr(0, 2));
    this->dst = algebraic_to_bitboard(_uci.substr(2, 2));

    // A fifth character represent the promotion (Always
    // lowercase)
    this->promotion = _uci.length() > 4 ? _uci[4] : 0;

    // Initialize internal data
    this->piece = EMPTY_CELL;
    this->castle_info = NOINFO;
}

Move::Move(char _piece, uint64_t _src, uint64_t _dst, char _promotion, castle_info_e _castle_info)
{
    this->piece = _piece;
    this->src = _src;
    this->dst = _dst;
    this->promotion = _promotion;

    // Initialize internal data
    this->uci = "";
    this->castle_info = _castle_info;
}

void Move::log()
{
    cerr << "Move: Piece = " << this->piece << endl;
    cerr << "Move: src = " << bitboard_to_algebraic(this->src) << endl;
    cerr << "Move: dst = " << bitboard_to_algebraic(this->dst) << endl;
    cerr << "Move: Promote to " << (char)(this->promotion ? this->promotion : EMPTY_CELL) << endl;
    cerr << "Move: Castle info = " << this->castle_info << endl;
    cerr << "Move UCI: " << this->to_uci() << endl;
}

string Move::to_uci()
{
    return this->to_uci(CHESS960_RULES, this->castle_info > 1);
}

string Move::to_uci(bool chess960_rules, bool castling)
{
    // cerr << "Move::to_uci() - chess960_rules = " <<
    // chess960_rules << " - castling = " << castling << endl;
    // this->log();

    if (this->uci != "")
        return this->uci;

    /*
        In my implementation, castling moves are always
       represented by a king moving to its own rook, as Chess960
       rules. With standard rules, the UCI representation of a
       castle is the king moving of 2 cells. So when the rules
       are standard, the destination position must be hardcoded
       so the UCI representation is correct.
    */
    uint64_t tmp_dst = this->dst;
    if (castling && !chess960_rules)
    {
        // Despite the rules, the final position
        // after castling is always the same:
        // Standard castles :       e1g1 or e1c1
        if (this->dst & 0xFFUL)
            tmp_dst = this->dst < this->src ? 1UL << 2 : 1UL << 6;
        else
            tmp_dst = this->dst < this->src ? 1UL << 58 : 1UL << 62;
    }

    // Normal move + castles:   e1a4
    string uci = bitboard_to_algebraic(this->src) + bitboard_to_algebraic(tmp_dst);

    // Promotion:               e7e8q
    if (this->promotion)
        // UCI representation of promotions is
        // lowercase
        uci += string(1, tolower(this->promotion));

    this->uci = uci;
    return uci;
}

// --- OPERATORS ---

bool Move::operator==(Move *other)
{
    return this->to_uci() == other->to_uci() &&
           (this->piece == other->piece || this->piece == EMPTY_CELL || other->piece == EMPTY_CELL
           ) &&
           this->src == other->src && this->dst == other->dst &&
           tolower(this->promotion) == tolower(other->promotion) &&
           this->castle_info == other->castle_info;
}

bool Move::compare_move_vector(vector<Move> movelst1, vector<Move> movelst2)
{
    bool success = true;

    // Assert all legal moves were found by the engine
    for (Move move_f : movelst1)
    {
        if (!_is_move_in_movelst(&move_f, movelst2))
        {
            cerr << "- Move "
                    "from "
                    "vector 1 "
                    "isn't in "
                    "vector 2 : "
                 << move_f.to_uci() << endl;
            move_f.log();
            success = false;
        }
    }

    // Assert all moves found by the engine are legal
    for (Move move_f : movelst2)
    {
        if (!_is_move_in_movelst(&move_f, movelst1))
        {
            cerr << "- Move "
                    "from "
                    "vector 2 "
                    "isn't in "
                    "vector 1 : "
                 << move_f.to_uci() << endl;
            move_f.log();
            success = false;
        }
    }

    return success;
}

// --- PRIVATE METHODS ---

bool Move::_is_move_in_movelst(Move *move, vector<Move> movelst)
{
    for (Move move_f : movelst)
    {
        if (*move == &move_f)
            return true;
    }

    return false;
};

/*
        Content of 'srcs/agents/MinMaxAlphaBetaAgent.cpp'
*/

MinMaxAlphaBetaAgent::MinMaxAlphaBetaAgent(AbstractHeuristic *heuristic, int ms_constraint)
{
    this->_heuristic = heuristic;
    this->_ms_constraint = ms_constraint;
    this->_ms_turn_stop = ms_constraint * 0.95;
    this->_depth_reached = 0;
    this->_nodes_explored = 0;
    this->_start_time = 0;
}

void MinMaxAlphaBetaAgent::get_qualities(Board *board, vector<Move> moves, vector<float> *qualities)
{
    this->_start_time = clock();

    // Debugging information for CG
    cerr << std::bitset<64>(board->get_castling_rights()) << endl;

    for (size_t i = 0; i < moves.size(); i++)
        qualities->push_back(0);

    // If the max_depth is too low, it would recalculate too much moves !
    // All position at depth max_depth MUST be computed, else some moves won't have a quality
    int max_depth = 2;
    this->_nodes_explored = 0;
    while (!this->is_time_up())
    {
        for (size_t i = 0; i < moves.size(); i++)
        {
            Board new_board = *board;
            new_board.apply_move(moves[i]);

            float move_quality = this->minmax(&new_board, max_depth, 1, -1, 1);

            // If time is up, it shouldn't update the current move quality, because we can't say how
            // much children it has explored So it would end up having a random quality
            if (this->is_time_up())
                break;

            qualities->at(i) = move_quality;
        }

        max_depth++;
    }

    this->_depth_reached = max_depth;

    float dtime = elapsed_time();
    if (dtime >= _ms_constraint)
        cerr << "MinMaxAlphaBetaAgent: TIMEOUT: dtime=" << dtime << "/" << this->_ms_constraint
             << "ms" << endl;
}

vector<string> MinMaxAlphaBetaAgent::get_stats()
{
    vector<string> stats;

    stats.push_back("version=BbMmabPv-3.1.3");
    stats.push_back("depth=" + to_string(this->_depth_reached));
    stats.push_back("states=" + to_string(this->_nodes_explored));
    cerr << "BbMmabPv-3.1.3\t: stats=" << stats[0] << " " << stats[1] << " " << stats[2] << endl;
    return stats;
}

string MinMaxAlphaBetaAgent::get_name()
{
    return Board::get_name() + ".MinMaxAlphaBetaAgent[" + to_string(this->_ms_constraint) + "ms]." +
           this->_heuristic->get_name();
}

float MinMaxAlphaBetaAgent::minmax(Board *board, int max_depth, int depth, float alpha, float beta)
{
    this->_nodes_explored++;

    // If we reach the max depth or the game is over, we evaluate the board
    if (depth == max_depth || this->is_time_up() || board->get_game_state() != GAME_CONTINUE)
        return this->_heuristic->evaluate(board);

    vector<Move> moves = board->get_available_moves();

    // Go deeper in each child nodes and keep the best one
    float best_quality;
    if (board->is_white_turn())
    {
        best_quality = this->max_node(board, &moves, max_depth, depth, alpha, beta);
    }
    else
    {
        best_quality = this->min_node(board, &moves, max_depth, depth, alpha, beta);
    }

    return best_quality;
}

float MinMaxAlphaBetaAgent::max_node(
    Board *board, vector<Move> *moves, int max_depth, int depth, float alpha, float beta
)
{
    // White wants to maximize the heuristic value
    float best_quality = -1;
    for (Move move : *moves)
    {
        Board new_board = *board;
        new_board.apply_move(move);

        float child_quality = this->minmax(&new_board, max_depth, depth + 1, alpha, beta);

        // Stop the search if we run out of time, the actual branch won't be used anyway
        if (this->is_time_up())
            break;

        best_quality = max(best_quality, child_quality);

        // Alpha-beta pruning - Stop the search when we know the current node won't be chosen
        // - Beta cut : If we're in a max node and the current child max quality is higher than a
        // brother node
        if (beta <= best_quality)
            return best_quality;

        // Update alpha for the next brother nodes
        alpha = max(alpha, best_quality);
    }

    return best_quality;
}

float MinMaxAlphaBetaAgent::min_node(
    Board *board, vector<Move> *moves, int max_depth, int depth, float alpha, float beta
)
{
    // Black wants to minimize the heuristic value
    float best_quality = 1;
    for (Move move : *moves)
    {
        Board new_board = *board;
        new_board.apply_move(move);

        float child_quality = this->minmax(&new_board, max_depth, depth + 1, alpha, beta);

        // Stop the search if we run out of time, the actual branch won't be used anyway
        if (this->is_time_up())
            break;

        best_quality = min(best_quality, child_quality);

        // Alpha-beta pruning - Stop the search when we know the current node won't be chosen
        // - Alpha cut : If we're in a min node and the current child min quality is lower than a
        // brother node
        if (alpha >= best_quality)
            return best_quality;

        // Update beta for the next brother nodes
        beta = min(beta, best_quality);
    }

    return best_quality;
}

bool MinMaxAlphaBetaAgent::is_time_up()
{
    return this->elapsed_time() >= this->_ms_turn_stop;
}

float MinMaxAlphaBetaAgent::elapsed_time()
{
    return (float)(clock() - this->_start_time) / CLOCKS_PER_SEC * 1000;
}

/*
        Content of 'srcs/heuristics/PiecesHeuristic.cpp'
*/

#include <algorithm>
float PiecesHeuristic::evaluate(Board *board)
{
    float state = board->get_game_state();
    if (state != GAME_CONTINUE)
    {
        if (state == BLACK_WIN)
            return -1;
        else if (state == DRAW)
            return 0;
        else
            return 1;
    }

    int white_material;
    int black_material;
    int material_evaluation = _material_evaluation(board, &white_material, &black_material);

    // Start game material = 100 * 8 + 305 * 2 + 333 * 2 + 563 * 2 + 950 = 4 152
    // Consider that end game is approximately at 1800 material or less
    int white_material_in_bound = min(max(white_material, material_end_game), material_start_game);
    int black_material_in_bound = min(max(black_material, material_end_game), material_start_game);

    // Interpolate current material in start and end game interval, to a coefficient between 0 and 1
    // 0 = start game, 1 = end game
    float white_eg_coefficient =
        (float)(material_start_game - white_material_in_bound) / material_start_end_game_diff;
    float black_eg_coefficient =
        (float)(material_start_game - black_material_in_bound) / material_start_end_game_diff;

    int pp_evaluation =
        _piece_positions_evaluation(board, white_eg_coefficient, black_eg_coefficient);

    int evaluation = material_evaluation + pp_evaluation;
    if (evaluation > 0)
        return 1 - 1.0 / (1 + evaluation);
    return -1 - 1.0 / (-1 + evaluation);
}

string PiecesHeuristic::get_name()
{
    return "PiecesHeuristic";
}

int PiecesHeuristic::_material_evaluation(Board *board, int *white_material, int *black_material)
{
    int white_pawn_count = _count_bits(board->white_pawns);
    int white_knight_count = _count_bits(board->white_knights);
    int white_bishop_count = _count_bits(board->white_bishops);
    int white_rook_count = _count_bits(board->white_rooks);
    int white_queen_count = _count_bits(board->white_queens);
    int black_pawn_count = _count_bits(board->black_pawns);
    int black_knight_count = _count_bits(board->black_knights);
    int black_bishop_count = _count_bits(board->black_bishops);
    int black_rook_count = _count_bits(board->black_rooks);
    int black_queen_count = _count_bits(board->black_queens);

    // Material evaluation
    *white_material = white_pawn_count * PAWN_VALUE + white_knight_count * KNIGHT_VALUE +
                      white_bishop_count * BISHOP_VALUE + white_rook_count * ROOK_VALUE +
                      white_queen_count * QUEEN_VALUE;
    *black_material = black_pawn_count * PAWN_VALUE + black_knight_count * KNIGHT_VALUE +
                      black_bishop_count * BISHOP_VALUE + black_rook_count * ROOK_VALUE +
                      black_queen_count * QUEEN_VALUE;

    return *white_material - *black_material;
}

int PiecesHeuristic::_piece_positions_evaluation(
    Board *board, float white_eg_coefficient, float black_eg_coefficient
)
{
    int pp_eval = 0;

    pp_eval += _lookup_bonuses_for_all_pieces(
        white_pawn_sg_bonus_table, white_pawn_eg_bonus_table, white_eg_coefficient,
        board->white_pawns
    );
    pp_eval += _lookup_bonuses_for_all_pieces(white_knight_bonus_table, board->white_knights);
    pp_eval += _lookup_bonuses_for_all_pieces(white_bishop_bonus_table, board->white_bishops);
    pp_eval += _lookup_bonuses_for_all_pieces(white_rook_bonus_table, board->white_rooks);
    pp_eval += _lookup_bonuses_for_all_pieces(white_queen_bonus_table, board->white_queens);
    pp_eval += _lookup_bonuses_for_all_pieces(
        white_king_sg_bonus_table, white_king_eg_bonus_table, white_eg_coefficient,
        board->white_king
    );

    pp_eval -= _lookup_bonuses_for_all_pieces(
        black_pawn_sg_bonus_table, black_pawn_eg_bonus_table, black_eg_coefficient,
        board->black_pawns
    );
    pp_eval -= _lookup_bonuses_for_all_pieces(black_knight_bonus_table, board->black_knights);
    pp_eval -= _lookup_bonuses_for_all_pieces(black_bishop_bonus_table, board->black_bishops);
    pp_eval -= _lookup_bonuses_for_all_pieces(black_rook_bonus_table, board->black_rooks);
    pp_eval -= _lookup_bonuses_for_all_pieces(black_queen_bonus_table, board->black_queens);
    pp_eval -= _lookup_bonuses_for_all_pieces(
        black_king_sg_bonus_table, black_king_eg_bonus_table, black_eg_coefficient,
        board->black_king
    );

    return pp_eval;
}

int PiecesHeuristic::_lookup_bonuses_for_all_pieces(int *bonus_table, uint64_t bitboard)
{
    int bonuses = 0;

    // Find all individual bits in bitboard
    while (bitboard)
    {
        int first_piece_i = _count_trailing_zeros(bitboard);

        // Lookup table for this piece
        bonuses += bonus_table[first_piece_i];

        // Remove the actual bit from bitboard, so we can find the next one
        bitboard ^= 1UL << first_piece_i;
    }

    return bonuses;
}

int PiecesHeuristic::_lookup_bonuses_for_all_pieces(
    int *sg_bonus_table, int *eg_bonus_table, float eg_coef, uint64_t bitboard
)
{
    int   bonuses = 0;
    float sg_coef = 1 - eg_coef;

    // Find all individual bits in bitboard
    while (bitboard)
    {
        int first_piece_i = _count_trailing_zeros(bitboard);

        // Lookup both start game and end game bonus tables, and mix them
        bonuses +=
            sg_bonus_table[first_piece_i] * sg_coef + eg_bonus_table[first_piece_i] * eg_coef;

        // Remove the actual bit from bitboard, so we can find the next one
        bitboard ^= 1UL << first_piece_i;
    }

    return bonuses;
}

/*
        Content of 'srcs/players/BotPlayer.cpp'
*/

BotPlayer::BotPlayer(AbstractAgent *agent)
{

    this->_agent = agent;
}

Move BotPlayer::choose_from(Board *board, vector<Move> moves)
{
    vector<float> qualities;
    this->_agent->get_qualities(board, moves, &qualities);

    // Should be in the constructor
    // Depending on the player turn, we want to maximize or minimize the heuristic value
    float (BotPlayer::*best_heuristic_choose)(float, float) =
        board->is_white_turn() ? &BotPlayer::max_float : &BotPlayer::min_float;

    int   best_index = 0;
    float best_quality = qualities.at(0);
    float new_best_quality;
    for (size_t i = 1; i < qualities.size(); i++)
    {
        new_best_quality = (this->*best_heuristic_choose)(best_quality, qualities.at(i));

        // cerr << "Move: " << moves.at(i).to_uci() << " | Quality: " << qualities.at(i) << " | Best
        // quality: " << best_quality << " | New best quality: " << new_best_quality << endl;
        if (best_quality != new_best_quality)
        {
            best_index = i;
            best_quality = new_best_quality;
        }
    }

    return moves.at(best_index);
}

vector<string> BotPlayer::get_stats()
{
    return this->_agent->get_stats();
}

string BotPlayer::get_name()
{
    return "Bot." + this->_agent->get_name();
}

float BotPlayer::max_float(float a, float b)
{
    return max(a, b);
}

float BotPlayer::min_float(float a, float b)
{
    return min(a, b);
}

/*
        Content of 'mains/main.cpp'
*/

using namespace std;

int main()
{
    GameEngine *game_engine =
        new GameEngine(new BotPlayer(new MinMaxAlphaBetaAgent(new PiecesHeuristic(), 50)));
    game_engine->infinite_game_loop();
}
