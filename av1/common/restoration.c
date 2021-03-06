/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 *
 */

#include <math.h>

#include "./aom_config.h"
#include "./aom_dsp_rtcd.h"
#include "./aom_scale_rtcd.h"
#include "aom_mem/aom_mem.h"
#include "av1/common/onyxc_int.h"
#if CONFIG_HORZONLY_FRAME_SUPERRES
#include "av1/common/resize.h"
#endif
#include "av1/common/restoration.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_mem/aom_mem.h"

#include "aom_ports/mem.h"

const sgr_params_type sgr_params[SGRPROJ_PARAMS] = {
// r1, eps1, r2, eps2
#if MAX_RADIUS == 2
  { 2, 12, 1, 4 },  { 2, 15, 1, 6 },  { 2, 18, 1, 8 },  { 2, 20, 1, 9 },
  { 2, 22, 1, 10 }, { 2, 25, 1, 11 }, { 2, 35, 1, 12 }, { 2, 45, 1, 13 },
  { 2, 55, 1, 14 }, { 2, 65, 1, 15 }, { 2, 75, 1, 16 }, { 2, 30, 1, 6 },
  { 2, 50, 1, 12 }, { 2, 60, 1, 13 }, { 2, 70, 1, 14 }, { 2, 80, 1, 15 },
#else
  { 2, 12, 1, 4 },  { 2, 15, 1, 6 },  { 2, 18, 1, 8 },  { 2, 20, 1, 9 },
  { 2, 22, 1, 10 }, { 2, 25, 1, 11 }, { 2, 35, 1, 12 }, { 2, 45, 1, 13 },
  { 2, 55, 1, 14 }, { 2, 65, 1, 15 }, { 2, 75, 1, 16 }, { 3, 30, 1, 10 },
  { 3, 50, 1, 12 }, { 3, 50, 2, 25 }, { 3, 60, 2, 35 }, { 3, 70, 2, 45 },
#endif  // MAX_RADIUS == 2
};

// Similar to av1_get_tile_rect(), except that we extend the bottommost tile in
// each frame to a multiple of 8 luma pixels.
// This is done to help simplify the implementation of striped-loop-restoration,
// by avoiding nasty edge cases which would otherwise appear when the (cropped)
// frame height is 57 or 63 (mod 64).
static AV1PixelRect get_ext_tile_rect(const TileInfo *tile_info,
                                      const AV1_COMMON *cm, int is_uv) {
  int ss_y = is_uv && cm->subsampling_y;
  AV1PixelRect tile_rect = av1_get_tile_rect(tile_info, cm, is_uv);
  tile_rect.bottom = ALIGN_POWER_OF_TWO(tile_rect.bottom, 3 - ss_y);
  return tile_rect;
}

// Count horizontal or vertical units per tile (use a width or height for
// tile_size, respectively). We basically want to divide the tile size by the
// size of a restoration unit. Rather than rounding up unconditionally as you
// might expect, we round to nearest, which models the way a right or bottom
// restoration unit can extend to up to 150% its normal width or height. The
// max with 1 is to deal with tiles that are smaller than half of a restoration
// unit.
static int count_units_in_tile(int unit_size, int tile_size) {
  return AOMMAX((tile_size + (unit_size >> 1)) / unit_size, 1);
}

void av1_alloc_restoration_struct(AV1_COMMON *cm, RestorationInfo *rsi,
                                  int is_uv) {
#if CONFIG_MAX_TILE
  // We need to allocate enough space for restoration units to cover the
  // largest tile. Without CONFIG_MAX_TILE, this is always the tile at the
  // top-left and we can use get_ext_tile_rect(). With CONFIG_MAX_TILE, we have
  // to do the computation ourselves, iterating over the tiles and keeping
  // track of the largest width and height, then upscaling.
  TileInfo tile;
  int max_mi_w = 0;
  int max_mi_h = 0;
  int tile_col = 0;
  int tile_row = 0;
  for (int i = 0; i < cm->tile_cols; ++i) {
    av1_tile_set_col(&tile, cm, i);
    if (tile.mi_col_end - tile.mi_col_start > max_mi_w) {
      max_mi_w = tile.mi_col_end - tile.mi_col_start;
      tile_col = i;
    }
  }
  for (int i = 0; i < cm->tile_rows; ++i) {
    av1_tile_set_row(&tile, cm, i);
    if (tile.mi_row_end - tile.mi_row_start > max_mi_h) {
      max_mi_h = tile.mi_row_end - tile.mi_row_start;
      tile_row = i;
    }
  }
  TileInfo tile_info;
  av1_tile_init(&tile_info, cm, tile_row, tile_col);
#else
  TileInfo tile_info;
  av1_tile_init(&tile_info, cm, 0, 0);
#endif  // CONFIG_MAX_TILE

  const AV1PixelRect tile_rect = get_ext_tile_rect(&tile_info, cm, is_uv);
  const int max_tile_w = tile_rect.right - tile_rect.left;
  const int max_tile_h = tile_rect.bottom - tile_rect.top;

  // To calculate hpertile and vpertile (horizontal and vertical units per
  // tile), we basically want to divide the largest tile width or height by the
  // size of a restoration unit. Rather than rounding up unconditionally as you
  // might expect, we round to nearest, which models the way a right or bottom
  // restoration unit can extend to up to 150% its normal width or height. The
  // max with 1 is to deal with tiles that are smaller than half of a
  // restoration unit.
  const int unit_size = rsi->restoration_unit_size;
  const int hpertile = count_units_in_tile(unit_size, max_tile_w);
  const int vpertile = count_units_in_tile(unit_size, max_tile_h);

  rsi->units_per_tile = hpertile * vpertile;
  rsi->horz_units_per_tile = hpertile;
  rsi->vert_units_per_tile = vpertile;

  const int ntiles = cm->tile_rows * cm->tile_cols;
  const int nunits = ntiles * rsi->units_per_tile;

  aom_free(rsi->unit_info);
  CHECK_MEM_ERROR(cm, rsi->unit_info,
                  (RestorationUnitInfo *)aom_memalign(
                      16, sizeof(*rsi->unit_info) * nunits));
}

void av1_free_restoration_struct(RestorationInfo *rst_info) {
  aom_free(rst_info->unit_info);
  rst_info->unit_info = NULL;
}

// TODO(debargha): This table can be substantially reduced since only a few
// values are actually used.
int sgrproj_mtable[MAX_EPS][MAX_NELEM];

static void GenSgrprojVtable() {
  int e, n;
  for (e = 1; e <= MAX_EPS; ++e)
    for (n = 1; n <= MAX_NELEM; ++n) {
      const int n2e = n * n * e;
      sgrproj_mtable[e - 1][n - 1] =
          (((1 << SGRPROJ_MTABLE_BITS) + n2e / 2) / n2e);
    }
}

void av1_loop_restoration_precal() { GenSgrprojVtable(); }

static void extend_frame_lowbd(uint8_t *data, int width, int height, int stride,
                               int border_horz, int border_vert) {
  uint8_t *data_p;
  int i;
  for (i = 0; i < height; ++i) {
    data_p = data + i * stride;
    memset(data_p - border_horz, data_p[0], border_horz);
    memset(data_p + width, data_p[width - 1], border_horz);
  }
  data_p = data - border_horz;
  for (i = -border_vert; i < 0; ++i) {
    memcpy(data_p + i * stride, data_p, width + 2 * border_horz);
  }
  for (i = height; i < height + border_vert; ++i) {
    memcpy(data_p + i * stride, data_p + (height - 1) * stride,
           width + 2 * border_horz);
  }
}

#if CONFIG_HIGHBITDEPTH
static void extend_frame_highbd(uint16_t *data, int width, int height,
                                int stride, int border_horz, int border_vert) {
  uint16_t *data_p;
  int i, j;
  for (i = 0; i < height; ++i) {
    data_p = data + i * stride;
    for (j = -border_horz; j < 0; ++j) data_p[j] = data_p[0];
    for (j = width; j < width + border_horz; ++j) data_p[j] = data_p[width - 1];
  }
  data_p = data - border_horz;
  for (i = -border_vert; i < 0; ++i) {
    memcpy(data_p + i * stride, data_p,
           (width + 2 * border_horz) * sizeof(uint16_t));
  }
  for (i = height; i < height + border_vert; ++i) {
    memcpy(data_p + i * stride, data_p + (height - 1) * stride,
           (width + 2 * border_horz) * sizeof(uint16_t));
  }
}
#endif

void extend_frame(uint8_t *data, int width, int height, int stride,
                  int border_horz, int border_vert, int highbd) {
#if !CONFIG_HIGHBITDEPTH
  assert(highbd == 0);
  (void)highbd;
#else
  if (highbd)
    extend_frame_highbd(CONVERT_TO_SHORTPTR(data), width, height, stride,
                        border_horz, border_vert);
  else
#endif
  extend_frame_lowbd(data, width, height, stride, border_horz, border_vert);
}

static void copy_tile_lowbd(int width, int height, const uint8_t *src,
                            int src_stride, uint8_t *dst, int dst_stride) {
  for (int i = 0; i < height; ++i)
    memcpy(dst + i * dst_stride, src + i * src_stride, width);
}

#if CONFIG_HIGHBITDEPTH
static void copy_tile_highbd(int width, int height, const uint16_t *src,
                             int src_stride, uint16_t *dst, int dst_stride) {
  for (int i = 0; i < height; ++i)
    memcpy(dst + i * dst_stride, src + i * src_stride, width * sizeof(*dst));
}
#endif

static void copy_tile(int width, int height, const uint8_t *src, int src_stride,
                      uint8_t *dst, int dst_stride, int highbd) {
#if !CONFIG_HIGHBITDEPTH
  assert(highbd == 0);
  (void)highbd;
#else
  if (highbd)
    copy_tile_highbd(width, height, CONVERT_TO_SHORTPTR(src), src_stride,
                     CONVERT_TO_SHORTPTR(dst), dst_stride);
  else
#endif
  copy_tile_lowbd(width, height, src, src_stride, dst, dst_stride);
}

#if CONFIG_STRIPED_LOOP_RESTORATION
#define REAL_PTR(hbd, d) ((hbd) ? (uint8_t *)CONVERT_TO_SHORTPTR(d) : (d))

