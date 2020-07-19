/*
 * Copyright (C) 2014 Rob Clark <robclark@freedesktop.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *    Rob Clark <robclark@freedesktop.org>
 */

#include "tgsi/tgsi_text.h"
#include "tgsi/tgsi_ureg.h"

#include "freedreno_program.h"
#include "freedreno_context.h"

static void
fd_vs_state_bind(struct pipe_context *pctx, void *hwcso)
{
	struct fd_context *ctx = fd_context(pctx);
	ctx->prog.vs = hwcso;
	ctx->dirty_shader[PIPE_SHADER_VERTEX] |= FD_DIRTY_SHADER_PROG;
	ctx->dirty |= FD_DIRTY_PROG;
}

static void
fd_tcs_state_bind(struct pipe_context *pctx, void *hwcso)
{
	struct fd_context *ctx = fd_context(pctx);
	ctx->prog.hs = hwcso;
	ctx->dirty_shader[PIPE_SHADER_TESS_CTRL] |= FD_DIRTY_SHADER_PROG;
	ctx->dirty |= FD_DIRTY_PROG;
}

static void
fd_tes_state_bind(struct pipe_context *pctx, void *hwcso)
{
	struct fd_context *ctx = fd_context(pctx);
	ctx->prog.ds = hwcso;
	ctx->dirty_shader[PIPE_SHADER_TESS_EVAL] |= FD_DIRTY_SHADER_PROG;
	ctx->dirty |= FD_DIRTY_PROG;
}

static void
fd_gs_state_bind(struct pipe_context *pctx, void *hwcso)
{
	struct fd_context *ctx = fd_context(pctx);
	ctx->prog.gs = hwcso;
	ctx->dirty_shader[PIPE_SHADER_GEOMETRY] |= FD_DIRTY_SHADER_PROG;
	ctx->dirty |= FD_DIRTY_PROG;
}

static void
fd_fs_state_bind(struct pipe_context *pctx, void *hwcso)
{
	struct fd_context *ctx = fd_context(pctx);
	ctx->prog.fs = hwcso;
	ctx->dirty_shader[PIPE_SHADER_FRAGMENT] |= FD_DIRTY_SHADER_PROG;
	ctx->dirty |= FD_DIRTY_PROG;
}

static const char *solid_fs =
	"FRAG                                        \n"
	"PROPERTY FS_COLOR0_WRITES_ALL_CBUFS 1       \n"
	"DCL CONST[0]                                \n"
	"DCL OUT[0], COLOR                           \n"
	"  0: MOV OUT[0], CONST[0]                   \n"
	"  1: END                                    \n";

static const char *solid_vs =
	"VERT                                        \n"
	"DCL IN[0]                                   \n"
	"DCL OUT[0], POSITION                        \n"
	"  0: MOV OUT[0], IN[0]                      \n"
	"  1: END                                    \n";

static const char *blit_vs =
	"VERT                                        \n"
	"DCL IN[0]                                   \n"
	"DCL IN[1]                                   \n"
	"DCL OUT[0], GENERIC[0]                      \n"
	"DCL OUT[1], POSITION                        \n"
	"  0: MOV OUT[0], IN[0]                      \n"
	"  0: MOV OUT[1], IN[1]                      \n"
	"  1: END                                    \n";

static void * assemble_tgsi(struct pipe_context *pctx,
		const char *src, bool frag)
{
	struct tgsi_token toks[32];
	struct pipe_shader_state cso = {
			.tokens = toks,
	};

	bool ret = tgsi_text_translate(src, toks, ARRAY_SIZE(toks));
	assume(ret);

	if (frag)
		return pctx->create_fs_state(pctx, &cso);
	else
		return pctx->create_vs_state(pctx, &cso);
}

static void *
fd_prog_blit(struct pipe_context *pctx, int rts, bool depth)
{
	int i;
	struct ureg_src tc;
	struct ureg_program *ureg;

	debug_assert(rts <= MAX_RENDER_TARGETS);

	ureg = ureg_create(PIPE_SHADER_FRAGMENT);
	if (!ureg)
		return NULL;

	tc = ureg_DECL_fs_input(
			ureg, TGSI_SEMANTIC_GENERIC, 0, TGSI_INTERPOLATE_PERSPECTIVE);
	for (i = 0; i < rts; i++)
		ureg_TEX(ureg, ureg_DECL_output(ureg, TGSI_SEMANTIC_COLOR, i),
				 TGSI_TEXTURE_2D, tc, ureg_DECL_sampler(ureg, i));
	if (depth)
		ureg_TEX(ureg,
				 ureg_writemask(
						 ureg_DECL_output(ureg, TGSI_SEMANTIC_POSITION, 0),
						 TGSI_WRITEMASK_Z),
				 TGSI_TEXTURE_2D, tc, ureg_DECL_sampler(ureg, rts));

	ureg_END(ureg);

	return ureg_create_shader_and_destroy(ureg, pctx);
}


void fd_prog_init(struct pipe_context *pctx)
{
	struct fd_context *ctx = fd_context(pctx);
	int i;

	pctx->bind_vs_state = fd_vs_state_bind;
	pctx->bind_tcs_state = fd_tcs_state_bind;
	pctx->bind_tes_state = fd_tes_state_bind;
	pctx->bind_gs_state = fd_gs_state_bind;
	pctx->bind_fs_state = fd_fs_state_bind;

	ctx->solid_prog.fs = assemble_tgsi(pctx, solid_fs, true);
	ctx->solid_prog.vs = assemble_tgsi(pctx, solid_vs, false);
	ctx->blit_prog[0].vs = assemble_tgsi(pctx, blit_vs, false);
	ctx->blit_prog[0].fs = fd_prog_blit(pctx, 1, false);

	if (ctx->screen->gpu_id < 300)
		return;

	for (i = 1; i < ctx->screen->max_rts; i++) {
		ctx->blit_prog[i].vs = ctx->blit_prog[0].vs;
		ctx->blit_prog[i].fs = fd_prog_blit(pctx, i + 1, false);
	}

	ctx->blit_z.vs = ctx->blit_prog[0].vs;
	ctx->blit_z.fs = fd_prog_blit(pctx, 0, true);
	ctx->blit_zs.vs = ctx->blit_prog[0].vs;
	ctx->blit_zs.fs = fd_prog_blit(pctx, 1, true);
}

void fd_prog_fini(struct pipe_context *pctx)
{
	struct fd_context *ctx = fd_context(pctx);
	int i;

	pctx->delete_vs_state(pctx, ctx->solid_prog.vs);
	pctx->delete_fs_state(pctx, ctx->solid_prog.fs);
	pctx->delete_vs_state(pctx, ctx->blit_prog[0].vs);
	for (i = 0; i < ctx->screen->max_rts; i++)
		pctx->delete_fs_state(pctx, ctx->blit_prog[i].fs);
	pctx->delete_fs_state(pctx, ctx->blit_z.fs);
	pctx->delete_fs_state(pctx, ctx->blit_zs.fs);
}
