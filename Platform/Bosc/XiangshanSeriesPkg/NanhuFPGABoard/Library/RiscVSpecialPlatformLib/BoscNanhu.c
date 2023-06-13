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

static u64 bosc_nanhu_tlbr_flush_limit(const struct fdt_match *match)
{
  return 0;
}

static int bosc_nanhu_fdt_fixup(void *fdt, const struct fdt_match *match)
{
  fdt_reserved_memory_nomap_fixup(fdt);

  return 0;
}

static const struct fdt_match bosc_nanhu_match[] = {
  { .compatible = "bosc,nanhu" },
  { },
};

const struct platform_override bosc_nanhu = {
  .match_table = bosc_nanhu_match,
  .tlbr_flush_limit = bosc_nanhu_tlbr_flush_limit,
  .fdt_fixup = bosc_nanhu_fdt_fixup,
};

const struct platform_override *special_platforms[] = {
  &bosc_nanhu,
};
INTN NumberOfPlaformsInArray = array_size(special_platforms);
