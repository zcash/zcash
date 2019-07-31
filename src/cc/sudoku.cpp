// start https://github.com/attractivechaos/plb/blob/master/sudoku/incoming/sudoku_solver.c
/************************************************************************************/
/*                                                                                  */
/* Author: Bill DuPree                                                              */
/* Name: sudoku_solver.c                                                            */
/* Language: C                                                                      */
/* Date: Feb. 25, 2006                                                              */
/* Copyright (C) Feb. 25, 2006, All rights reserved.                                */
/*                                                                                  */
/* This is a program that solves Su Doku (aka Sudoku, Number Place, etc.) puzzles   */
/* primarily using deductive logic. It will only resort to trial-and-error and      */
/* backtracking approaches upon exhausting all of its deductive moves.              */
/*                                                                                  */
/* Puzzles must be of the standard 9x9 variety using the (ASCII) characters '1'     */
/* through '9' for the puzzle solution set. Puzzles should be submitted as 81       */
/* character strings which, when read left-to-right will fill a 9x9 Sudoku grid     */
/* from left-to-right and top-to-bottom. In the puzzle specification, the           */
/* characters 1 - 9 represent the puzzle "givens" or clues. Any other non-blank     */
/* character represents an unsolved cell.                                           */
/*                                                                                  */
/* The puzzle solving algorithm is "home grown." I did not borrow any of the usual  */
/* techniques from the literature, e.g. Donald Knuth's "Dancing Links." Instead     */
/* I "rolled my own" from scratch. As such, its performance can only be blamed      */
/* on yours truly. Still, I feel it is quite fast. On a 333 MHz Pentium II Linux    */
/* box it solves typical medium force puzzles in approximately 800 microseconds or  */
/* about 1,200 puzzles per second, give or take. On an Athlon XP 3000 (Barton core) */
/* it solves about 6,600 puzzles per sec.                                           */
/*                                                                                  */
/* DESCRIPTION OF ALGORITHM:                                                        */
/*                                                                                  */
/* The puzzle algorithm initially assumes every unsolved cell can assume every      */
/* possible value. It then uses the placement of the givens to refine the choices   */
/* available to each cell. I call this the markup phase.                            */
/*                                                                                  */
/* After markup completes, the algorithm then looks for "singleton" cells with      */
/* values that, due to constraints imposed by the row, column, or 3x3 region, may   */
/* only assume one possible value. Once these cells are assigned values, the        */
/* algorithm returns to the markup phase to apply these changes to the remaining    */
/* candidate solutions. The markup/singleton phases alternate until either no more  */
/* changes occur, or the puzzle is solved. I call the markup/singleton elimination  */
/* loop the "Simple Solver" because in a large percentage of cases it solves the    */
/* puzzle.                                                                          */
/*                                                                                  */
/* If the simple solver portion of the algorithm doesn't produce a solution, then   */
/* more advanced deductive rules are applied. I've implemented two additional rules */
/* as part of the deductive puzzle solver. The first is subset elimination wherein  */
/* a row/column/region is scanned for X number of cells with X number of matching   */
/* candidate solutions. If such subsets are found in the row, column, or region,    */
/* then the candidates values from the subset may be eliminated from all other      */
/* unsolved cells within the row, column, or region, respectively.                  */
/*                                                                                  */
/* The second advanced deductive rule examines each region looking for candidate    */
/* values that exclusively align themselves along a single row or column, i.e. a    */
/* a vector. If such candidate values are found, then they may be eliminated from   */
/* the cells outside of the region that are part of the aligned row or column.      */
/*                                                                                  */
/* Note that each of the advanced deductive rules calls all preceeding rules, in    */
/* order, if that advanced rule has effected a change in puzzle markup.             */
/*                                                                                  */
/* Finally, if no solution is found after iteratively applying all deductive rules, */
/* then we begin trial-and-error using recursion for backtracking. A working copy   */
/* is created from our puzzle, and using this copy the first cell with the          */
/* smallest number of candidate solutions is chosen. One of the solutions values is */
/* assigned to that cell, and the solver algorithm is called using this working     */
/* copy as its starting point. Eventually, either a solution, or an impasse is      */
/* reached.                                                                         */
/*                                                                                  */
/* If we reach an impasse, the recursion unwinds and the next trial solution is     */
/* attempted. If a solution is found (at any point) the values for the solution are */
/* added to a list. Again, so long as we are examining all possibilities, the       */
/* recursion unwinds so that the next trial may be attempted. It is in this manner  */
/* that we enumerate puzzles with multiple solutions.                               */
/*                                                                                  */
/* Note that it is certainly possible to add to the list of applied deductive       */
/* rules. The techniques known as "X-Wing" and "Swordfish" come to mind. On the     */
/* other hand, adding these additional rules will, in all likelihood, slow the      */
/* solver down by adding to the computational burden while producing very few       */
/* results. I've seen the law of diminishing returns even in some of the existing   */
/* rules, e.g. in subset elimination I only look at two and three valued subsets    */
/* because taking it any further than that degraded performance.                    */
/*                                                                                  */
/* PROGRAM INVOCATION:                                                              */
/*                                                                                  */
/* This program is a console (or command line) based utility and has the following  */
/* usage:                                                                           */
/*                                                                                  */
/*      sudoku_solver {-p puzzle | -f <puzzle_file>} [-o <outfile>]                 */
/*              [-r <reject_file>] [-1][-a][-c][-g][-l][-m][-n][-s]                 */
/*                                                                                  */
/* where:                                                                           */
/*                                                                                  */
/*        -1      Search for first solution, otherwise all solutions are returned   */
/*        -a      Requests that the answer (solution) be printed                    */
/*        -c      Print a count of solutions for each puzzle                        */
/*        -d      Print the recursive trial depth required to solve the puzzle      */
/*        -e      Print a step-by-step explanation of the solution(s)               */
/*        -f      Takes an argument which specifes an input file                    */
/*                containing one or more unsolved puzzles (default: stdin)          */
/*        -G      Print the puzzle solution(s) in a 9x9 grid format                 */
/*        -g      Print the number of given clues                                   */
/*        -l      Print the recursive trial depth required to solve the puzzle      */
/*        -m      Print an octal mask for the puzzle givens                         */
/*        -n      Number each result                                                */
/*        -o      Specifies an output file for the solutions (default: stdout)      */
/*        -p      Takes an argument giving a single inline puzzle to be solved      */
/*        -r      Specifies an output file for unsolvable puzzles                   */
/*                (default: stderr)                                                 */
/*        -s      Print the puzzle's score or difficulty rating                     */
/*        -?      Print usage information                                           */
/*                                                                                  */
/* The return code is zero if all puzzles had unique solutions,                     */
/* (or have one or more solutions when -1 is specified) and non-zero                */
/* when no unique solution exists.                                                  */
/*                                                                                  */
/* PUZZLE SCORING                                                                   */
/*                                                                                  */
/* A word about puzzle scoring, i.e. rating a puzzle's difficulty, is in order.     */
/* Rating Sudoku puzzles is a rather subjective thing, and thus it is difficult to  */
/* really develop an objective puzzle rating system. I, however, have attempted     */
/* this feat (several times with varying degrees of success ;-) and I think the     */
/* heuristics I'm currently applying aren't too bad for rating the relative         */
/* difficulty of solving a puzzle.                                                  */
/*                                                                                  */
/* The following is a brief rundown of how it works. The initial puzzle markup is   */
/* a "free" operation, i.e. no points are scored for the first markup pass. I feel  */
/* this is appropriate because a person solving a puzzle will always have to do     */
/* their own eyeballing and scanning of the puzzle. Subsequent passes are           */
/* scored at one point per candidate eliminated because these passes indicate       */
/* that more deductive work is required. Secondly, the "reward" for solving a cell  */
/* is set to one point, and as long as the solution only requires simple markup     */
/* and elimination of singletons, this level of reward remains unchanged.           */
/*                                                                                  */
/* This reward changes, however, when advanced solving rules are required. Puzzles  */
/* that remain unsolved after the first pass through the simple solver phase have   */
/* a higher "reward", i.e. it is incremented by two. Thus, if subset or vector      */
/* elimination is required, all subsequently solved cells score higher bounties.    */
/* In addition, the successful application of these deductive techniques score      */
/* their own penalties.                                                             */
/*                                                                                  */
/* Finally, if a trial-and-error approach is called for, then the "reward" is       */
/* incremented by another five points. Thus, the total penalty for each level of    */
/* recursion is an additional seven points per solved cell, i.e.                    */
/* (recursive_depth * 7) + 1 points per solved cell. Trial solutions are also       */
/* penalized by a weighting factor that is based upon the number of unsolved cells  */
/* that remain upon reentry to the solver and the depth of recursion. (I've seen a  */
/* pathological puzzle from the "Minimum Sudoku" web site require 16 levels of      */
/* recursion and score a whopping 228,642 points using this scoring system!)        */
/*                                                                                  */
/* And that brings me to this topic: What do all these points mean?                 */
/*                                                                                  */
/* Well, who knows? This is still subjective, and the weighting system I've chosen  */
/* for point scoring is is largely arbitrary. But based upon feedback from a number */
/* of individuals, a rough scale of difficulty plays out as follows:                */
/*                                                                                  */
/*   DEGREE OF DIFFICULTY   |  SCORE                                                */
/* -------------------------+------------------------------------------             */
/*   TRIVIAL                |  80 points or less                                    */
/*   EASY                   |  81 - 150 points                                      */
/*   MEDIUM                 |  151 - 250 points                                     */
/*   HARD                   |  251 - 400 points                                     */
/*   VERY HARD              |  401 - 900 points                                     */
/*   DIABOLICAL             |  901 and up                                           */
/*                                                                                  */
/* Experience shows that puzzles in the HARD category, in a few cases, will         */
/* require a small amount of trial-and-error. The VERY HARD puzzles will likely     */
/* require trial-and-error, and in some cases more than one level of trial-and-     */
/* error. As for the DIABOLICAL puzzles--why waste your time? These are best left   */
/* to masochists, savants and automated solvers. YMMV.                              */
/*                                                                                  */
/* LICENSE:                                                                         */
/*                                                                                  */
/* This program is free software; you can redistribute it and/or modify             */
/* it under the terms of the GNU General Public License as published by             */
/* the Free Software Foundation; either version 2 of the License, or                */
/* (at your option) any later version.                                              */
/*                                                                                  */
/* This program is distributed in the hope that it will be useful,                  */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of                   */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                    */
/* GNU General Public License for more details.                                     */
/*                                                                                  */
/* You should have received a copy of the GNU General Public License                */
/* along with this program; if not, write to the Free Software                      */
/* Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA       */
/*                                                                                  */
/* CONTACT:                                                                         */
/*                                                                                  */
/* Email: bdupree@techfinesse.com                                                   */
/* Post: Bill DuPree, 609 Wenonah Ave, Oak Park, IL 60304 USA                       */
/*                                                                                  */
/************************************************************************************/
/*                                                                                  */
/* CHANGE LOG:                                                                      */
/*                                                                                  */
/* Rev.	  Date        Init.	Description                                         */
/* -------------------------------------------------------------------------------- */
/* 1.00   2006-02-25  WD	Initial version.                                    */
/* 1.01   2006-03-13  WD	Fixed return code calc. Added signon message.       */
/* 1.10   2006-03-20  WD        Added explain option, add'l speed optimizations     */
/* 1.11   2006-03-23  WD        More simple speed optimizations, cleanup, bug fixes */
/*                                                                                  */
/************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <limits.h>

#define VERSION "1.11"

#define PUZZLE_ORDER 3
#define PUZZLE_DIM (PUZZLE_ORDER*PUZZLE_ORDER)
#define PUZZLE_CELLS (PUZZLE_DIM*PUZZLE_DIM)

/* Command line options */
#ifdef EXPLAIN
#define OPTIONS "?1acdef:Ggmno:p:r:s"
#else
#define OPTIONS "?1acdf:Ggmno:p:r:s"
#endif
extern char *optarg;
extern int optind, opterr, optopt;

static char *myname;    /* Name that we were invoked under */

static FILE *solnfile, *rejects;

/* This is the list of cell coordinates specified on a row basis */

static int const row[PUZZLE_DIM][PUZZLE_DIM] = {
    {  0,  1,  2,  3,  4,  5,  6,  7,  8 },
    {  9, 10, 11, 12, 13, 14, 15, 16, 17 },
    { 18, 19, 20, 21, 22, 23, 24, 25, 26 },
    { 27, 28, 29, 30, 31, 32, 33, 34, 35 },
    { 36, 37, 38, 39, 40, 41, 42, 43, 44 },
    { 45, 46, 47, 48, 49, 50, 51, 52, 53 },
    { 54, 55, 56, 57, 58, 59, 60, 61, 62 },
    { 63, 64, 65, 66, 67, 68, 69, 70, 71 },
    { 72, 73, 74, 75, 76, 77, 78, 79, 80 }};

/* This is the list of cell coordinates specified on a column basis */

static int const col[PUZZLE_DIM][PUZZLE_DIM] = {
    {  0,  9, 18, 27, 36, 45, 54, 63, 72 },
    {  1, 10, 19, 28, 37, 46, 55, 64, 73 },
    {  2, 11, 20, 29, 38, 47, 56, 65, 74 },
    {  3, 12, 21, 30, 39, 48, 57, 66, 75 },
    {  4, 13, 22, 31, 40, 49, 58, 67, 76 },
    {  5, 14, 23, 32, 41, 50, 59, 68, 77 },
    {  6, 15, 24, 33, 42, 51, 60, 69, 78 },
    {  7, 16, 25, 34, 43, 52, 61, 70, 79 },
    {  8, 17, 26, 35, 44, 53, 62, 71, 80 }};

/* This is the list of cell coordinates specified on a 3x3 region basis */

static int const region[PUZZLE_DIM][PUZZLE_DIM] = {
    {  0,  1,  2,  9, 10, 11, 18, 19, 20 },
    {  3,  4,  5, 12, 13, 14, 21, 22, 23 },
    {  6,  7,  8, 15, 16, 17, 24, 25, 26 },
    { 27, 28, 29, 36, 37, 38, 45, 46, 47 },
    { 30, 31, 32, 39, 40, 41, 48, 49, 50 },
    { 33, 34, 35, 42, 43, 44, 51, 52, 53 },
    { 54, 55, 56, 63, 64, 65, 72, 73, 74 },
    { 57, 58, 59, 66, 67, 68, 75, 76, 77 },
    { 60, 61, 62, 69, 70, 71, 78, 79, 80 }};

