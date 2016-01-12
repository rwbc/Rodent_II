#include "rodent.h"

int Quiesce(POS *p, int ply, int alpha, int beta, int *pv) {

  int best, score, move, new_pv[MAX_PLY];
  MOVES m[1];
  UNDO u[1];

  // Statistics and attempt at quick exit

  if (InCheck(p)) return QuiesceFlee(p, ply, alpha, beta, pv);

  nodes++;
  Check();
  if (abort_search) return 0;
  *pv = 0;
  if (IsDraw(p)) return DrawScore(p);
  if (ply >= MAX_PLY - 1) return Evaluate(p, 1);

  // Get a stand-pat score and adjust bounds
  // (exiting if eval exceeds beta)

  best = Evaluate(p, 1);
  if (best >= beta) return best;
  if (best > alpha) alpha = best;

  // Transposition table read

  if (TransRetrieve(p->hash_key, &move, &score, alpha, beta, 0, ply))
    return score;

  InitCaptures(p, m);

  // Main loop

  while ((move = NextCapture(m))) {

  // Delta pruning

  if (best + tp_value[TpOnSq(p, Tsq(move))] + 300 < alpha) continue;

  // Pruning of bad captures

  if (BadCapture(p, move)) continue;

    p->DoMove(move, u);
    if (Illegal(p)) { p->UndoMove(move, u); continue; }
    score = -Quiesce(p, ply + 1, -beta, -alpha, new_pv);
    p->UndoMove(move, u);
    if (abort_search) return 0;

  // Beta cutoff

  if (score >= beta) {
    TransStore(p->hash_key, *pv, best, LOWER, 0, ply);
    return score;
  }

  // Adjust alpha and score

    if (score > best) {
      best = score;
      if (score > alpha) {
        alpha = score;
        BuildPv(pv, new_pv, move);
      }
    }
  }

  if (*pv) TransStore(p->hash_key, *pv, best, EXACT, 0, ply);
  else      TransStore(p->hash_key,   0, best, UPPER, 0, ply);

  return best;
}

int QuiesceChecks(POS *p, int ply, int alpha, int beta, int *pv)
{
  int stand_pat, best, score, move, new_pv[MAX_PLY];
  int is_pv = (beta > alpha + 1);
  MOVES m[1];
  UNDO u[1];

  if (InCheck(p)) return QuiesceFlee(p, ply, alpha, beta, pv);

  nodes++;
  Check();
  if (abort_search) return 0;
  *pv = 0;

  if (IsDraw(p)) return DrawScore(p);

  if (ply >= MAX_PLY - 1)
    return Evaluate(p, 1);

  best = stand_pat = Evaluate(p, 1);

  if (best >= beta) return best;
  if (best > alpha) alpha = best;

  if (TransRetrieve(p->hash_key, &move, &score, alpha, beta, 0, ply))
     return score;

  InitCaptures(p, m);
  while ((move = NextCaptureOrCheck(m))) {

    p->DoMove(move, u);
    if (Illegal(p)) { p->UndoMove(move, u); continue; }

    /*  if (qdepth < checkDepth)
    score = -QuiesceChecks(p, ply + 1, qdepth+1, -beta, -alpha, new_pv);
    else*/
    score = -Quiesce(p, ply + 1, -beta, -alpha, new_pv);

    p->UndoMove(move, u);
    if (abort_search) return 0;

    if (score >= beta) {
      TransStore(p->hash_key, move, score, LOWER, 0, ply);
        return score;
    }

    if (score > best) {
      best = score;
      if (score > alpha) {
        alpha = score;
        BuildPv(pv, new_pv, move);
      }
    }
  }

  if (*pv) TransStore(p->hash_key, *pv, best, EXACT, 0, ply);
  else     TransStore(p->hash_key, 0, best, UPPER, 0, ply);

  return best;
}


int QuiesceFlee(POS *p, int ply, int alpha, int beta, int *pv) {

  int best, score, move, new_pv[MAX_PLY];
  int fl_check, mv_type;
  int is_pv = (beta > alpha + 1);

  MOVES m[1];
  UNDO u[1];

  // Periodically check for timeout, ponderhit or stop command

  nodes++;
  Check();

  // Quick exit on a timeout or on a statically detected draw

  if (abort_search) return 0;
  if (ply) *pv = 0;
  if (IsDraw(p) ) return DrawScore(p);

  // Retrieving data from transposition table. We hope for a cutoff
  // or at least for a move to improve move ordering.

  move = 0;
  if (TransRetrieve(p->hash_key, &move, &score, alpha, beta, 0, ply))
    return score;

  // Safeguard against exceeding ply limit

  if (ply >= MAX_PLY - 1)
    return Evaluate(p, 1);

  // Are we in check? Knowing that is useful when it comes 
  // to pruning/reduction decisions

  fl_check = InCheck(p);

  // Init moves and variables before entering main loop

  best = -INF;
  InitMoves(p, m, move, -1, ply);

  // Main loop

  while ((move = NextMove(m, &mv_type))) {
    p->DoMove(move, u);
    if (Illegal(p)) { p->UndoMove(move, u); continue; }
    
    score = -Quiesce(p, ply, -beta, -alpha, new_pv);

    
    p->UndoMove(move, u);
    if (abort_search) return 0;

    // Beta cutoff

    if (score >= beta) {
      TransStore(p->hash_key, move, score, LOWER, 0, ply);
      return score;
    }

    // Updating score and alpha

    if (score > best) {
      best = score;
      if (score > alpha) {
        alpha = score;
        BuildPv(pv, new_pv, move);
      }
    }

  } // end of the main loop

  // Return correct checkmate/stalemate score

  if (best == -INF)
    return InCheck(p) ? -MATE + ply : DrawScore(p);

  // Save score in the transposition table

  if (*pv) TransStore(p->hash_key, *pv, best, EXACT, 0, ply);
  else     TransStore(p->hash_key, 0, best, UPPER, 0, ply);

  return best;
}
