/*
 * $Id: degutil.h,v 1.1.1.1 2002/11/03 11:08:24 kenta Exp $
 *
 * Copyright 2002 Kenta Cho. All rights reserved.
 */

/**
 * Degutil header file.
 *
 * @version $Revision: 1.1.1.1 $
 */
#define DIV 1024
const int16_t TAN_TABLE_SIZE = 1024;
const int16_t SIN_TABLE_SIZE = 256;
#define SC_TABLE_SIZE 1024

extern int sctbl[];

/**
 * @brief Initialises the degree lookup tables.
 */
void initDegutil();

/**
 * @brief Returns a legacy direction index for the given vector.
 * @param x X component.
 * @param y Y component.
 * @return Direction index in the 0-1023 range.
 */
int getDeg(int x, int y);

/**
 * @brief Returns an integer distance between two coordinates.
 * @param x X component.
 * @param y Y component.
 * @return Distance value.
 */
int getDistance(int x, int y);