/* Flags for cellflags member */
#define GIVEN 1
#define FOUND 2
#define STUCK 3

/* Return codes for funcs that modify puzzle markup */
#define NOCHANGE 0
#define CHANGE   1

typedef struct grd {
    short cellflags[PUZZLE_CELLS];
    short solved[PUZZLE_CELLS];
    short cell[PUZZLE_CELLS];
    short tail, givens, exposed, maxlvl, inc, reward;
    unsigned int score, solncount;
    struct grd *next;
} grid;

typedef int (*return_soln)(grid *g);

static grid *soln_list = NULL;

typedef struct {
    short row, col, region;
} cellmap;

/* Array structure to help map cell index back to row, column, and region */
static cellmap const map[PUZZLE_CELLS] = {
    { 0, 0, 0 },
    { 0, 1, 0 },
    { 0, 2, 0 },
    { 0, 3, 1 },
    { 0, 4, 1 },
    { 0, 5, 1 },
    { 0, 6, 2 },
    { 0, 7, 2 },
    { 0, 8, 2 },
    { 1, 0, 0 },
    { 1, 1, 0 },
    { 1, 2, 0 },
    { 1, 3, 1 },
    { 1, 4, 1 },
    { 1, 5, 1 },
    { 1, 6, 2 },
    { 1, 7, 2 },
    { 1, 8, 2 },
    { 2, 0, 0 },
    { 2, 1, 0 },
    { 2, 2, 0 },
    { 2, 3, 1 },
    { 2, 4, 1 },
    { 2, 5, 1 },
    { 2, 6, 2 },
    { 2, 7, 2 },
    { 2, 8, 2 },
    { 3, 0, 3 },
    { 3, 1, 3 },
    { 3, 2, 3 },
    { 3, 3, 4 },
    { 3, 4, 4 },
    { 3, 5, 4 },
    { 3, 6, 5 },
    { 3, 7, 5 },
    { 3, 8, 5 },
    { 4, 0, 3 },
    { 4, 1, 3 },
    { 4, 2, 3 },
    { 4, 3, 4 },
    { 4, 4, 4 },
    { 4, 5, 4 },
    { 4, 6, 5 },
    { 4, 7, 5 },
    { 4, 8, 5 },
    { 5, 0, 3 },
    { 5, 1, 3 },
    { 5, 2, 3 },
    { 5, 3, 4 },
    { 5, 4, 4 },
    { 5, 5, 4 },
    { 5, 6, 5 },
    { 5, 7, 5 },
    { 5, 8, 5 },
    { 6, 0, 6 },
    { 6, 1, 6 },
    { 6, 2, 6 },
    { 6, 3, 7 },
    { 6, 4, 7 },
    { 6, 5, 7 },
    { 6, 6, 8 },
    { 6, 7, 8 },
    { 6, 8, 8 },
    { 7, 0, 6 },
    { 7, 1, 6 },
    { 7, 2, 6 },
    { 7, 3, 7 },
    { 7, 4, 7 },
    { 7, 5, 7 },
    { 7, 6, 8 },
    { 7, 7, 8 },
    { 7, 8, 8 },
    { 8, 0, 6 },
    { 8, 1, 6 },
    { 8, 2, 6 },
    { 8, 3, 7 },
    { 8, 4, 7 },
    { 8, 5, 7 },
    { 8, 6, 8 },
    { 8, 7, 8 },
    { 8, 8, 8 }
};

static const short symtab[1<<PUZZLE_DIM] = {
    '.','1','2','.','3','.','.','.','4','.','.','.','.','.','.','.','5','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.',
    '6','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.',
    '7','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.',
    '.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.',
    '8','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.',
    '.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.',
    '.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.',
    '.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.',
    '9','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.',
    '.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.',
    '.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.',
    '.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.',
    '.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.',
    '.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.',
    '.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.',
    '.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.'};

static int enumerate_all = 1;
static int lvl = 0;

#ifdef EXPLAIN
static int explain = 0;
#endif

/* Function prototype(s) */
static int mult_elimination(grid *g);
static void print_grid(char *sud, FILE *h);
static char *format_answer(grid *g, char *outbuf);
static void diagnostic_grid(grid *g, FILE *h);

static inline int is_given(int c) { return (c >= '1') && (c <= '9'); }

#if defined(DEBUG)
static void mypause()
{
    char buf[8];
    printf("\tPress enter -> ");
    fgets(buf, 8, stdin);
}
#endif

#if 0
/* Generic (and slow) bitcount function */
static int bitcount(short cell)
{
    int i, count, mask;
    
    mask = 1;
    for (i = count = 0; i < 16; i++) {
        if (mask & cell) count++;
        mask <<= 1;
    }
    return count;
}
#endif

/*****************************************************/
/* Return the number of '1' bits in a cell.          */
/* Rather than count bits, do a quick table lookup.  */
/* Warning: Only valid for 9 low order bits.         */
/*****************************************************/

static inline short bitcount(short cell)
{
    static const short bcounts[512] = {
        0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4,1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,
        1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
        1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
        2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
        1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
        2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
        2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
        3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,4,5,5,6,5,6,6,7,5,6,6,7,6,7,7,8,
        1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
        2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
        2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
        3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,4,5,5,6,5,6,6,7,5,6,6,7,6,7,7,8,
        2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
        3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,4,5,5,6,5,6,6,7,5,6,6,7,6,7,7,8,
        3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,4,5,5,6,5,6,6,7,5,6,6,7,6,7,7,8,
        4,5,5,6,5,6,6,7,5,6,6,7,6,7,7,8,5,6,6,7,6,7,7,8,6,7,7,8,7,8,8,9};
    
    return bcounts[cell];
}

#ifdef EXPLAIN

/**************************************************/
/* Indent two spaces for each level of recursion. */
/**************************************************/
static inline void explain_indent(FILE *h)
{
    int i;
    
    for (i = 0; i < lvl-1; i++) fprintf(h, "  ");
}

/******************************************************************/
/* Construct a string representing the possible values a cell may */
/* contain according to current markup.                           */
/******************************************************************/
static char *clues(short cell)
{
    int i, m, multi, mask;
    static char buf[64], *p;
    
    multi = m = bitcount(cell);
    
    if (!multi) return "NULL";
    
    if (multi > 1) {
        strcpy(buf, "tuple (");
    }
    else {
        strcpy(buf, "value ");
    }
    
    p = buf + strlen(buf);
    
    for (mask = i = 1; i <= PUZZLE_DIM; i++) {
        if (mask & cell) {
            *p++ = symtab[mask];
            multi -= 1;
            if (multi) { *p++ = ','; *p++ = ' '; }
        }
        mask <<= 1;
    }
    if (m > 1) *p++ = ')';
    *p = 0;
    return buf;
}

/*************************************************************/
/* Explain removal of a candidate value from a changed cell. */
/*************************************************************/
static void explain_markup_elim(grid *g, int chgd, int clue)
{
    int chgd_row, chgd_col, clue_row, clue_col;
    
    chgd_row = map[chgd].row+1;
    chgd_col = map[chgd].col+1;
    clue_row = map[clue].row+1;
    clue_col = map[clue].col+1;
    
    explain_indent(solnfile);
    fprintf(solnfile, "Candidate %s removed from row %d, col %d because of cell at row %d, col %d\n",
            clues(g->cell[clue]), chgd_row, chgd_col, clue_row, clue_col);
}

/*****************************************/
/* Dump the state of the current markup. */
/*****************************************/
static void explain_current_markup(grid *g)
{
    if (g->exposed >= PUZZLE_CELLS) return;
    
    fprintf(solnfile, "\n");
    explain_indent(solnfile);
    fprintf(solnfile, "Current markup is as follows:");
    diagnostic_grid(g, solnfile);
    fprintf(solnfile, "\n");
}

/****************************************/
/* Explain the solving of a given cell. */
/****************************************/
static void explain_solve_cell(grid *g, int chgd)
{
    int chgd_row, chgd_col;
    
    chgd_row = map[chgd].row+1;
    chgd_col = map[chgd].col+1;
    
    explain_indent(solnfile);
    fprintf(solnfile, "Cell at row %d, col %d solved with %s\n",
            chgd_row, chgd_col, clues(g->cell[chgd]));
}

/******************************************************************/
/* Explain the current impasse reached during markup elimination. */
/******************************************************************/
static void explain_markup_impasse(grid *g, int chgd, int clue)
{
    int chgd_row, chgd_col, clue_row, clue_col;
    
    chgd_row = map[chgd].row+1;
    chgd_col = map[chgd].col+1;
    clue_row = map[clue].row+1;
    clue_col = map[clue].col+1;
    
    explain_indent(solnfile);
    fprintf(solnfile, "Impasse for cell at row %d, col %d because cell at row %d, col %d removes last candidate\n",
            chgd_row, chgd_col, clue_row, clue_col);
    explain_current_markup(g);
}

/****************************************/
/* Explain naked and/or hidden singles. */
/****************************************/
static void explain_singleton(grid *g, int chgd, int mask, char *vdesc)
{
    int chgd_row, chgd_col, chgd_reg;
    
    chgd_row = map[chgd].row+1;
    chgd_col = map[chgd].col+1;
    chgd_reg = map[chgd].region+1;
    
    explain_indent(solnfile);
    fprintf(solnfile, "Cell of region %d at row %d, col %d will only solve for %s in this %s\n",
            chgd_reg, chgd_row, chgd_col, clues(mask), vdesc);
    explain_solve_cell(g, chgd);
}

/*********************************/
/* Explain initial puzzle state. */
/*********************************/
static void explain_markup()
{
    fprintf(solnfile, "\n");
    explain_indent(solnfile);
    fprintf(solnfile, "Assume all cells may contain any values in the range: [1 - 9]\n");
}

/************************/
/* Explain given clues. */
/************************/
static void explain_given(int cell, char val)
{
    int cell_row, cell_col;
    
    cell_row = map[cell].row+1;
    cell_col = map[cell].col+1;
    
    explain_indent(solnfile);
    fprintf(solnfile, "Cell at row %d, col %d is given clue value %c\n", cell_row, cell_col, val);
}

/*******************************************/
/* Explain region/row/column interactions. */
/*******************************************/
static void explain_vector_elim(char *desc, int i, int cell, int val, int region)
{
    int cell_row, cell_col;
    
    cell_row = map[cell].row+1;
    cell_col = map[cell].col+1;
    
    explain_indent(solnfile);
    fprintf(solnfile, "Candidate %s removed from cell at row %d, col %d because it aligns along %s %d in region %d\n",
            clues(val), cell_row, cell_col, desc, i+1, region+1);
}

/******************************************************************/
/* Explain the current impasse reached during vector elimination. */
/******************************************************************/
static void explain_vector_impasse(grid *g, char *desc, int i, int cell, int val, int region)
{
    int cell_row, cell_col;
    
    cell_row = map[cell].row+1;
    cell_col = map[cell].col+1;
    
    explain_indent(solnfile);
    fprintf(solnfile, "Impasse at cell at row %d, col %d because candidate %s aligns along %s %d in region %d\n",
            cell_row, cell_col, clues(val), desc, i+1, region+1);
    explain_current_markup(g);
}

/*****************************************************************/
/* Explain the current impasse reached during tuple elimination. */
/*****************************************************************/
static void explain_tuple_impasse(grid *g, char *desc, int elt, int tuple, int count, int bits)
{
    explain_indent(solnfile);
    fprintf(solnfile, "Impasse in %s %d because too many (%d) cells have %d-valued %s\n",
            desc, elt+1, count, bits, clues(tuple));
    explain_current_markup(g);
}

/*********************************************************************/
/* Explain the removal of a tuple of candidate solutions from a cell */
/*********************************************************************/
static void explain_tuple_elim(char *desc, int elt, int tuple, int cell)
{
    explain_indent(solnfile);
    fprintf(solnfile, "Values of %s in %s %d removed from cell at row %d, col %d\n",
            clues(tuple), desc, elt+1, map[cell].row+1, map[cell].col+1);
    
}

/**************************************************/
/* Indicate that a viable solution has been found */
/**************************************************/
static void explain_soln_found(grid *g)
{
    char buf[90];
    
    fprintf(solnfile, "\n");
    explain_indent(solnfile);
    fprintf(solnfile, "Solution found: %s\n", format_answer(g, buf));
    print_grid(buf, solnfile);
    fprintf(solnfile, "\n");
}

/***************************/
/* Show the initial puzzle */
/***************************/
static void explain_grid(grid *g)
{
    char buf[90];
    
    fprintf(solnfile, "Initial puzzle: %s\n", format_answer(g, buf));
    print_grid(buf, solnfile);
    explain_current_markup(g);
    fprintf(solnfile, "\n");
}

/*************************************************/
/* Explain attempt at a trial and error solution */
/*************************************************/
static void explain_trial(int cell, int value)
{
    explain_indent(solnfile);
    fprintf(solnfile, "Attempt trial where cell at row %d, col %d is assigned value %s\n",
            map[cell].row+1, map[cell].col+1, clues(value));
}

/**********************************************/
/* Explain back out of current trial solution */
/**********************************************/
static void explain_backtrack()
{
    if (lvl <= 1) return;
    
    explain_indent(solnfile);
    fprintf(solnfile, "Backtracking\n\n");
}

#define EXPLAIN_MARKUP                                 if (explain) explain_markup()
#define EXPLAIN_CURRENT_MARKUP(g)                      if (explain) explain_current_markup((g))
#define EXPLAIN_GIVEN(cell, val)	               if (explain) explain_given((cell), (val))
#define EXPLAIN_MARKUP_ELIM(g, chgd, clue)             if (explain) explain_markup_elim((g), (chgd), (clue))
#define EXPLAIN_MARKUP_SOLVE(g, cell)                  if (explain) explain_solve_cell((g), (cell))
#define EXPLAIN_MARKUP_IMPASSE(g, chgd, clue)          if (explain) explain_markup_impasse((g), (chgd), (clue))
#define EXPLAIN_SINGLETON(g, chgd, mask, vdesc)        if (explain) explain_singleton((g), (chgd), (mask), (vdesc))
#define EXPLAIN_VECTOR_ELIM(desc, i, cell, v, r)       if (explain) explain_vector_elim((desc), (i), (cell), (v), (r))
#define EXPLAIN_VECTOR_IMPASSE(g, desc, i, cell, v, r) if (explain) explain_vector_impasse((g), (desc), (i), (cell), (v), (r))
#define EXPLAIN_VECTOR_SOLVE(g, cell)                  if (explain) explain_solve_cell((g), (cell))
#define EXPLAIN_TUPLE_IMPASSE(g, desc, j, c, count, i) if (explain) explain_tuple_impasse((g), (desc), (j), (c), (count), (i))
#define EXPLAIN_TUPLE_ELIM(desc, j, c, cell)           if (explain) explain_tuple_elim((desc), (j), (c), (cell))
#define EXPLAIN_TUPLE_SOLVE(g, cell)                   if (explain) explain_solve_cell((g), (cell))
#define EXPLAIN_SOLN_FOUND(g)			       if (explain) explain_soln_found((g));
#define EXPLAIN_GRID(g)			               if (explain) explain_grid((g));
#define EXPLAIN_TRIAL(cell, val)		       if (explain) explain_trial((cell), (val));
#define EXPLAIN_BACKTRACK                              if (explain) explain_backtrack();
#define EXPLAIN_INDENT(h)			       if (explain) explain_indent((h))

