/*
  Copyright (c) 2006-2009 Gluster, Inc. <http://www.gluster.com>
  This file is part of GlusterFS.

  GlusterFS is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published
  by the Free Software Foundation; either version 3 of the License,
  or (at your option) any later version.

  GlusterFS is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see
  <http://www.gnu.org/licenses/>.
*/

#ifndef _CONFIG_H
#define _CONFIG_H
#include "config.h"
#endif
#include <inttypes.h>


#include "globals.h"
#include "glusterfs.h"
#include "compat.h"
#include "dict.h"
#include "protocol-common.h"
#include "xlator.h"
#include "logging.h"
#include "timer.h"
#include "defaults.h"
#include "compat.h"
#include "compat-errno.h"
#include "statedump.h"
#include "glusterd-mem-types.h"
#include "glusterd.h"
#include "glusterd-sm.h"
#include "glusterd-op-sm.h"
#include "glusterd-utils.h"

#include "glusterd1.h"
#include "cli1.h"
#include "rpc-clnt.h"
#include "glusterd1-xdr.h"

#include <sys/resource.h>
#include <inttypes.h>

/* for default_*_cbk functions */
#include "defaults.c"
#include "common-utils.h"


/*typedef int32_t (*glusterd_mop_t) (call_frame_t *frame,
                            gf_hdr_common_t *hdr, size_t hdrlen);*/

//static glusterd_mop_t glusterd_ops[GF_MOP_MAXVALUE];



static int
glusterd_friend_find_by_hostname (const char *hoststr,
                                  glusterd_peerinfo_t  **peerinfo)
{
        int                     ret = -1;
        glusterd_conf_t         *priv = NULL;
        glusterd_peerinfo_t     *entry = NULL;

        GF_ASSERT (hoststr);
        GF_ASSERT (peerinfo);

        *peerinfo = NULL;
        priv    = THIS->private;

        GF_ASSERT (priv);

        list_for_each_entry (entry, &priv->peers, uuid_list) {
                if (entry->hostname && (!strncmp (entry->hostname, hoststr,
                                        sizeof (entry->hostname)))) {

                        gf_log ("glusterd", GF_LOG_NORMAL,
                                 "Friend %s found.. state: %d", hoststr,
                                  entry->state.state);
                        *peerinfo = entry;
                        return 0;
                }
        }

        return ret;
}

static int
glusterd_friend_find_by_uuid (uuid_t uuid,
                              glusterd_peerinfo_t  **peerinfo)
{
        int                     ret = -1;
        glusterd_conf_t         *priv = NULL;
        glusterd_peerinfo_t     *entry = NULL;

        GF_ASSERT (peerinfo);

        *peerinfo = NULL;
        priv    = THIS->private;

        GF_ASSERT (priv);

        list_for_each_entry (entry, &priv->peers, uuid_list) {
                if (!uuid_compare (entry->uuid, uuid)) {

                        gf_log ("glusterd", GF_LOG_NORMAL,
                                 "Friend found.. state: %d",
                                  entry->state.state);
                        *peerinfo = entry;
                        return 0;
                }
        }

        return ret;
}

static int
glusterd_handle_friend_req (rpcsvc_request_t *req, uuid_t  uuid, char *hostname)
{
        int                             ret = -1;
        glusterd_peerinfo_t             *peerinfo = NULL;
        glusterd_friend_sm_event_t      *event = NULL;
        glusterd_friend_req_ctx_t       *ctx = NULL;


        ret = glusterd_friend_find (uuid, hostname, &peerinfo);

        if (ret) {
                gf_log ("glusterd", GF_LOG_NORMAL,
                         "Unable to find peer");

        }

        ret = glusterd_friend_sm_new_event
                        (GD_FRIEND_EVENT_RCVD_FRIEND_REQ, &event);

        if (ret) {
                gf_log ("", GF_LOG_ERROR, "event generation failed: %d", ret);
                return ret;
        }

        event->peerinfo = peerinfo;

        ctx = GF_CALLOC (1, sizeof (*ctx), gf_gld_mt_friend_req_ctx_t);

        if (!ctx) {
                gf_log ("", GF_LOG_ERROR, "Unable to allocate memory");
                ret = -1;
                goto out;
        }

        uuid_copy (ctx->uuid, uuid);
        if (hostname)
                ctx->hostname = gf_strdup (hostname);
        ctx->req = req;

        event->ctx = ctx;

        ret = glusterd_friend_sm_inject_event (event);

        if (ret) {
                gf_log ("glusterd", GF_LOG_ERROR, "Unable to inject event %d, "
                        "ret = %d", event->event, ret);
                goto out;
        }

        ret = 0;

out:
        if (0 != ret) {
                if (ctx && ctx->hostname)
                        GF_FREE (ctx->hostname);
                if (ctx)
                        GF_FREE (ctx);
        }

        return ret;
}


static int
glusterd_handle_unfriend_req (rpcsvc_request_t *req, uuid_t  uuid, char *hostname)
{
        int                             ret = -1;
        glusterd_peerinfo_t             *peerinfo = NULL;
        glusterd_friend_sm_event_t      *event = NULL;
        glusterd_friend_req_ctx_t       *ctx = NULL;

        ret = glusterd_friend_find (uuid, hostname, &peerinfo);

        if (ret) {
                gf_log ("glusterd", GF_LOG_NORMAL,
                         "Unable to find peer");

        }

        ret = glusterd_friend_sm_new_event
                        (GD_FRIEND_EVENT_REMOVE_FRIEND, &event);

        if (ret) {
                gf_log ("", GF_LOG_ERROR, "event generation failed: %d", ret);
                return ret;
        }

        event->peerinfo = peerinfo;

        ctx = GF_CALLOC (1, sizeof (*ctx), gf_gld_mt_friend_req_ctx_t);

        if (!ctx) {
                gf_log ("", GF_LOG_ERROR, "Unable to allocate memory");
                ret = -1;
                goto out;
        }

        uuid_copy (ctx->uuid, uuid);
        if (hostname)
                ctx->hostname = gf_strdup (hostname);
        ctx->req = req;

        event->ctx = ctx;

        ret = glusterd_friend_sm_inject_event (event);

        if (ret) {
                gf_log ("glusterd", GF_LOG_ERROR, "Unable to inject event %d, "
                        "ret = %d", event->event, ret);
                goto out;
        }

        ret = 0;

out:
        if (0 != ret) {
                if (ctx && ctx->hostname)
                        GF_FREE (ctx->hostname);
                if (ctx)
                        GF_FREE (ctx);
        }

        return ret;
}

