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

/**
 * @brief Computes the inner product of two vectors.
 * @param v1 First vector.
 * @param v2 Second vector.
 * @return Inner product value.
 */
long long vctInnerProduct(Vector *v1, Vector *v2);

/**
 * @brief Computes the element-wise difference between two vectors.
 * @param v1 First vector.
 * @param v2 Second vector.
 * @return Difference vector.
 */
Vector vctGetElement(Vector *v1, Vector *v2);

/**
 * @brief Adds v2 to v1 in place.
 * @param v1 Destination vector.
 * @param v2 Source vector.
 */
void vctAdd(Vector *v1, Vector *v2);

/**
 * @brief Subtracts v2 from v1 in place.
 * @param v1 Destination vector.
 * @param v2 Source vector.
 */
void vctSub(Vector *v1, Vector *v2);

/**
 * @brief Multiplies v1 by a scalar in place.
 * @param v1 Vector to scale.
 * @param a  Scalar multiplier.
 */
void vctMul(Vector *v1, int a);

/**
 * @brief Divides v1 by a scalar in place.
 * @param v1 Vector to scale.
 * @param a  Scalar divisor.
 */
void vctDiv(Vector *v1, int a);

/**
 * @brief Tests whether a point lies on the same side of a line segment.
 * @param checkPos Point to test.
 * @param pos1     First segment endpoint.
 * @param pos2     Second segment endpoint.
 * @return Non-zero if the side test passes, zero otherwise.
 */
int vctCheckSide(Vector *checkPos, Vector *pos1, Vector *pos2);

/**
 * @brief Returns the magnitude of a vector.
 * @param v Vector to measure.
 * @return Vector length.
 */
int vctSize(Vector *v);

/**
 * @brief Returns the distance between two vectors.
 * @param v1 First vector.
 * @param v2 Second vector.
 * @return Distance between the vectors.
 */
int vctDist(Vector *v1, Vector *v2);

#endif