#else

#define EXPLAIN_MARKUP
#define EXPLAIN_CURRENT_MARKUP(g)
#define EXPLAIN_GIVEN(cell, val)
#define EXPLAIN_MARKUP_ELIM(g, chgd, clue)
#define EXPLAIN_MARKUP_SOLVE(g, cell)
#define EXPLAIN_MARKUP_IMPASSE(g, chgd, clue)
#define EXPLAIN_SINGLETON(g, chgd, mask, vdesc);
#define EXPLAIN_VECTOR_ELIM(desc, i, cell, v, r)
#define EXPLAIN_VECTOR_IMPASSE(g, desc, i, cell, v, r)
#define EXPLAIN_VECTOR_SOLVE(g, cell)
#define EXPLAIN_TUPLE_IMPASSE(g, desc, j, c, count, i)
#define EXPLAIN_TUPLE_ELIM(desc, j, c, cell)
#define EXPLAIN_TUPLE_SOLVE(g, cell)
#define EXPLAIN_SOLN_FOUND(g)
#define EXPLAIN_GRID(g)
#define EXPLAIN_TRIAL(cell, val)
#define EXPLAIN_BACKTRACK
#define EXPLAIN_INDENT(h)

#endif


/*****************************************************/
/* Initialize a grid to an empty state.              */
/* At the start, all cells can have any value        */
/* so set all 9 lower order bits in each cell.       */
/* In effect, the 9x9 grid now has markup that       */
/* specifies that each cell can assume any value     */
/* of 1 through 9.                                   */
/*****************************************************/

static void init_grid(grid *g)
{
    int i;
    
    for (i = 0; i < PUZZLE_CELLS; i++) g->cell[i] = 0x01ff;
    memset(g->cellflags, 0, PUZZLE_CELLS*sizeof(g->cellflags[0]));
    g->exposed = 0;
    g->givens = 0;
    g->inc = 0;
    g->maxlvl = 0;
    g->score = 0;
    g->solncount = 0;
    g->reward = 1;
    g->next = NULL;
    g->tail = 0;
    EXPLAIN_MARKUP;
}

/*****************************************************/
/* Convert a puzzle from the input format,           */
/* i.e. a string of 81 non-blank characters          */
/* with ASCII digits '1' thru '9' specified          */
/* for the givens, and non-numeric characters        */
/* for the remaining cells. The string, read         */
/* left-to-right fills the 9x9 Sudoku grid           */
/* in left-to-right, top-to-bottom order.            */
/*****************************************************/

static void cvt_to_grid(grid *g, char *game)
{
    int i;
    
    init_grid(g);
    
    for (i = 0; i < PUZZLE_CELLS; i++) {
        if (is_given(game[i])) {
            /* warning -- ASCII charset assumed */
            g->cell[i] = 1 << (game[i] - '1');
            g->cellflags[i] = GIVEN;
            g->givens += 1;
            g->solved[g->exposed++] = i;
            EXPLAIN_GIVEN(i, game[i]);
        }
    }
    EXPLAIN_GRID(g);
}

/****************************************************************/
/* Print the partially solved puzzle and all associated markup  */
/* in 9x9 fashion.                                              */
/****************************************************************/

static void diagnostic_grid(grid *g, FILE *h)
{
    int i, j, flag;
    short c;
    char line1[40], line2[40], line3[40], cbuf1[5], cbuf2[5], cbuf3[5], outbuf[PUZZLE_CELLS+1];
    
    /* Sanity check */
    for (flag = 1, i = 0; flag && i < PUZZLE_CELLS; i++) {
        if (bitcount(g->cell[i]) != 1) {
            flag = 0;
        }
    }
    
    /* Don't need to print grid with diagnostic markup? */
    if (flag) {
        format_answer(g, outbuf);
        print_grid(outbuf, h);
        fflush(h);
        return;
    }
    
    strcpy(cbuf1, "   |");
    strcpy(cbuf2, cbuf1);
    strcpy(cbuf3, cbuf1);
    fprintf(h, "\n");
    
    for (i = 0; i < PUZZLE_DIM; i++) {
        
        *line1 = *line2 = *line3 = 0;
        
        for (j = 0; j < PUZZLE_DIM; j++) {
            
            c = g->cell[row[i][j]];
            
            if (bitcount(c) == 1) {
                strcpy(cbuf1, "   |");
                strcpy(cbuf2, cbuf1);
                strcpy(cbuf3, cbuf1);
                cbuf2[1] = symtab[c];
            }
            else {
                if (c & 1) cbuf1[0] = '*'; else cbuf1[0] = '.';
                if (c & 2) cbuf1[1] = '*'; else cbuf1[1] = '.';
                if (c & 4) cbuf1[2] = '*'; else cbuf1[2] = '.';
                if (c & 8) cbuf2[0] = '*'; else cbuf2[0] = '.';
                if (c & 16) cbuf2[1] = '*'; else cbuf2[1] = '.';
                if (c & 32) cbuf2[2] = '*'; else cbuf2[2] = '.';
                if (c & 64) cbuf3[0] = '*'; else cbuf3[0] = '.';
                if (c & 128) cbuf3[1] = '*'; else cbuf3[1] = '.';
                if (c & 256) cbuf3[2] = '*'; else cbuf3[2] = '.';
            }
            
            strcat(line1, cbuf1);
            strcat(line2, cbuf2);
            strcat(line3, cbuf3);
        }
        
        EXPLAIN_INDENT(h);
        fprintf(h, "+---+---+---+---+---+---+---+---+---+\n");
        EXPLAIN_INDENT(h);
        fprintf(h, "|%s\n", line1);
        EXPLAIN_INDENT(h);
        fprintf(h, "|%s\n", line2);
        EXPLAIN_INDENT(h);
        fprintf(h, "|%s\n", line3);
    }
    EXPLAIN_INDENT(h);
    fprintf(h, "+---+---+---+---+---+---+---+---+---+\n"); fflush(h);
}

/***********************************************************************/
/* Validate that a sudoku grid contains a valid solution. Return 1 if  */
/* true, 0 if false. If the verbose argument is non-zero, then print   */
/* reasons for invalidating the solution to stderr.                    */
/***********************************************************************/

static int validate(grid *g, int verbose)
{
    int i, j, regmask, rowmask, colmask, flag = 1;
    
    /* Sanity check */
    for (i = 0; i < PUZZLE_CELLS; i++) {
        if (bitcount(g->cell[i]) != 1) {
            if (verbose) {
                fprintf(rejects, "Cell %d at row %d, col %d has no unique soln.\n", 1+i, 1+map[i].row, 1+map[i].col); fflush(rejects);
                flag = 0;
            } else return 0;
        }
    }
    
    /* Check rows */
    for (i = 0; i < PUZZLE_DIM; i++) {
        for (rowmask = j = 0; j < PUZZLE_DIM; j++) {
            if (bitcount(g->cell[row[i][j]]) == 1) rowmask |= g->cell[row[i][j]];
        }
        if (rowmask != 0x01ff) {
            if (verbose) {
                fprintf(rejects, "Row %d is inconsistent.\n", 1+i); fflush(rejects);
                flag = 0;
            } else return 0;
        }
    }
    
    /* Check columns */
    for (i = 0; i < PUZZLE_DIM; i++) {
        for (colmask = j = 0; j < PUZZLE_DIM; j++) {
            if (bitcount(g->cell[col[i][j]]) == 1) colmask |= g->cell[col[i][j]];
        }
        if (colmask != 0x01ff) {
            if (verbose) {
                fprintf(rejects, "Column %d is inconsistent.\n", 1+i); fflush(rejects);
                flag = 0;
            } else return 0;
        }
    }
    
    /* Check 3x3 regions */
    for (i = 0; i < PUZZLE_DIM; i++) {
        for (regmask = j = 0; j < PUZZLE_DIM; j++) {
            if (bitcount(g->cell[region[i][j]]) == 1) regmask |= g->cell[region[i][j]];
        }
        if (regmask != 0x01ff) {
            if (verbose) {
                fprintf(rejects, "Region %d is inconsistent.\n", 1+i); fflush(rejects);
                flag = 0;
            } else return 0;
        }
    }
    
    return flag;
}

/********************************************************************************/
/* This function uses the cells with unique values, i.e. the given              */
/* or subsequently discovered solution values, to eliminate said values         */
/* as candidates in other as yet unsolved cells in the associated               */
/* rows, columns, and 3x3 regions.                                              */
/*                                                                              */
/* The function has three possible return values:                               */
/*   NOCHANGE - Markup did not change during the last pass,                     */
/*   CHANGE   - Markup was modified, and                                        */
/*   STUCK    - Markup results are invalid, i.e. a cell has no candidate values */
/********************************************************************************/

static int mark_cells(grid *g)
{
    int i, chgflag, bc;
    int const *r, *c, *reg;
    short elt, mask, before;
    
    
    chgflag = NOCHANGE;
    
    while (g->tail < g->exposed) {
        
        elt = g->solved[g->tail++];
        
        r = row[map[elt].row];
        c = col[map[elt].col];
        reg = region[map[elt].region];
        
        mask = ~g->cell[elt];
        
        for (i = 0; i < PUZZLE_DIM; i++) {
            
            if (r[i] != elt) {
                
                /* Get the cell value */
                before = g->cell[r[i]];
                
                /* Eliminate this candidate value whilst preserving other candidate values */
                g->cell[r[i]] &= mask;
                
                /* Did the cell change value? */
                if (before != g->cell[r[i]]) {
                    
                    chgflag |= CHANGE;	/* Flag that puzzle markup was changed */
                    g->score += g->inc;	/* More work means higher scoring      */
                    
                    if (!(bc = bitcount(g->cell[r[i]]))) {
                        EXPLAIN_MARKUP_IMPASSE(g, r[i], elt);
                        return STUCK;	/* Crap out if no candidates remain */
                    }
                    
                    EXPLAIN_MARKUP_ELIM(g, r[i], elt);
                    
                    /* Check if we solved for this cell, i.e. bit count indicates a unique value */
                    if (bc == 1) {
                        g->cellflags[r[i]] = FOUND;	/* Mark cell as found  */
                        g->score += g->reward;		/* Add to puzzle score */
                        g->solved[g->exposed++] = r[i];
                        EXPLAIN_MARKUP_SOLVE(g, r[i]);
                    }
                }
            }
            
            if (c[i] != elt) {
                
                /* Get the cell value */
                before = g->cell[c[i]];
                
                /* Eliminate this candidate value whilst preserving other candidate values */
                g->cell[c[i]] &= mask;
                
                /* Did the cell change value? */
                if (before != g->cell[c[i]]) {
                    
                    chgflag |= CHANGE;	/* Flag that puzzle markup was changed */
                    g->score += g->inc;	/* More work means higher scoring      */
                    
                    if (!(bc = bitcount(g->cell[c[i]]))) {
                        EXPLAIN_MARKUP_IMPASSE(g, c[i], elt);
                        return STUCK;	/* Crap out if no candidates remain */
                    }
                    
                    EXPLAIN_MARKUP_ELIM(g, c[i], elt);
                    
                    /* Check if we solved for this cell, i.e. bit count indicates a unique value */
                    if (bc == 1) {
                        g->cellflags[c[i]] = FOUND;	/* Mark cell as found  */
                        g->score += g->reward;		/* Add to puzzle score */
                        g->solved[g->exposed++] = c[i];
                        EXPLAIN_MARKUP_SOLVE(g, c[i]);
                    }
                }
            }
            
            if (reg[i] != elt) {
                
                /* Get the cell value */
                before = g->cell[reg[i]];
                
                /* Eliminate this candidate value whilst preserving other candidate values */
                g->cell[reg[i]] &= mask;
                
                /* Did the cell change value? */
                if (before != g->cell[reg[i]]) {
                    
                    chgflag |= CHANGE;	/* Flag that puzzle markup was changed */
                    g->score += g->inc;	/* More work means higher scoring      */
                    
                    if (!(bc = bitcount(g->cell[reg[i]]))) {
                        EXPLAIN_MARKUP_IMPASSE(g, reg[i], elt);
                        return STUCK;	/* Crap out if no candidates remain */
                    }
                    
                    EXPLAIN_MARKUP_ELIM(g, reg[i], elt);
                    
                    /* Check if we solved for this cell, i.e. bit count indicates a unique value */
                    if (bc == 1) {
                        g->cellflags[reg[i]] = FOUND;	/* Mark cell as found  */
                        g->score += g->reward;		/* Add to puzzle score */
                        g->solved[g->exposed++] = reg[i];
                        EXPLAIN_MARKUP_SOLVE(g, reg[i]);
                    }
                }
            }
            
        }
    }
    
    return chgflag;
}


/*******************************************************************/
/* Identify and "solve" all cells that, by reason of their markup, */
/* can only assume one specific value, i.e. the cell is the only   */
/* one in a row/column/region (specified by vector) that is        */
/* able to assume a particular value.                              */
/*                                                                 */
/* The function has two possible return values:                    */
/*   NOCHANGE - Markup did not change during the last pass,        */
/*   CHANGE   - Markup was modified.                               */
/*******************************************************************/