static int
glusterd_add_peer_detail_to_dict (glusterd_peerinfo_t   *peerinfo,
                                  dict_t  *friends, int   count)
{

        int             ret = -1;
        char            key[256] = {0, };

        GF_ASSERT (peerinfo);
        GF_ASSERT (friends);

        snprintf (key, 256, "friend%d.uuid", count);
        uuid_unparse (peerinfo->uuid, peerinfo->uuid_str);
        ret = dict_set_str (friends, key, peerinfo->uuid_str);
        if (ret)
                goto out;

        snprintf (key, 256, "friend%d.hostname", count);
        ret = dict_set_str (friends, key, peerinfo->hostname);
        if (ret)
                goto out;

        snprintf (key, 256, "friend%d.state", count);
        ret = dict_set_int32 (friends, key, (int32_t)peerinfo->state.state);
        if (ret)
                goto out;

out:
        return ret;
}

int
glusterd_friend_find (uuid_t uuid, char *hostname,
                      glusterd_peerinfo_t **peerinfo)
{
        int     ret = -1;

        if (uuid) {
                ret = glusterd_friend_find_by_uuid (uuid, peerinfo);

                if (ret) {
                        gf_log ("glusterd", GF_LOG_NORMAL,
                                 "Unable to find peer by uuid");
                } else {
                        goto out;
                }

        }

        if (hostname) {
                ret = glusterd_friend_find_by_hostname (hostname, peerinfo);

                if (ret) {
                        gf_log ("glusterd", GF_LOG_NORMAL,
                                "Unable to find hostname: %s", hostname);
                } else {
                        goto out;
                }
        }

out:
        return ret;
}

int
glusterd_handle_cluster_lock (rpcsvc_request_t *req)
{
        gd1_mgmt_cluster_lock_req       lock_req = {{0},};
        int32_t                         ret = -1;
        char                            str[50] = {0,};
        glusterd_op_sm_event_t          *event = NULL;
        glusterd_op_lock_ctx_t          *ctx = NULL;

        GF_ASSERT (req);

        if (!gd_xdr_to_mgmt_friend_req (req->msg[0], &lock_req)) {
                //failed to decode msg;
                req->rpc_err = GARBAGE_ARGS;
                goto out;
        }
        uuid_unparse (lock_req.uuid, str);

        gf_log ("glusterd", GF_LOG_NORMAL,
                "Received LOCK from uuid: %s", str);

        ret = glusterd_op_sm_new_event (GD_OP_EVENT_LOCK, &event);

        if (ret) {
                //respond back here
                return ret;
        }

        ctx = GF_CALLOC (1, sizeof (*ctx), gf_gld_mt_op_stage_ctx_t);

        if (!ctx) {
                //respond here
                return -1;
        }

        uuid_copy (ctx->uuid, lock_req.uuid);
        ctx->req = req;
        event->ctx = ctx;

        ret = glusterd_op_sm_inject_event (event);

out:
        gf_log ("", GF_LOG_NORMAL, "Returning %d", ret);

        return ret;
}

int
glusterd_handle_stage_op (rpcsvc_request_t *req)
{
        int32_t                         ret = -1;
        char                            str[50];
        gd1_mgmt_stage_op_req           stage_req = {{0,}};
        glusterd_op_sm_event_t          *event = NULL;
        glusterd_op_stage_ctx_t         *ctx = NULL;

        GF_ASSERT (req);

        if (!gd_xdr_to_mgmt_stage_op_req (req->msg[0], &stage_req)) {
                //failed to decode msg;
                req->rpc_err = GARBAGE_ARGS;
                goto out;
        }

        uuid_unparse (stage_req.uuid, str);
        gf_log ("glusterd", GF_LOG_NORMAL,
                "Received stage op from uuid: %s", str);

        ret = glusterd_op_sm_new_event (GD_OP_EVENT_STAGE_OP, &event);

        if (ret) {
                //respond back here
                return ret;
        }

        ctx = GF_CALLOC (1, sizeof (*ctx), gf_gld_mt_op_stage_ctx_t);

        if (!ctx) {
                //respond here
                return -1;
        }

        //CHANGE THIS
        uuid_copy (ctx->stage_req.uuid, stage_req.uuid);
        ctx->stage_req.op = stage_req.op;
        ctx->stage_req.buf.buf_len = stage_req.buf.buf_len;
        ctx->stage_req.buf.buf_val = GF_CALLOC (1, stage_req.buf.buf_len,
                                                gf_gld_mt_string);
        if (!ctx->stage_req.buf.buf_val)
                goto out;

        memcpy (ctx->stage_req.buf.buf_val, stage_req.buf.buf_val,
                stage_req.buf.buf_len);


        ctx->req   = req;
        event->ctx = ctx;

        ret = glusterd_op_sm_inject_event (event);

out:
        return ret;
}

int
glusterd_handle_commit_op (rpcsvc_request_t *req)
{
        int32_t                         ret = -1;
        char                            str[50];
        glusterd_op_sm_event_t          *event = NULL;
        gd1_mgmt_commit_op_req          commit_req = {{0},};
        glusterd_op_commit_ctx_t        *ctx = NULL;

        GF_ASSERT (req);

        if (!gd_xdr_to_mgmt_commit_op_req (req->msg[0], &commit_req)) {
                //failed to decode msg;
                req->rpc_err = GARBAGE_ARGS;
                goto out;
        }

        uuid_unparse (commit_req.uuid, str);

        gf_log ("glusterd", GF_LOG_NORMAL,
                "Received commit op from uuid: %s", str);

        ret = glusterd_op_sm_new_event (GD_OP_EVENT_COMMIT_OP, &event);

        if (ret) {
                //respond back here
                return ret;
        }

        ctx = GF_CALLOC (1, sizeof (*ctx), gf_gld_mt_op_commit_ctx_t);

        if (!ctx) {
                //respond here
                return -1;
        }

        ctx->req = req;
        //CHANGE THIS
        uuid_copy (ctx->stage_req.uuid, commit_req.uuid);
        ctx->stage_req.op = commit_req.op;
        ctx->stage_req.buf.buf_len = commit_req.buf.buf_len;
        ctx->stage_req.buf.buf_val = GF_CALLOC (1, commit_req.buf.buf_len,
                                                gf_gld_mt_string);
        if (!ctx->stage_req.buf.buf_val)
                goto out;

        memcpy (ctx->stage_req.buf.buf_val, commit_req.buf.buf_val,
                commit_req.buf.buf_len);
        event->ctx = ctx;

        ret = glusterd_op_sm_inject_event (event);

out:
        return ret;
}

