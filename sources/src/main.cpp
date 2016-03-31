/*
Rodent, a UCI chess playing engine derived from Sungorus 1.4
Copyright (C) 2009-2011 Pablo Vazquez (Sungorus author)
Copyright (C) 2011-2016 Pawel Koziol

Rodent is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published
by the Free Software Foundation, either version 3 of the License,
or (at your option) any later version.

Rodent is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty
of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "rodent.h"
#include "magicmoves.h"
#include "timer.h"
#include "book.h"

sTimer Timer; // class for setting and observing time limits
sBook  MainBook;  // opening book
sBook  GuideBook;
cEval Eval;
POS p;

int main() {
  
  eval_blur = 0;
  draw_score = 0;
  time_percentage = 100;
  book_filter = 20;
  use_book = 1;
  panel_style = 0;
  verbose = 1;
  np_bonus = 6;
  rp_malus = 3;
  keep_queen = 0;
  keep_rook = 0;
  keep_bishop = 0;
  keep_knight = 0;
  keep_pawn = 0;
  hist_limit = 24576;
  hist_perc = 175;

  Timer.Init();
  initmagicmoves();
  Init();
  InitWeights();
  InitEval();
  InitSearch();
  MainBook.bookName = "rodent.bin";
  MainBook.OpenPolyglot();
  GuideBook.bookName = "guide.bin";
  GuideBook.OpenPolyglot();
  ReadPersonality("basic.ini");
  UciLoop();
  MainBook.ClosePolyglot();
  GuideBook.ClosePolyglot();
  return 0;
}