static int find_singletons(grid *g, int const *vector, char *vdesc)
{
    int i, j, mask, hist[PUZZLE_DIM], value[PUZZLE_DIM], found = NOCHANGE;
    
    /* We are going to create a histogram of cell candidate values */
    /* for the specified cell vector (row/column/region).          */
    /* First set all buckets to zero.                              */
    memset(hist, 0, sizeof(hist[0])*PUZZLE_DIM);
    
    /* For each cell in the vector... */
    for (i = 0; i < PUZZLE_DIM; i++) {
        
        /* For each possible candidate value... */
        for (mask = 1, j = 0; j < PUZZLE_DIM; j++) {
            
            /* If the cell may possibly assume this value... */
            if (g->cell[vector[i]] & mask) {
                
                value[j] = vector[i];	/* Save the cell coordinate */
                hist[j] += 1;		/* Bump bucket in histogram */
            }
            
            mask <<= 1;	/* Next candidate value */
        }
    }
    
    /* Examine each bucket in the histogram... */
    for (mask = 1, i = 0; i < PUZZLE_DIM; i++) {
        
        /* If the bucket == 1 and the cell is not already solved,  */
        /* then the cell has a unique solution specified by "mask" */
        if (hist[i] == 1 && !g->cellflags[value[i]]) {
            
            found = CHANGE;			/* Indicate that markup has been changed */
            g->cell[value[i]] = mask;	/* Assign solution value to cell         */
            g->cellflags[value[i]] = FOUND;	/* Mark cell as solved                   */
            g->score += g->reward;		/* Bump puzzle score                     */
            g->solved[g->exposed++] = value[i];
            EXPLAIN_SINGLETON(g, value[i], mask, vdesc);
        }
        
        mask <<= 1;		/* Get next candidate value */
    }
    
    return found;
}


/*******************************************************************/
/* Find all cells with unique solutions (according to markup)      */
/* and mark them as found. Do this for each row, column, and       */
/* region.                                                         */
/*                                                                 */
/* The function has two possible return values:                    */
/*   NOCHANGE - Markup did not change during the last pass,        */
/*   CHANGE   - Markup was modified.                               */
/*******************************************************************/

static int eliminate_singles(grid *g)
{
    int i, found = NOCHANGE;
    
    /* Do rows */
    for (i = 0; i < PUZZLE_DIM; i++) {
        found |= find_singletons(g, row[i], (char *)"row");
    }
    
    /* Do columns */
    for (i = 0; i < PUZZLE_DIM; i++) {
        found |= find_singletons(g, col[i], (char *)"column");
    }
    
    /* Do regions */
    for (i = 0; i < PUZZLE_DIM; i++) {
        found |= find_singletons(g, region[i], (char *)"region");
    }
    
    return found;
}

/********************************************************************************/
/* Solves simple puzzles, i.e. single elimination                               */
/*                                                                              */
/* The function has three possible return values:                               */
/*   NOCHANGE - Markup did not change during the last pass,                     */
/*   CHANGE   - Markup was modified, and                                        */
/*   STUCK    - Markup results are invalid, i.e. a cell has no candidate values */
/********************************************************************************/
static int simple_solver(grid *g)
{
    int flag = NOCHANGE;
    
    /* Mark the unsolved cells with candidate solutions based upon the current set of "givens" and solved cells */
    while ((flag |= mark_cells(g)) == CHANGE) {
        
        g->inc = 1;	     /* After initial markup, we start scoring for additional markup work */
        
        EXPLAIN_CURRENT_MARKUP(g);
        
        /* Continue to eliminate cells with unique candidate solutions from the game until */
        /* elimination and repeated markup efforts produce no changes in the remaining     */
        /* candidate solutions.                                                            */
        if (eliminate_singles(g) == NOCHANGE) break;
        
        EXPLAIN_CURRENT_MARKUP(g);
    }
    
    return flag;
}

/************************************************************************************/
/* Test a region to see if the candidate solutions for a paticular number           */
/* are confined to one row or column, and if so, eliminate                          */
/* their occurences in the remainder of the given row or column.                    */
/*                                                                                  */
/* The function has three possible return values:                                   */
/*   NOCHANGE - Markup did not change during the last pass,                         */
/*   CHANGE   - Markup was modified, and                                            */
/*   STUCK    - Markup results are invalid, i.e. a cell has no candidate values     */
/************************************************************************************/

static int region_vector_elim(grid *g, int region_no, int num)
{
    int i, j, r, c, mask, t, found;
    short rowhist[PUZZLE_DIM], colhist[PUZZLE_DIM];
    
    /* Init */
    found = NOCHANGE;
    memset(rowhist, 0, sizeof(rowhist[0])*PUZZLE_DIM);
    memset(colhist, 0, sizeof(colhist[0])*PUZZLE_DIM);
    
    mask = 1 << num;
    
    /* Create histograms for row and column placements for the value being checked */
    for (i = 0; i < PUZZLE_DIM; i++) {
        j = region[region_no][i];
        if ((g->cell[j] & mask)) {
            rowhist[map[j].row] += 1;
            colhist[map[j].col] += 1;
        }
    }
    
    /* Figure out if this number lies in only one row or column */
    
    /* Check rows first*/
    r = c = -1;
    for (i = 0; i < PUZZLE_DIM; i++) {
        if (rowhist[i]) {
            if (r < 0) {
                r = i;
            }
            else {
                r = -1;
                break;
            }
        }
    }
    
    /* Now check columns */
    for (i = 0; i < PUZZLE_DIM; i++) {
        if (colhist[i]) {
            if (c < 0) {
                c = i;
            }
            else {
                c = -1;
                break;
            }
        }
    }
    
    /* If the number is only in one row, then eliminate this number from the cells in the row outside of this region */
    if (r >= 0) {
        for (i = 0; i < PUZZLE_DIM; i++) {
            j = row[r][i];
            if (map[j].region != region_no && !g->cellflags[j]) {
                t = g->cell[j];
                if ((g->cell[j] &= ~mask) == 0) {
                    EXPLAIN_VECTOR_IMPASSE(g, "row", r, j, mask, region_no);
                    g->score += 10;
                    return STUCK;
                }
                if (t != g->cell[j]) {
                    found = CHANGE;
                    g->score += g->inc;
                    EXPLAIN_VECTOR_ELIM("row", r, j, mask, region_no);
                    if (bitcount(g->cell[j]) == 1) {
                        g->cellflags[j] = FOUND;
                        g->score += g->reward;
                        g->solved[g->exposed++] = j;
                        EXPLAIN_VECTOR_SOLVE(g, j);
                    }
                }
            }
        }
    }
    
    /* If the number is only in one column, then eliminate this number from the cells in the column outside of this region */
    else if (c >= 0) {
        for (i = 0; i < PUZZLE_DIM; i++) {
            j = col[c][i];
            if (map[j].region != region_no && !g->cellflags[j]) {
                t = g->cell[j];
                if ((g->cell[j] &= ~mask) == 0) {
                    EXPLAIN_VECTOR_IMPASSE(g, "column", c, j, mask, region_no);
                    g->score += 10;
                    return STUCK;
                }
                if (t != g->cell[j]) {
                    found = CHANGE;
                    g->score += g->inc;
                    EXPLAIN_VECTOR_ELIM("column", c, j, mask, region_no);
                    if (bitcount(g->cell[j]) == 1) {
                        g->cellflags[j] = FOUND;
                        g->score += g->reward;
                        g->solved[g->exposed++] = j;
                        EXPLAIN_VECTOR_SOLVE(g, j);
                    }
                }
            }
        }
    }
    
    if (found == CHANGE) {
        g->score += 10;	/* Bump score for sucessfully invoking this rule */
    }
    
    return found;
}

/**********************************************************************************/
/* Test all regions to see if the possibilities for a number                      */
/* are confined to specific rows or columns, and if so, eliminate                 */
/* the occurence of candidate solutions from the remainder of the                 */
/* specified row or column.                                                       */
/*                                                                                */
/* The function has three possible return values:                                 */
/*   NOCHANGE - Markup did not change during the last pass,                       */
/*   CHANGE   - Markup was modified, and                                          */
/*   STUCK    - Markup results are invalid, i.e. a cell has no candidate values   */
/**********************************************************************************/

static int vector_elimination(grid *g)
{
    int i, j, rc;
    
    /* For each region... */
    for (rc = NOCHANGE, i = 0; i < PUZZLE_DIM && rc != STUCK; i++) {
        
        /* For each digit... */
        for (j = 0; j < PUZZLE_DIM && rc != STUCK; j++) {
            
            /* Eliminate candidates outside of regions when a particular */
            /* candidate value aligns itself to a row or column within   */
            /* a 3x3 region.                                             */
            rc |= region_vector_elim(g, i, j);
        }
    }
    
    return rc;
}

/**********************************************************************************/
/* This function implements the rule that when a subset of cells                  */
/* in a row/column/region contain matching subsets of candidate                   */
/* solutions, i.e. 2 matching possibilities for 2 cells, 3                        */
/* matching possibilities for 3 cells, etc., then those                           */
/* candidates may be eliminated from the other cells in the                       */
/* row, column, or region.                                                        */
/*                                                                                */
/* The function has three possible return values:                                 */
/*   NOCHANGE - Markup did not change during the last pass,                       */
/*   CHANGE   - Markup was modified, and                                          */
/*   STUCK    - Markup results are invalid, i.e. a cell has no candidate values   */
/**********************************************************************************/

static int elim_matches(grid *g, int const *cell_list, char *desc, int ndx)
{
    int i, j, k, e, count, rc, flag;
    short c, mask, tmp, elts[PUZZLE_DIM], eliminated[PUZZLE_DIM];
    static int counts[1<<PUZZLE_DIM];
    
    rc = NOCHANGE;
    
    /* Check for two and three valued subsets. Any more than that burns CPU cycles */
    /* and just slows us down.                                                     */
    for (i = 2; i < 4; i++) {
        
        flag = 0;
        
        /* Create histogram to detect cells with matching subsets */
        for (j = 0; j < PUZZLE_DIM; j++) {
            k = cell_list[j];
            elts[j] = g->cell[k];			/* Copy original cell candidates */
            
            if (bitcount(g->cell[k]) == i) {
                counts[g->cell[k]] += 1;        /* The bucket records the number of cells with this subset */
            }
        }
        
        /* For each cell in the list... */
        for (e = j = 0; j < PUZZLE_DIM; j++) {
            
            c = g->cell[cell_list[j]];		/* Get cell's candidates */
            
            /* Check to see if we've already eliminated this subset */
            for (k = 0; k < e; k++)
                if (c == eliminated[k]) break;
            if (e && k < e) continue;
            
            /* Get count from histogram bucket */
            count = (int) (counts[c]);
            
            /* If too few solution candidates for the number of cells, then we're stuck */
            if (count > i) {
                EXPLAIN_TUPLE_IMPASSE(g, desc, ndx, c, count, i);
                /* Clean up static array */
                for (k = 0; k < 9; k++) counts[elts[k]] = 0;
                g->score += 10;
                return STUCK;
            }
            
            /* Do candidate and cell counts match? */
            if (count == i) {
                
                /* Compute mask used to eliminate candidates from other cells */
                mask = ~c;
                
                /* Record (for later) the values being eliminated */
                eliminated[e++] = c;
                
                /* Eliminate candidates from the other cells in the list */
                
                /* For each cell... */
                for (k = 0; k < PUZZLE_DIM; k++) {
                    
                    /* If the cell candidates do not exactly match the current subset... */
                    if (c != g->cell[cell_list[k]] && !g->cellflags[cell_list[k]]) {
                        
                        /* Get cell candidates */
                        tmp = g->cell[cell_list[k]];
                        
                        /* Eliminate candidates with our mask */
                        g->cell[cell_list[k]] &= mask;
                        
                        /* Did the elimination change the candidates? */
                        if (tmp != g->cell[cell_list[k]]) {
                            
                            /* Note the change and bump the score */
                            flag = CHANGE;
                            g->score += i;
                            
                            EXPLAIN_TUPLE_ELIM(desc, ndx, c, cell_list[k]);
                            
                            /* Did we solve the cell under consideration? */
                            if (bitcount(g->cell[cell_list[k]]) == 1) {
                                
                                /* Mark cell as found and bump the score */
                                g->cellflags[cell_list[k]] = FOUND;
                                g->score += g->reward;
                                g->solved[g->exposed++] = cell_list[k];
                                EXPLAIN_TUPLE_SOLVE(g, cell_list[k]);
                            }
                        }
                    }
                }
            }
        }
        
        /* Cleanup the static histogram array */
        for (j = 0; j < PUZZLE_DIM; j++) counts[elts[j]] = 0;
        
        rc |= flag;
    }
    
    return rc;
}

/**********************************************************************************/
/* Eliminate subsets from rows, columns, and regions.                             */
/*                                                                                */
/* The function has three possible return values:                                 */
/*   NOCHANGE - Markup did not change during the last pass,                       */
/*   CHANGE   - Markup was modified, and                                          */
/*   STUCK    - Markup results are invalid, i.e. a cell has no candidate values   */
/**********************************************************************************/

static int mult_elimination(grid *g)
{
    int i, rc = NOCHANGE;
    
    /* Eliminate subsets from rows */
    for (i = 0; i < PUZZLE_DIM; i++) {
        rc |= elim_matches(g, row[i], (char *)"row", i);
    }
    
    /* Eliminate subsets from columns */
    for (i = 0; i < PUZZLE_DIM; i++) {
        rc |= elim_matches(g, col[i], (char *)"column", i);
    }
    
    /* Eliminate subsets from regions */
    for (i = 0; i < PUZZLE_DIM; i++) {
        rc |= elim_matches(g, region[i], (char *)"region", i);
    }
    
    return rc;
}

