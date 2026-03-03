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
#include <impl/vector2d.hpp>

using SaturnMath::Types::Vector2D;
using SaturnMath::Types::Fxp;

/* Helper: convert int to Fxp */
static inline Fxp intToFxp(int val) {
  return Fxp::Convert(val);
}

/* Helper: convert Vector to Vector2D */
static inline Vector2D vecToVec2D(const Vector *v) {
  return Vector2D(intToFxp(v->x), intToFxp(v->y));
}

/* Helper: convert Vector2D to Vector */
static inline Vector vec2DToVec(const Vector2D &v) {
  return {v.X.template As<int>(), v.Y.template As<int>()};
}

float vctInnerProduct(Vector *v1, Vector *v2) {
  Vector2D v1_f = vecToVec2D(v1);
  Vector2D v2_f = vecToVec2D(v2);
  return v1_f.Dot(v2_f).template As<float>();
}

Vector vctGetElement(Vector *v1, Vector *v2) {
  Vector ans;
  Vector2D v1_f = vecToVec2D(v1);
  Vector2D v2_f = vecToVec2D(v2);
  
  Fxp ll = v2_f.X * v2_f.X + v2_f.Y * v2_f.Y;
  if ( ll == 0 ) {
    ans.x = ans.y = 0;
  } else {
    Fxp mag = v1_f.Dot(v2_f);
    Vector2D proj = v2_f * (mag / ll);
    ans = vec2DToVec(proj);
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
  Vector2D v_f = vecToVec2D(v);
  return v_f.Length().template As<int>();
}

int vctDist(Vector *v1, Vector *v2) {
  int ax = absN(v1->x - v2->x), ay = absN(v1->y - v2->y);
  if ( ax > ay ) {
    return ax + (ay >> 1);
  } else {
    return ay + (ax >> 1);
  }
}
