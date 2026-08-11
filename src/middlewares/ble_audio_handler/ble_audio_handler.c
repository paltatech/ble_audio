#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/byteorder.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/audio/audio.h>
#include <zephyr/bluetooth/audio/bap.h>
#include <zephyr/bluetooth/audio/pacs.h>
#include <zephyr/logging/log.h>

#include "ble_audio_handler.h"

LOG_MODULE_REGISTER(ble_audio_handler, LOG_LEVEL_INF);

#define AVAILABLE_SINK_CONTEXT                                                                     \
	(BT_AUDIO_CONTEXT_TYPE_UNSPECIFIED | BT_AUDIO_CONTEXT_TYPE_CONVERSATIONAL |                \
	 BT_AUDIO_CONTEXT_TYPE_MEDIA)

static const struct bt_audio_codec_cap lc3_codec_cap = BT_AUDIO_CODEC_CAP_LC3(
	BT_AUDIO_CODEC_CAP_FREQ_ANY, BT_AUDIO_CODEC_CAP_DURATION_10,
	BT_AUDIO_CODEC_CAP_CHAN_COUNT_SUPPORT(1), 40u, 120u, 1u, AVAILABLE_SINK_CONTEXT);

static const struct bt_audio_codec_qos_pref qos_pref =
	BT_AUDIO_CODEC_QOS_PREF(true, BT_GAP_LE_PHY_2M, 0x02, 10, 40000, 40000, 40000, 40000);

static struct bt_bap_stream sink_streams[CONFIG_BT_ASCS_ASE_SNK_COUNT];
static struct bt_pacs_cap cap_sink = {
	.codec_cap = &lc3_codec_cap,
};

static struct bt_conn *default_conn;
static struct bt_le_ext_adv *adv;
static const struct ble_audio_handler_cb *user_cb;

static uint8_t unicast_server_addata[] = {
	BT_UUID_16_ENCODE(BT_UUID_ASCS_VAL),
	BT_AUDIO_UNICAST_ANNOUNCEMENT_TARGETED,
	BT_BYTES_LIST_LE16(AVAILABLE_SINK_CONTEXT),
	BT_BYTES_LIST_LE16(0x0000), /* No source context - sink only */
	0x00,			    /* Metadata length */
};

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(BT_UUID_ASCS_VAL)),
	BT_DATA(BT_DATA_SVC_DATA16, unicast_server_addata, ARRAY_SIZE(unicast_server_addata)),
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static struct bt_bap_stream *stream_alloc(void)
{
	for (size_t i = 0; i < ARRAY_SIZE(sink_streams); i++) {
		if (!sink_streams[i].conn) {
			return &sink_streams[i];
		}
	}

	return NULL;
}

static int ascs_config(struct bt_conn *conn, const struct bt_bap_ep *ep, enum bt_audio_dir dir,
		       const struct bt_audio_codec_cfg *codec_cfg, struct bt_bap_stream **stream,
		       struct bt_audio_codec_qos_pref *const pref, struct bt_bap_ascs_rsp *rsp)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(ep);
	ARG_UNUSED(codec_cfg);

	if (dir != BT_AUDIO_DIR_SINK) {
		/* Unreachable in practice: CONFIG_BT_ASCS_ASE_SRC_COUNT=0 means
		 * ASCS never offers a source ASE to negotiate in the first
		 * place. Kept as a defensive guard.
		 */
		*rsp = BT_BAP_ASCS_RSP(BT_BAP_ASCS_RSP_CODE_CONF_UNSUPPORTED,
				       BT_BAP_ASCS_REASON_NONE);
		return -ENOTSUP;
	}

	*stream = stream_alloc();
	if (*stream == NULL) {
		LOG_WRN("No streams available");
		*rsp = BT_BAP_ASCS_RSP(BT_BAP_ASCS_RSP_CODE_NO_MEM, BT_BAP_ASCS_REASON_NONE);
		return -ENOMEM;
	}

	*pref = qos_pref;

	return 0;
}