// Helper function: Save one column of left/right context to the appropriate
// column buffers, then extend the edge of the current tile into that column.
//
// Note: The code to deal with above/below boundaries may have filled out
// the corners of the border with data from the tiles to our left or right,
// which isn't allowed. To fix that up, we need to include the top and
// bottom context regions in the area which we extend.
// But note that we don't need to store the pixels we overwrite in the
// corners of the context area - those have already been overwritten once,
// so their original values are already in rlbs->tmp_save_{above,below}.
#if CONFIG_LOOPFILTERING_ACROSS_TILES
static void setup_boundary_column(const uint8_t *src8, int src_stride,
                                  uint8_t *dst8, int dst_stride, uint16_t *buf,
                                  int h, int use_highbd) {
  if (use_highbd) {
    const uint16_t *src16 = CONVERT_TO_SHORTPTR(src8);
    uint16_t *dst16 = CONVERT_TO_SHORTPTR(dst8);
    for (int i = -RESTORATION_BORDER; i < 0; i++)
      dst16[i * dst_stride] = src16[i * src_stride];
    for (int i = 0; i < h; i++) {
      buf[i] = dst16[i * dst_stride];
      dst16[i * dst_stride] = src16[i * src_stride];
    }
    for (int i = h; i < h + RESTORATION_BORDER; i++)
      dst16[i * dst_stride] = src16[i * src_stride];
  } else {
    for (int i = -RESTORATION_BORDER; i < 0; i++)
      dst8[i * dst_stride] = src8[i * src_stride];
    for (int i = 0; i < h; i++) {
      buf[i] = dst8[i * dst_stride];
      dst8[i * dst_stride] = src8[i * src_stride];
    }
    for (int i = h; i < h + RESTORATION_BORDER; i++)
      dst8[i * dst_stride] = src8[i * src_stride];
  }
}

static void restore_boundary_column(uint8_t *dst8, int dst_stride,
                                    const uint16_t *buf, int h,
                                    int use_highbd) {
  if (use_highbd) {
    uint16_t *dst16 = CONVERT_TO_SHORTPTR(dst8);
    for (int i = 0; i < h; i++) dst16[i * dst_stride] = buf[i];
  } else {
    for (int i = 0; i < h; i++) dst8[i * dst_stride] = (uint8_t)(buf[i]);
  }
}
#endif  // CONFIG_LOOPFILTERING_ACROSS_TILES

// With striped loop restoration, the filtering for each 64-pixel stripe gets
// most of its input from the output of CDEF (stored in data8), but we need to
// fill out a border of 3 pixels above/below the stripe according to the
// following
// rules:
//
// * At a frame boundary, we copy the outermost row of CDEF pixels three times.
//   This extension is done by a call to extend_frame() at the start of the loop
//   restoration process, so the value of copy_above/copy_below doesn't strictly
//   matter.
//   However, by setting *copy_above = *copy_below = 1 whenever loop filtering
//   across tiles is disabled, we can allow
//   {setup,restore}_processing_stripe_boundary to assume that the top/bottom
//   data has always been copied, simplifying the behaviour at the left and
//   right edges of tiles.
//
// * If we're at a tile boundary and loop filtering across tiles is enabled,
//   then there is a logical stripe which is 64 pixels high, but which is split
//   into an 8px high and a 56px high stripe so that the processing (and
//   coefficient set usage) can be aligned to tiles.
//   In this case, we use the 3 rows of CDEF output across the boundary for
//   context; this corresponds to leaving the frame buffer as-is.
//
// * If we're at a tile boundary and loop filtering across tiles is disabled,
//   then we take the outermost row of CDEF pixels *within the current tile*
//   and copy it three times. Thus we behave exactly as if the tile were a full
//   frame.
//
// * Otherwise, we're at a stripe boundary within a tile. In that case, we
//   take 2 rows of deblocked pixels and extend them to 3 rows of context.
//
// The distinction between the latter two cases is handled by the
// av1_loop_restoration_save_boundary_lines() function, so here we just need
// to decide if we're overwriting the above/below boundary pixels or not.
static void get_stripe_boundary_info(const RestorationTileLimits *limits,
                                     const AV1PixelRect *tile_rect, int ss_y,
#if CONFIG_LOOPFILTERING_ACROSS_TILES
                                     int loop_filter_across_tiles_enabled,
#endif  // CONFIG_LOOPFILTERING_ACROSS_TILES
                                     int *copy_above, int *copy_below) {
  *copy_above = 1;
  *copy_below = 1;

#if CONFIG_LOOPFILTERING_ACROSS_TILES
  if (loop_filter_across_tiles_enabled) {
#endif
    const int full_stripe_height = RESTORATION_PROC_UNIT_SIZE >> ss_y;
    const int rtile_offset = RESTORATION_TILE_OFFSET >> ss_y;

    const int first_stripe_in_tile = (limits->v_start == tile_rect->top);
    const int this_stripe_height =
        full_stripe_height - (first_stripe_in_tile ? rtile_offset : 0);
    const int last_stripe_in_tile =
        (limits->v_start + this_stripe_height >= tile_rect->bottom);

    if (first_stripe_in_tile) *copy_above = 0;
    if (last_stripe_in_tile) *copy_below = 0;
#if CONFIG_LOOPFILTERING_ACROSS_TILES
  }
#endif
}

// Overwrite the border pixels around a processing stripe so that the conditions
// listed above get_stripe_boundary_info() are preserved.
// We save the pixels which get overwritten into a temporary buffer, so that
// they can be restored by restore_processing_stripe_boundary() after we've
// processed the stripe.
//
// limits gives the rectangular limits of the remaining stripes for the current
// restoration unit. rsb is the stored stripe boundaries (taken from either
// deblock or CDEF output as necessary).
//
// tile_rect is the limits of the current tile and tile_stripe0 is the index of
// the first stripe in this tile (needed to convert the tile-relative stripe
// index we get from limits into something we can look up in rsb).
static void setup_processing_stripe_boundary(
    const RestorationTileLimits *limits, const RestorationStripeBoundaries *rsb,
    int rsb_row, int use_highbd, int h,
#if CONFIG_LOOPFILTERING_ACROSS_TILES
    const AV1PixelRect *tile_rect, int loop_filter_across_tiles_enabled,
#endif  // CONFIG_LOOPFILTERING_ACROSS_TILES
    uint8_t *data8, int data_stride, RestorationLineBuffers *rlbs,
    int copy_above, int copy_below) {
  assert(CONFIG_HIGHBITDEPTH || !use_highbd);

  // Offsets within the line buffers. The buffer logically starts at column
  // -RESTORATION_EXTRA_HORZ so the 1st column (at x0 - RESTORATION_EXTRA_HORZ)
  // has column x0 in the buffer.
  const int buf_stride = rsb->stripe_boundary_stride;
  const int buf_x0_off = limits->h_start;
  const int line_width =
      (limits->h_end - limits->h_start) + 2 * RESTORATION_EXTRA_HORZ;
  const int line_size = line_width << use_highbd;

  const int data_x0 = limits->h_start - RESTORATION_EXTRA_HORZ;

  // Replace RESTORATION_BORDER pixels above the top of the stripe
  // We expand RESTORATION_CTX_VERT=2 lines from rsb->stripe_boundary_above
  // to fill RESTORATION_BORDER=3 lines of above pixels. This is done by
  // duplicating the topmost of the 2 lines (see the AOMMAX call when
  // calculating src_row, which gets the values 0, 0, 1 for i = -3, -2, -1).
  //
  // Special case: If we're at the top of a tile, which isn't on the topmost
  // tile row, and we're allowed to loop filter across tiles, then we have a
  // logical 64-pixel-high stripe which has been split into an 8-pixel high
  // stripe and a 56-pixel high stripe (the current one). So, in this case,
  // we want to leave the boundary alone!
  if (copy_above) {
    uint8_t *data8_tl = data8 + data_x0 + limits->v_start * data_stride;

    for (int i = -RESTORATION_BORDER; i < 0; ++i) {
      const int buf_row = rsb_row + AOMMAX(i + RESTORATION_CTX_VERT, 0);
      const int buf_off = buf_x0_off + buf_row * buf_stride;
      const uint8_t *buf = rsb->stripe_boundary_above + (buf_off << use_highbd);
      uint8_t *dst8 = data8_tl + i * data_stride;
      // Save old pixels, then replace with data from stripe_boundary_above
      memcpy(rlbs->tmp_save_above[i + RESTORATION_BORDER],
             REAL_PTR(use_highbd, dst8), line_size);
      memcpy(REAL_PTR(use_highbd, dst8), buf, line_size);
    }
  }

  // Replace RESTORATION_BORDER pixels below the bottom of the stripe.
  // The second buffer row is repeated, so src_row gets the values 0, 1, 1
  // for i = 0, 1, 2.
  if (copy_below) {
    const int stripe_end = limits->v_start + h;
    uint8_t *data8_bl = data8 + data_x0 + stripe_end * data_stride;

    for (int i = 0; i < RESTORATION_BORDER; ++i) {
      const int buf_row = rsb_row + AOMMIN(i, RESTORATION_CTX_VERT - 1);
      const int buf_off = buf_x0_off + buf_row * buf_stride;
      const uint8_t *src = rsb->stripe_boundary_below + (buf_off << use_highbd);

      uint8_t *dst8 = data8_bl + i * data_stride;
      // Save old pixels, then replace with data from stripe_boundary_below
      memcpy(rlbs->tmp_save_below[i], REAL_PTR(use_highbd, dst8), line_size);
      memcpy(REAL_PTR(use_highbd, dst8), src, line_size);
    }
  }

#if CONFIG_LOOPFILTERING_ACROSS_TILES
  if (!loop_filter_across_tiles_enabled) {
    // If loopfiltering across tiles is disabled, we need to check if we're at
    // the edge of the current tile column. If we are, we need to extend the
    // leftmost/rightmost column within the tile by 3 pixels, so that the output
    // doesn't depend on pixels from the next column over.
    // This applies to the top and bottom borders too, since those may have
    // been filled out with data from the tile to the top-left (etc.) of us.
    const int at_tile_left_border = (limits->h_start == tile_rect->left);
    const int at_tile_right_border = (limits->h_end == tile_rect->right);

    if (at_tile_left_border) {
      uint8_t *dst8 = data8 + limits->h_start + limits->v_start * data_stride;
      for (int j = -RESTORATION_BORDER; j < 0; j++)
        setup_boundary_column(dst8, data_stride, dst8 + j, data_stride,
                              rlbs->tmp_save_left[j + RESTORATION_BORDER], h,
                              use_highbd);
    }

    if (at_tile_right_border) {
      uint8_t *dst8 = data8 + limits->h_end + limits->v_start * data_stride;
      for (int j = 0; j < RESTORATION_BORDER; j++)
        setup_boundary_column(dst8 - 1, data_stride, dst8 + j, data_stride,
                              rlbs->tmp_save_right[j], h, use_highbd);
    }
  }
#endif  // CONFIG_LOOPFILTERING_ACROSS_TILES
}