/**************************************************/
/* Entry point to the recursive solver algorithm. */
/**************************************************/
static int rsolve(grid *g, return_soln soln_callback)
{
    int i, j, min, c, weight, mask, flag = 0;
    grid mygrid;
    
    /* Keep track of recursive depth */
    lvl += 1;
    if (lvl > g->maxlvl) g->maxlvl = lvl;
    
    for (;;) {
        
        /* Attempt a simple solution */
        if (simple_solver(g) == STUCK) break;
        
        /* Check for solution */
        if (g->exposed >= PUZZLE_CELLS) break;
        
        g->reward += 2;		/* Bump reward as we graduate to more "advanced" solving techniques */
        
        /* Eliminate tuples */
        if ((flag = mult_elimination(g)) == CHANGE) {
            EXPLAIN_CURRENT_MARKUP(g);
            continue;
        }
        
        /* Check if impasse */
        if (flag == STUCK) break;
        
        /* Check for solution */
        if (g->exposed >= PUZZLE_CELLS) break;
        
        /* Eliminate clues aligned within regions from exterior cells in rows or columns */
        if ((flag = vector_elimination(g)) == CHANGE) {
            EXPLAIN_CURRENT_MARKUP(g);
            continue;
        }
        
        /* Check if impasse */
        if (flag == STUCK) break;
        
        /* Check for solution */
        if (g->exposed >= PUZZLE_CELLS) break;
        
        g->reward += 5;		/* Bump reward as we are about to start trial soutions */
        
        /* Attempt a trial solution */
        memcpy(&mygrid, g, sizeof(grid));	/* Make working copy of puzzle */
        
        /* Find the first cell with the smallest number of alternatives */
        for (weight= 0, c = -1, min = PUZZLE_DIM, i = 0; i < PUZZLE_CELLS; i++) {
            if (!mygrid.cellflags[i]) {
                j = bitcount(mygrid.cell[i]);
                weight += 1;
                if (j < min) {
                    min = j;
                    c = i;
                }
            }
        }
        
        mygrid.score += weight;	/* Add penalty to score */
        
        /* Cell at index 'c' will be our starting point */
        if (c >= 0) for (mask = 1, i = 0; i < PUZZLE_DIM; i++) {
            
            /* Is this a candidate? */
            if (mask & g->cell[c]) {
                
                EXPLAIN_TRIAL(c, mask);
                
                mygrid.score += (int)(((50.0 * lvl * weight) / (double)(PUZZLE_CELLS)) + 0.5);	/* Add'l penalty */
                
                /* Try one of the possible candidates for this cell */
                mygrid.cell[c] = mask;
                mygrid.cellflags[c] = FOUND;
                mygrid.solved[mygrid.exposed++] = c;
                
                EXPLAIN_CURRENT_MARKUP(&mygrid);
                flag = rsolve(&mygrid, soln_callback);	/* Recurse with working copy of puzzle */
                
                /* Did we find a solution? */
                if (flag == FOUND && !enumerate_all) {
                    EXPLAIN_BACKTRACK;
                    lvl -= 1;
                    return FOUND;
                }
                
                /* Preserve score, solution count and recursive depth as we back out of recursion */
                g->score = mygrid.score;
                g->solncount = mygrid.solncount;
                g->maxlvl = mygrid.maxlvl;
                memcpy(&mygrid, g, sizeof(grid));
            }
            mask <<= 1;	/* Get next possible candidate */
        }
        
        break;
    }
    
    if (g->exposed == PUZZLE_CELLS && validate(g, 0)) {
        soln_callback(g);
        g->solncount += 1;
        EXPLAIN_SOLN_FOUND(g);
        EXPLAIN_BACKTRACK;
        lvl -= 1;
        flag = FOUND;
    } else {
        EXPLAIN_BACKTRACK;
        lvl -= 1;
        flag = STUCK;
        if (!lvl && !g->solncount) validate(g, 1);		/* Print verbose diagnostic for insoluble puzzle */
    }
    
    return flag;
}

/*****************************************************************/
/* Add a puzzle solution to the singly linked list of solutions. */
/* Crap out if no memory available.                              */
/*****************************************************************/

static int add_soln(grid *g)
{
    grid *tmp;
    
    if ((tmp = (grid *)malloc(sizeof(grid))) == NULL) {
        fprintf(stderr, "Out of memory.\n");
        exit(1);
    }
    memcpy(tmp, g, sizeof(grid));
    tmp->next = soln_list;
    soln_list = tmp;
    return 0;
}

/************************************/
/* Print hints as to command usage. */
/************************************/

static void usage()
{
    fprintf(stderr, "Usage:\n\t%s {-p puzzle | -f <puzzle_file>} [-o <outfile>]\n", myname);
    fprintf(stderr, "\t\t[-r <reject_file>] [-1][-a][-c][-G][-g][-l][-m][-n][-s]\n");
    fprintf(stderr, "where:\n\t-1\tSearch for first solution, otherwise all solutions are returned\n"
            "\t-a\tRequests that the answer (solution) be printed\n"
            "\t-c\tPrint a count of solutions for each puzzle\n"
            "\t-d\tPrint the recursive trial depth required to solve the puzzle\n"
#ifdef EXPLAIN
            "\t-e\tPrint a step-by-step explanation of the solution(s)\n"
#endif
            "\t-f\tTakes an argument which specifes an input file\n\t\tcontaining one or more unsolved puzzles (default: stdin)\n"
            "\t-G\tPrint the puzzle solution(s) in a 9x9 grid format\n"
            "\t-g\tPrint the number of given clues\n"
            "\t-m\tPrint an octal mask for the puzzle givens\n"
            "\t-n\tNumber each result\n"
            "\t-o\tSpecifies an output file for the solutions (default: stdout)\n"
            "\t-p\tTakes an argument giving a single inline puzzle to be solved\n"
            "\t-r\tSpecifies an output file for unsolvable puzzles\n\t\t(default: stderr)\n"
            "\t-s\tPrint the puzzle's score or difficulty rating\n"
            "\t-?\tPrint usage information\n\n");
    fprintf(stderr, "The return code is zero if all puzzles had unique solutions,\n"
            "(or have one or more solutions when -1 is specified) and non-zero\n"
            "when no unique solution exists.\n");
}

/********************************************************/
/* Print the puzzle as an 81 character string of digits */
/********************************************************/

static char *format_answer(grid *g, char *outbuf)
{
    int i;
    
    for (i = 0; i < PUZZLE_CELLS; i++)
        outbuf[i] = symtab[g->cell[i]];
    outbuf[i] = 0;
    
    return outbuf;
}

/*******************************************/
/* Print the puzzle as a standard 9x9 grid */
/*******************************************/

static void print_grid(char *sud, FILE *h)
{
    
    fprintf(h, "\n");
    EXPLAIN_INDENT(h);
    fprintf(h, "+---+---+---+\n");
    
    EXPLAIN_INDENT(h);
    fprintf(h, "|%*.*s|%*.*s|%*.*s|\n", PUZZLE_ORDER, PUZZLE_ORDER, sud, PUZZLE_ORDER, PUZZLE_ORDER, sud+3, PUZZLE_ORDER, PUZZLE_ORDER, sud+6);
    EXPLAIN_INDENT(h);
    fprintf(h, "|%*.*s|%*.*s|%*.*s|\n", PUZZLE_ORDER, PUZZLE_ORDER, sud+9, PUZZLE_ORDER, PUZZLE_ORDER, sud+12, PUZZLE_ORDER, PUZZLE_ORDER, sud+15);
    EXPLAIN_INDENT(h);
    fprintf(h, "|%*.*s|%*.*s|%*.*s|\n", PUZZLE_ORDER, PUZZLE_ORDER, sud+18, PUZZLE_ORDER, PUZZLE_ORDER, sud+21, PUZZLE_ORDER, PUZZLE_ORDER, sud+24);
    
    EXPLAIN_INDENT(h);
    fprintf(h, "+---+---+---+\n");
    
    EXPLAIN_INDENT(h);
    fprintf(h, "|%*.*s|%*.*s|%*.*s|\n", PUZZLE_ORDER, PUZZLE_ORDER, sud+27, PUZZLE_ORDER, PUZZLE_ORDER, sud+30, PUZZLE_ORDER, PUZZLE_ORDER, sud+33);
    EXPLAIN_INDENT(h);
    fprintf(h, "|%*.*s|%*.*s|%*.*s|\n", PUZZLE_ORDER, PUZZLE_ORDER, sud+36, PUZZLE_ORDER, PUZZLE_ORDER, sud+39, PUZZLE_ORDER, PUZZLE_ORDER, sud+42);
    EXPLAIN_INDENT(h);
    fprintf(h, "|%*.*s|%*.*s|%*.*s|\n", PUZZLE_ORDER, PUZZLE_ORDER, sud+45, PUZZLE_ORDER, PUZZLE_ORDER, sud+48, PUZZLE_ORDER, PUZZLE_ORDER, sud+51);
    
    EXPLAIN_INDENT(h);
    fprintf(h, "+---+---+---+\n");
    
    EXPLAIN_INDENT(h);
    fprintf(h, "|%*.*s|%*.*s|%*.*s|\n", PUZZLE_ORDER, PUZZLE_ORDER, sud+54, PUZZLE_ORDER, PUZZLE_ORDER, sud+57, PUZZLE_ORDER, PUZZLE_ORDER, sud+60);
    EXPLAIN_INDENT(h);
    fprintf(h, "|%*.*s|%*.*s|%*.*s|\n", PUZZLE_ORDER, PUZZLE_ORDER, sud+63, PUZZLE_ORDER, PUZZLE_ORDER, sud+66, PUZZLE_ORDER, PUZZLE_ORDER, sud+69);
    EXPLAIN_INDENT(h);
    fprintf(h, "|%*.*s|%*.*s|%*.*s|\n", PUZZLE_ORDER, PUZZLE_ORDER, sud+72, PUZZLE_ORDER, PUZZLE_ORDER, sud+75, PUZZLE_ORDER, PUZZLE_ORDER, sud+78);
    
    EXPLAIN_INDENT(h);
    fprintf(h, "+---+---+---+\n");
}

/*****************************************************/
/* Based upon the Left-to-Right-Top-to-Bottom puzzle */
/* presented in "sbuf", create a 27 octal digit      */
/* mask of the givens in the 28 character buffer     */
/* pointed to by "mbuf." Return a pointer to mbuf.   */
/*****************************************************/

static char *cvt_to_mask(char *mbuf, char *sbuf)
{
    char *mask_buf = mbuf;
    static const char *maskchar = "01234567";
    int i, m;
    
    mask_buf[PUZZLE_DIM*3] = 0;
    for (i = 0; i < PUZZLE_CELLS; i += 3) {
        m = 0;
        if (is_given(sbuf[i])) {
            m |= 4;
        }
        else {
            sbuf[i] = '0';
        }
        if (is_given(sbuf[i+1])) {
            m |= 2;
        }
        else {
            sbuf[i+1] = '0';
        }
        if (is_given(sbuf[i+2])) {
            m |= 1;
        }
        else {
            sbuf[i+2] = '0';
        }
        *mask_buf++ = maskchar[m];
    }
    return mbuf;
}

/*******************/
/* Mainline logic. */
/*******************/

int dupree_solver(int32_t dispflag,int32_t *scorep,char *puzzle)
{
    int argc; char *argv[4];
    int i, rc, bog, count, solved, unsolved, solncount=0, flag, prt_count, prt_num, prt_score, prt_answer, prt_depth, prt_grid, prt_mask, prt_givens, prt, len;
    char *infile=0, *outfile=0, *rejectfile=0, inbuf[128], outbuf[128], mbuf[28];
    grid g, *s=0;
    FILE *h=0;
    soln_list = NULL;
    myname = (char *)"internal";
    /* Get our command name from invoking command line
    if ((myname = strrchr(argv[0], '/')) == NULL)
        myname = argv[0];
    else
        myname++;
    argc = 3;
     argv[1] = "-p";
     argv[2] = puzzle;
     argv[3] = 0;*/
    /* Print sign-on message to console */
    //fprintf(stderr, "%s version %s\n", myname, VERSION); fflush(stderr);
    argc = 1;
    /* Init */
    h = 0;//stdin;
    solnfile = stdout;
    rejects = stderr;
    rejectfile = infile = outfile = NULL;
    rc = bog = prt_mask = prt_grid = prt_score = prt_depth = prt_answer = prt_count = prt_num = prt_givens = 0;
    *inbuf = 0;
#ifdef skip
    /* Parse command line options */
    while ((opt = getopt(argc, argv, OPTIONS)) != -1) {
        switch (opt) {
            case '1':
                enumerate_all = 0;		/* only find first soln */
                break;
            case 'a':
                prt_answer = 1;		/* print solution */
                break;
            case 'c':
                prt_count = 1;		/* number solutions */
                break;
            case 'd':
                prt_depth = 1;
                break;
#ifdef EXPLAIN
            case 'e':
                explain = 1;
                break;
#endif
            case 'f':
                if (*inbuf) {		// -p and -f options are mutually exclusive
                    fprintf(stderr, "The -p and -f options are mutually exclusive\n");
                    usage();
                    exit(1);
                }
                infile = optarg;	// get name of input file
                break;
            case 'G':
                prt_grid = 1;
                break;
            case 'g':
                prt_givens = 1;
                break;
            case 'm':
                prt_mask = 1;
                break;
            case 'n':
                prt_num = 1;
                break;
            case 'o':
                outfile = optarg;
                break;
            case 'p':
                if (infile) {
                    fprintf(stderr, "The -p and -f options are mutually exclusive\n");
                    usage();
                    exit(1);
                }
                if (strlen(optarg) == PUZZLE_CELLS) {
                    strcpy(inbuf, optarg);
                }
                else {
                    fprintf(stderr, "Invalid puzzle specified: %s\n", optarg);
                    usage();
                    exit(1);
                }
                h = NULL;
                break;
            case 'r':
                rejectfile = optarg;
                break;
            case 's':
                prt_score = 1;
                break;
            default:
            case '?':
                usage();
                exit(1);
        }
    }
    /* Anthing else on the command line is bogus */
    if (argc > optind) {
        fprintf(stderr, "Extraneous args: ");
        for (i = optind; i < argc; i++) {
            fprintf(stderr, "%s ", argv[i]);
        }
        fprintf(stderr, "\n\n");
        usage();
        exit(1);
    }
    
    if (!enumerate_all && prt_score) {
        fprintf(stderr, "Scoring is meaningless when multi-solution mode is disabled.\n");
    }
    
    if (rejectfile && !(rejects = fopen(rejectfile, "w"))) {
        fprintf(stderr, "Failed to open reject output file: %s\n", rejectfile);
        exit(1);
    }
    
    if (outfile && !(solnfile = fopen(outfile, "w"))) {
        fprintf(stderr, "Failed to open solution output file: %s\n", outfile);
        exit(1);
    }
    
    /*if (infile && strcmp(infile, "-") && !(h = fopen(infile, "r"))) {
     fprintf(stderr, "Failed to open input game file: %s\n", infile);
     exit(1);
     }
     if (h) fgets(inbuf, 128, h);*/
#endif
    prt_answer = dispflag;		/* print solution */
    //prt_count = dispflag;		/* number solutions */
    prt_score = dispflag;
    prt_givens = dispflag;
    prt_num = dispflag;
    /* Set prt flag if we're printing anything at all */
    prt = prt_mask | prt_grid | prt_score | prt_depth | prt_answer | prt_num | prt_givens;
    
    strcpy(inbuf,puzzle);
    count = solved = unsolved = 0;
    //printf("inbuf.(%s)\n",inbuf);
    while (*inbuf) {
        
        if ((len = (int32_t)strlen(inbuf)) && inbuf[len-1] == '\n') {
            len -= 1;
            inbuf[len] = 0;
        }
        
        count += 1;
        if (len != PUZZLE_CELLS) {
            fprintf(rejects, "%d: %s bogus puzzle format\n", count, inbuf); fflush(rejects);
            *inbuf = 0;
            bog += 1;
            //if (h) fgets(inbuf, 128, h);
            continue;
        }
        
        cvt_to_grid(&g, inbuf);
        if (g.givens < 17) {
            fprintf(rejects, "%d: %*.*s bogus puzzle has less than 17 givens\n", count, PUZZLE_CELLS, PUZZLE_CELLS, inbuf); fflush(rejects);
            *inbuf = 0;
            bog += 1;
            //if (h) fgets(inbuf, 128, h);
            continue;
        }
        
        for (s = soln_list; s;) {
            s = soln_list->next;
            free(soln_list);
            soln_list = s;
        }
        
        flag = rsolve(&g, add_soln);
        if (soln_list) {
            solved++;
            for (solncount = 0, s = soln_list; s; s = s->next) {
                solncount += 1;
                if (prt_num) {
                    char nbuf[32];
                    if (!enumerate_all)
                        sprintf(nbuf, "%d: ", count);
                    else
                        sprintf(nbuf, "%d:%d ", count, solncount);
                    fprintf(solnfile, "%-s", nbuf);
                }
                if (solncount > 1 || !enumerate_all) g.score = 0;
                if (prt_score) fprintf(solnfile, "score: %-7d ", g.score);
                if (prt_depth) fprintf(solnfile, "depth: %-3d ", g.maxlvl);
                if (prt_answer || prt_grid) format_answer(s, outbuf);
                if (prt_answer) fprintf(solnfile, "%s", outbuf);
                if (prt_mask) fprintf(solnfile, " %s", cvt_to_mask(mbuf, inbuf));
                if (prt_givens) fprintf(solnfile, " %d", g.givens);
                if (prt_grid) print_grid(outbuf, solnfile);
                if (prt) fprintf(solnfile, "\n");
                if (s->next == NULL && prt_count) fprintf(solnfile, "count: %d\n", solncount);
            }
            if (solncount > 1 && enumerate_all) {
                rc |= 1;
            }
            for (s = soln_list; s;) {
                s = soln_list->next;
                free(soln_list);
                soln_list = s;
            }
        }
        else {
            unsolved++;
            rc |= 1;
            fprintf(rejects, "%d: %*.*s unsolved\n", count, PUZZLE_CELLS, PUZZLE_CELLS, inbuf); fflush(rejects);
            diagnostic_grid(&g, rejects);
#if defined(DEBUG)
            mypause();
#endif
        }
        
        *inbuf = 0;
        //if (h) fgets(inbuf, 128, h);
    }
    
    //if (prt) fprintf(solnfile, "\nPuzzles: %d, Solved: %d, Unsolved: %d, Bogus: %d\n", count, solved, unsolved, bog);
    *scorep = g.score;
    return solncount;
}
// end https://github.com/attractivechaos/plb/blob/master/sudoku/incoming/sudoku_solver.c