static int ascs_reconfig(struct bt_bap_stream *stream, enum bt_audio_dir dir,
			 const struct bt_audio_codec_cfg *codec_cfg,
			 struct bt_audio_codec_qos_pref *const pref, struct bt_bap_ascs_rsp *rsp)
{
	ARG_UNUSED(stream);
	ARG_UNUSED(dir);
	ARG_UNUSED(codec_cfg);
	ARG_UNUSED(pref);

	*rsp = BT_BAP_ASCS_RSP(BT_BAP_ASCS_RSP_CODE_CONF_UNSUPPORTED, BT_BAP_ASCS_REASON_NONE);

	return -ENOTSUP;
}

static int ascs_qos(struct bt_bap_stream *stream, const struct bt_audio_codec_qos *qos,
		    struct bt_bap_ascs_rsp *rsp)
{
	ARG_UNUSED(stream);
	ARG_UNUSED(qos);
	ARG_UNUSED(rsp);

	return 0;
}

static int ascs_enable(struct bt_bap_stream *stream, const uint8_t meta[], size_t meta_len,
		       struct bt_bap_ascs_rsp *rsp)
{
	struct ble_audio_handler_stream_info info;
	int ret;

	ARG_UNUSED(meta);
	ARG_UNUSED(meta_len);

	ret = bt_audio_codec_cfg_get_freq(stream->codec_cfg);
	if (ret <= 0) {
		LOG_ERR("Codec frequency not set");
		*rsp = BT_BAP_ASCS_RSP(BT_BAP_ASCS_RSP_CODE_CONF_INVALID,
				       BT_BAP_ASCS_REASON_CODEC_DATA);
		return -EINVAL;
	}
	info.freq_hz = bt_audio_codec_cfg_freq_to_freq_hz(ret);

	ret = bt_audio_codec_cfg_get_frame_dur(stream->codec_cfg);
	if (ret <= 0) {
		LOG_ERR("Frame duration not set");
		*rsp = BT_BAP_ASCS_RSP(BT_BAP_ASCS_RSP_CODE_CONF_INVALID,
				       BT_BAP_ASCS_REASON_CODEC_DATA);
		return -EINVAL;
	}
	info.frame_duration_us = bt_audio_codec_cfg_frame_dur_to_frame_dur_us(ret);

	if (user_cb != NULL && user_cb->stream_configured != NULL) {
		user_cb->stream_configured(&info);
	}

	return 0;
}

static int ascs_start(struct bt_bap_stream *stream, struct bt_bap_ascs_rsp *rsp)
{
	ARG_UNUSED(stream);
	ARG_UNUSED(rsp);

	return 0;
}

static int ascs_metadata(struct bt_bap_stream *stream, const uint8_t meta[], size_t meta_len,
			 struct bt_bap_ascs_rsp *rsp)
{
	ARG_UNUSED(stream);
	ARG_UNUSED(meta);
	ARG_UNUSED(meta_len);
	ARG_UNUSED(rsp);

	return 0;
}

static int ascs_disable(struct bt_bap_stream *stream, struct bt_bap_ascs_rsp *rsp)
{
	ARG_UNUSED(stream);
	ARG_UNUSED(rsp);

	return 0;
}

static int ascs_stop(struct bt_bap_stream *stream, struct bt_bap_ascs_rsp *rsp)
{
	ARG_UNUSED(stream);
	ARG_UNUSED(rsp);

	return 0;
}

static int ascs_release(struct bt_bap_stream *stream, struct bt_bap_ascs_rsp *rsp)
{
	ARG_UNUSED(stream);
	ARG_UNUSED(rsp);

	return 0;
}

static const struct bt_bap_unicast_server_cb unicast_server_cb = {
	.config = ascs_config,
	.reconfig = ascs_reconfig,
	.qos = ascs_qos,
	.enable = ascs_enable,
	.start = ascs_start,
	.metadata = ascs_metadata,
	.disable = ascs_disable,
	.stop = ascs_stop,
	.release = ascs_release,
};

static void stream_recv(struct bt_bap_stream *stream, const struct bt_iso_recv_info *info,
			struct net_buf *buf)
{
	ARG_UNUSED(stream);

	if (user_cb == NULL || user_cb->stream_recv == NULL) {
		return;
	}