// This function restores the boundary lines modified by
// setup_processing_stripe_boundary.
static void restore_processing_stripe_boundary(
    const RestorationTileLimits *limits, const RestorationLineBuffers *rlbs,
    int use_highbd, int h,
#if CONFIG_LOOPFILTERING_ACROSS_TILES
    const AV1PixelRect *tile_rect, int loop_filter_across_tiles_enabled,
#endif  // CONFIG_LOOPFILTERING_ACROSS_TILES
    uint8_t *data8, int data_stride, int copy_above, int copy_below) {
  assert(CONFIG_HIGHBITDEPTH || !use_highbd);

  const int line_width =
      (limits->h_end - limits->h_start) + 2 * RESTORATION_EXTRA_HORZ;
  const int line_size = line_width << use_highbd;

  const int data_x0 = limits->h_start - RESTORATION_EXTRA_HORZ;

  if (copy_above) {
    uint8_t *data8_tl = data8 + data_x0 + limits->v_start * data_stride;
    for (int i = -RESTORATION_BORDER; i < 0; ++i) {
      uint8_t *dst8 = data8_tl + i * data_stride;
      memcpy(REAL_PTR(use_highbd, dst8),
             rlbs->tmp_save_above[i + RESTORATION_BORDER], line_size);
    }
  }

  if (copy_below) {
    const int stripe_bottom = limits->v_start + h;
    uint8_t *data8_bl = data8 + data_x0 + stripe_bottom * data_stride;

    for (int i = 0; i < RESTORATION_BORDER; ++i) {
      if (stripe_bottom + i >= limits->v_end + RESTORATION_BORDER) break;

      uint8_t *dst8 = data8_bl + i * data_stride;
      memcpy(REAL_PTR(use_highbd, dst8), rlbs->tmp_save_below[i], line_size);
    }
  }

#if CONFIG_LOOPFILTERING_ACROSS_TILES
  if (!loop_filter_across_tiles_enabled) {
    // Restore any pixels we overwrote at the left/right edge of this
    // processing unit
    // Note: We don't need to restore the corner pixels, even if we overwrote
    // them in the equivalent place in setup_processing_stripe_boundary:
    // Because !loop_filter_across_tiles_enabled => copy_above = copy_below = 1,
    // the corner pixels will already have been restored before we get here.
    const int at_tile_left_border = (limits->h_start == tile_rect->left);
    const int at_tile_right_border = (limits->h_end == tile_rect->right);

    if (at_tile_left_border) {
      uint8_t *dst8 = data8 + limits->h_start + limits->v_start * data_stride;
      for (int j = -RESTORATION_BORDER; j < 0; j++)
        restore_boundary_column(dst8 + j, data_stride,
                                rlbs->tmp_save_left[j + RESTORATION_BORDER], h,
                                use_highbd);
    }

    if (at_tile_right_border) {
      uint8_t *dst8 = data8 + limits->h_end + limits->v_start * data_stride;
      for (int j = 0; j < RESTORATION_BORDER; j++)
        restore_boundary_column(dst8 + j, data_stride, rlbs->tmp_save_right[j],
                                h, use_highbd);
    }
  }
#endif  // CONFIG_LOOPFILTERING_ACROSS_TILES
}
#endif

#if USE_WIENER_HIGH_INTERMEDIATE_PRECISION
#define wiener_convolve8_add_src aom_convolve8_add_src_hip
#else
#define wiener_convolve8_add_src aom_convolve8_add_src
#endif

static void wiener_filter_stripe(const RestorationUnitInfo *rui,
                                 int stripe_width, int stripe_height,
                                 int procunit_width, const uint8_t *src,
                                 int src_stride, uint8_t *dst, int dst_stride,
                                 int32_t *tmpbuf, int bit_depth) {
  (void)tmpbuf;
  (void)bit_depth;
  assert(bit_depth == 8);

  for (int j = 0; j < stripe_width; j += procunit_width) {
    int w = AOMMIN(procunit_width, (stripe_width - j + 15) & ~15);
    const uint8_t *src_p = src + j;
    uint8_t *dst_p = dst + j;
    wiener_convolve8_add_src(src_p, src_stride, dst_p, dst_stride,
                             rui->wiener_info.hfilter, 16,
                             rui->wiener_info.vfilter, 16, w, stripe_height);
  }
}

/* Calculate windowed sums (if sqr=0) or sums of squares (if sqr=1)
   over the input. The window is of size (2r + 1)x(2r + 1), and we
   specialize to r = 1, 2, 3. A default function is used for r > 3.

   Each loop follows the same format: We keep a window's worth of input
   in individual variables and select data out of that as appropriate.
*/
static void boxsum1(int32_t *src, int width, int height, int src_stride,
                    int sqr, int32_t *dst, int dst_stride) {
  int i, j, a, b, c;

  // Vertical sum over 3-pixel regions, from src into dst.
  if (!sqr) {
    for (j = 0; j < width; ++j) {
      a = src[j];
      b = src[src_stride + j];
      c = src[2 * src_stride + j];

      dst[j] = a + b;
      for (i = 1; i < height - 2; ++i) {
        // Loop invariant: At the start of each iteration,
        // a = src[(i - 1) * src_stride + j]
        // b = src[(i    ) * src_stride + j]
        // c = src[(i + 1) * src_stride + j]
        dst[i * dst_stride + j] = a + b + c;
        a = b;
        b = c;
        c = src[(i + 2) * src_stride + j];
      }
      dst[i * dst_stride + j] = a + b + c;
      dst[(i + 1) * dst_stride + j] = b + c;
    }
  } else {
    for (j = 0; j < width; ++j) {
      a = src[j] * src[j];
      b = src[src_stride + j] * src[src_stride + j];
      c = src[2 * src_stride + j] * src[2 * src_stride + j];

      dst[j] = a + b;
      for (i = 1; i < height - 2; ++i) {
        dst[i * dst_stride + j] = a + b + c;
        a = b;
        b = c;
        c = src[(i + 2) * src_stride + j] * src[(i + 2) * src_stride + j];
      }
      dst[i * dst_stride + j] = a + b + c;
      dst[(i + 1) * dst_stride + j] = b + c;
    }
  }

  // Horizontal sum over 3-pixel regions of dst
  for (i = 0; i < height; ++i) {
    a = dst[i * dst_stride];
    b = dst[i * dst_stride + 1];
    c = dst[i * dst_stride + 2];

    dst[i * dst_stride] = a + b;
    for (j = 1; j < width - 2; ++j) {
      // Loop invariant: At the start of each iteration,
      // a = src[i * src_stride + (j - 1)]
      // b = src[i * src_stride + (j    )]
      // c = src[i * src_stride + (j + 1)]
      dst[i * dst_stride + j] = a + b + c;
      a = b;
      b = c;
      c = dst[i * dst_stride + (j + 2)];
    }
    dst[i * dst_stride + j] = a + b + c;
    dst[i * dst_stride + (j + 1)] = b + c;
  }
}

static void boxsum2(int32_t *src, int width, int height, int src_stride,
                    int sqr, int32_t *dst, int dst_stride) {
  int i, j, a, b, c, d, e;

  // Vertical sum over 5-pixel regions, from src into dst.
  if (!sqr) {
    for (j = 0; j < width; ++j) {
      a = src[j];
      b = src[src_stride + j];
      c = src[2 * src_stride + j];
      d = src[3 * src_stride + j];
      e = src[4 * src_stride + j];

      dst[j] = a + b + c;
      dst[dst_stride + j] = a + b + c + d;
      for (i = 2; i < height - 3; ++i) {
        // Loop invariant: At the start of each iteration,
        // a = src[(i - 2) * src_stride + j]
        // b = src[(i - 1) * src_stride + j]
        // c = src[(i    ) * src_stride + j]
        // d = src[(i + 1) * src_stride + j]
        // e = src[(i + 2) * src_stride + j]
        dst[i * dst_stride + j] = a + b + c + d + e;
        a = b;
        b = c;
        c = d;
        d = e;
        e = src[(i + 3) * src_stride + j];
      }
      dst[i * dst_stride + j] = a + b + c + d + e;
      dst[(i + 1) * dst_stride + j] = b + c + d + e;
      dst[(i + 2) * dst_stride + j] = c + d + e;
    }
  } else {
    for (j = 0; j < width; ++j) {
      a = src[j] * src[j];
      b = src[src_stride + j] * src[src_stride + j];
      c = src[2 * src_stride + j] * src[2 * src_stride + j];
      d = src[3 * src_stride + j] * src[3 * src_stride + j];
      e = src[4 * src_stride + j] * src[4 * src_stride + j];

      dst[j] = a + b + c;
      dst[dst_stride + j] = a + b + c + d;
      for (i = 2; i < height - 3; ++i) {
        dst[i * dst_stride + j] = a + b + c + d + e;
        a = b;
        b = c;
        c = d;
        d = e;
        e = src[(i + 3) * src_stride + j] * src[(i + 3) * src_stride + j];
      }
      dst[i * dst_stride + j] = a + b + c + d + e;
      dst[(i + 1) * dst_stride + j] = b + c + d + e;
      dst[(i + 2) * dst_stride + j] = c + d + e;
    }
  }

  // Horizontal sum over 5-pixel regions of dst
  for (i = 0; i < height; ++i) {
    a = dst[i * dst_stride];
    b = dst[i * dst_stride + 1];
    c = dst[i * dst_stride + 2];
    d = dst[i * dst_stride + 3];
    e = dst[i * dst_stride + 4];

    dst[i * dst_stride] = a + b + c;
    dst[i * dst_stride + 1] = a + b + c + d;
    for (j = 2; j < width - 3; ++j) {
      // Loop invariant: At the start of each iteration,
      // a = src[i * src_stride + (j - 2)]
      // b = src[i * src_stride + (j - 1)]
      // c = src[i * src_stride + (j    )]
      // d = src[i * src_stride + (j + 1)]
      // e = src[i * src_stride + (j + 2)]
      dst[i * dst_stride + j] = a + b + c + d + e;
      a = b;
      b = c;
      c = d;
      d = e;
      e = dst[i * dst_stride + (j + 3)];
    }
    dst[i * dst_stride + j] = a + b + c + d + e;
    dst[i * dst_stride + (j + 1)] = b + c + d + e;
    dst[i * dst_stride + (j + 2)] = c + d + e;
  }
}

