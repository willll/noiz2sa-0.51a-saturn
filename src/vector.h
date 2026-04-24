/*
 * $Id: vector.h,v 1.1.1.1 2002/11/03 11:08:24 kenta Exp $
 *
 * Copyright 2002 Kenta Cho. All rights reserved.
 */

/**
 * Vector data (wrapped from SaturnMathPP Vector2D).
 *
 * @version $Revision: 1.1.1.1 $
 */
#ifndef DEF_VECTOR
#define DEF_VECTOR

#include <srl.hpp>

using SRL::Math::Types::Fxp;

#define absN(a) ((a) < 0 ? - (a) : (a))

/* Wrapper struct for Vector2D to maintain API compatibility */
typedef struct {
  int x, y;
} Vector;

long long vctInnerProduct(Vector *v1, Vector *v2);
Vector vctGetElement(Vector *v1, Vector *v2);
void vctAdd(Vector *v1, Vector *v2);
void vctSub(Vector *v1, Vector *v2);
void vctMul(Vector *v1, int a);
void vctDiv(Vector *v1, int a);
int vctCheckSide(Vector *checkPos, Vector *pos1, Vector *pos2);
int vctSize(Vector *v);
int vctDist(Vector *v1, Vector *v2);

#endif