// start https://github.com/mentalmove/SudokuGenerator
//
//  main.c
//  SudokuGenerator
//
//  Malte Pagel
//

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>


#define SMALL_LINE 3
#define LINE 9
#define TOTAL 81

#define LIMIT 16777216

#define SHOW_SOLVED 1


struct dimensions_collection {
    int row;
    int column;
    int small_square;
};


static int indices[TOTAL];
static int riddle[TOTAL];
static int solved[TOTAL];
static int unsolved[TOTAL];
static int tries_to_set = 0;
static int taking_back;
static int global_unset_count = 0;


struct dimensions_collection get_collection(int);
int contains_element(int*, int, int);
void get_horizontal(int, int*);
void get_vertical(int, int*);
void get_square(int, int*);
int set_values(int, int);
void take_back(int);

int show_solution(int*);


int show_solution (int* solution) {
    
    int i;
    int counter = 0;
    
    printf( " -----------------------------------\n" );
    
    for ( i = 0; i < TOTAL; i++ ) {
        if ( i % LINE == 0 )
            printf( "|" );
        
        if ( solution[i] ) {
            printf( " %d ", solution[i]);
            counter++;
        }
        else
            printf( "   ");
        
        if ( i % LINE == (LINE - 1) ) {
            printf( "|\n" );
            if ( i != (TOTAL - 1) ) {
                if ( i % (SMALL_LINE * LINE) == (SMALL_LINE * LINE - 1) )
                    printf( "|-----------+-----------+-----------|\n" );
                else
                    printf( "|- - - - - -|- - - - - -|- - - - - -|\n" );
            }
        }
        else {
            if ( i % SMALL_LINE == (SMALL_LINE - 1) )
                printf( "|");
            else
                printf( ":" );
        }
    }
    
    printf( " -----------------------------------" );
    
    return counter;
}


/**
 * Takes a position inside the large square and returns
 *		- the row number
 *		- the column number
 *		- the small square number
 * where this position is situated in
 */
struct dimensions_collection get_collection (int index) {
    struct dimensions_collection ret;
    
    ret.row = (int) (index / LINE);
    ret.column = index % LINE;
    ret.small_square = SMALL_LINE * (int) (ret.row / SMALL_LINE) + (int) (ret.column / SMALL_LINE);
    
    return ret;
}

/**
 * Is 'the_element' in 'the_array'?
 */
int contains_element (int* the_array, int the_element, int length) {
    for ( int i = 0; i < length; i++ )
        if ( the_array[i] == the_element )
            return 1;
    return 0;
}

/**
 * Sets all members of row 'row'
 */
void get_horizontal (int row, int* ret) {
    int j = 0;
    for ( int i = (row * LINE); i < (row * LINE) + LINE; i++ )
        ret[j++] = riddle[i];
}
/**
 * Sets all members of column 'col'
 */
void get_vertical (int col, int* ret) {
    int j = 0;
    for ( int i = col; i < TOTAL; i += LINE )
        ret[j++] = riddle[i];
}
/**
 * Sets all members of small square 'which'
 */
void get_square (int which, int* ret) {
    for ( int i = 0; i < SMALL_LINE; i++ )
        for ( int j = 0; j < SMALL_LINE; j++ )
            ret[SMALL_LINE * i + j] = riddle[LINE * i + which * SMALL_LINE + j + ((int) (which / SMALL_LINE) * (SMALL_LINE - 1) * LINE)];
}

/**
 * Recursive function:
 * Try for each position the numbers from 1 to LINE
 *      (except 'forbidden_number' if given).
 * If all numbers collide with already set numbers, move is bad.
 * If a number doesn't collide with already set numbers,
 *	- move is bad if next move collides with the already set numbers
 *              (including actual one)
 *	- move is good if it's the last one
 */
int set_values (int index, int forbidden_number) {
    
    if ( taking_back && tries_to_set > (2 * LIMIT) )
        return 1;
    
    int real_index = indices[index];
    struct dimensions_collection blocks = get_collection(real_index);
    int elements[LINE];
    
    for ( int i = 1; i <= LINE; i++ ) {
        if ( forbidden_number && i == forbidden_number )
            continue;
        
        tries_to_set++;
        
        get_horizontal(blocks.row, elements);
        if ( contains_element(elements, i, LINE) )
            continue;
        
        get_vertical(blocks.column, elements);
        if ( contains_element(elements, i, LINE) )
            continue;
        
        get_square(blocks.small_square, elements);
        if ( contains_element(elements, i, LINE) )
            continue;
        
        riddle[real_index] = i;
        
        if ( index == (TOTAL - 1) || set_values((index + 1), 0) )
            return 1;
    }
    
    riddle[real_index] = 0;
    
    return 0;
}

/**
 * Some steps to hide unnecessary numbers:
 *	a) Define last piece as 'special piece'
 *	b) Remember this piece's value
 *	c) Try to create riddle from this position on,
 *          but forbid the value of the special piece
 *	d)	I)  If operation fails, define the piece before the special piece
 *          as 'special piece' and continue with b)
 *		II) If operation is possible, reset 'special piece'
 *			and put it to start of list
 *	e) Stop if all pieces are tried or calculation limit is reached
 */
void take_back (int unset_count) {
    
    global_unset_count++;
    
    int i;
    
    int tmp = riddle[indices[TOTAL - unset_count]];
    int redundant = set_values((TOTAL - unset_count), tmp);
    
    if ( !redundant ) {
        unsolved[indices[TOTAL - unset_count]] = 0;
        take_back(++unset_count);
    }
    else {
        riddle[indices[TOTAL - unset_count]] = tmp;
        for ( i = 1; i < unset_count; i++ )
            riddle[indices[TOTAL - unset_count + i]] = 0;
        
        for ( i = (TOTAL - unset_count); i > 0; i-- )
            indices[i] = indices[i - 1];
        indices[0] = tmp;
        
        if ( global_unset_count < TOTAL && tries_to_set < LIMIT )
            take_back(unset_count);
    }
}


int sudoku(uint8_t solved9[LINE][LINE],uint8_t unsolved9[LINE][LINE],uint32_t srandi)
{
    int i, j, random, small_rows, small_cols, tmp, redundant,ind;
    int multi_raw[LINE][LINE];
    
    memset(indices,0,sizeof(indices));
    memset(solved,0,sizeof(solved));
    memset(unsolved,0,sizeof(unsolved));
    tries_to_set = 0;
    taking_back = 0;
    global_unset_count = 0;
    
    //time_t t;
    //time(&t);
    srand(srandi);
    
    /**
     * Initialization:
     * Fields are set to 0 ( i.e. we dont' know the number yet)
     */
    for ( i = 0; i < TOTAL; i++ )
        riddle[i] = 0;
    
    /**
     * Second initialization:
     * LINE times numbers from 0 to (LINE - 1),
     * i.e. every square
     */
    int big_rows_array[] = {0, 1, 2};
    int big_cols_array[] = {0, 1, 2};
    random = rand() % 4;
    switch (random) {
        case 1:
            big_rows_array[0] = 2;
            big_rows_array[1] = 1;
            big_rows_array[2] = 0;
            break;
        case 2:
            big_cols_array[0] = 2;
            big_cols_array[1] = 1;
            big_cols_array[2] = 0;
            break;
        case 3:
            big_rows_array[0] = 2;
            big_rows_array[1] = 1;
            big_rows_array[2] = 0;
            big_cols_array[0] = 2;
            big_cols_array[1] = 1;
            big_cols_array[2] = 0;
    }
    int big_rows, big_cols, big_rows_index, big_cols_index, start_value;
    i = 0;
    j = 0;
    for ( big_rows_index = 0; big_rows_index < SMALL_LINE; big_rows_index++ ) {
        big_rows = big_rows_array[big_rows_index];
        for ( big_cols_index = 0; big_cols_index < SMALL_LINE; big_cols_index++ ) {
            big_cols = big_cols_array[big_cols_index];
            start_value = big_rows * LINE * SMALL_LINE + (big_cols * SMALL_LINE);
            for ( small_rows = 0; small_rows < SMALL_LINE; small_rows++ )
                for ( small_cols = 0; small_cols < SMALL_LINE; small_cols++ )
                    multi_raw[i][j++] = small_rows * LINE + small_cols + start_value;
            i++;
            j = 0;
        }
    }
    
    
    /**
     * Randomization for every element of multi_raw.
     * Suffle only inside squares
     */
    for ( i = 0; i < LINE; i++ ) {
        for ( j = 0; j < LINE; j++ ) {
            random = rand() % LINE;
            if ( j == random )
                continue;
            tmp = multi_raw[i][j];
            multi_raw[i][j] = multi_raw[i][random];
            multi_raw[i][random] = tmp;
        }
    }
    
    /**
     * Linearization
     */
    for ( i = 0; i < LINE; i++ )
        for ( j = 0; j < LINE; j++ )
            indices[i * LINE + j] = multi_raw[i][j];
    
    
    /**
     * Setting numbers, start with the first one.
     * Variable 'redundant' is needed only for formal reasons
     */
    taking_back = 0;
    redundant = set_values(0, 0);
    
    
    memcpy(solved, riddle, (TOTAL * sizeof(int)));
    memcpy(unsolved, riddle, (TOTAL * sizeof(int)));
    
    
    /**
     * Exchanging some (few) indices for more randomized game
     */
    int random2;
    for ( i = (LINE - 1); i > 0; i-- ) {
        for ( j = 0; j < (int) (sqrt(i)); j++ ) {
            
            if ( !(rand() % ((int) (i * sqrt(i)))) || !(LINE - j) )
                continue;
            
            random = i * LINE + (int) (rand() % (LINE - j));
            random2 = rand() % TOTAL;
            
            if ( random == random2 )
                continue;
            
            tmp = indices[random];
            indices[random] = indices[random2];
            indices[random2] = tmp;
        }
    }
    
    
    tries_to_set = 0;
    taking_back = 1;
    take_back(1);
    
    
    if ( SHOW_SOLVED ) {
        printf( "\n\n" );
        redundant = show_solution(solved);
    }
    
    int counter = show_solution(unsolved);
    printf( "\t *** %d numbers left *** \n", counter );
    ind = 0;
    for (i=0; i<LINE; i++)
        for (j=0; j<LINE; j++,ind++)
        {
            solved9[i][j] = solved[ind];
            unsolved9[i][j] = unsolved[ind];
        }
    
    return 0;
}
// end https://github.com/mentalmove/SudokuGenerator