#if MAX_RADIUS > 2
static void boxsum3(int32_t *src, int width, int height, int src_stride,
                    int sqr, int32_t *dst, int dst_stride) {
  int i, j, a, b, c, d, e, f, g;

  // Vertical sum over 7-pixel regions, from src into dst.
  if (!sqr) {
    for (j = 0; j < width; ++j) {
      a = src[j];
      b = src[1 * src_stride + j];
      c = src[2 * src_stride + j];
      d = src[3 * src_stride + j];
      e = src[4 * src_stride + j];
      f = src[5 * src_stride + j];
      g = src[6 * src_stride + j];

      dst[j] = a + b + c + d;
      dst[dst_stride + j] = a + b + c + d + e;
      dst[2 * dst_stride + j] = a + b + c + d + e + f;
      for (i = 3; i < height - 4; ++i) {
        dst[i * dst_stride + j] = a + b + c + d + e + f + g;
        a = b;
        b = c;
        c = d;
        d = e;
        e = f;
        f = g;
        g = src[(i + 4) * src_stride + j];
      }
      dst[i * dst_stride + j] = a + b + c + d + e + f + g;
      dst[(i + 1) * dst_stride + j] = b + c + d + e + f + g;
      dst[(i + 2) * dst_stride + j] = c + d + e + f + g;
      dst[(i + 3) * dst_stride + j] = d + e + f + g;
    }
  } else {
    for (j = 0; j < width; ++j) {
      a = src[j] * src[j];
      b = src[1 * src_stride + j] * src[1 * src_stride + j];
      c = src[2 * src_stride + j] * src[2 * src_stride + j];
      d = src[3 * src_stride + j] * src[3 * src_stride + j];
      e = src[4 * src_stride + j] * src[4 * src_stride + j];
      f = src[5 * src_stride + j] * src[5 * src_stride + j];
      g = src[6 * src_stride + j] * src[6 * src_stride + j];

      dst[j] = a + b + c + d;
      dst[dst_stride + j] = a + b + c + d + e;
      dst[2 * dst_stride + j] = a + b + c + d + e + f;
      for (i = 3; i < height - 4; ++i) {
        dst[i * dst_stride + j] = a + b + c + d + e + f + g;
        a = b;
        b = c;
        c = d;
        d = e;
        e = f;
        f = g;
        g = src[(i + 4) * src_stride + j] * src[(i + 4) * src_stride + j];
      }
      dst[i * dst_stride + j] = a + b + c + d + e + f + g;
      dst[(i + 1) * dst_stride + j] = b + c + d + e + f + g;
      dst[(i + 2) * dst_stride + j] = c + d + e + f + g;
      dst[(i + 3) * dst_stride + j] = d + e + f + g;
    }
  }

  // Horizontal sum over 7-pixel regions of dst
  for (i = 0; i < height; ++i) {
    a = dst[i * dst_stride];
    b = dst[i * dst_stride + 1];
    c = dst[i * dst_stride + 2];
    d = dst[i * dst_stride + 3];
    e = dst[i * dst_stride + 4];
    f = dst[i * dst_stride + 5];
    g = dst[i * dst_stride + 6];

    dst[i * dst_stride] = a + b + c + d;
    dst[i * dst_stride + 1] = a + b + c + d + e;
    dst[i * dst_stride + 2] = a + b + c + d + e + f;
    for (j = 3; j < width - 4; ++j) {
      dst[i * dst_stride + j] = a + b + c + d + e + f + g;
      a = b;
      b = c;
      c = d;
      d = e;
      e = f;
      f = g;
      g = dst[i * dst_stride + (j + 4)];
    }
    dst[i * dst_stride + j] = a + b + c + d + e + f + g;
    dst[i * dst_stride + (j + 1)] = b + c + d + e + f + g;
    dst[i * dst_stride + (j + 2)] = c + d + e + f + g;
    dst[i * dst_stride + (j + 3)] = d + e + f + g;
  }
}

// Generic version for any r. To be removed after experiments are done.
static void boxsumr(int32_t *src, int width, int height, int src_stride, int r,
                    int sqr, int32_t *dst, int dst_stride) {
  int32_t *tmp = aom_malloc(width * height * sizeof(*tmp));
  int tmp_stride = width;
  int i, j;
  if (sqr) {
    for (j = 0; j < width; ++j) tmp[j] = src[j] * src[j];
    for (j = 0; j < width; ++j)
      for (i = 1; i < height; ++i)
        tmp[i * tmp_stride + j] =
            tmp[(i - 1) * tmp_stride + j] +
            src[i * src_stride + j] * src[i * src_stride + j];
  } else {
    memcpy(tmp, src, sizeof(*tmp) * width);
    for (j = 0; j < width; ++j)
      for (i = 1; i < height; ++i)
        tmp[i * tmp_stride + j] =
            tmp[(i - 1) * tmp_stride + j] + src[i * src_stride + j];
  }
  for (i = 0; i <= r; ++i)
    memcpy(&dst[i * dst_stride], &tmp[(i + r) * tmp_stride],
           sizeof(*tmp) * width);
  for (i = r + 1; i < height - r; ++i)
    for (j = 0; j < width; ++j)
      dst[i * dst_stride + j] =
          tmp[(i + r) * tmp_stride + j] - tmp[(i - r - 1) * tmp_stride + j];
  for (i = height - r; i < height; ++i)
    for (j = 0; j < width; ++j)
      dst[i * dst_stride + j] = tmp[(height - 1) * tmp_stride + j] -
                                tmp[(i - r - 1) * tmp_stride + j];

  for (i = 0; i < height; ++i) tmp[i * tmp_stride] = dst[i * dst_stride];
  for (i = 0; i < height; ++i)
    for (j = 1; j < width; ++j)
      tmp[i * tmp_stride + j] =
          tmp[i * tmp_stride + j - 1] + dst[i * src_stride + j];

  for (j = 0; j <= r; ++j)
    for (i = 0; i < height; ++i)
      dst[i * dst_stride + j] = tmp[i * tmp_stride + j + r];
  for (j = r + 1; j < width - r; ++j)
    for (i = 0; i < height; ++i)
      dst[i * dst_stride + j] =
          tmp[i * tmp_stride + j + r] - tmp[i * tmp_stride + j - r - 1];
  for (j = width - r; j < width; ++j)
    for (i = 0; i < height; ++i)
      dst[i * dst_stride + j] =
          tmp[i * tmp_stride + width - 1] - tmp[i * tmp_stride + j - r - 1];
  aom_free(tmp);
}
#endif  // MAX_RADIUS > 2

static void boxsum(int32_t *src, int width, int height, int src_stride, int r,
                   int sqr, int32_t *dst, int dst_stride) {
  if (r == 1)
    boxsum1(src, width, height, src_stride, sqr, dst, dst_stride);
  else if (r == 2)
    boxsum2(src, width, height, src_stride, sqr, dst, dst_stride);
#if MAX_RADIUS > 2
  else if (r == 3)
    boxsum3(src, width, height, src_stride, sqr, dst, dst_stride);
  else if (r > 3)
    boxsumr(src, width, height, src_stride, r, sqr, dst, dst_stride);
#endif  // MAX_RADIUS > 2
  else
    assert(0 && "Invalid value of r in self-guided filter");
}

#if MAX_RADIUS > 2
static void boxnum(int width, int height, int r, int8_t *num, int num_stride) {
  int i, j;
  for (i = 0; i <= r; ++i) {
    for (j = 0; j <= r; ++j) {
      num[i * num_stride + j] = (r + 1 + i) * (r + 1 + j);
      num[i * num_stride + (width - 1 - j)] = num[i * num_stride + j];
      num[(height - 1 - i) * num_stride + j] = num[i * num_stride + j];
      num[(height - 1 - i) * num_stride + (width - 1 - j)] =
          num[i * num_stride + j];
    }
  }
  for (j = 0; j <= r; ++j) {
    const int val = (2 * r + 1) * (r + 1 + j);
    for (i = r + 1; i < height - r; ++i) {
      num[i * num_stride + j] = val;
      num[i * num_stride + (width - 1 - j)] = val;
    }
  }
  for (i = 0; i <= r; ++i) {
    const int val = (2 * r + 1) * (r + 1 + i);
    for (j = r + 1; j < width - r; ++j) {
      num[i * num_stride + j] = val;
      num[(height - 1 - i) * num_stride + j] = val;
    }
  }
  for (i = r + 1; i < height - r; ++i) {
    for (j = r + 1; j < width - r; ++j) {
      num[i * num_stride + j] = (2 * r + 1) * (2 * r + 1);
    }
  }
}
#endif  // MAX_RADIUS > 2

void decode_xq(const int *xqd, int *xq) {
  xq[0] = xqd[0];
  xq[1] = (1 << SGRPROJ_PRJ_BITS) - xq[0] - xqd[1];
}

const int32_t x_by_xplus1[256] = {
  // Special case: Map 0 -> 1 (corresponding to a value of 1/256)
  // instead of 0. See comments in av1_selfguided_restoration_internal() for why
  1,   128, 171, 192, 205, 213, 219, 224, 228, 230, 233, 235, 236, 238, 239,
  240, 241, 242, 243, 243, 244, 244, 245, 245, 246, 246, 247, 247, 247, 247,
  248, 248, 248, 248, 249, 249, 249, 249, 249, 250, 250, 250, 250, 250, 250,
  250, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 252, 252, 252, 252,
  252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 253, 253,
  253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253,
  253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 254, 254, 254,
  254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254,
  254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254,
  254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254,
  254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254,
  254, 254, 254, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  256,
};

const int32_t one_by_x[MAX_NELEM] = {
  4096, 2048, 1365, 1024, 819, 683, 585, 512, 455, 410, 372, 341, 315,
  293,  273,  256,  241,  228, 216, 205, 195, 186, 178, 171, 164,
#if MAX_RADIUS > 2
  158,  152,  146,  141,  137, 132, 128, 124, 120, 117, 114, 111, 108,
  105,  102,  100,  98,   95,  93,  91,  89,  87,  85,  84
#endif  // MAX_RADIUS > 2
};

