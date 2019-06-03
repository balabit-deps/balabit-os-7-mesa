/**************************************************************************
 * 
 * Copyright 2007 VMware, Inc.
 * All Rights Reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL VMWARE AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * 
 **************************************************************************/

/**
 * glRasterPos implementation.  Basically render a GL_POINT with our
 * private draw module.  Plug in a special "rasterpos" stage at the end
 * of the 'draw' pipeline to capture the results and update the current
 * raster pos attributes.
 *
 * Authors:
 *   Brian Paul
 */


#include "main/imports.h"
#include "main/macros.h"
#include "main/arrayobj.h"
#include "main/feedback.h"
#include "main/rastpos.h"
#include "main/state.h"
#include "main/varray.h"

#include "st_context.h"
#include "st_atom.h"
#include "st_draw.h"
#include "st_program.h"
#include "st_cb_rasterpos.h"
#include "draw/draw_context.h"
#include "draw/draw_pipe.h"
#include "vbo/vbo.h"


/**
 * Our special drawing pipeline stage (replaces rasterization).
 */
struct rastpos_stage
{
   struct draw_stage stage;   /**< Base class */
   struct gl_context *ctx;            /**< Rendering context */

   /* vertex attrib info we can setup once and re-use */
   struct gl_vertex_array_object *VAO;
   struct _mesa_prim prim;
};


static inline struct rastpos_stage *
rastpos_stage( struct draw_stage *stage )
{
   return (struct rastpos_stage *) stage;
}

static void
rastpos_flush( struct draw_stage *stage, unsigned flags )
{
   /* no-op */
}

static void
rastpos_reset_stipple_counter( struct draw_stage *stage )
{
   /* no-op */
}

static void
rastpos_tri( struct draw_stage *stage, struct prim_header *prim )
{
   /* should never get here */
   assert(0);
}

static void
rastpos_line( struct draw_stage *stage, struct prim_header *prim )
{
   /* should never get here */
   assert(0);
}

static void
rastpos_destroy(struct draw_stage *stage)
{
   struct rastpos_stage *rstage = (struct rastpos_stage*)stage;
   _mesa_reference_vao(rstage->ctx, &rstage->VAO, NULL);
   free(stage);
}


/**
 * Update a raster pos attribute from the vertex result if it's present,
 * else copy the current attrib.
 */
static void
update_attrib(struct gl_context *ctx, const ubyte *outputMapping,
              const struct vertex_header *vert,
              GLfloat *dest,
              GLuint result, GLuint defaultAttrib)
{
   const GLfloat *src;
   const GLuint k = outputMapping[result];
   if (k != ~0U)
      src = vert->data[k];
   else
      src = ctx->Current.Attrib[defaultAttrib];
   COPY_4V(dest, src);
}


/**
 * Normally, this function would render a GL_POINT.
 */
static void
rastpos_point(struct draw_stage *stage, struct prim_header *prim)
{
   struct rastpos_stage *rs = rastpos_stage(stage);
   struct gl_context *ctx = rs->ctx;
   struct st_context *st = st_context(ctx);
   const GLfloat height = (GLfloat) ctx->DrawBuffer->Height;
   const ubyte *outputMapping = st->vp->result_to_output;
   const GLfloat *pos;
   GLuint i;

   /* if we get here, we didn't get clipped */
   ctx->Current.RasterPosValid = GL_TRUE;

   /* update raster pos */
   pos = prim->v[0]->data[0];
   ctx->Current.RasterPos[0] = pos[0];
   if (st_fb_orientation(ctx->DrawBuffer) == Y_0_TOP)
      ctx->Current.RasterPos[1] = height - pos[1]; /* invert Y */
   else
      ctx->Current.RasterPos[1] = pos[1];
   ctx->Current.RasterPos[2] = pos[2];
   ctx->Current.RasterPos[3] = pos[3];

   /* update other raster attribs */
   update_attrib(ctx, outputMapping, prim->v[0],
                 ctx->Current.RasterColor,
                 VARYING_SLOT_COL0, VERT_ATTRIB_COLOR0);

   update_attrib(ctx, outputMapping, prim->v[0],
                 ctx->Current.RasterSecondaryColor,
                 VARYING_SLOT_COL1, VERT_ATTRIB_COLOR1);

   for (i = 0; i < ctx->Const.MaxTextureCoordUnits; i++) {
      update_attrib(ctx, outputMapping, prim->v[0],
                    ctx->Current.RasterTexCoords[i],
                    VARYING_SLOT_TEX0 + i, VERT_ATTRIB_TEX0 + i);
   }

   if (ctx->RenderMode == GL_SELECT) {
      _mesa_update_hitflag( ctx, ctx->Current.RasterPos[2] );
   }
}