	if ((info->flags & BT_ISO_FLAGS_VALID) == 0) {
		user_cb->stream_recv(NULL, 0);
		return;
	}

	user_cb->stream_recv(buf->data, buf->len);
}

static void stream_stopped(struct bt_bap_stream *stream, uint8_t reason)
{
	ARG_UNUSED(stream);

	LOG_INF("Stream stopped, reason 0x%02X", reason);

	if (user_cb != NULL && user_cb->stream_stopped != NULL) {
		user_cb->stream_stopped();
	}
}

static void stream_enabled(struct bt_bap_stream *stream)
{
	int err;

	/* The unicast server is responsible for starting sink ASEs after the
	 * client has enabled them.
	 */
	err = bt_bap_stream_start(stream);
	if (err != 0) {
		LOG_ERR("Failed to start stream %p: %d", (void *)stream, err);
	}
}

static struct bt_bap_stream_ops stream_ops = {
	.recv = stream_recv,
	.stopped = stream_stopped,
	.enabled = stream_enabled,
};

static void start_advertising(void)
{
	int err;

	err = bt_le_ext_adv_start(adv, BT_LE_EXT_ADV_START_DEFAULT);
	if (err != 0) {
		LOG_ERR("Failed to start advertising: %d", err);
		return;
	}

	LOG_INF("Advertising started");
}

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err != 0) {
		LOG_WRN("Connection failed: %u", err);
		default_conn = NULL;
		return;
	}

	default_conn = bt_conn_ref(conn);

	LOG_INF("Connected");

	if (user_cb != NULL && user_cb->connected != NULL) {
		user_cb->connected();
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	if (conn != default_conn) {
		return;
	}

	LOG_INF("Disconnected, reason 0x%02x", reason);

	bt_conn_unref(default_conn);
	default_conn = NULL;

	if (user_cb != NULL && user_cb->disconnected != NULL) {
		user_cb->disconnected();
	}

	start_advertising();
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

static int set_pacs(void)
{
	int err;

	bt_pacs_cap_register(BT_AUDIO_DIR_SINK, &cap_sink);

	if (IS_ENABLED(CONFIG_BT_PAC_SNK_LOC)) {
		err = bt_pacs_set_location(BT_AUDIO_DIR_SINK, BT_AUDIO_LOCATION_FRONT_CENTER);
		if (err != 0) {
			LOG_ERR("Failed to set sink location: %d", err);
			return err;
		}
	}

	if (IS_ENABLED(CONFIG_BT_PAC_SNK)) {
		err = bt_pacs_set_supported_contexts(BT_AUDIO_DIR_SINK, AVAILABLE_SINK_CONTEXT);
		if (err != 0) {
			LOG_ERR("Failed to set sink supported contexts: %d", err);
			return err;
		}

		err = bt_pacs_set_available_contexts(BT_AUDIO_DIR_SINK, AVAILABLE_SINK_CONTEXT);
		if (err != 0) {
			LOG_ERR("Failed to set sink available contexts: %d", err);
			return err;
		}
	}

	return 0;
}

int ble_audio_handler_start(const struct ble_audio_handler_cb *cb)
{
	int err;

	user_cb = cb;

	err = bt_enable(NULL);
	if (err != 0) {
		LOG_ERR("Bluetooth init failed: %d", err);
		return err;
	}

	LOG_INF("Bluetooth initialized");

	bt_bap_unicast_server_register_cb(&unicast_server_cb);

	for (size_t i = 0; i < ARRAY_SIZE(sink_streams); i++) {
		bt_bap_stream_cb_register(&sink_streams[i], &stream_ops);
	}

	err = set_pacs();
	if (err != 0) {
		return err;
	}

	err = bt_le_ext_adv_create(BT_LE_EXT_ADV_CONN, NULL, &adv);
	if (err != 0) {
		LOG_ERR("Failed to create advertising set: %d", err);
		return err;
	}

	err = bt_le_ext_adv_set_data(adv, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err != 0) {
		LOG_ERR("Failed to set advertising data: %d", err);
		return err;
	}

	start_advertising();

	return 0;
}