static void av1_selfguided_restoration_internal(int32_t *dgd, int width,
                                                int height, int dgd_stride,
                                                int32_t *dst, int dst_stride,
                                                int bit_depth, int r, int eps) {
  const int width_ext = width + 2 * SGRPROJ_BORDER_HORZ;
  const int height_ext = height + 2 * SGRPROJ_BORDER_VERT;
  // Adjusting the stride of A and B here appears to avoid bad cache effects,
  // leading to a significant speed improvement.
  // We also align the stride to a multiple of 16 bytes, for consistency
  // with the SIMD version of this function.
  int buf_stride = ((width_ext + 3) & ~3) + 16;
  int32_t A_[RESTORATION_PROC_UNIT_PELS];
  int32_t B_[RESTORATION_PROC_UNIT_PELS];
  int32_t *A = A_;
  int32_t *B = B_;
#if MAX_RADIUS > 2
  const int num_stride = width_ext;
  int8_t num_[RESTORATION_PROC_UNIT_PELS];
  int8_t *num = num_ + SGRPROJ_BORDER_VERT * num_stride + SGRPROJ_BORDER_HORZ;
#endif
  int i, j;

  assert(r <= MAX_RADIUS && "Need MAX_RADIUS >= r");
  assert(r <= SGRPROJ_BORDER_VERT - 1 && r <= SGRPROJ_BORDER_HORZ - 1 &&
         "Need SGRPROJ_BORDER_* >= r+1");

  boxsum(dgd - dgd_stride * SGRPROJ_BORDER_VERT - SGRPROJ_BORDER_HORZ,
         width_ext, height_ext, dgd_stride, r, 0, B, buf_stride);
  boxsum(dgd - dgd_stride * SGRPROJ_BORDER_VERT - SGRPROJ_BORDER_HORZ,
         width_ext, height_ext, dgd_stride, r, 1, A, buf_stride);
#if MAX_RADIUS > 2
  boxnum(width_ext, height_ext, r, num_, num_stride);
#endif
  A += SGRPROJ_BORDER_VERT * buf_stride + SGRPROJ_BORDER_HORZ;
  B += SGRPROJ_BORDER_VERT * buf_stride + SGRPROJ_BORDER_HORZ;
  // Calculate the eventual A[] and B[] arrays. Include a 1-pixel border - ie,
  // for a 64x64 processing unit, we calculate 66x66 pixels of A[] and B[].
  for (i = -1; i < height + 1; ++i) {
    for (j = -1; j < width + 1; ++j) {
      const int k = i * buf_stride + j;
#if MAX_RADIUS > 2
      const int n = num[i * num_stride + j];
#else
      const int n = (2 * r + 1) * (2 * r + 1);
#endif

      // a < 2^16 * n < 2^22 regardless of bit depth
      uint32_t a = ROUND_POWER_OF_TWO(A[k], 2 * (bit_depth - 8));
      // b < 2^8 * n < 2^14 regardless of bit depth
      uint32_t b = ROUND_POWER_OF_TWO(B[k], bit_depth - 8);

      // Each term in calculating p = a * n - b * b is < 2^16 * n^2 < 2^28,
      // and p itself satisfies p < 2^14 * n^2 < 2^26.
      // This bound on p is due to:
      // https://en.wikipedia.org/wiki/Popoviciu's_inequality_on_variances
      //
      // Note: Sometimes, in high bit depth, we can end up with a*n < b*b.
      // This is an artefact of rounding, and can only happen if all pixels
      // are (almost) identical, so in this case we saturate to p=0.
      uint32_t p = (a * n < b * b) ? 0 : a * n - b * b;

      // Note: If MAX_RADIUS <= 2, then this 's' is a function only of
      // r and eps. Further, this is the only place we use 'eps', so we could
      // pre-calculate 's' for each parameter set and store that in place of
      // 'eps'.
      uint32_t s = sgrproj_mtable[eps - 1][n - 1];

      // p * s < (2^14 * n^2) * round(2^20 / n^2 eps) < 2^34 / eps < 2^32
      // as long as eps >= 4. So p * s fits into a uint32_t, and z < 2^12
      // (this holds even after accounting for the rounding in s)
      const uint32_t z = ROUND_POWER_OF_TWO(p * s, SGRPROJ_MTABLE_BITS);

      // Note: We have to be quite careful about the value of A[k].
      // This is used as a blend factor between individual pixel values and the
      // local mean. So it logically has a range of [0, 256], including both
      // endpoints.
      //
      // This is a pain for hardware, as we'd like something which can be stored
      // in exactly 8 bits.
      // Further, in the calculation of B[k] below, if z == 0 and r == 2,
      // then A[k] "should be" 0. But then we can end up setting B[k] to a value
      // slightly above 2^(8 + bit depth), due to rounding in the value of
      // one_by_x[25-1].
      //
      // Thus we saturate so that, when z == 0, A[k] is set to 1 instead of 0.
      // This fixes the above issues (256 - A[k] fits in a uint8, and we can't
      // overflow), without significantly affecting the final result: z == 0
      // implies that the image is essentially "flat", so the local mean and
      // individual pixel values are very similar.
      //
      // Note that saturating on the other side, ie. requring A[k] <= 255,
      // would be a bad idea, as that corresponds to the case where the image
      // is very variable, when we want to preserve the local pixel value as
      // much as possible.
      A[k] = x_by_xplus1[AOMMIN(z, 255)];  // in range [1, 256]

      // SGRPROJ_SGR - A[k] < 2^8 (from above), B[k] < 2^(bit_depth) * n,
      // one_by_x[n - 1] = round(2^12 / n)
      // => the product here is < 2^(20 + bit_depth) <= 2^32,
      // and B[k] is set to a value < 2^(8 + bit depth)
      // This holds even with the rounding in one_by_x and in the overall
      // result, as long as SGRPROJ_SGR - A[k] is strictly less than 2^8.
      B[k] = (int32_t)ROUND_POWER_OF_TWO((uint32_t)(SGRPROJ_SGR - A[k]) *
                                             (uint32_t)B[k] *
                                             (uint32_t)one_by_x[n - 1],
                                         SGRPROJ_RECIP_BITS);
    }
  }
  // Use the A[] and B[] arrays to calculate the filtered image
  for (i = 0; i < height; ++i) {
    for (j = 0; j < width; ++j) {
      const int k = i * buf_stride + j;
      const int l = i * dgd_stride + j;
      const int m = i * dst_stride + j;
      const int nb = 5;
      const int32_t a =
          (A[k] + A[k - 1] + A[k + 1] + A[k - buf_stride] + A[k + buf_stride]) *
              4 +
          (A[k - 1 - buf_stride] + A[k - 1 + buf_stride] +
           A[k + 1 - buf_stride] + A[k + 1 + buf_stride]) *
              3;
      const int32_t b =
          (B[k] + B[k - 1] + B[k + 1] + B[k - buf_stride] + B[k + buf_stride]) *
              4 +
          (B[k - 1 - buf_stride] + B[k - 1 + buf_stride] +
           B[k + 1 - buf_stride] + B[k + 1 + buf_stride]) *
              3;
      const int32_t v = a * dgd[l] + b;
      dst[m] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS + nb - SGRPROJ_RST_BITS);
    }
  }
}

void av1_selfguided_restoration_c(const uint8_t *dgd8, int width, int height,
                                  int dgd_stride, int32_t *flt1, int32_t *flt2,
                                  int flt_stride, const sgr_params_type *params,
                                  int bit_depth, int highbd) {
  int32_t dgd32_[RESTORATION_PROC_UNIT_PELS];
  const int dgd32_stride = width + 2 * SGRPROJ_BORDER_HORZ;
  int32_t *dgd32 =
      dgd32_ + dgd32_stride * SGRPROJ_BORDER_VERT + SGRPROJ_BORDER_HORZ;

  if (highbd) {
    const uint16_t *dgd16 = CONVERT_TO_SHORTPTR(dgd8);
    for (int i = -SGRPROJ_BORDER_VERT; i < height + SGRPROJ_BORDER_VERT; ++i) {
      for (int j = -SGRPROJ_BORDER_HORZ; j < width + SGRPROJ_BORDER_HORZ; ++j) {
        dgd32[i * dgd32_stride + j] = dgd16[i * dgd_stride + j];
      }
    }
  } else {
    for (int i = -SGRPROJ_BORDER_VERT; i < height + SGRPROJ_BORDER_VERT; ++i) {
      for (int j = -SGRPROJ_BORDER_HORZ; j < width + SGRPROJ_BORDER_HORZ; ++j) {
        dgd32[i * dgd32_stride + j] = dgd8[i * dgd_stride + j];
      }
    }
  }

  av1_selfguided_restoration_internal(dgd32, width, height, dgd32_stride, flt1,
                                      flt_stride, bit_depth, params->r1,
                                      params->e1);
  av1_selfguided_restoration_internal(dgd32, width, height, dgd32_stride, flt2,
                                      flt_stride, bit_depth, params->r2,
                                      params->e2);
}

void apply_selfguided_restoration_c(const uint8_t *dat8, int width, int height,
                                    int stride, int eps, const int *xqd,
                                    uint8_t *dst8, int dst_stride,
                                    int32_t *tmpbuf, int bit_depth,
                                    int highbd) {
  int xq[2];
  int32_t *flt1 = tmpbuf;
  int32_t *flt2 = flt1 + RESTORATION_TILEPELS_MAX;
  assert(width * height <= RESTORATION_TILEPELS_MAX);
  av1_selfguided_restoration_c(dat8, width, height, stride, flt1, flt2, width,
                               &sgr_params[eps], bit_depth, highbd);
  decode_xq(xqd, xq);
  for (int i = 0; i < height; ++i) {
    for (int j = 0; j < width; ++j) {
      const int k = i * width + j;
      uint8_t *dst8ij = dst8 + i * dst_stride + j;
      const uint8_t *dat8ij = dat8 + i * stride + j;

      const uint16_t pre_u = highbd ? *CONVERT_TO_SHORTPTR(dat8ij) : *dat8ij;
      const int32_t u = (int32_t)pre_u << SGRPROJ_RST_BITS;
      const int32_t f1 = flt1[k] - u;
      const int32_t f2 = flt2[k] - u;
      const int32_t v = xq[0] * f1 + xq[1] * f2 + (u << SGRPROJ_PRJ_BITS);
      const int16_t w =
          (int16_t)ROUND_POWER_OF_TWO(v, SGRPROJ_PRJ_BITS + SGRPROJ_RST_BITS);

      const uint16_t out = clip_pixel_highbd(w, bit_depth);
      if (highbd)
        *CONVERT_TO_SHORTPTR(dst8ij) = out;
      else
        *dst8ij = (uint8_t)out;
    }
  }
}

static void sgrproj_filter_stripe(const RestorationUnitInfo *rui,
                                  int stripe_width, int stripe_height,
                                  int procunit_width, const uint8_t *src,
                                  int src_stride, uint8_t *dst, int dst_stride,
                                  int32_t *tmpbuf, int bit_depth) {
  (void)bit_depth;
  assert(bit_depth == 8);

  for (int j = 0; j < stripe_width; j += procunit_width) {
    int w = AOMMIN(procunit_width, stripe_width - j);
    apply_selfguided_restoration(src + j, w, stripe_height, src_stride,
                                 rui->sgrproj_info.ep, rui->sgrproj_info.xqd,
                                 dst + j, dst_stride, tmpbuf, bit_depth, 0);
  }
}

#if CONFIG_HIGHBITDEPTH
#if USE_WIENER_HIGH_INTERMEDIATE_PRECISION
#define wiener_highbd_convolve8_add_src aom_highbd_convolve8_add_src_hip
#else
#define wiener_highbd_convolve8_add_src aom_highbd_convolve8_add_src
#endif