int
glusterd_handle_cli_probe (rpcsvc_request_t *req)
{
        int32_t                         ret = -1;
        gf1_cli_probe_req               cli_req = {0,};

        GF_ASSERT (req);

        if (!gf_xdr_to_cli_probe_req (req->msg[0], &cli_req)) {
                //failed to decode msg;
                req->rpc_err = GARBAGE_ARGS;
                goto out;
        }

        gf_log ("glusterd", GF_LOG_NORMAL, "Received CLI probe req");


        ret = glusterd_probe_begin (req, cli_req.hostname);

out:
        return ret;
}

int
glusterd_handle_cli_deprobe (rpcsvc_request_t *req)
{
        int32_t                         ret = -1;
        gf1_cli_probe_req               cli_req = {0,};

        GF_ASSERT (req);

        if (!gf_xdr_to_cli_probe_req (req->msg[0], &cli_req)) {
                //failed to decode msg;
                req->rpc_err = GARBAGE_ARGS;
                goto out;
        }

        gf_log ("glusterd", GF_LOG_NORMAL, "Received CLI deprobe req");


        ret = glusterd_deprobe_begin (req, cli_req.hostname);

out:
        return ret;
}

int
glusterd_handle_cli_list_friends (rpcsvc_request_t *req)
{
        int32_t                         ret = -1;
        gf1_cli_peer_list_req           cli_req = {0,};
        dict_t                          *dict = NULL;

        GF_ASSERT (req);

        if (!gf_xdr_to_cli_peer_list_req (req->msg[0], &cli_req)) {
                //failed to decode msg;
                req->rpc_err = GARBAGE_ARGS;
                goto out;
        }

        gf_log ("glusterd", GF_LOG_NORMAL, "Received cli list req");

        if (cli_req.dict.dict_len) {
                /* Unserialize the dictionary */
                dict  = dict_new ();

                ret = dict_unserialize (cli_req.dict.dict_val,
                                        cli_req.dict.dict_len,
                                        &dict);
                if (ret < 0) {
                        gf_log ("glusterd", GF_LOG_ERROR,
                                "failed to "
                                "unserialize req-buffer to dictionary");
                        goto out;
                }
        }

        ret = glusterd_list_friends (req, dict, cli_req.flags);

out:
        return ret;
}

int
glusterd_handle_create_volume (rpcsvc_request_t *req)
{
        int32_t                         ret = -1;
        gf1_cli_create_vol_req          cli_req = {0,};
        dict_t                          *dict = NULL;

        GF_ASSERT (req);

        if (!gf_xdr_to_cli_create_vol_req (req->msg[0], &cli_req)) {
                //failed to decode msg;
                req->rpc_err = GARBAGE_ARGS;
                goto out;
        }

        gf_log ("glusterd", GF_LOG_NORMAL, "Received create volume req");

        if (cli_req.bricks.bricks_len) {
                /* Unserialize the dictionary */
                dict  = dict_new ();

                ret = dict_unserialize (cli_req.bricks.bricks_val,
                                        cli_req.bricks.bricks_len,
                                        &dict);
                if (ret < 0) {
                        gf_log ("glusterd", GF_LOG_ERROR,
                                "failed to "
                                "unserialize req-buffer to dictionary");
                        goto out;
                }
        }

        ret = glusterd_create_volume (req, dict);

out:
        return ret;
}

int
glusterd_op_lock_send_resp (rpcsvc_request_t *req, int32_t status)
{

        gd1_mgmt_cluster_lock_rsp       rsp = {{0},};
        int                             ret = -1;

        GF_ASSERT (req);
        glusterd_get_uuid (&rsp.uuid);
        rsp.op_ret = status;

        ret = glusterd_submit_reply (req, &rsp, NULL, 0, NULL,
                                     gd_xdr_serialize_mgmt_cluster_lock_rsp);

        gf_log ("glusterd", GF_LOG_NORMAL,
                "Responded, ret: %d", ret);

        return 0;
}

int
glusterd_op_unlock_send_resp (rpcsvc_request_t *req, int32_t status)
{

        gd1_mgmt_cluster_unlock_rsp     rsp = {{0},};
        int                             ret = -1;

        GF_ASSERT (req);
        rsp.op_ret = status;
        glusterd_get_uuid (&rsp.uuid);

        ret = glusterd_submit_reply (req, &rsp, NULL, 0, NULL,
                                     gd_xdr_serialize_mgmt_cluster_unlock_rsp);

        gf_log ("glusterd", GF_LOG_NORMAL,
                "Responded to unlock, ret: %d", ret);

        return ret;
}

int
glusterd_handle_cluster_unlock (rpcsvc_request_t *req)
{
        gd1_mgmt_cluster_unlock_req     unlock_req = {{0}, };
        int32_t                         ret = -1;
        char                            str[50] = {0, };
        glusterd_op_lock_ctx_t          *ctx = NULL;
        glusterd_op_sm_event_t          *event = NULL;

        GF_ASSERT (req);

        if (!gd_xdr_to_mgmt_friend_req (req->msg[0], &unlock_req)) {
                //failed to decode msg;
                req->rpc_err = GARBAGE_ARGS;
                goto out;
        }

        uuid_unparse (unlock_req.uuid, str);

        gf_log ("glusterd", GF_LOG_NORMAL,
                "Received UNLOCK from uuid: %s", str);

        ret = glusterd_op_sm_new_event (GD_OP_EVENT_UNLOCK, &event);

        if (ret) {
                //respond back here
                return ret;
        }

        ctx = GF_CALLOC (1, sizeof (*ctx), gf_gld_mt_op_lock_ctx_t);

        if (!ctx) {
                //respond here
                return -1;
        }
        event->ctx = ctx;
        uuid_copy (ctx->uuid, unlock_req.uuid);
        ctx->req = req;

        ret = glusterd_op_sm_inject_event (event);

out:
        return ret;
}

