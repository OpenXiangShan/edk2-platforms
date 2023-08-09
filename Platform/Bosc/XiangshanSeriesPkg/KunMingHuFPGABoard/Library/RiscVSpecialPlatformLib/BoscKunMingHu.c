/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Western Digital Corporation or its affiliates.
 *
 * Authors:
 *   Anup Patel <anup.patel@wdc.com>

   Copyright (c) 2021, Hewlett Packard Enterprise Development LP. All rights reserved.<BR>

   SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#include <Library/RiscVSpecialPlatformLib.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi_utils/fdt/fdt_fixup.h>

static u64 bosc_kunminghu_tlbr_flush_limit(const struct fdt_match *match)
{
  return 0;
}

static int bosc_kunminghu_fdt_fixup(void *fdt, const struct fdt_match *match)
{
  fdt_reserved_memory_nomap_fixup(fdt);

  return 0;
}

static const struct fdt_match bosc_kunminghu_match[] = {
  { .compatible = "bosc,kunminghu" },
  { },
};

const struct platform_override bosc_kunminghu = {
  .match_table = bosc_kunminghu_match,
  .tlbr_flush_limit = bosc_kunminghu_tlbr_flush_limit,
  .fdt_fixup = bosc_kunminghu_fdt_fixup,
};

const struct platform_override *special_platforms[] = {
  &bosc_kunminghu,
};
INTN NumberOfPlaformsInArray = array_size(special_platforms);