/******************************************************************************
 * Copyright © 2014-2019 The SuperNET Developers.                             *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * SuperNET software, including this file may be copied, modified, propagated *
 * or distributed except according to the terms contained in the LICENSE file *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/

#include "cJSON.h"
#define SUDOKU_NINETH 387420489

void sudoku_rowdisp(uint32_t x)
{
    int32_t i; char vals[10],val;
    x %= SUDOKU_NINETH;
    for (i=0; i<9; i++)
    {
        val = (x % 9);
        vals[i] = '1' + val;
        //printf("%d",val);
        x /= 9;
    }
    vals[i++] = 0;
    printf("%s\n",vals);
}

int32_t sudoku_privkeydisp(uint8_t key32[32])
{
    uint8_t i,val,flags[9]; char str[10]; uint32_t x,ind;
    memset(flags,0,sizeof(flags));
    ind = 0;
    for (i=0; i<8; i++)
    {
        x = (uint32_t)key32[ind++] << 24;
        x += (uint32_t)key32[ind++] << 16;
        x += (uint32_t)key32[ind++] << 8;
        x += (uint32_t)key32[ind++];
        sudoku_rowdisp(x);
        val = (x / SUDOKU_NINETH);
        flags[val]++;
        str[i] = val + '1';
    }
    for (i=0; i<9; i++)
        if ( flags[i] == 0 )
        {
            str[8] = i + '1';
            str[9] = 0;
            printf("%s\n",str);
            return(0);
        }
    return(-1);
}

void sudoku_privkey(uint8_t *privkey,uint8_t vals9[9][9])
{
    uint32_t x,keyvals[8]; int32_t i,j,ind;
    for (i=0; i<9; i++)
    {
        x = 0;
        for (j=8; j>=0; j--)
        {
            x *= 9;
            x += vals9[i][j]-1;
        }
        if ( i < 8 )
            keyvals[i] = x;
        else
        {
            for (j=0; j<8; j++)
                keyvals[j] += SUDOKU_NINETH * (vals9[i][j]-1);
        }
    }
    for (i=ind=0; i<8; i++)
    {
        privkey[ind++] = ((keyvals[i] >> 24) & 0xff);
        privkey[ind++] = ((keyvals[i] >> 16) & 0xff);
        privkey[ind++] = ((keyvals[i] >> 8) & 0xff);
        privkey[ind++] = (keyvals[i] & 0xff);
    }
}

void sudoku_gen(uint8_t key32[32],uint8_t unsolved[9][9],uint32_t srandi)
{
    uint8_t vals9[9][9],uniq9[9][9]; int32_t i,j;
    sudoku(vals9,unsolved,srandi);
    sudoku_privkey(key32,vals9);
    sudoku_privkeydisp(key32);
}

//////////////////////// start of CClib interface
// ./komodod -ac_name=SUDOKU -ac_supply=1000000 -pubkey=<yourpubkey> -addnode=5.9.102.210 -gen -genproclimit=1 -ac_cclib=sudoku -ac_perc=10000000 -ac_reward=100000000 -ac_cc=60000 -ac_script=2ea22c80203d1579313abe7d8ea85f48c65ea66fc512c878c0d0e6f6d54036669de940febf8103120c008203000401cc &
/* cclib "gen" 17 \"10\"
 5d13c1ad80daf37215c74809a36720c2ada90bacadb2e10bf0866092ce558432
*/

/* cclib "txidinfo" 17 \"5d13c1ad80daf37215c74809a36720c2ada90bacadb2e10bf0866092ce558432\"
{
    "result": "success",
    "txid": "5d13c1ad80daf37215c74809a36720c2ada90bacadb2e10bf0866092ce558432",
    "result": "success",
    "amount": 1.00000000,
    "unsolved": "46-8---15-75-61-3----4----8-1--75-----3--24----2-----6-4----------73----------36-",
    "name": "sudoku",
    "method": "txidinfo"
}*/

/* cclib "pending" 17
{
    "result": "success",
    "name": "sudoku",
    "method": "pending",
    "pending": [
                "5d13c1ad80daf37215c74809a36720c2ada90bacadb2e10bf0866092ce558432"
                ]
}*/

/*
 cclib "solution" 17 \"[%22fdc9409741f2ede29307da1a06438da0ea6f8d885d2d5c3199c4ef541ec1b5fd%22,%22469823715875961234231457698914675823653182479782394156346219587528736941197548362%22,1548777525,1548777526,...]\"
 {
 "name": "sudoku",
 "method": "solution",
 "sudokuaddr": "RSeoPJvMUSLfUHM1BomB97geW9zPznwHXk",
 "amount": 1.00000000,
 "result": "success",
 "hex": "0400008085202f8901328455ce926086f00be1b2adac0ba9adc22067a30948c71572f3da80adc1135d010000007b4c79a276a072a26ba067a565802102c57d40c1ddc92a5246a937bd7338823f1e8c916b137f2092d38cf250d74cb5ab8140f92d54f611aa3cb3d187eaadd56b06f3a8c0f5fba23956b26fdefc6038d9b6282de38525f72ebd8945a7994cef63ebca711ecf8fe6baeefcc218cf58efb59dc2a100af03800111a10001ffffffff02f0b9f505000000002321039433dc3749aece1bd568f374a45da3b0bc6856990d7da3cd175399577940a775ac0000000000000000fd9f016a4d9b01115351343639383233373135383735393631323334323331343537363938393134363735383233363533313832343739373832333934313536333436323139353837353238373336393431313937353438333632fd4401000000005c5078355c50783600000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000100000000000000000000000000"
 }
 
 cclib solution 17 \"[%224d50336780d5a300a1f01b12fe36f46a82f3b9935bb115e01e0113dc4f337aae%22,%22234791685716258943589643712865934127341827596927516438492375861178462359653189274%22,0,0,1548859143,1548859146,0,1548859146,0,1548859148,1548859149,0,1548859151,1548859152,0,1548859154,1548859155,1548859158,1548859159,0,0,0,1548859161,1548859163,0,1548859164,1548859168,0,1548859168,1548859170,1548859172,1548859172,1548859175,0,0,1548859176,0,0,1548859178,1548859178,0,0,1548859180,1548859181,1548859183,1548859184,1548859185,1548859186,1548859188,1548859190,1548859191,1548859192,1548859192,0,0,1548859195,1548859196,1548859197,1548859198,0,0,1548859199,1548859202,1548859202,0,1548859204,1548859205,1548859206,1548859209,1548859210,1548859211,1548859212,0,1548859214,1548859216,0,1548859217,1548859218,1548859219,1548859220,0,1548859222,1548859222]\"
 */

int32_t sudoku_captcha(int32_t dispflag,uint32_t timestamps[81],int32_t height)
{
    int32_t i,solvetime,diff,avetime,n = 0,retval = 0; uint64_t variance = 0; std::vector<uint32_t> list;
    for (i=0; i<81; i++)
    {
        if ( timestamps[i] != 0 )
        {
            list.push_back(timestamps[i]);
            n++;
        }
    }
    if ( n > 81/2 )
    {
        std::sort(list.begin(),list.end());
        solvetime = (list[n-1] - list[0]);
        if ( list[0] >= list[n-1] )
        {
            printf("list[0] %u vs list[%d-1] %u\n",list[0],n,list[n-1]);
            retval = -1;
        }
        else if ( list[n-1] > chainActive.LastTip()->nTime+200 )
            retval = -2;
        else if ( solvetime >= 777 )
            retval = 0;
        else
        {
            avetime = (solvetime / (n-1));
            if ( avetime == 0 )
                retval = -3;
            for (i=0; i<n-1; i++)
            {
                diff = (list[i+1] - list[i]);
                if ( dispflag != 0 )
                    printf("%d ",diff);
                diff -= avetime;
                variance += (diff * diff);
            }
            variance /= (n - 1);
            if ( dispflag != 0 )
                printf("solvetime.%d n.%d avetime.%d variance.%llu vs ave2 %d\n",solvetime,n,avetime,(long long)variance,avetime*avetime);
            if ( variance < avetime )
                retval = -5;
            else return(0);
        }
    } else retval = -6;
    if ( dispflag != 0 && retval != 0 )
        fprintf(stderr,"ERR >>>>>>>>>>>>>>> ht.%d retval.%d\n",height,retval);
    if ( height <= 2036 )
        return(0);
    else return(retval);
}

CScript sudoku_genopret(uint8_t unsolved[9][9])
{
    CScript opret; uint8_t evalcode = EVAL_SUDOKU; std::vector<uint8_t> data; int32_t i,j;
    for (i=0; i<9; i++)
        for (j=0; j<9; j++)
            data.push_back(unsolved[i][j]);
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << 'G' << data);
    return(opret);
}

CScript sudoku_solutionopret(char *solution,uint32_t timestamps[81])
{
    CScript opret; uint8_t evalcode = EVAL_SUDOKU; std::string str(solution); std::vector<uint8_t> data; int32_t i;
    for (i=0; i<81; i++)
    {
        data.push_back((timestamps[i] >> 24) & 0xff);
        data.push_back((timestamps[i] >> 16) & 0xff);
        data.push_back((timestamps[i] >> 8) & 0xff);
        data.push_back(timestamps[i] & 0xff);
    }
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << 'S' << str << data);
    return(opret);
}

uint8_t sudoku_solutionopreturndecode(char solution[82],uint32_t timestamps[81],CScript scriptPubKey)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f; std::string str; std::vector<uint8_t> data; int32_t i,ind; uint32_t x;
    GetOpReturnData(scriptPubKey,vopret);
    script = (uint8_t *)vopret.data();
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> str; ss >> data) != 0 && e == EVAL_SUDOKU && f == 'S' )
    {
        if ( data.size() == 81*sizeof(uint32_t) && str.size() == 81 )
        {
            strcpy(solution,str.c_str());
            for (i=ind=0; i<81; i++)
            {
                if ( solution[i] < '1' || solution[i] > '9' )
                    break;
                x = data[ind++];
                x <<= 8, x |= (data[ind++] & 0xff);
                x <<= 8, x |= (data[ind++] & 0xff);
                x <<= 8, x |= (data[ind++] & 0xff);
                timestamps[i] = x;
            }
            if ( i == 81 )
                return(f);
        } else fprintf(stderr,"datasize %d sol[%d]\n",(int32_t)data.size(),(int32_t)str.size());
    }
    return(0);
}

uint8_t sudoku_genopreturndecode(char *unsolved,CScript scriptPubKey)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f; std::vector<uint8_t> data; int32_t i;
    GetOpReturnData(scriptPubKey,vopret);
    script = (uint8_t *)vopret.data();
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> data) != 0 && e == EVAL_SUDOKU && f == 'G' )
    {
        if ( data.size() == 81 )
        {
            for (i=0; i<81; i++)
                unsolved[i] = data[i] == 0 ? '-' : '0' + data[i];
            unsolved[i] = 0;
            return(f);
        }
    }
    return(0);
}

UniValue sudoku_generate(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    UniValue result(UniValue::VOBJ); CPubKey sudokupk,pk; uint8_t privkey[32],unsolved[9][9],pub33[33]; uint32_t srandi; int32_t i,score; uint256 hash; char coinaddr[64],str[82],*jsonstr; uint64_t inputsum,amount,change=0; std::string rawtx;
    amount = COIN;
    /*if ( params != 0 )
    {
        if ( (jsonstr= jprint(params,0)) != 0 )
        {
            if ( jsonstr[0] == '"' && jsonstr[strlen(jsonstr)-1] == '"' )
            {
                jsonstr[strlen(jsonstr)-1] = 0;
                jsonstr++;
            }
            amount = atof(jsonstr) * COIN + 0.0000000049;
        }
    }*/
    result.push_back(Pair("result","success"));
    result.push_back(Pair("name","sudoku"));
    result.push_back(Pair("method","gen"));
    hash = chainActive.LastTip()->GetBlockHash();
    memcpy(&srandi,&hash,sizeof(srandi));
    srandi ^= (uint32_t)time(NULL);
    while ( 1 )
    {
        sudoku_gen(privkey,unsolved,srandi);
        for (i=0; i<TOTAL; i++)
            str[i] = '0' + unsolved[i/9][i%9];
        str[i] = 0;
        printf("solve: %s\n",str);
        if ( dupree_solver(1,&score,str) == 1 )
        {
            amount = score * COIN;
            break;
        }
    }
    priv2addr(coinaddr,pub33,privkey);
    pk = buf2pk(pub33);
    sudokupk = GetUnspendable(cp,0);
    result.push_back(Pair("srand",(int)srandi));
    result.push_back(Pair("amount",ValueFromAmount(amount)));
    if ( (inputsum= AddCClibInputs(cp,mtx,sudokupk,amount+2*txfee,16,cp->unspendableCCaddr,1)) >= amount+2*txfee )
    {
        //printf("inputsum %.8f\n",(double)inputsum/COIN);
        mtx.vout.push_back(MakeCC1vout(cp->evalcode,txfee,sudokupk));
        mtx.vout.push_back(MakeCC1vout(cp->evalcode,amount,pk));
        if ( inputsum > amount + 2*txfee )
            change = (inputsum - amount - 2*txfee);
        if ( change > txfee )
        {
            if ( change > 10000*COIN )
            {
                mtx.vout.push_back(MakeCC1vout(cp->evalcode,change/2,sudokupk));
                mtx.vout.push_back(MakeCC1vout(cp->evalcode,change/2,sudokupk));
            } else mtx.vout.push_back(MakeCC1vout(cp->evalcode,change,sudokupk));
        }
        rawtx = FinalizeCCTx(0,cp,mtx,pubkey2pk(Mypubkey()),txfee,sudoku_genopret(unsolved));
        if ( rawtx.size() > 0 )
        {
            CTransaction tx;
            result.push_back(Pair("hex",rawtx));
            if ( DecodeHexTx(tx,rawtx) != 0 )
            {
                LOCK(cs_main);
                if ( myAddtomempool(tx) != 0 )
                {
                    RelayTransaction(tx);
                    result.push_back(Pair("txid",tx.GetHash().ToString()));
                }
            }
        } else result.push_back(Pair("error","couldnt finalize CCtx"));
    } else result.push_back(Pair("error","not enough SUDOKU funds"));
    return(result);
}