int
glusterd_op_stage_send_resp (rpcsvc_request_t   *req,
                             int32_t op, int32_t status)
{

        gd1_mgmt_stage_op_rsp           rsp = {{0},};
        int                             ret = -1;

        GF_ASSERT (req);
        rsp.op_ret = status;
        glusterd_get_uuid (&rsp.uuid);
        rsp.op = op;

        ret = glusterd_submit_reply (req, &rsp, NULL, 0, NULL,
                                     gd_xdr_serialize_mgmt_stage_op_rsp);

        gf_log ("glusterd", GF_LOG_NORMAL,
                "Responded to stage, ret: %d", ret);

        return ret;
}

int
glusterd_op_commit_send_resp (rpcsvc_request_t *req,
                               int32_t op, int32_t status)
{
        gd1_mgmt_commit_op_rsp          rsp = {{0}, };
        int                             ret = -1;

        GF_ASSERT (req);
        rsp.op_ret = status;
        glusterd_get_uuid (&rsp.uuid);
        rsp.op = op;

        ret = glusterd_submit_reply (req, &rsp, NULL, 0, NULL,
                                     gd_xdr_serialize_mgmt_commit_op_rsp);

        gf_log ("glusterd", GF_LOG_NORMAL,
                "Responded to commit, ret: %d", ret);

        return ret;
}

int
glusterd_handle_incoming_friend_req (rpcsvc_request_t *req)
{
        int32_t                 ret = -1;
        gd1_mgmt_friend_req     friend_req = {{0},};
        char                    str[50];

        GF_ASSERT (req);
        if (!gd_xdr_to_mgmt_friend_req (req->msg[0], &friend_req)) {
                //failed to decode msg;
                req->rpc_err = GARBAGE_ARGS;
                goto out;
        }
        uuid_unparse (friend_req.uuid, str);

        gf_log ("glusterd", GF_LOG_NORMAL,
                "Received probe from uuid: %s", str);

        ret = glusterd_handle_friend_req (req, friend_req.uuid,
                                          friend_req.hostname);

out:

        return ret;
}

int
glusterd_handle_incoming_unfriend_req (rpcsvc_request_t *req)
{
        int32_t                 ret = -1;
        gd1_mgmt_friend_req     friend_req = {{0},};
        char                    str[50];

        GF_ASSERT (req);
        if (!gd_xdr_to_mgmt_friend_req (req->msg[0], &friend_req)) {
                //failed to decode msg;
                req->rpc_err = GARBAGE_ARGS;
                goto out;
        }
        uuid_unparse (friend_req.uuid, str);

        gf_log ("glusterd", GF_LOG_NORMAL,
                "Received unfriend from uuid: %s", str);

        ret = glusterd_handle_unfriend_req (req, friend_req.uuid,
                                            friend_req.hostname);

out:

        return ret;
}

int
glusterd_handle_probe_query (rpcsvc_request_t *req)
{
        int32_t             ret = -1;
        char                str[50];
        xlator_t            *this = NULL;
        glusterd_conf_t     *conf = NULL;
        gd1_mgmt_probe_req  probe_req = {{0},};
        gd1_mgmt_probe_rsp  rsp = {{0},};
        char                hostname[1024] = {0};

        GF_ASSERT (req);

        probe_req.hostname = hostname;

        if (!gd_xdr_to_mgmt_probe_req (req->msg[0], &probe_req)) {
                //failed to decode msg;
                req->rpc_err = GARBAGE_ARGS;
                goto out;
        }


        uuid_unparse (probe_req.uuid, str);

        gf_log ("glusterd", GF_LOG_NORMAL,
                "Received probe from uuid: %s", str);


        this = THIS;

        conf = this->private;

        uuid_copy (rsp.uuid, conf->uuid);
        rsp.hostname = gf_strdup (probe_req.hostname);

        ret = glusterd_submit_reply (req, &rsp, NULL, 0, NULL,
                                     gd_xdr_serialize_mgmt_probe_rsp);

        gf_log ("glusterd", GF_LOG_NORMAL,
                "Responded to %s, ret: %d", probe_req.hostname, ret);

out:
        return ret;
}

/*int
glusterd_handle_friend_req_resp (call_frame_t *frame,
                                 gf_hdr_common_t *rsp_hdr, size_t hdrlen)
{
        gf_mop_probe_rsp_t      *rsp = NULL;
        int32_t                 ret = -1;
        char                    str[50];
        glusterd_peerinfo_t     *peerinfo = NULL;
        int32_t                 op_ret = -1;
        glusterd_friend_sm_event_type_t event_type = GD_FRIEND_EVENT_NONE;
        glusterd_friend_sm_event_t      *event = NULL;

        GF_ASSERT (rsp_hdr);

        rsp   = gf_param (rsp_hdr);
        uuid_unparse (rsp->uuid, str);

        op_ret = rsp_hdr->rsp.op_ret;

        gf_log ("glusterd", GF_LOG_NORMAL,
                "Received %s from uuid: %s, host: %s",
                (op_ret)?"RJT":"ACC", str, rsp->hostname);

        ret = glusterd_friend_find (rsp->uuid, rsp->hostname, &peerinfo);

        if (ret) {
                GF_ASSERT (0);
        }

        if (op_ret)
                event_type = GD_FRIEND_EVENT_RCVD_ACC;
        else
                event_type = GD_FRIEND_EVENT_RCVD_RJT;

        ret = glusterd_friend_sm_new_event (event_type, &event);

        if (ret) {
                gf_log ("glusterd", GF_LOG_ERROR,
                         "Unable to get event");
                return ret;
        }

        event->peerinfo = peerinfo;
        ret = glusterd_friend_sm_inject_event (event);

        gf_log ("glusterd", GF_LOG_NORMAL, "Received resp to friend req");

        return 0;
}*/