static void wiener_filter_stripe_highbd(const RestorationUnitInfo *rui,
                                        int stripe_width, int stripe_height,
                                        int procunit_width, const uint8_t *src8,
                                        int src_stride, uint8_t *dst8,
                                        int dst_stride, int32_t *tmpbuf,
                                        int bit_depth) {
  (void)tmpbuf;

  for (int j = 0; j < stripe_width; j += procunit_width) {
    int w = AOMMIN(procunit_width, (stripe_width - j + 15) & ~15);
    const uint8_t *src8_p = src8 + j;
    uint8_t *dst8_p = dst8 + j;
    wiener_highbd_convolve8_add_src(
        src8_p, src_stride, dst8_p, dst_stride, rui->wiener_info.hfilter, 16,
        rui->wiener_info.vfilter, 16, w, stripe_height, bit_depth);
  }
}

static void sgrproj_filter_stripe_highbd(const RestorationUnitInfo *rui,
                                         int stripe_width, int stripe_height,
                                         int procunit_width,
                                         const uint8_t *src8, int src_stride,
                                         uint8_t *dst8, int dst_stride,
                                         int32_t *tmpbuf, int bit_depth) {
  for (int j = 0; j < stripe_width; j += procunit_width) {
    int w = AOMMIN(procunit_width, stripe_width - j);
    apply_selfguided_restoration(src8 + j, w, stripe_height, src_stride,
                                 rui->sgrproj_info.ep, rui->sgrproj_info.xqd,
                                 dst8 + j, dst_stride, tmpbuf, bit_depth, 1);
  }
}
#endif  // CONFIG_HIGHBITDEPTH

typedef void (*stripe_filter_fun)(const RestorationUnitInfo *rui,
                                  int stripe_width, int stripe_height,
                                  int procunit_width, const uint8_t *src,
                                  int src_stride, uint8_t *dst, int dst_stride,
                                  int32_t *tmpbuf, int bit_depth);

#if CONFIG_HIGHBITDEPTH
#define NUM_STRIPE_FILTERS 4
#else
#define NUM_STRIPE_FILTERS 2
#endif

static const stripe_filter_fun stripe_filters[NUM_STRIPE_FILTERS] = {
  wiener_filter_stripe, sgrproj_filter_stripe,
#if CONFIG_HIGHBITDEPTH
  wiener_filter_stripe_highbd, sgrproj_filter_stripe_highbd
#endif  // CONFIG_HIGHBITDEPTH
};

// Filter one restoration unit
void av1_loop_restoration_filter_unit(
    const RestorationTileLimits *limits, const RestorationUnitInfo *rui,
#if CONFIG_STRIPED_LOOP_RESTORATION
    const RestorationStripeBoundaries *rsb, RestorationLineBuffers *rlbs,
    const AV1PixelRect *tile_rect, int tile_stripe0,
#if CONFIG_LOOPFILTERING_ACROSS_TILES
    int loop_filter_across_tiles_enabled,
#endif  // CONFIG_LOOPFILTERING_ACROSS_TILES
#endif  // CONFIG_STRIPED_LOOP_RESTORATION
    int ss_x, int ss_y, int highbd, int bit_depth, uint8_t *data8, int stride,
    uint8_t *dst8, int dst_stride, int32_t *tmpbuf) {
  RestorationType unit_rtype = rui->restoration_type;

  int unit_h = limits->v_end - limits->v_start;
  int unit_w = limits->h_end - limits->h_start;
  uint8_t *data8_tl = data8 + limits->v_start * stride + limits->h_start;
  uint8_t *dst8_tl = dst8 + limits->v_start * dst_stride + limits->h_start;

  if (unit_rtype == RESTORE_NONE) {
    copy_tile(unit_w, unit_h, data8_tl, stride, dst8_tl, dst_stride, highbd);
    return;
  }

  const int filter_idx = 2 * highbd + (unit_rtype == RESTORE_SGRPROJ);
  assert(filter_idx < NUM_STRIPE_FILTERS);
  const stripe_filter_fun stripe_filter = stripe_filters[filter_idx];

  const int procunit_width = RESTORATION_PROC_UNIT_SIZE >> ss_x;

// Convolve the whole tile one stripe at a time
#if CONFIG_STRIPED_LOOP_RESTORATION
  RestorationTileLimits remaining_stripes = *limits;
  int i = 0;
  while (i < unit_h) {
    int copy_above, copy_below;
    remaining_stripes.v_start = limits->v_start + i;

    get_stripe_boundary_info(&remaining_stripes, tile_rect, ss_y,
#if CONFIG_LOOPFILTERING_ACROSS_TILES
                             loop_filter_across_tiles_enabled,
#endif  // CONFIG_LOOPFILTERING_ACROSS_TILES
                             &copy_above, &copy_below);

    const int full_stripe_height = RESTORATION_PROC_UNIT_SIZE >> ss_y;
    const int rtile_offset = RESTORATION_TILE_OFFSET >> ss_y;

    // Work out where this stripe's boundaries are within
    // rsb->stripe_boundary_{above,below}
    const int tile_stripe =
        (remaining_stripes.v_start - tile_rect->top + rtile_offset) /
        full_stripe_height;
    const int frame_stripe = tile_stripe0 + tile_stripe;
    const int rsb_row = RESTORATION_CTX_VERT * frame_stripe;

    // Calculate this stripe's height, based on two rules:
    // * The topmost stripe in each tile is 8 luma pixels shorter than usual.
    // * We can't extend past the end of the current restoration unit
    const int nominal_stripe_height =
        full_stripe_height - ((tile_stripe == 0) ? rtile_offset : 0);
    const int h = AOMMIN(nominal_stripe_height,
                         remaining_stripes.v_end - remaining_stripes.v_start);

    setup_processing_stripe_boundary(
        &remaining_stripes, rsb, rsb_row, highbd, h,
#if CONFIG_LOOPFILTERING_ACROSS_TILES
        tile_rect, loop_filter_across_tiles_enabled,
#endif  // CONFIG_LOOPFILTERING_ACROSS_TILES
        data8, stride, rlbs, copy_above, copy_below);

    stripe_filter(rui, unit_w, h, procunit_width, data8_tl + i * stride, stride,
                  dst8_tl + i * dst_stride, dst_stride, tmpbuf, bit_depth);

    restore_processing_stripe_boundary(&remaining_stripes, rlbs, highbd, h,
#if CONFIG_LOOPFILTERING_ACROSS_TILES
                                       tile_rect,
                                       loop_filter_across_tiles_enabled,
#endif  // CONFIG_LOOPFILTERING_ACROSS_TILES
                                       data8, stride, copy_above, copy_below);

    i += h;
  }
#else
  const int stripe_height = RESTORATION_PROC_UNIT_SIZE >> ss_y;
  for (int i = 0; i < unit_h; i += stripe_height) {
    const int h = AOMMIN(stripe_height, (unit_h - i + 15) & ~15);
    stripe_filter(rui, unit_w, h, procunit_width, data8_tl + i * stride, stride,
                  dst8_tl + i * dst_stride, dst_stride, tmpbuf, bit_depth);
  }
#endif  // CONFIG_STRIPED_LOOP_RESTORATION
}

typedef struct {
  const RestorationInfo *rsi;
#if CONFIG_STRIPED_LOOP_RESTORATION
  RestorationLineBuffers *rlbs;
  const AV1_COMMON *cm;
  int tile_stripe0;
#endif  // CONFIG_STRIPED_LOOP_RESTORATION
  int ss_x, ss_y;
  int highbd, bit_depth;
  uint8_t *data8, *dst8;
  int data_stride, dst_stride;
  int32_t *tmpbuf;
} FilterFrameCtxt;

static void filter_frame_on_tile(int tile_row, int tile_col, void *priv) {
  (void)tile_col;
#if CONFIG_STRIPED_LOOP_RESTORATION
  FilterFrameCtxt *ctxt = (FilterFrameCtxt *)priv;
  ctxt->tile_stripe0 =
      (tile_row == 0) ? 0 : ctxt->cm->rst_end_stripe[tile_row - 1];
#else
  (void)tile_row;
  (void)priv;
#endif  // CONFIG_STRIPED_LOOP_RESTORATION
}

static void filter_frame_on_unit(const RestorationTileLimits *limits,
                                 const AV1PixelRect *tile_rect,
                                 int rest_unit_idx, void *priv) {
  FilterFrameCtxt *ctxt = (FilterFrameCtxt *)priv;
  const RestorationInfo *rsi = ctxt->rsi;

#if !CONFIG_STRIPED_LOOP_RESTORATION
  (void)tile_rect;
#endif  // CONFIG_STRIPED_LOOP_RESTORATION

  av1_loop_restoration_filter_unit(
      limits, &rsi->unit_info[rest_unit_idx],
#if CONFIG_STRIPED_LOOP_RESTORATION
      &rsi->boundaries, ctxt->rlbs, tile_rect, ctxt->tile_stripe0,
#if CONFIG_LOOPFILTERING_ACROSS_TILES
      ctxt->cm->loop_filter_across_tiles_enabled,
#endif  // CONFIG_LOOPFILTERING_ACROSS_TILES
#endif  // CONFIG_STRIPED_LOOP_RESTORATION
      ctxt->ss_x, ctxt->ss_y, ctxt->highbd, ctxt->bit_depth, ctxt->data8,
      ctxt->data_stride, ctxt->dst8, ctxt->dst_stride, ctxt->tmpbuf);
}

void av1_loop_restoration_filter_frame(YV12_BUFFER_CONFIG *frame,
                                       AV1_COMMON *cm) {
  typedef void (*copy_fun)(const YV12_BUFFER_CONFIG *src,
                           YV12_BUFFER_CONFIG *dst);
  static const copy_fun copy_funs[3] = { aom_yv12_copy_y, aom_yv12_copy_u,
                                         aom_yv12_copy_v };

  YV12_BUFFER_CONFIG dst;
  memset(&dst, 0, sizeof(dst));
  const int frame_width = frame->crop_widths[0];
  const int frame_height = ALIGN_POWER_OF_TWO(frame->crop_heights[0], 3);
  if (aom_realloc_frame_buffer(
          &dst, frame_width, frame_height, cm->subsampling_x, cm->subsampling_y,
#if CONFIG_HIGHBITDEPTH
          cm->use_highbitdepth,
#endif
          AOM_BORDER_IN_PIXELS, cm->byte_alignment, NULL, NULL, NULL) < 0)
    aom_internal_error(&cm->error, AOM_CODEC_MEM_ERROR,
                       "Failed to allocate restoration dst buffer");

#if CONFIG_STRIPED_LOOP_RESTORATION
  RestorationLineBuffers rlbs;
#endif  // CONFIG_STRIPED_LOOP_RESTORATION
#if CONFIG_HIGHBITDEPTH
  const int bit_depth = cm->bit_depth;
  const int highbd = cm->use_highbitdepth;
#else
  const int bit_depth = 8;
  const int highbd = 0;
#endif

  for (int plane = 0; plane < 3; ++plane) {
    const RestorationInfo *rsi = &cm->rst_info[plane];
    RestorationType rtype = rsi->frame_restoration_type;
    if (rtype == RESTORE_NONE) {
      copy_funs[plane](frame, &dst);
      continue;
    }

    const int is_uv = plane > 0;
    const int ss_y = is_uv && cm->subsampling_y;
    const int plane_width = frame->crop_widths[is_uv];
    const int plane_height =
        ALIGN_POWER_OF_TWO(frame->crop_heights[is_uv], 3 - ss_y);

    extend_frame(frame->buffers[plane], plane_width, plane_height,
                 frame->strides[is_uv], RESTORATION_BORDER, RESTORATION_BORDER,
                 highbd);

    FilterFrameCtxt ctxt;
    ctxt.rsi = rsi;
#if CONFIG_STRIPED_LOOP_RESTORATION
    ctxt.rlbs = &rlbs;
    ctxt.cm = cm;
#endif  // CONFIG_STRIPED_LOOP_RESTORATION
    ctxt.ss_x = is_uv && cm->subsampling_x;
    ctxt.ss_y = is_uv && cm->subsampling_y;
    ctxt.highbd = highbd;
    ctxt.bit_depth = bit_depth;
    ctxt.data8 = frame->buffers[plane];
    ctxt.dst8 = dst.buffers[plane];
    ctxt.data_stride = frame->strides[is_uv];
    ctxt.dst_stride = dst.strides[is_uv];
    ctxt.tmpbuf = cm->rst_tmpbuf;

    av1_foreach_rest_unit_in_frame(cm, plane, filter_frame_on_tile,
                                   filter_frame_on_unit, &ctxt);
  }

  for (int plane = 0; plane < 3; ++plane) {
    copy_funs[plane](&dst, frame);
  }
  aom_free_frame_buffer(&dst);
}

