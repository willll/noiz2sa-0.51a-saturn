/*
 * $Id: vector.c,v 1.1.1.1 2002/11/03 11:08:24 kenta Exp $
 *
 * Copyright 2002 Kenta Cho. All rights reserved.
 */

/**
 * Vector functions (wrapper for SaturnMathPP Vector2D).
 *
 * @version $Revision: 1.1.1.1 $
 */
#include "vector.h"

static int intSqrtU64(unsigned long long value)
{
  unsigned long long op = value;
  unsigned long long res = 0;
  unsigned long long one = 1ull << 62;

  while (one > op)
  {
    one >>= 2;
  }

  while (one != 0)
  {
    if (op >= res + one)
    {
      op -= res + one;
      res = (res >> 1) + one;
    }
    else
    {
      res >>= 1;
    }
    one >>= 2;
  }

  return (int)res;
}

long long vctInnerProduct(Vector *v1, Vector *v2) {
  const long long x = (long long)v1->x * (long long)v2->x;
  const long long y = (long long)v1->y * (long long)v2->y;
  return x + y;
}

Vector vctGetElement(Vector *v1, Vector *v2) {
  Vector ans;

  const long long ll = (long long)v2->x * (long long)v2->x +
                       (long long)v2->y * (long long)v2->y;
  if (ll == 0)
  {
    ans.x = ans.y = 0;
  }
  else
  {
    const long long mag = (long long)v1->x * (long long)v2->x +
                          (long long)v1->y * (long long)v2->y;
    ans.x = (int)((mag * (long long)v2->x) / ll);
    ans.y = (int)((mag * (long long)v2->y) / ll);
  }

  return ans;
}

void vctAdd(Vector *v1, Vector *v2) {
  v1->x += v2->x;
  v1->y += v2->y;
}

void vctSub(Vector *v1, Vector *v2) {
  v1->x -= v2->x;
  v1->y -= v2->y;
}

void vctMul(Vector *v1, int a) {
  v1->x *= a;
  v1->y *= a;
}

void vctDiv(Vector *v1, int a) {
  v1->x /= a;
  v1->y /= a;
}

int vctCheckSide(Vector *checkPos, Vector *pos1, Vector *pos2) {
  int xo = pos2->x - pos1->x, yo = pos2->y - pos1->y;
  if ( xo == 0 ) {
    if ( yo == 0 ) return 0;
    return checkPos->x - pos1->x;
  } else if ( yo == 0 ) {
    return pos1->y - checkPos->y;
  } else {
    if ( xo * yo > 0 ) { 
      return (checkPos->x - pos1->x) / xo - (checkPos->y - pos1->y) / yo;
    } else {
      return -(checkPos->x - pos1->x) / xo + (checkPos->y - pos1->y) / yo;
    }
  }
}

int vctSize(Vector *v) {
  const long long xx = (long long)v->x * (long long)v->x;
  const long long yy = (long long)v->y * (long long)v->y;
  return intSqrtU64((unsigned long long)(xx + yy));
}

int vctDist(Vector *v1, Vector *v2) {
  int ax = absN(v1->x - v2->x), ay = absN(v1->y - v2->y);
  if ( ax > ay ) {
    return ax + (ay >> 1);
  } else {
    return ay + (ax >> 1);
  }
}
