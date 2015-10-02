#include <stdio.h>
#include <string.h>
#include "math.h"
#include "rodent.h"
#include "timer.h"

void Think(POS *p, int *pv)
{
	ClearHist();
	tt_date = (tt_date + 1) & 255;
	nodes = 0;
	abort_search = 0;
	Timer.SetStartTime();
	Iterate(p, pv);
}

void Iterate(POS *p, int *pv)
{
	int val = 0;
	U64 nps = 0;
	int cur_val = 0;
	Timer.SetIterationTiming();
	for (root_depth = 1; root_depth <= Timer.GetData(MAX_DEPTH); root_depth++) {
		int elapsed = Timer.GetElapsedTime();
		if (elapsed) nps = nodes * 1000 / elapsed;
		printf("info depth %d time %d nodes %d nps %d\n", root_depth, elapsed, nodes, nps);
		cur_val = Search(p, 0, -INF, INF, root_depth, 0, pv);
		if (abort_search || Timer.FinishIteration()) break;
		val = cur_val;
	}
}

int Search(POS *p, int ply, int alpha, int beta, int depth, int was_null, int *pv)
{
  int best, score, move, new_depth, new_pv[MAX_PLY];
  int fl_check, mv_type, reduction;
  int is_pv = (beta > alpha + 1);
  int mv_tried = 0;
  int quiet_tried = 0;

  MOVES m[1];
  UNDO u[1];

  // Quiescence search entry point

  if (depth <= 0)
    return Quiesce(p, ply, alpha, beta, pv);

  // Periodically check for timeout, ponderhit or stop command

  nodes++;
  Check();

  // Quick exit on a timeout or on a statically detected draw
  
  if (abort_search) return 0;
  if (ply) *pv = 0;
  if (IsDraw(p) && ply) return 0;

  // Retrieving data from transposition table. We hope for a cutoff
  // or at least for a move to improve move ordering.

  move = 0;
  if (TransRetrieve(p->hash_key, &move, &score, alpha, beta, depth, ply)) {
	  
	  // For move ordering purposes, a cutoff from hash is treated
	  // exactly like a cutoff from search

	  if (score >= beta) UpdateHistory(p, move, depth, ply);

	  // In pv nodes only exact scores are returned. This is done because
	  // there is much more pruning and reductions in zero-window nodes,
	  // so retrieving such scores in pv nodes works like retrieving scores
	  // from slightly lower depth.

	  if (!is_pv || (score > alpha && score < beta))
		  return score;
  }
  
  // Safeguard against exceeding ply limit
  
  if (ply >= MAX_PLY - 1)
    return Evaluate(p);

  // Are we in check? Knowing that is useful when it comes 
  // to pruning/reduction decisions

  fl_check = InCheck(p); 

  // Null move

  if (depth > 1 
  && !was_null
  && !fl_check 
  && MayNull(p)) {
    int eval = Evaluate(p);
    if (eval > beta) {
	  reduction = 3;
      p->DoNull(u);
      score = -Search(p, ply + 1, -beta, -beta + 1, depth - reduction, 1, new_pv);
      p->UndoNull(u);
      if (abort_search) return 0;

      if (score >= beta)
        return score;
	  }
  }

  // None of the attempts at an early cutoff worked, we need a real search.

  best = -INF;
  InitMoves(p, m, move, ply);
  
  // Main loop
  
  while ((move = NextMove(m, &mv_type))) {
    p->DoMove(move, u);
    if (Illegal(p)) { p->UndoMove(move, u); continue; }

	// Update move statistics (needed for reduction/pruning decisions)

	mv_tried++;
	if (mv_type == MV_NORMAL) quiet_tried++;

	// Set new search depth

    new_depth = depth - 1 + InCheck(p);

	// Late move pruning

	if (!is_pv
	&& quiet_tried > 4 * depth
	&& !fl_check
	&& !InCheck(p)
	&& depth <= 3
	&& MoveType(move) != CASTLE
	&& mv_type == MV_NORMAL) {
		p->UndoMove(move, u); continue;
	}

	// Late move reduction

	reduction = 0;

	if (depth >= 2 
	&& mv_tried > 3 
	&& !fl_check 
	&& !InCheck(p) 
	&& MoveType(move) != CASTLE 
	&&  mv_type == MV_NORMAL) {
		reduction = 1;
		if (!is_pv && mv_tried > 6) reduction = 2;
		new_depth -= reduction;
	}

    re_search:

	// PVS

    if (best == -INF)
      score = -Search(p, ply + 1, -beta, -alpha, new_depth, 0, new_pv);
    else {
      score = -Search(p, ply + 1, -alpha - 1, -alpha, new_depth, 0, new_pv);
      if (!abort_search && score > alpha && score < beta)
        score = -Search(p, ply + 1, -beta, -alpha, new_depth, 0, new_pv);
    }

	// Reduced move scored above alpha - we need to re-search it

	if (reduction && score > alpha) {
		new_depth += reduction;
		reduction = 0;
		goto re_search;
	}

    p->UndoMove(move, u);
    if (abort_search) return 0;

	// Beta cutoff

    if (score >= beta) {
      UpdateHistory(p, move, depth, ply);
      TransStore(p->hash_key, move, score, LOWER, depth, ply);
      return score;
    }

	// Updating score and alpha

    if (score > best) {
      best = score;
      if (score > alpha) {
        alpha = score;
        BuildPv(pv, new_pv, move);
        if (!ply) DisplayPv(score, pv);
      }
    }

  } // end of the main loop

  // Return correct checkmate/stalemate score

  if (best == -INF)
    return InCheck(p) ? -MATE + ply : 0;

  // Save score in the transposition table

  if (*pv) {
    UpdateHistory(p, *pv, depth, ply);
    TransStore(p->hash_key, *pv, best, EXACT, depth, ply);
  } else
    TransStore(p->hash_key, 0, best, UPPER, depth, ply);

  return best;
}