/*int
glusterd_handle_probe_resp (call_frame_t *frame, gf_hdr_common_t *rsp_hdr,
                            size_t hdrlen)
{
        gf_mop_probe_rsp_t      *rsp = NULL;
        int32_t                 ret = -1;
        char                    str[50];
        glusterd_peerinfo_t        *peerinfo = NULL;
        glusterd_friend_sm_event_t *event = NULL;
        glusterd_peerinfo_t        *dup_peerinfo = NULL;

        GF_ASSERT (rsp_hdr);

        rsp   = gf_param (rsp_hdr);
        uuid_unparse (rsp->uuid, str);

        gf_log ("glusterd", GF_LOG_NORMAL,
                "Received probe resp from uuid: %s, host: %s",
                str, rsp->hostname);

        ret = glusterd_friend_find (rsp->uuid, rsp->hostname, &peerinfo);

        if (ret) {
                GF_ASSERT (0);
        }

        if (!peerinfo->hostname) {
                glusterd_friend_find_by_hostname (rsp->hostname, &dup_peerinfo);
                GF_ASSERT (dup_peerinfo);
                GF_ASSERT (dup_peerinfo->hostname);
                peerinfo->hostname = gf_strdup (rsp->hostname);
                peerinfo->trans = dup_peerinfo->trans;
                list_del_init (&dup_peerinfo->uuid_list);
                GF_FREE (dup_peerinfo->hostname);
                GF_FREE (dup_peerinfo);
        }
        GF_ASSERT (peerinfo->hostname);
        uuid_copy (peerinfo->uuid, rsp->uuid);

        ret = glusterd_friend_sm_new_event
                        (GD_FRIEND_EVENT_INIT_FRIEND_REQ, &event);

        if (ret) {
                gf_log ("glusterd", GF_LOG_ERROR,
                         "Unable to get event");
                return ret;
        }

        event->peerinfo = peerinfo;
        ret = glusterd_friend_sm_inject_event (event);

        return 0;
}*/

/*
static glusterd_mop_t glusterd_ops[GF_MOP_MAXVALUE] = {
        [GF_MOP_PROBE_QUERY]     = glusterd_handle_probe_query,
        [GF_MOP_FRIEND_REQ]      = glusterd_handle_incoming_friend_req,
        [GF_MOP_STAGE_OP]        = glusterd_handle_stage_op,
        [GF_MOP_COMMIT_OP]       = glusterd_handle_commit_op,
        [GF_MOP_CLUSTER_LOCK]    = glusterd_handle_cluster_lock,
        [GF_MOP_CLUSTER_UNLOCK]  = glusterd_handle_cluster_unlock,
};

static glusterd_mop_t glusterd_resp_ops [GF_MOP_MAXVALUE] = {
        [GF_MOP_PROBE_QUERY]    = glusterd_handle_probe_resp,
        [GF_MOP_FRIEND_REQ]     = glusterd_handle_friend_req_resp,
};
*/

/*int
glusterd_xfer_probe_msg (glusterd_peerinfo_t *peerinfo, xlator_t *this)
{
        gf_hdr_common_t       *hdr = NULL;
        gf_mop_probe_req_t    *req = NULL;
        size_t                hdrlen = -1;
        int                   ret = -1;
        glusterd_conf_t       *priv = NULL;
        call_frame_t          *dummy_frame = NULL;
        int                   len = 0;

        GF_ASSERT (peerinfo);
        GF_ASSERT (this);

        priv = this->private;
        GF_ASSERT (priv);

        len = STRLEN_0 (peerinfo->hostname);
        hdrlen = gf_hdr_len (req, len);
        hdr    = gf_hdr_new (req, len);

        GF_VALIDATE_OR_GOTO (this->name, hdr, unwind);

        req         = gf_param (hdr);
        memcpy (&req->uuid, &priv->uuid, sizeof(uuid_t));
        strncpy (req->hostname, peerinfo->hostname, len);

        dummy_frame = create_frame (this, this->ctx->pool);

        if (!dummy_frame)
                goto unwind;

        dummy_frame->local = peerinfo->trans;

        ret = glusterd_xfer (dummy_frame, this,
                             peerinfo->trans,
                             GF_OP_TYPE_MOP_REQUEST, GF_MOP_PROBE_QUERY,
                             hdr, hdrlen, NULL, 0, NULL);

        return ret;

unwind:
        if (hdr)
                GF_FREE (hdr);

        return 0;
}*/

/*int
glusterd_xfer_friend_req_msg (glusterd_peerinfo_t *peerinfo, xlator_t *this)
{
        gf_hdr_common_t       *hdr = NULL;
        gf_mop_probe_req_t    *req = NULL;
        size_t                hdrlen = -1;
        int                   ret = -1;
        glusterd_conf_t       *priv = NULL;
        call_frame_t          *dummy_frame = NULL;
        int                   len = 0;

        GF_ASSERT (peerinfo);
        GF_ASSERT (this);

        priv = this->private;
        GF_ASSERT (priv);

        len = STRLEN_0 (peerinfo->hostname);
        hdrlen = gf_hdr_len (req, len);
        hdr    = gf_hdr_new (req, len);

        GF_VALIDATE_OR_GOTO (this->name, hdr, unwind);

        req         = gf_param (hdr);
        memcpy (&req->uuid, &priv->uuid, sizeof(uuid_t));
        strncpy (req->hostname, peerinfo->hostname, len);

        dummy_frame = create_frame (this, this->ctx->pool);

        if (!dummy_frame)
                goto unwind;

        dummy_frame->local = peerinfo->trans;

        ret = glusterd_xfer (dummy_frame, this,
                             peerinfo->trans,
                             GF_OP_TYPE_MOP_REQUEST, GF_MOP_FRIEND_REQ,
                             hdr, hdrlen, NULL, 0, NULL);

        return ret;

unwind:
        if (hdr)
                GF_FREE (hdr);

        //STACK_UNWIND (frame, -1, EINVAL, NULL);
        return 0;
}*/

/*int
glusterd_xfer_cluster_lock_req (xlator_t *this, int32_t *lock_count)
{
        gd1_mgmt_cluster_lock_req       req = {{0},};
        int                             ret = -1;
        glusterd_conf_t                 *priv = NULL;
        call_frame_t                    *dummy_frame = NULL;
        glusterd_peerinfo_t             *peerinfo = NULL;
        int                             pending_lock = 0;
        rpc_clnt_procedure_t            *proc = NULL;

        GF_ASSERT (this);
        GF_ASSERT (lock_count);

        priv = this->private;
        GF_ASSERT (priv);

        uuid_copy (req.uuid, priv->uuid);

        dummy_frame = create_frame (this, this->ctx->pool);

        if (!dummy_frame)
                goto unwind;


        list_for_each_entry (peerinfo, &priv->peers, uuid_list) {
                GF_ASSERT (peerinfo);

                if (peerinfo->state.state != GD_FRIEND_STATE_BEFRIENDED)
                        continue;


                ret = glusterd_submit_request (peerinfo, &req, dummy_frame,
                                               prog, GD_MGMT_PROBE_QUERY,
                                               NULL, gd_xdr_from_mgmt_probe_req,
                                               this);
                if (!ret)
                        pending_lock++;
        }

        gf_log ("glusterd", GF_LOG_NORMAL, "Sent lock req to %d peers",
                                            pending_lock);
        *lock_count = pending_lock;

unwind:

        return ret;
}*/