static void foreach_rest_unit_in_tile(const AV1PixelRect *tile_rect,
                                      int tile_row, int tile_col, int tile_cols,
                                      int hunits_per_tile, int units_per_tile,
                                      int unit_size, int ss_y,
                                      rest_unit_visitor_t on_rest_unit,
                                      void *priv) {
  const int tile_w = tile_rect->right - tile_rect->left;
  const int tile_h = tile_rect->bottom - tile_rect->top;
  const int ext_size = unit_size * 3 / 2;

  const int tile_idx = tile_col + tile_row * tile_cols;
  const int unit_idx0 = tile_idx * units_per_tile;

  int y0 = 0, i = 0;
  while (y0 < tile_h) {
    int remaining_h = tile_h - y0;
    int h = (remaining_h < ext_size) ? remaining_h : unit_size;

    RestorationTileLimits limits;
    limits.v_start = tile_rect->top + y0;
    limits.v_end = tile_rect->top + y0 + h;
    assert(limits.v_end <= tile_rect->bottom);
#if CONFIG_STRIPED_LOOP_RESTORATION
    // Offset the tile upwards to align with the restoration processing stripe
    const int voffset = RESTORATION_TILE_OFFSET >> ss_y;
    limits.v_start = AOMMAX(tile_rect->top, limits.v_start - voffset);
    if (limits.v_end < tile_rect->bottom) limits.v_end -= voffset;
#else
    (void)ss_y;
#endif  // CONFIG_STRIPED_LOOP_RESTORATION

    int x0 = 0, j = 0;
    while (x0 < tile_w) {
      int remaining_w = tile_w - x0;
      int w = (remaining_w < ext_size) ? remaining_w : unit_size;

      limits.h_start = tile_rect->left + x0;
      limits.h_end = tile_rect->left + x0 + w;
      assert(limits.h_end <= tile_rect->right);

      const int unit_idx = unit_idx0 + i * hunits_per_tile + j;
      on_rest_unit(&limits, tile_rect, unit_idx, priv);

      x0 += w;
      ++j;
    }

    y0 += h;
    ++i;
  }
}

void av1_foreach_rest_unit_in_frame(const struct AV1Common *cm, int plane,
                                    rest_tile_start_visitor_t on_tile,
                                    rest_unit_visitor_t on_rest_unit,
                                    void *priv) {
  const int is_uv = plane > 0;
  const int ss_y = is_uv && cm->subsampling_y;

  const RestorationInfo *rsi = &cm->rst_info[plane];

  TileInfo tile_info;
  for (int tile_row = 0; tile_row < cm->tile_rows; ++tile_row) {
    av1_tile_set_row(&tile_info, cm, tile_row);
    for (int tile_col = 0; tile_col < cm->tile_cols; ++tile_col) {
      av1_tile_set_col(&tile_info, cm, tile_col);

      const AV1PixelRect tile_rect = get_ext_tile_rect(&tile_info, cm, is_uv);

      if (on_tile) on_tile(tile_row, tile_col, priv);

      foreach_rest_unit_in_tile(&tile_rect, tile_row, tile_col, cm->tile_cols,
                                rsi->horz_units_per_tile, rsi->units_per_tile,
                                rsi->restoration_unit_size, ss_y, on_rest_unit,
                                priv);
    }
  }
}

#if CONFIG_MAX_TILE
// Get the horizontal or vertical index of the tile containing mi_x. For a
// horizontal index, mi_x should be the left-most column for some block in mi
// units and tile_x_start_sb should be cm->tile_col_start_sb. The return value
// will be "tile_col" for the tile containing that block.
//
// For a vertical index, mi_x should be the block's top row and tile_x_start_sb
// should be cm->tile_row_start_sb. The return value will be "tile_row" for the
// tile containing the block.
static int get_tile_idx(const int *tile_x_start_sb, int mi_x, int mib_log2) {
  int sb_x = mi_x >> mib_log2;

  for (int i = 0; i < MAX_TILE_COLS; ++i) {
    if (tile_x_start_sb[i + 1] > sb_x) return i;
  }

  // This shouldn't happen if tile_x_start_sb has been filled in
  // correctly.
  assert(0);
  return 0;
}
#endif

int av1_loop_restoration_corners_in_sb(const struct AV1Common *cm, int plane,
                                       int mi_row, int mi_col, BLOCK_SIZE bsize,
                                       int *rcol0, int *rcol1, int *rrow0,
                                       int *rrow1, int *tile_tl_idx) {
  assert(rcol0 && rcol1 && rrow0 && rrow1);

  if (bsize != cm->sb_size) return 0;
  if (cm->rst_info[plane].frame_restoration_type == RESTORE_NONE) return 0;

  const int is_uv = plane > 0;

// Which tile contains the superblock? Find that tile's top-left in mi-units,
// together with the tile's size in pixels.
#if CONFIG_MAX_TILE
  const int mib_log2 = cm->mib_size_log2;
  const int tile_row = get_tile_idx(cm->tile_row_start_sb, mi_row, mib_log2);
  const int tile_col = get_tile_idx(cm->tile_col_start_sb, mi_col, mib_log2);
#else
  const int tile_row = mi_row / cm->tile_height;
  const int tile_col = mi_col / cm->tile_width;
#endif  // CONFIG_MAX_TILE

  TileInfo tile_info;
  av1_tile_init(&tile_info, cm, tile_row, tile_col);

  const AV1PixelRect tile_rect = get_ext_tile_rect(&tile_info, cm, is_uv);
  const int tile_w = tile_rect.right - tile_rect.left;
  const int tile_h = tile_rect.bottom - tile_rect.top;

  const int mi_top = tile_info.mi_row_start;
  const int mi_left = tile_info.mi_col_start;

  // Compute the mi-unit corners of the superblock relative to the top-left of
  // the tile
  const int mi_rel_row0 = mi_row - mi_top;
  const int mi_rel_col0 = mi_col - mi_left;
  const int mi_rel_row1 = mi_rel_row0 + mi_size_high[bsize];
  const int mi_rel_col1 = mi_rel_col0 + mi_size_wide[bsize];

  const RestorationInfo *rsi = &cm->rst_info[plane];
  const int size = rsi->restoration_unit_size;

  // Calculate the number of restoration units in this tile (which might be
  // strictly less than rsi->horz_units_per_tile and rsi->vert_units_per_tile)
  const int horz_units = count_units_in_tile(size, tile_w);
  const int vert_units = count_units_in_tile(size, tile_h);

  // The size of an MI-unit on this plane of the image
  const int ss_x = is_uv && cm->subsampling_x;
  const int ss_y = is_uv && cm->subsampling_y;
  const int mi_size_x = MI_SIZE >> ss_x;
  const int mi_size_y = MI_SIZE >> ss_y;

#if CONFIG_HORZONLY_FRAME_SUPERRES
  // Write m for the relative mi column or row, D for the superres denominator
  // and N for the superres numerator. If u is the upscaled (called "unscaled"
  // elsewhere) pixel offset then we can write the downscaled pixel offset in
  // two ways as:
  //
  //   MI_SIZE * m = N / D u
  //
  // from which we get u = D * MI_SIZE * m / N
  const int mi_to_num_x = mi_size_x * cm->superres_scale_denominator;
  const int mi_to_num_y = mi_size_y;
  const int denom_x = size * SCALE_NUMERATOR;
  const int denom_y = size;
#else
  const int mi_to_num_x = mi_size_x;
  const int mi_to_num_y = mi_size_y;
  const int denom_x = size;
  const int denom_y = size;
#endif  // CONFIG_HORZONLY_FRAME_SUPERRES

  const int rnd_x = denom_x - 1;
  const int rnd_y = denom_y - 1;

  // rcol0/rrow0 should be the first column/row of restoration units (relative
  // to the top-left of the tile) that doesn't start left/below of
  // mi_col/mi_row. For this calculation, we need to round up the division (if
  // the sb starts at rtile column 10.1, the first matching rtile has column
  // index 11)
  *rcol0 = (mi_rel_col0 * mi_to_num_x + rnd_x) / denom_x;
  *rrow0 = (mi_rel_row0 * mi_to_num_y + rnd_y) / denom_y;

  // rel_col1/rel_row1 is the equivalent calculation, but for the superblock
  // below-right. If we're at the bottom or right of the tile, this restoration
  // unit might not exist, in which case we'll clamp accordingly.
  *rcol1 = AOMMIN((mi_rel_col1 * mi_to_num_x + rnd_x) / denom_x, horz_units);
  *rrow1 = AOMMIN((mi_rel_row1 * mi_to_num_y + rnd_y) / denom_y, vert_units);

  const int tile_idx = tile_col + tile_row * cm->tile_cols;
  *tile_tl_idx = tile_idx * rsi->units_per_tile;

  return *rcol0 < *rcol1 && *rrow0 < *rrow1;
}