int Quiesce(POS *p, int ply, int alpha, int beta, int *pv)
{
  int best, score, move, new_pv[MAX_PLY];
  MOVES m[1];
  UNDO u[1];

  // Statistics and attempt at quick exit

  nodes++;
  Check();
  if (abort_search) return 0;
  *pv = 0;
  if (IsDraw(p)) return 0;
  if (ply >= MAX_PLY - 1) return Evaluate(p);

  best = Evaluate(p); 
  if (best >= beta)
    return best;
  if (best > alpha)
    alpha = best;

  InitCaptures(p, m);
  while ((move = NextCapture(m))) {
    p->DoMove(move, u);
    if (Illegal(p)) { p->UndoMove(move, u); continue; }
    score = -Quiesce(p, ply + 1, -beta, -alpha, new_pv);
    p->UndoMove(move, u);
    if (abort_search) return 0;
    if (score >= beta)
      return score;
    if (score > best) {
      best = score;
      if (score > alpha) {
        alpha = score;
        BuildPv(pv, new_pv, move);
      }
    }
  }
  return best;
}

int IsDraw(POS *p)
{
	// Draw by 50 move rule

	if (p->rev_moves > 100) return 1;

	// Draw by repetition

	for (int i = 4; i <= p->rev_moves; i += 2)
		if (p->hash_key == p->rep_list[p->head - i])
			return 1;

	// Draw by insufficient material (bare kings or Km vs K)

	if (!Illegal(p)) {
		if (p->cnt[WC][P] + p->cnt[BC][P] + p->cnt[WC][Q] + p->cnt[BC][Q] + p->cnt[WC][R] + p->cnt[BC][R] == 0
		&&  p->cnt[WC][N] + p->cnt[BC][N] + p->cnt[WC][B] + p->cnt[BC][B] <= 1) return 0;
	}

	return 0; // default: no draw
}

void DisplayPv(int score, int *pv)
{
  char *type, pv_str[512];
  U64 nps = 0;
  int elapsed = Timer.GetElapsedTime();
  if (elapsed) nps = nodes * 1000 / elapsed;

  type = "mate";
  if (score < -MAX_EVAL)
    score = (-MATE - score) / 2;
  else if (score > MAX_EVAL)
    score = (MATE - score + 1) / 2;
  else
    type = "cp";
  PvToStr(pv, pv_str);
  printf("info depth %d time %d nodes %d nps %d score %s %d pv %s\n",
      root_depth, elapsed, nodes, nps, type, score, pv_str);
}

void Check(void)
{
  char command[80];

  if (nodes & 4095 || root_depth == 1)
    return;
  if (InputAvailable()) {
    ReadLine(command, sizeof(command));
    if (strcmp(command, "stop") == 0)
      abort_search = 1;
    else if (strcmp(command, "ponderhit") == 0)
      pondering = 0;
  }
  if (Timeout()) abort_search = 1;
}

int Timeout()
{
	return (!pondering && !Timer.IsInfiniteMode() && Timer.TimeHasElapsed());
}