/*int
glusterd_xfer_cluster_unlock_req (xlator_t *this, int32_t *pending_count)
{
        gf_hdr_common_t       *hdr = NULL;
        gf_mop_cluster_unlock_req_t    *req = NULL;
        size_t                hdrlen = -1;
        int                   ret = -1;
        glusterd_conf_t       *priv = NULL;
        call_frame_t          *dummy_frame = NULL;
        glusterd_peerinfo_t   *peerinfo = NULL;
        int                   pending_unlock = 0;

        GF_ASSERT (this);
        GF_ASSERT (pending_count);

        priv = this->private;
        GF_ASSERT (priv);

        hdrlen = gf_hdr_len (req, 0);
        hdr    = gf_hdr_new (req, 0);

        GF_VALIDATE_OR_GOTO (this->name, hdr, unwind);

        req         = gf_param (hdr);
        uuid_copy (req->uuid, priv->uuid);

        dummy_frame = create_frame (this, this->ctx->pool);

        if (!dummy_frame)
                goto unwind;


        list_for_each_entry (peerinfo, &priv->peers, uuid_list) {
                GF_ASSERT (peerinfo);

                if (peerinfo->state.state != GD_FRIEND_STATE_BEFRIENDED)
                        continue;


                ret = glusterd_xfer (dummy_frame, this,
                                     peerinfo->trans,
                                     GF_OP_TYPE_MOP_REQUEST,
                                     GF_MOP_CLUSTER_UNLOCK,
                                     hdr, hdrlen, NULL, 0, NULL);
                if (!ret)
                        pending_unlock++;
        }

        gf_log ("glusterd", GF_LOG_NORMAL, "Sent unlock req to %d peers",
                                            pending_unlock);
        *pending_count = pending_unlock;

unwind:
        if (hdr)
                GF_FREE (hdr);

        return ret;
}*/


int
glusterd_friend_add (const char *hoststr,
                     glusterd_friend_sm_state_t state,
                     uuid_t *uuid,
                     struct rpc_clnt    *rpc,
                     glusterd_peerinfo_t **friend)
{
        int                     ret = 0;
        glusterd_conf_t         *priv = NULL;
        glusterd_peerinfo_t     *peerinfo = NULL;
        dict_t                  *options = NULL;
        char                    *port_str = NULL;
        int                     port_num = 0;
        struct rpc_clnt_config  rpc_cfg = {0,};

        priv = THIS->private;

        peerinfo = GF_CALLOC (1, sizeof(*peerinfo), gf_gld_mt_peerinfo_t);

        if (!peerinfo)
                return -1;

        if (friend)
                *friend = peerinfo;

        peerinfo->state.state = state;
        if (hoststr) {
                peerinfo->hostname = gf_strdup (hoststr);
                rpc_cfg.remote_host = gf_strdup (hoststr);
        }
        INIT_LIST_HEAD (&peerinfo->uuid_list);

        list_add_tail (&peerinfo->uuid_list, &priv->peers);

        if (uuid) {
                uuid_copy (peerinfo->uuid, *uuid);
        }


        if (hoststr) {
                options = dict_new ();
                if (!options)
                        return -1;

                ret = dict_set_str (options, "remote-host", (char *)hoststr);
                if (ret)
                        goto out;


                port_str = getenv ("GLUSTERD_REMOTE_PORT");
                port_num = atoi (port_str);
                rpc_cfg.remote_port = port_num;

                gf_log ("glusterd", GF_LOG_NORMAL, "remote-port: %d", port_num);

                //ret = dict_set_int32 (options, "remote-port", GLUSTERD_DEFAULT_PORT);
                ret = dict_set_int32 (options, "remote-port", port_num);
                if (ret)
                        goto out;

                ret = dict_set_str (options, "transport.address-family", "inet");
                if (ret)
                        goto out;

                rpc = rpc_clnt_init (&rpc_cfg, options, THIS->ctx, THIS->name);

                if (!rpc) {
                        gf_log ("glusterd", GF_LOG_ERROR,
                                "rpc init failed for peer: %s!", hoststr);
                        ret = -1;
                        goto out;
                }

                ret = rpc_clnt_register_notify (rpc, glusterd_rpc_notify,
                                                peerinfo);

                peerinfo->rpc = rpc;

        }

        gf_log ("glusterd", GF_LOG_NORMAL, "connect returned %d", ret);

out:
        return ret;

}

/*int
glusterd_friend_probe (const char *hoststr)
{
        int                     ret = -1;
        glusterd_peerinfo_t     *peerinfo = NULL;


        ret = glusterd_friend_find (NULL, (char *)hoststr, &peerinfo);

        if (ret) {
                //We should not reach this state ideally
                GF_ASSERT (0);
                goto out;
        }

        ret = glusterd_xfer_probe_msg (peerinfo, THIS);

out:
        return ret;
}*/

int
glusterd_probe_begin (rpcsvc_request_t *req, const char *hoststr)
{
        int                             ret = -1;
        glusterd_peerinfo_t             *peerinfo = NULL;
        glusterd_friend_sm_event_t      *event = NULL;
        glusterd_probe_ctx_t            *ctx = NULL;

        GF_ASSERT (hoststr);
        GF_ASSERT (req);

        ret = glusterd_friend_find (NULL, (char *)hoststr, &peerinfo);

        if (ret) {
                gf_log ("glusterd", GF_LOG_NORMAL, "Unable to find peerinfo"
                                " for host: %s", hoststr);
                ret = glusterd_friend_add ((char *)hoststr,
                                           GD_FRIEND_STATE_DEFAULT,
                                           NULL, NULL, &peerinfo);
        }

        ret = glusterd_friend_sm_new_event
                        (GD_FRIEND_EVENT_PROBE, &event);

        if (ret) {
                gf_log ("glusterd", GF_LOG_ERROR, "Unable to get new event");
                return ret;
        }

        ctx = GF_CALLOC (1, sizeof(*ctx), gf_gld_mt_probe_ctx_t);

        if (!ctx) {
                return ret;
        }

        ctx->hostname = gf_strdup (hoststr);
        ctx->req = req;

        event->peerinfo = peerinfo;
        event->ctx = ctx;

        ret = glusterd_friend_sm_inject_event (event);

        if (ret) {
                gf_log ("glusterd", GF_LOG_ERROR, "Unable to inject event %d, "
                        "ret = %d", event->event, ret);
                return ret;
        }

        return ret;
}