UniValue sudoku_txidinfo(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ); int32_t numvouts; char CCaddr[64],str[65],*txidstr; uint256 txid,hashBlock; CTransaction tx; char unsolved[82]; CBlockIndex *pindex;
    if ( params != 0 )
    {
        result.push_back(Pair("result","success"));
        if ( (txidstr= jprint(params,0)) != 0 )
        {
            if ( txidstr[0] == '"' && txidstr[strlen(txidstr)-1] == '"' )
            {
                txidstr[strlen(txidstr)-1] = 0;
                txidstr++;
            }
            //printf("params -> (%s)\n",txidstr);
            decode_hex((uint8_t *)&txid,32,txidstr);
            txid = revuint256(txid);
            result.push_back(Pair("txid",txid.GetHex()));
            if ( myGetTransaction(txid,tx,hashBlock) != 0 && (numvouts= tx.vout.size()) > 1 )
            {
                if ( sudoku_genopreturndecode(unsolved,tx.vout[numvouts-1].scriptPubKey) == 'G' )
                {
                    result.push_back(Pair("result","success"));
                    if ( (pindex= komodo_blockindex(hashBlock)) != 0 )
                        result.push_back(Pair("height",pindex->GetHeight()));
                    Getscriptaddress(CCaddr,tx.vout[1].scriptPubKey);
                    result.push_back(Pair("sudokuaddr",CCaddr));
                    result.push_back(Pair("amount",ValueFromAmount(tx.vout[1].nValue)));
                    result.push_back(Pair("unsolved",unsolved));
                }
                else
                {
                    result.push_back(Pair("result","error"));
                    result.push_back(Pair("error","couldnt extract sudoku_generate opreturn"));
                }
            }
            else
            {
                result.push_back(Pair("result","error"));
                result.push_back(Pair("error","couldnt find txid"));
            }
        }
    }
    else
    {
        result.push_back(Pair("result","error"));
        result.push_back(Pair("error","missing txid in params"));
    }
    result.push_back(Pair("name","sudoku"));
    result.push_back(Pair("method","txidinfo"));
    return(result);
}

UniValue sudoku_pending(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ),a(UniValue::VARR);
    char coinaddr[64],unsolved[82]; int64_t nValue,total=0; uint256 txid,hashBlock; CTransaction tx; int32_t vout,numvouts; CPubKey sudokupk; CBlockIndex *pindex;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    sudokupk = GetUnspendable(cp,0);
    GetCCaddress(cp,coinaddr,sudokupk);
    SetCCunspents(unspentOutputs,coinaddr,true);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        //char str[65]; fprintf(stderr,"%s check %s/v%d %.8f\n",coinaddr,uint256_str(str,txid),vout,(double)it->second.satoshis/COIN);
        if ( it->second.satoshis != txfee || vout != 0 )
            continue;
        if ( myGetTransaction(txid,tx,hashBlock) != 0 && (numvouts= tx.vout.size()) > 1 )
        {
            if ( (nValue= IsCClibvout(cp,tx,vout,coinaddr)) == txfee && myIsutxo_spentinmempool(ignoretxid,ignorevin,txid,vout) == 0 )
            {
                if ( sudoku_genopreturndecode(unsolved,tx.vout[numvouts-1].scriptPubKey) == 'G' )
                {
                    UniValue obj(UniValue::VOBJ);
                    if ( (pindex= komodo_blockindex(hashBlock)) != 0 )
                        obj.push_back(Pair("height",pindex->GetHeight()));
                    obj.push_back(Pair("amount",ValueFromAmount(tx.vout[1].nValue)));
                    obj.push_back(Pair("txid",txid.GetHex()));
                    a.push_back(obj);
                    total += tx.vout[1].nValue;
                }
            }
        }
    }
    result.push_back(Pair("result","success"));
    result.push_back(Pair("name","sudoku"));
    result.push_back(Pair("method","pending"));
    result.push_back(Pair("pending",a));
    result.push_back(Pair("numpending",(int64_t)a.size()));
    result.push_back(Pair("total",ValueFromAmount(total)));
    return(result);
}

UniValue sudoku_solution(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    UniValue result(UniValue::VOBJ); int32_t i,j,good,ind,n,numvouts; uint256 txid; char *jsonstr,*newstr,*txidstr,coinaddr[64],checkaddr[64],CCaddr[64],*solution=0,unsolved[82]; CPubKey pk,mypk; uint8_t vals9[9][9],priv32[32],pub33[33]; uint32_t timestamps[81]; uint64_t balance,inputsum; std::string rawtx; CTransaction tx; uint256 hashBlock;
    mypk = pubkey2pk(Mypubkey());
    memset(timestamps,0,sizeof(timestamps));
    result.push_back(Pair("name","sudoku"));
    result.push_back(Pair("method","solution"));
    good = 0;
    if ( params != 0 )
    {
        if ( params != 0 && (n= cJSON_GetArraySize(params)) > 0 )
        {
            if ( n > 2 && n <= (sizeof(timestamps)/sizeof(*timestamps))+2 )
            {
                for (i=2; i<n; i++)
                {
                    timestamps[i-2] = juinti(params,i);
                    //printf("%u ",timestamps[i]);
                }
                if ( (solution= jstri(params,1)) != 0 && strlen(solution) == 81 )
                {
                    for (i=ind=0; i<9; i++)
                        for (j=0; j<9; j++)
                        {
                            if ( solution[ind] < '1' || solution[ind] > '9' )
                            {
                                result.push_back(Pair("result","error"));
                                result.push_back(Pair("error","illegal solution"));
                                return(result);
                            }
                            vals9[i][j] = solution[ind++] - '0';
                        }
                    sudoku_privkey(priv32,vals9);
                    priv2addr(coinaddr,pub33,priv32);
                    pk = buf2pk(pub33);
                    GetCCaddress(cp,CCaddr,pk);
                    result.push_back(Pair("sudokuaddr",CCaddr));
                    balance = CCaddress_balance(CCaddr,1);
                    result.push_back(Pair("amount",ValueFromAmount(balance)));
                    if ( sudoku_captcha(1,timestamps,komodo_nextheight()) < 0 )
                    {
                        result.push_back(Pair("result","error"));
                        result.push_back(Pair("error","captcha failure"));
                        return(result);
                    }
                    else
                    {
                        if ( (txidstr= jstri(params,0)) != 0 )
                        {
                            decode_hex((uint8_t *)&txid,32,txidstr);
                            txid = revuint256(txid);
                            result.push_back(Pair("txid",txid.GetHex()));
                            if ( CCgettxout(txid,0,1,0) < 0 )
                                result.push_back(Pair("error","already solved"));
                            else if ( myGetTransaction(txid,tx,hashBlock) != 0 && (numvouts= tx.vout.size()) > 1 )
                            {
                                Getscriptaddress(checkaddr,tx.vout[1].scriptPubKey);
                                if ( strcmp(checkaddr,CCaddr) != 0 )
                                {
                                    result.push_back(Pair("result","error"));
                                    result.push_back(Pair("error","wrong solution"));
                                    result.push_back(Pair("yours",CCaddr));
                                    return(result);
                                }
                                if ( sudoku_genopreturndecode(unsolved,tx.vout[numvouts-1].scriptPubKey) == 'G' )
                                {
                                    for (i=0; i<81; i++)
                                    {
                                        if ( unsolved[i] < '1' || unsolved[i] > '9')
                                            continue;
                                        else if ( unsolved[i] != solution[i] )
                                        {
                                            printf("i.%d [%c] != [%c]\n",i,unsolved[i],solution[i]);
                                            result.push_back(Pair("error","wrong sudoku solved"));
                                            break;
                                        }
                                    }
                                    if ( i == 81 )
                                        good = 1;
                                } else result.push_back(Pair("error","cant decode sudoku"));
                            } else result.push_back(Pair("error","couldnt find sudoku"));
                        }
                        if ( good != 0 )
                        {
                            mtx.vin.push_back(CTxIn(txid,0,CScript()));
                            if ( (inputsum= AddCClibInputs(cp,mtx,pk,balance,16,CCaddr,1)) >= balance )
                            {
                                mtx.vout.push_back(CTxOut(balance,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
                                CCaddr2set(cp,cp->evalcode,pk,priv32,CCaddr);
                                rawtx = FinalizeCCTx(0,cp,mtx,pubkey2pk(Mypubkey()),txfee,sudoku_solutionopret(solution,timestamps));
                                if ( rawtx.size() > 0 )
                                {
                                    result.push_back(Pair("result","success"));
                                    result.push_back(Pair("hex",rawtx));
                                }
                                else result.push_back(Pair("error","couldnt finalize CCtx"));
                            } else result.push_back(Pair("error","couldnt find funds in solution address"));
                        }
                    }
                }
            }
            else
            {
                printf("n.%d params.(%s)\n",n,jprint(params,0));
                result.push_back(Pair("error","couldnt get all params"));
            }
            return(result);
        }
        else
        {
            result.push_back(Pair("result","error"));
            result.push_back(Pair("error","couldnt parse parameters"));
            result.push_back(Pair("parameters",newstr));
            return(result);
        }
    }
    result.push_back(Pair("result","error"));
    result.push_back(Pair("error","missing parameters"));
    return(result);
}

int32_t sudoku_minval(uint32_t timestamps[81])
{
    int32_t i,ind = -1; uint32_t mintimestamp = 0xffffffff;
    for (i=0; i<81; i++)
        if ( timestamps[i] != 0 && timestamps[i] < mintimestamp )
        {
            mintimestamp = timestamps[i], ind = i;
            //fprintf(stderr,"%d ",i);
        }
    //fprintf(stderr,"mintimestamp.%u\n",mintimestamp);
    return(ind);
}

bool sudoku_validate(struct CCcontract_info *cp,int32_t height,Eval *eval,const CTransaction tx)
{
    static char laststr[512];
    CScript scriptPubKey; std::vector<uint8_t> vopret; uint8_t *script,e,f,funcid; int32_t i,ind,errflag,dispflag,score,numvouts; char unsolved[82],solution[82],str[512]; uint32_t timestamps[81]; CTransaction vintx; uint256 hashBlock;
    if ( (numvouts= tx.vout.size()) > 1 )
    {
        scriptPubKey = tx.vout[numvouts-1].scriptPubKey;
        GetOpReturnData(scriptPubKey,vopret);
        if ( vopret.size() > 2 )
        {
            script = (uint8_t *)vopret.data();
            if ( script[0] == EVAL_SUDOKU )
            {
                switch ( script[1] )
                {
                    case 'G':
                        if ( sudoku_genopreturndecode(unsolved,scriptPubKey) == 'G' )
                        {
                            //fprintf(stderr,"unsolved.(%s)\n",unsolved);
                            if ( dupree_solver(0,&score,unsolved) != 1 || score*COIN != tx.vout[1].nValue )
                            {
                                sprintf(str,"ht.%d score.%d vs %.8f %s",height,score,(double)tx.vout[1].nValue/COIN,tx.GetHash().ToString().c_str());
                                if ( strcmp(str,laststr) != 0 )
                                {
                                    strcpy(laststr,str);
                                    fprintf(stderr,"%s\n",str);
                                }
                                if ( strcmp(ASSETCHAINS_SYMBOL,"SUDOKU") != 0 || height > 2000 )
                                    return eval->Invalid("mismatched sudoku value vs score");
                                else return(true);
                            } else return(true);
                        }
                        fprintf(stderr,"height.%d txid.%s\n",height,tx.GetHash().ToString().c_str());
                        return eval->Invalid("invalid generate opreturn");
                    case 'S':
                        sprintf(str,"SOLVED ht.%d %.8f %s",height,(double)tx.vout[0].nValue/COIN,tx.GetHash().ToString().c_str());
                        if ( strcmp(str,laststr) != 0 )
                        {
                            strcpy(laststr,str);
                            fprintf(stderr,"%s\n",str);
                            dispflag = 1;
                        } else dispflag = 0;
                        if ( sudoku_solutionopreturndecode(solution,timestamps,scriptPubKey) == 'S' )
                        {
                            if ( tx.vin.size() > 1 && tx.vin[0].prevout.hash == tx.vin[1].prevout.hash && tx.vin[0].prevout.n == 0 && tx.vin[1].prevout.n == 1 && myGetTransaction(tx.vin[0].prevout.hash,vintx,hashBlock) != 0 )
                            {
                                if ( vintx.vout.size() > 1 && sudoku_genopreturndecode(unsolved,vintx.vout[vintx.vout.size()-1].scriptPubKey) == 'G' )
                                {
                                    for (i=errflag=0; i<81; i++)
                                    {
                                        if ( 0 && dispflag != 0 )
                                            fprintf(stderr,"%u ",timestamps[i]);
                                        if ( (timestamps[i] != 0 && unsolved[i] >= '1' && unsolved[i] <= '9') || (timestamps[i] == 0 && (unsolved[i] < '1' || unsolved[i] > '9')) )
                                            errflag++;
                                    }
                                    if ( errflag != 0 )
                                    {
                                        if ( dispflag != 0 )
                                            fprintf(stderr,"ht.%d errflag.%d %s\n",height,errflag,unsolved);
                                        if ( (height != 1220 && height != 1383) || strcmp(ASSETCHAINS_SYMBOL,"SUDOKU") != 0  )
                                            return eval->Invalid("invalid timestamp vs unsolved");
                                    }
                                    if ( dupree_solver(0,&score,unsolved) != 1 )
                                    {
                                        if ( dispflag != 0 )
                                            fprintf(stderr,"non-unique sudoku at ht.%d\n",height);
                                        if ( strcmp(ASSETCHAINS_SYMBOL,"SUDOKU") != 0 )
                                            return eval->Invalid("invalid sudoku with multiple solutions");
                                    }
                                    if ( dispflag != 0 )
                                        fprintf(stderr,"%s score.%d %s\n",solution,score,unsolved);
                                    if ( sudoku_captcha(dispflag,timestamps,height) < 0 )
                                        return eval->Invalid("failed captcha");
                                    /*for (i=lasttime=0; i<81; i++)
                                    {
                                        if ( (ind= sudoku_minval(timestamps)) >= 0 )
                                        {
                                            unsolved[ind] = solution[ind];
                                            if ( lasttime == 0 )
                                                lasttime = timestamps[ind];
                                            if ( dupree_solver(0,&score,unsolved) != 1 )
                                                fprintf(stderr,"i.%d ind.%d non-unique\n",i,ind);
                                            if ( dispflag != 0 )
                                                fprintf(stderr,"%d.%d ",score,timestamps[ind]-lasttime);
                                            lasttime = timestamps[ind];
                                            timestamps[ind] = 0;
                                        } else break;
                                    }
                                    if ( dispflag != 0 )
                                        fprintf(stderr,"scores convergence\n");*/
                                    return(true);
                                } else return eval->Invalid("invalid solution opret");
                            }
                            else if ( strcmp(ASSETCHAINS_SYMBOL,"SUDOKU") == 0 && height == 236 )
                                return(true);
                            else return eval->Invalid("invalid solution vin");
                        }
                        fprintf(stderr,"solution ht.%d %s bad opret\n",height,tx.GetHash().ToString().c_str());
                        return eval->Invalid("invalid solution opreturn");
                    default: return eval->Invalid("invalid funcid");
                }
            } else return eval->Invalid("invalid evalcode");
            
        }
    }
    return eval->Invalid("not enough vouts");
}