#if CONFIG_STRIPED_LOOP_RESTORATION
// Extend to left and right
static void extend_lines(uint8_t *buf, int width, int height, int stride,
                         int extend, int use_highbitdepth) {
  for (int i = 0; i < height; ++i) {
    if (use_highbitdepth) {
#if CONFIG_HIGHBITDEPTH
      uint16_t *buf16 = (uint16_t *)buf;
      aom_memset16(buf16 - extend, buf16[0], extend);
      aom_memset16(buf16 + width, buf16[width - 1], extend);
#else
      assert(0 && "use_highbitdepth set but CONFIG_HIGHBITDEPTH not enabled");
#endif  // CONFIG_HIGHBITDEPTH
    } else {
      memset(buf - extend, buf[0], extend);
      memset(buf + width, buf[width - 1], extend);
    }
    buf += stride;
  }
}

static void save_deblock_boundary_lines(
    const YV12_BUFFER_CONFIG *frame, const AV1_COMMON *cm, int plane, int row,
    int stripe, int use_highbd, int is_above,
    RestorationStripeBoundaries *boundaries) {
  const int is_uv = plane > 0;
  const int src_width = frame->crop_widths[is_uv];
  const uint8_t *src_buf = REAL_PTR(use_highbd, frame->buffers[plane]);
  const int src_stride = frame->strides[is_uv] << use_highbd;
  const uint8_t *src_rows = src_buf + row * src_stride;

  uint8_t *bdry_buf = is_above ? boundaries->stripe_boundary_above
                               : boundaries->stripe_boundary_below;
  uint8_t *bdry_start = bdry_buf + (RESTORATION_EXTRA_HORZ << use_highbd);
  const int bdry_stride = boundaries->stripe_boundary_stride << use_highbd;
  uint8_t *bdry_rows = bdry_start + RESTORATION_CTX_VERT * stripe * bdry_stride;

// Because we're rounding to multiple of 8 luma pixels and this is
// only called when row < src_height, there must be at least 2 luma
// or chroma pixels available.
#ifndef NDEBUG
  const int ss_y = is_uv && cm->subsampling_y;
  const int src_height = cm->mi_rows << (MI_SIZE_LOG2 - ss_y);
  assert(row + RESTORATION_CTX_VERT <= src_height);
#endif  // NDEBUG

#if CONFIG_HORZONLY_FRAME_SUPERRES
  const int ss_x = is_uv && cm->subsampling_x;
  const int upscaled_width = (cm->superres_upscaled_width + ss_x) >> ss_x;
  const int step = av1_get_upscale_convolve_step(src_width, upscaled_width);
#if CONFIG_HIGHBITDEPTH
  if (use_highbd)
    av1_highbd_convolve_horiz_rs(
        (uint16_t *)src_rows, src_stride >> 1, (uint16_t *)bdry_rows,
        bdry_stride >> 1, upscaled_width, RESTORATION_CTX_VERT,
        &av1_resize_filter_normative[0][0], UPSCALE_NORMATIVE_TAPS, 0, step,
        cm->bit_depth);
  else
#endif  // CONFIG_HIGHBITDEPTH
    av1_convolve_horiz_rs(src_rows, src_stride, bdry_rows, bdry_stride,
                          upscaled_width, RESTORATION_CTX_VERT,
                          &av1_resize_filter_normative[0][0],
                          UPSCALE_NORMATIVE_TAPS, 0, step);
#else
  (void)cm;
  const int upscaled_width = src_width;
  const int line_bytes = src_width << use_highbd;
  for (int i = 0; i < RESTORATION_CTX_VERT; i++) {
    memcpy(bdry_rows + i * bdry_stride, src_rows + i * src_stride, line_bytes);
  }
#endif  // CONFIG_HORZONLY_FRAME_SUPERRES
  extend_lines(bdry_rows, upscaled_width, RESTORATION_CTX_VERT, bdry_stride,
               RESTORATION_EXTRA_HORZ, use_highbd);
}

static void save_cdef_boundary_lines(const YV12_BUFFER_CONFIG *frame,
                                     const AV1_COMMON *cm, int plane, int row,
                                     int stripe, int use_highbd, int is_above,
                                     RestorationStripeBoundaries *boundaries) {
  const int is_uv = plane > 0;
  const uint8_t *src_buf = REAL_PTR(use_highbd, frame->buffers[plane]);
  const int src_stride = frame->strides[is_uv] << use_highbd;
  const uint8_t *src_rows = src_buf + row * src_stride;

  uint8_t *bdry_buf = is_above ? boundaries->stripe_boundary_above
                               : boundaries->stripe_boundary_below;
  uint8_t *bdry_start = bdry_buf + (RESTORATION_EXTRA_HORZ << use_highbd);
  const int bdry_stride = boundaries->stripe_boundary_stride << use_highbd;
  uint8_t *bdry_rows = bdry_start + RESTORATION_CTX_VERT * stripe * bdry_stride;

#if CONFIG_HORZONLY_FRAME_SUPERRES
  // At the point where this function is called, we've already applied
  // superres. So we don't need to extend the lines here, we can just
  // pull directly from the topmost row of the upscaled frame.
  const int ss_x = is_uv && cm->subsampling_x;
  const int upscaled_width = (cm->superres_upscaled_width + ss_x) >> ss_x;
#else
  (void)cm;
  const int src_width = frame->crop_widths[is_uv];
  const int upscaled_width = src_width;
#endif  // CONFIG_HORZONLY_FRAME_SUPERRES
  const int line_bytes = upscaled_width << use_highbd;
  for (int i = 0; i < RESTORATION_CTX_VERT; i++) {
    // Copy the line at 'row' into both context lines. This is because
    // we want to (effectively) extend the outermost row of CDEF data
    // from this tile to produce a border, rather than using deblocked
    // pixels from the tile above/below.
    memcpy(bdry_rows + i * bdry_stride, src_rows, line_bytes);
  }
  extend_lines(bdry_rows, upscaled_width, RESTORATION_CTX_VERT, bdry_stride,
               RESTORATION_EXTRA_HORZ, use_highbd);
}

static void save_tile_row_boundary_lines(const YV12_BUFFER_CONFIG *frame,
                                         int tile_row,
                                         const TileInfo *tile_info,
                                         int use_highbd, int plane,
                                         AV1_COMMON *cm, int after_cdef) {
  const int is_uv = plane > 0;
  const int ss_y = is_uv && cm->subsampling_y;
  const int stripe_height = RESTORATION_PROC_UNIT_SIZE >> ss_y;
  const int stripe_off = RESTORATION_TILE_OFFSET >> ss_y;

  // Get the tile rectangle, with height rounded up to the next multiple of 8
  // luma pixels (only relevant for the bottom tile of the frame)
  const AV1PixelRect tile_rect = get_ext_tile_rect(tile_info, cm, is_uv);

  RestorationStripeBoundaries *boundaries = &cm->rst_info[plane].boundaries;

  const int stripe0 = (tile_row == 0) ? 0 : cm->rst_end_stripe[tile_row - 1];

  int plane_height = cm->mi_rows << (MI_SIZE_LOG2 - ss_y);

  int tile_stripe;
  for (tile_stripe = 0;; ++tile_stripe) {
    const int rel_y0 = AOMMAX(0, tile_stripe * stripe_height - stripe_off);
    const int y0 = tile_rect.top + rel_y0;
    if (y0 >= tile_rect.bottom) break;

    const int rel_y1 = (tile_stripe + 1) * stripe_height - stripe_off;
    const int y1 = AOMMIN(tile_rect.top + rel_y1, tile_rect.bottom);

    const int frame_stripe = stripe0 + tile_stripe;

    int use_deblock_above, use_deblock_below;
#if CONFIG_LOOPFILTERING_ACROSS_TILES
    if (!cm->loop_filter_across_tiles_enabled) {
// In this case, we should use CDEF pixels for the above context
// of the topmost stripe in each region, and for the below context
// of the bottommost stripe in each tile.
//
// As a special case, when dependent-horztiles is enabled, we may be
// allowed to use pixels from the tile above us. But we don't use pixels
// from the tile below in that case, to match the behaviour of
// av1_setup_across_tile_boundary_info()
#if CONFIG_DEPENDENT_HORZTILES
      if (cm->dependent_horz_tiles && !tile_info->tg_horz_boundary)
        use_deblock_above = (frame_stripe > 0);
      else
#endif
        use_deblock_above = (tile_stripe > 0);

      use_deblock_below = (y1 < tile_rect.bottom);
    } else {
#endif  // CONFIG_LOOPFILTERING_ACROSS_TILES
      // In this case, we should only use CDEF pixels at the top
      // and bottom of the frame as a whole; internal tile boundaries
      // can use deblocked pixels from adjacent tiles for context.
      use_deblock_above = (frame_stripe > 0);
      use_deblock_below = (y1 < plane_height);
#if CONFIG_LOOPFILTERING_ACROSS_TILES
    }
#endif  // CONFIG_LOOPFILTERING_ACROSS_TILES

    if (!after_cdef) {
      // Save deblocked context where needed.
      if (use_deblock_above) {
        save_deblock_boundary_lines(frame, cm, plane, y0 - RESTORATION_CTX_VERT,
                                    frame_stripe, use_highbd, 1, boundaries);
      }
      if (use_deblock_below) {
        save_deblock_boundary_lines(frame, cm, plane, y1, frame_stripe,
                                    use_highbd, 0, boundaries);
      }
    } else {
      // Save CDEF context where needed. Note that we need to save the CDEF
      // context for a particular boundary iff we *didn't* save deblocked
      // context for that boundary.
      //
      // In addition, we need to save copies of the outermost line within
      // the tile, rather than using data from outside the tile.
      if (!use_deblock_above) {
        save_cdef_boundary_lines(frame, cm, plane, y0, frame_stripe, use_highbd,
                                 1, boundaries);
      }
      if (!use_deblock_below) {
        save_cdef_boundary_lines(frame, cm, plane, y1 - 1, frame_stripe,
                                 use_highbd, 0, boundaries);
      }
    }
  }
}

// For each RESTORATION_PROC_UNIT_SIZE pixel high stripe, save 4 scan
// lines to be used as boundary in the loop restoration process. The
// lines are saved in rst_internal.stripe_boundary_lines
void av1_loop_restoration_save_boundary_lines(const YV12_BUFFER_CONFIG *frame,
                                              AV1_COMMON *cm, int after_cdef) {
#if CONFIG_HIGHBITDEPTH
  const int use_highbd = cm->use_highbitdepth;
#else
  const int use_highbd = 0;
#endif

  for (int p = 0; p < MAX_MB_PLANE; ++p) {
    TileInfo tile_info;
    for (int tile_row = 0; tile_row < cm->tile_rows; ++tile_row) {
      av1_tile_init(&tile_info, cm, tile_row, 0);
      save_tile_row_boundary_lines(frame, tile_row, &tile_info, use_highbd, p,
                                   cm, after_cdef);
    }
  }
}
#endif  // CONFIG_STRIPED_LOOP_RESTORATION