int
glusterd_deprobe_begin (rpcsvc_request_t *req, const char *hoststr)
{
        int                             ret = -1;
        glusterd_peerinfo_t             *peerinfo = NULL;
        glusterd_friend_sm_event_t      *event = NULL;
        glusterd_probe_ctx_t            *ctx = NULL;

        GF_ASSERT (hoststr);
        GF_ASSERT (req);

        ret = glusterd_friend_find (NULL, (char *)hoststr, &peerinfo);

        if (ret) {
                gf_log ("glusterd", GF_LOG_NORMAL, "Unable to find peerinfo"
                                " for host: %s", hoststr);
                goto out;
        }

        if (!peerinfo->rpc) {
                //handle this case
                goto out;
        }

        ret = glusterd_friend_sm_new_event
                        (GD_FRIEND_EVENT_INIT_REMOVE_FRIEND, &event);

        if (ret) {
                gf_log ("glusterd", GF_LOG_ERROR, "Unable to get new event");
                return ret;
        }

        ctx = GF_CALLOC (1, sizeof(*ctx), gf_gld_mt_probe_ctx_t);

        if (!ctx) {
                return ret;
        }

        ctx->hostname = gf_strdup (hoststr);
        ctx->req = req;

        event->peerinfo = peerinfo;
        event->ctx = ctx;

        ret = glusterd_friend_sm_inject_event (event);

        if (ret) {
                gf_log ("glusterd", GF_LOG_ERROR, "Unable to inject event %d, "
                        "ret = %d", event->event, ret);
                return ret;
        }

out:
        return ret;
}

/*int
glusterd_interpret (xlator_t *this, transport_t *trans,
                    char *hdr_p, size_t hdrlen, struct iobuf *iobuf)
{
        glusterd_connection_t       *conn = NULL;
        gf_hdr_common_t             *hdr = NULL;
        xlator_t                    *bound_xl = NULL;
        call_frame_t                *frame = NULL;
        peer_info_t                 *peerinfo = NULL;
        int32_t                      type = -1;
        int32_t                      op = -1;
        int32_t                      ret = 0;

        hdr  = (gf_hdr_common_t *)hdr_p;
        type = ntoh32 (hdr->type);
        op   = ntoh32 (hdr->op);

        conn = trans->xl_private;
        if (conn)
                bound_xl = conn->bound_xl;

        if (GF_MOP_PROBE_QUERY != op) {
                //ret = glusterd_validate_sender (hdr, hdrlen);
        }

        peerinfo = &trans->peerinfo;
        switch (type) {

        case GF_OP_TYPE_MOP_REQUEST:
                if ((op < 0) || (op >= GF_MOP_MAXVALUE)) {
                        gf_log (this->name, GF_LOG_ERROR,
                                "invalid mop %"PRId32" from  %s",
                                op, peerinfo->identifier);
                        break;
                }
                frame = glusterd_get_frame_for_call (trans, hdr);
		frame->op = op;
                ret = glusterd_ops[op] (frame, hdr, hdrlen);
                break;

        case GF_OP_TYPE_MOP_REPLY:
                if ((op < 0) || (op >= GF_MOP_MAXVALUE)) {
                        gf_log (this->name, GF_LOG_ERROR,
                                "invalid mop %"PRId32" from  %s",
                                op, peerinfo->identifier);
                        break;
                }
                ret = glusterd_resp_ops[op] (frame, hdr, hdrlen);
                gf_log ("glusterd", GF_LOG_NORMAL, "Obtained MOP response");
                break;


        default:
                gf_log (this->name, GF_LOG_ERROR,
                        "Unknown type: %d", type);
                ret = -1;
                break;
        }

        return ret;
}
*/

int
glusterd_xfer_friend_remove_resp (rpcsvc_request_t *req, char *hostname)
{
        gd1_mgmt_friend_rsp  rsp = {{0}, };
        int32_t              ret = -1;
        xlator_t             *this = NULL;
        glusterd_conf_t      *conf = NULL;

        GF_ASSERT (hostname);

        rsp.op_ret = 0;
        this = THIS;
        GF_ASSERT (this);

        conf = this->private;

        uuid_copy (rsp.uuid, conf->uuid);
        rsp.hostname = hostname;

        ret = glusterd_submit_reply (req, &rsp, NULL, 0, NULL,
                                     gd_xdr_serialize_mgmt_friend_rsp);

        gf_log ("glusterd", GF_LOG_NORMAL,
                "Responded to %s, ret: %d", hostname, ret);
        return ret;
}

int
glusterd_xfer_friend_add_resp (rpcsvc_request_t *req, char *hostname)
{
        gd1_mgmt_friend_rsp  rsp = {{0}, };
        int32_t              ret = -1;
        xlator_t             *this = NULL;
        glusterd_conf_t      *conf = NULL;

        GF_ASSERT (hostname);

        rsp.op_ret = 0;
        this = THIS;
        GF_ASSERT (this);

        conf = this->private;

        uuid_copy (rsp.uuid, conf->uuid);
        rsp.hostname = gf_strdup (hostname);


        ret = glusterd_submit_reply (req, &rsp, NULL, 0, NULL,
                                     gd_xdr_serialize_mgmt_friend_rsp);

        gf_log ("glusterd", GF_LOG_NORMAL,
                "Responded to %s, ret: %d", hostname, ret);
        return ret;
}

int
glusterd_xfer_cli_probe_resp (rpcsvc_request_t *req, int32_t op_ret,
                              int32_t op_errno, char *hostname)
{
        gf1_cli_probe_rsp    rsp = {0, };
        int32_t              ret = -1;

        GF_ASSERT (req);

        rsp.op_ret = op_ret;
        rsp.op_errno = op_errno;
        rsp.hostname = hostname;

        ret = glusterd_submit_reply (req, &rsp, NULL, 0, NULL,
                                     gf_xdr_serialize_cli_probe_rsp);

        gf_log ("glusterd", GF_LOG_NORMAL, "Responded to CLI, ret: %d",ret);

        return ret;
}

