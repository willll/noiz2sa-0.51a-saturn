/*
 * $Id: barragemanager.h,v 1.2 2003/08/10 04:09:46 kenta Exp $
 *
 * Copyright 2002 Kenta Cho. All rights reserved.
 */

/**
 * Barrage data.
 *
 * @version $Revision: 1.2 $
 */
#ifndef BARRAGEMANAGER_H_
#define BARRAGEMANAGER_H_

#include <srl.hpp>
#include "bulletml_binary/bulletmlparser_blb.hpp"

using SRL::Math::Types::Fxp;

#define BARRAGE_TYPE_NUM 3
#define BARRAGE_MAX 16

#define BOSS_TYPE 3

typedef struct {
  BulletMLParserBLB *bulletml;
  Fxp maxRank, rank;
  int type;
  int frq;
} Barrage;

#include "brgmng_mtd.h"
#endif
