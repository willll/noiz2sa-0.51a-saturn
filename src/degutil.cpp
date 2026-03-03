/*
 * $Id: degutil.c,v 1.1.1.1 2002/11/03 11:08:24 kenta Exp $
 *
 * Copyright 2002 Kenta Cho. All rights reserved.
 */

/**
 * Changing the cordinate into the angle.
 *
 * @version $Revision: 1.1.1.1 $
 */
#include <impl/trigonometry.hpp>
#include "degutil.h"

using SaturnMath::Trigonometry;
using SaturnMath::Types::Angle;
using SaturnMath::Types::Fxp;

static int tantbl[TAN_TABLE_SIZE+2];
int sctbl[SC_TABLE_SIZE+SC_TABLE_SIZE/4];

void initDegutil() {
  int i, d = 0;
  const Fxp tanScale = Fxp::Convert(TAN_TABLE_SIZE);
  const Fxp sinScale = Fxp::Convert(256);

  for ( i=0 ; i<TAN_TABLE_SIZE ; i++ ) {
    while ( d < (DIV/8) ) {
      const Angle angle = Angle::BuildRaw(static_cast<uint16_t>(d << 6));
      const int tanScaled = (Trigonometry::Tan(angle) * tanScale).As<int32_t>();
      if ( tanScaled >= i ) {
        break;
      }
      d++;
    }
    tantbl[i] = d;
  }
  tantbl[TAN_TABLE_SIZE] = tantbl[TAN_TABLE_SIZE+1] = 128;

  for ( i=0 ; i<SC_TABLE_SIZE+SC_TABLE_SIZE/4 ; i++ ) {
    const Angle angle = Angle::BuildRaw(static_cast<uint16_t>(i << 6));
    sctbl[i] = (Trigonometry::Sin(angle) * sinScale).As<int32_t>();
  }
}

int getDeg(int x, int y) {
  int tx, ty;
  int f, od, tn;

  if ( x==0 && y==0 ) {
    return(0);
  }

  if ( x < 0 ) {
    tx = -x;
    if ( y < 0 ) {
      ty = -y;
      if ( tx > ty ) {
	f = 1;
	od = DIV*3/4; tn = ty*TAN_TABLE_SIZE/tx;
      } else {
	f = -1;
	od = DIV; tn = tx*TAN_TABLE_SIZE/ty;
      }
    } else {
      ty = y;
      if ( tx > ty ) {
	f = -1;
	od = DIV*3/4; tn=ty*TAN_TABLE_SIZE/tx;
      } else {
	f=1;
	od = DIV/2; tn=tx*TAN_TABLE_SIZE/ty;
      }
    }
  } else {
    tx = x;
    if ( y < 0 ) {
      ty = -y;
      if ( tx > ty ) {
	f = -1;
	od = DIV/4; tn = ty*TAN_TABLE_SIZE/tx;
      } else {
	f = 1;
	od = 0; tn = tx*TAN_TABLE_SIZE/ty;
      }
    } else {
      ty = y;
      if ( tx > ty ) {
	f = 1;
	od = DIV/4; tn = ty*TAN_TABLE_SIZE/tx;
      } else {
	f = -1;
	od = DIV/2; tn = tx*TAN_TABLE_SIZE/ty;
      }
    }
  }
  return((od+tantbl[tn]*f)&(DIV-1));
}

int getDistance(int x, int y) {
  if ( x < 0 ) x = -x;
  if ( y < 0 ) y = -y;
  if ( x > y ) {
    return x + (y>>1);
  } else {
    return y + (x>>1);
  }
}