int32_t
glusterd_op_txn_begin ()
{
        int32_t                 ret = -1;
        glusterd_conf_t         *priv = NULL;
        glusterd_op_sm_event_t     *event = NULL;
        int32_t                 locked = 0;

        priv = THIS->private;
        GF_ASSERT (priv);

        ret = glusterd_lock (priv->uuid);

        if (ret) {
                gf_log ("glusterd", GF_LOG_ERROR,
                        "Unable to acquire local lock, ret: %d", ret);
                goto out;
        }

        locked = 1;
        gf_log ("glusterd", GF_LOG_NORMAL, "Acquired local lock");

        ret = glusterd_op_sm_new_event (GD_OP_EVENT_START_LOCK, &event);

        if (ret) {
                gf_log ("glusterd", GF_LOG_ERROR,
                        "Unable to get event, ret: %d", ret);
                goto out;
        }

        ret = glusterd_op_sm_inject_event (event);

        gf_log ("glusterd", GF_LOG_NORMAL, "Returning %d", ret);

out:
        if (locked && ret)
                glusterd_unlock (priv->uuid);
        return ret;
}

int32_t
glusterd_create_volume (rpcsvc_request_t *req, dict_t *dict)
{
        int32_t      ret       = -1;
        char        *volname   = NULL;
        char        *bricks    = NULL;
        int          type      = 0;
        //int          sub_count = 2;
        int          count     = 0;
        //char         cmd_str[8192] = {0,};

        GF_ASSERT (req);
        GF_ASSERT (dict);

        glusterd_op_set_op (GD_OP_CREATE_VOLUME);

        glusterd_op_set_ctx (GD_OP_CREATE_VOLUME, dict);

        ret = dict_get_str (dict, "volname", &volname);
        if (ret)
                goto out;

        ret = dict_get_int32 (dict, "type", &type);
        if (ret)
                goto out;

        ret = dict_get_int32 (dict, "count", &count);
        if (ret)
                goto out;

        ret = dict_get_str (dict, "bricks", &bricks);
        if (ret)
                goto out;

        /*switch (type) {
                case GF_CLUSTER_TYPE_REPLICATE:
                {
                        ret = dict_get_int32 (dict, "replica-count", &sub_count);
                        if (ret)
                                goto out;
                        snprintf (cmd_str, 8192,
                                  "glusterfs-volgen -n %s -c /etc/glusterd -r 1 %s",
                                  volname, bricks);
                        ret = system (cmd_str);
                        break;
                }
                case GF_CLUSTER_TYPE_STRIPE:
                {
                        ret = dict_get_int32 (dict, "stripe-count", &sub_count);
                        if (ret)
                                goto out;
                        snprintf (cmd_str, 8192,
                                  "glusterfs-volgen -n %s -c /etc/glusterd -r 0 %s",
                                  volname, bricks);
                        ret = system (cmd_str);
                        break;
                }
                case GF_CLUSTER_TYPE_NONE:
                {
                        snprintf (cmd_str, 8192,
                                  "glusterfs-volgen -n %s -c /etc/glusterd %s",
                                  volname, bricks);
                        ret = system (cmd_str);
                        break;
                }
        } */

        ret = glusterd_op_txn_begin ();

out:
        return ret;
}


int32_t
glusterd_list_friends (rpcsvc_request_t *req, dict_t *dict, int32_t flags)
{
        int32_t                 ret = -1;
        glusterd_conf_t         *priv = NULL;
        glusterd_peerinfo_t     *entry = NULL;
        int32_t                 count = 0;
        dict_t                  *friends = NULL;
        gf1_cli_peer_list_rsp   rsp = {0,};

        priv = THIS->private;
        GF_ASSERT (priv);

        if (!list_empty (&priv->peers)) {
                friends = dict_new ();
                if (!friends) {
                        gf_log ("", GF_LOG_WARNING, "Out of Memory");
                        goto out;
                }
        } else {
                ret = 0;
                goto out;
        }

        if (flags == GF_CLI_LIST_ALL) {
                        list_for_each_entry (entry, &priv->peers, uuid_list) {
                                count++;
                                ret = glusterd_add_peer_detail_to_dict (entry,
                                                                friends, count);
                                if (ret)
                                        goto out;

                        }

                        ret = dict_set_int32 (friends, "count", count);

                        if (ret)
                                goto out;
        }

        ret = dict_allocate_and_serialize (friends, &rsp.friends.friends_val,
                                           (size_t *)&rsp.friends.friends_len);

        if (ret)
                goto out;

        ret = 0;
out:

        if (ret) {
                if (friends)
                        dict_destroy (friends);
        }

        rsp.op_ret = ret;

        ret = glusterd_submit_reply (req, &rsp, NULL, 0, NULL,
                                     gf_xdr_serialize_cli_peer_list_rsp);

        return ret;
}

int
glusterd_rpc_notify (struct rpc_clnt *rpc, void *mydata, rpc_clnt_event_t event,
                     void *data)
{
        xlator_t        *this = NULL;
        char            *handshake = NULL;
        glusterd_conf_t     *conf = NULL;
        int             ret = 0;

        this = mydata;
        conf = this->private;


        switch (event) {
        case RPC_CLNT_CONNECT:
        {
                // connect happened, send 'get_supported_versions' mop
                ret = dict_get_str (this->options, "disable-handshake",
                                    &handshake);

                gf_log (this->name, GF_LOG_TRACE, "got RPC_CLNT_CONNECT");

                if ((ret < 0) || (strcasecmp (handshake, "on"))) {
                        //ret = client_handshake (this, conf->rpc);

                } else {
                        //conf->rpc->connected = 1;
                        ret = default_notify (this, GF_EVENT_CHILD_UP, NULL);
                }
                break;
        }

        case RPC_CLNT_DISCONNECT:

                //Inject friend disconnected here

                gf_log (this->name, GF_LOG_TRACE, "got RPC_CLNT_DISCONNECT");

                //default_notify (this, GF_EVENT_CHILD_DOWN, NULL);
                break;

        default:
                gf_log (this->name, GF_LOG_TRACE,
                        "got some other RPC event %d", event);
                ret = 0;
                break;
        }

        return ret;
}