/**
 * Create rasterpos "drawing" stage.
 */
static struct rastpos_stage *
new_draw_rastpos_stage(struct gl_context *ctx, struct draw_context *draw)
{
   struct rastpos_stage *rs = ST_CALLOC_STRUCT(rastpos_stage);

   rs->stage.draw = draw;
   rs->stage.next = NULL;
   rs->stage.point = rastpos_point;
   rs->stage.line = rastpos_line;
   rs->stage.tri = rastpos_tri;
   rs->stage.flush = rastpos_flush;
   rs->stage.destroy = rastpos_destroy;
   rs->stage.reset_stipple_counter = rastpos_reset_stipple_counter;
   rs->stage.destroy = rastpos_destroy;
   rs->ctx = ctx;

   rs->VAO = _mesa_new_vao(ctx, ~((GLuint)0));
   _mesa_vertex_attrib_binding(ctx, rs->VAO, VERT_ATTRIB_POS, 0);
   _mesa_update_array_format(ctx, rs->VAO, VERT_ATTRIB_POS, 4, GL_FLOAT,
                             GL_RGBA, GL_FALSE, GL_FALSE, GL_FALSE, 0);
   _mesa_enable_vertex_array_attrib(ctx, rs->VAO, 0);

   rs->prim.mode = GL_POINTS;
   rs->prim.indexed = 0;
   rs->prim.begin = 1;
   rs->prim.end = 1;
   rs->prim.start = 0;
   rs->prim.count = 1;
   rs->prim.pad = 0;
   rs->prim.num_instances = 1;
   rs->prim.base_instance = 0;
   rs->prim.is_indirect = 0;

   return rs;
}


static void
st_RasterPos(struct gl_context *ctx, const GLfloat v[4])
{
   struct st_context *st = st_context(ctx);
   struct draw_context *draw = st_get_draw_context(st);
   struct rastpos_stage *rs;

   if (!st->draw)
      return;

   if (ctx->VertexProgram._Current == NULL ||
       ctx->VertexProgram._Current == ctx->VertexProgram._TnlProgram) {
      /* No vertex shader/program is enabled, used the simple/fast fixed-
       * function implementation of RasterPos.
       */
      _mesa_RasterPos(ctx, v);
      return;
   }

   if (st->rastpos_stage) {
      /* get rastpos stage info */
      rs = rastpos_stage(st->rastpos_stage);
   }
   else {
      /* create rastpos draw stage */
      rs = new_draw_rastpos_stage(ctx, draw);
      st->rastpos_stage = &rs->stage;
   }

   /* plug our rastpos stage into the draw module */
   draw_set_rasterize_stage(st->draw, st->rastpos_stage);

   /* make sure everything's up to date */
   st_validate_state(st, ST_PIPELINE_RENDER);

   /* This will get set only if rastpos_point(), above, gets called */
   ctx->Current.RasterPosValid = GL_FALSE;

   /* All vertex attribs but position were previously initialized above.
    * Just plug in position pointer now.
    */
   rs->VAO->VertexAttrib[VERT_ATTRIB_POS].Ptr = (GLubyte *) v;
   rs->VAO->NewArrays |= VERT_BIT_POS;
   _mesa_set_draw_vao(ctx, rs->VAO, VERT_BIT_POS);

   /* Draw the point. */
   st_feedback_draw_vbo(ctx, &rs->prim, 1, NULL, GL_TRUE, 0, 1,
                        NULL, 0, NULL);

   /* restore draw's rasterization stage depending on rendermode */
   if (ctx->RenderMode == GL_FEEDBACK) {
      draw_set_rasterize_stage(draw, st->feedback_stage);
   }
   else if (ctx->RenderMode == GL_SELECT) {
      draw_set_rasterize_stage(draw, st->selection_stage);
   }
}



void st_init_rasterpos_functions(struct dd_function_table *functions)
{
   functions->RasterPos = st_RasterPos;
}
