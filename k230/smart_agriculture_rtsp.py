import gc
import network
import os
import socket
import sys
import time
import uctypes
import ujson

import multimedia as mm
import ulab.numpy as np
from libs.PlatTasks import DetectionApp
from media.display import *
from media.media import *
from media.sensor import *
from media.vencoder import *


WIFI_SSID = "abciq"
WIFI_PASSWORD = "12345678"

SESSION_NAME = "face"
RTSP_PORT = 8554

STREAM_WIDTH = 512
STREAM_HEIGHT = 288
STREAM_FPS = 5
STREAM_BITRATE = 100

AI_WIDTH = 1920
AI_HEIGHT = 1080
AI_INFER_INTERVAL = 2
GC_INTERVAL = 20

PEST_ROOT_PATH = "/sdcard/mp_deployment_source"
PEST_CONFIDENCE_THRESHOLD = 0.20
DEBUG_PRINT_INTERVAL = 25
STATUS_PORT = 9000
STATUS_SEND_INTERVAL = 20


def align_up(x, align):
    return ((x + align - 1) // align) * align


def connect_wifi(ssid, password):
    sta = network.WLAN(0)
    if sta.isconnected() and sta.ifconfig()[0] != "0.0.0.0":
        return sta

    try:
        sta.disconnect()
        time.sleep_ms(200)
    except Exception:
        pass

    sta.connect(ssid, password)
    while sta.ifconfig()[0] == "0.0.0.0":
        time.sleep(1)

    return sta


class SmartAgricultureRtspServer:
    def __init__(self):
        self.session_name = SESSION_NAME
        self.port = RTSP_PORT
        self.rtspserver = mm.rtsp_server()
        self.venc_chn = VENC_CHN_ID_0
        self.running = False

        self.sensor = None
        self.encoder = None
        self.link = None

        self.det_app = None
        self.labels = []
        self.frame_count = 0
        self.last_pest_name = "NONE_DETECTED"
        self.last_sent_pest = ""

        self.status_socket = None
        self.sta = network.WLAN(0)

    def load_json(self, path):
        with open(path, "r") as handle:
            return ujson.load(handle)

    def get_wifi_ip(self):
        try:
            return self.sta.ifconfig()[0]
        except Exception:
            return "0.0.0.0"

    def get_rtsp_url(self):
        return self.rtspserver.rtspserver_getrtspurl(self.session_name)

    def init_status_sender(self):
        try:
            self.status_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.status_socket.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
            print("[LAN] UDP broadcast ready:", STATUS_PORT)
        except Exception as exc:
            self.status_socket = None
            print("[LAN] UDP init failed:", exc)

    def close_status_sender(self):
        if self.status_socket is None:
            return
        try:
            self.status_socket.close()
        except Exception:
            pass
        self.status_socket = None

    def init_pest_detector(self):
        deploy_conf = self.load_json(PEST_ROOT_PATH + "/deploy_config.json")
        kmodel_path = PEST_ROOT_PATH + "/" + deploy_conf["kmodel_path"]
        self.labels = deploy_conf["categories"]
        model_conf = deploy_conf["confidence_threshold"]
        nms_threshold = deploy_conf["nms_threshold"]
        model_input_size = deploy_conf["img_size"]
        model_type = deploy_conf["model_type"]

        anchors = []
        if model_type == "AnchorBaseDet":
            anchors = deploy_conf["anchors"][0] + deploy_conf["anchors"][1] + deploy_conf["anchors"][2]

        self.det_app = DetectionApp(
            "video",
            kmodel_path,
            self.labels,
            model_input_size,
            anchors,
            model_type,
            model_conf,
            nms_threshold,
            [AI_WIDTH, AI_HEIGHT],
            [STREAM_WIDTH, STREAM_HEIGHT],
            debug_mode=0
        )
        self.det_app.config_preprocess()
        print("[AI] Pest detector ready:", kmodel_path)
        print("[AI] Labels:", self.labels)

    def init_stream(self):
        width = align_up(STREAM_WIDTH, 16)
        height = STREAM_HEIGHT

        self.sensor = Sensor()
        self.sensor.reset()
        self.sensor.set_framesize(width=width, height=height, alignment=12)
        self.sensor.set_pixformat(Sensor.YUV420SP)
        self.sensor.set_framesize(width=AI_WIDTH, height=AI_HEIGHT, chn=CAM_CHN_ID_1)
        self.sensor.set_pixformat(PIXEL_FORMAT_RGB_888_PLANAR, chn=CAM_CHN_ID_1)

        self.encoder = Encoder()
        self.encoder.SetOutBufs(self.venc_chn, 8, width, height)
        self.link = None

        Display.init(Display.ST7701, width=640, height=480, to_ide=True)
        MediaManager.init()

        chn_attr = ChnAttrStr(
            self.encoder.PAYLOAD_TYPE_H264,
            self.encoder.H264_PROFILE_MAIN,
            width,
            height,
            bit_rate=STREAM_BITRATE,
            dst_frame_rate=STREAM_FPS,
            src_frame_rate=STREAM_FPS,
        )
        self.encoder.Create(self.venc_chn, chn_attr)
        print("[RTSP] encoder channel created")

    def start_rtsp_service(self):
        self.rtspserver.rtspserver_init(self.port)
        self.rtspserver.rtspserver_createsession(
            self.session_name,
            mm.multi_media_type.media_h264,
            False
        )
        self.rtspserver.rtspserver_start()
        print("[RTSP] server started")

    def stop_rtsp_service(self):
        try:
            self.rtspserver.rtspserver_stop()
        except Exception:
            pass
        try:
            self.rtspserver.rtspserver_deinit()
        except Exception:
            pass

    def start(self):
        self.init_stream()
        self.init_pest_detector()
        self.init_status_sender()
        self.start_rtsp_service()

        self.encoder.Start(self.venc_chn)
        print("[RTSP] encoder started")
        self.sensor.run()
        print("[RTSP] sensor running")
        print("[RTSP] URL:", self.get_rtsp_url())

        self.running = True
        self.loop()

    def send_status(self, pest_name):
        if self.status_socket is None:
            return

        payload = {
            "pest": pest_name,
            "wither": "NONE",
            "camera_ip": self.get_wifi_ip(),
            "rtsp_url": self.get_rtsp_url(),
        }
        try:
            self.status_socket.sendto(
                ujson.dumps(payload).encode("utf-8"),
                ("255.255.255.255", STATUS_PORT)
            )
            self.last_sent_pest = pest_name
        except Exception as exc:
            if self.frame_count % DEBUG_PRINT_INTERVAL == 0:
                print("[LAN] UDP send error:", exc)

    def normalize_box(self, box):
        if len(box) < 4:
            return 0, 0, 0, 0

        x1 = int(box[0])
        y1 = int(box[1])
        x2 = int(box[2])
        y2 = int(box[3])
        if x2 > x1 and y2 > y1:
            return x1, y1, x2 - x1, y2 - y1
        return x1, y1, int(box[2]), int(box[3])

    def draw_detection_box(self, img, box):
        try:
            x, y, w, h = self.normalize_box(box)
            if w <= 0 or h <= 0:
                return

            x = x * STREAM_WIDTH // AI_WIDTH
            y = y * STREAM_HEIGHT // AI_HEIGHT
            w = w * STREAM_WIDTH // AI_WIDTH
            h = h * STREAM_HEIGHT // AI_HEIGHT

            if w < 4:
                w = 4
            if h < 4:
                h = 4

            img.draw_rectangle(int(x), int(y), int(w), int(h))
        except Exception as exc:
            if self.frame_count % DEBUG_PRINT_INTERVAL == 0:
                print("[AI] draw box error:", exc)

    def run_pest_detection(self, ai_img, ai_np=None):
        pest_name = "NONE_DETECTED"
        best_score = 0.0
        best_box = None

        try:
            if ai_np is None:
                ai_np = ai_img.to_numpy_ref()
            pest_res = self.det_app.run(ai_np)
        except Exception:
            pest_res = self.det_app.run(ai_img)

        if pest_res and isinstance(pest_res, dict) and len(pest_res.get("scores", [])) > 0:
            for index in range(len(pest_res["scores"])):
                score = pest_res["scores"][index]
                if score > best_score:
                    best_score = score
                    label_index = int(pest_res["idx"][index])
                    if 0 <= label_index < len(self.labels):
                        pest_name = self.labels[label_index]
                    else:
                        pest_name = "UNKNOWN"

                    boxes = pest_res.get("boxes", [])
                    if index < len(boxes):
                        best_box = boxes[index]

            if best_score < PEST_CONFIDENCE_THRESHOLD:
                pest_name = "NONE_DETECTED"
                best_box = None

        return pest_name, best_score, best_box

    def calc_uv_addr(self, img):
        frame_pixels = img.width() * img.height()
        if img.width() == 800 and img.height() == 480:
            return img.phyaddr() + frame_pixels + 1024
        if img.width() == 1920 and img.height() == 1080:
            return img.phyaddr() + frame_pixels + 3072
        if img.width() == 640 and img.height() == 360:
            return img.phyaddr() + frame_pixels + 3072
        return img.phyaddr() + frame_pixels

    def loop(self):
        stream_data = StreamData()
        frame_info = k_video_frame_info()

        try:
            while self.running:
                self.frame_count += 1

                ai_img = self.sensor.snapshot(chn=CAM_CHN_ID_1)
                if ai_img == -1:
                    time.sleep_ms(5)
                    continue

                pest_name = self.last_pest_name or "NONE_DETECTED"
                best_box = None
                best_score = 0.0

                if self.frame_count % AI_INFER_INTERVAL == 0:
                    try:
                        ai_np = ai_img.to_numpy_ref()
                        pest_name, best_score, best_box = self.run_pest_detection(ai_img, ai_np)
                        if self.frame_count % DEBUG_PRINT_INTERVAL == 0:
                            print("[AI] top1:", pest_name, "score:", best_score)
                    except Exception as exc:
                        print("[AI] inference error:", exc)

                yuv_img = self.sensor.snapshot(chn=CAM_CHN_ID_0)
                if yuv_img == -1:
                    time.sleep_ms(5)
                    continue

                if best_box is not None:
                    self.draw_detection_box(yuv_img, best_box)

                if pest_name != self.last_pest_name:
                    self.last_pest_name = pest_name
                    print("[AI] Pest:", pest_name)

                if self.frame_count % STATUS_SEND_INTERVAL == 0 or pest_name != self.last_sent_pest:
                    self.send_status(pest_name)

                frame_info.v_frame.width = yuv_img.width()
                frame_info.v_frame.height = yuv_img.height()
                frame_info.v_frame.pixel_format = Sensor.YUV420SP
                frame_info.pool_id = yuv_img.poolid()
                frame_info.v_frame.phys_addr[0] = yuv_img.phyaddr()
                frame_info.v_frame.phys_addr[1] = self.calc_uv_addr(yuv_img)

                self.encoder.SendFrame(self.venc_chn, frame_info)
                self.encoder.GetStream(self.venc_chn, stream_data)
                for pack_idx in range(stream_data.pack_cnt):
                    payload = bytes(
                        uctypes.bytearray_at(
                            stream_data.data[pack_idx],
                            stream_data.data_size[pack_idx]
                        )
                    )
                    self.rtspserver.rtspserver_sendvideodata(
                        self.session_name,
                        payload,
                        stream_data.data_size[pack_idx],
                        1000
                    )
                self.encoder.ReleaseStream(self.venc_chn, stream_data)

                if self.frame_count % GC_INTERVAL == 0:
                    gc.collect()
                time.sleep_us(10)
                os.exitpoint()

        except BaseException as exc:
            print("[RTSP] stream exception:", exc)
            try:
                sys.print_exception(exc)
            except Exception:
                pass
        finally:
            self.stop()

    def stop(self):
        self.running = False
        self.close_status_sender()

        try:
            if self.sensor is not None:
                self.sensor.stop()
        except Exception:
            pass

        try:
            if self.link is not None:
                del self.link
        except Exception:
            pass

        try:
            if self.encoder is not None:
                self.encoder.Stop(self.venc_chn)
        except Exception:
            pass

        try:
            if self.encoder is not None:
                self.encoder.Destroy(self.venc_chn)
        except Exception:
            pass

        self.stop_rtsp_service()

        try:
            Display.deinit()
        except Exception:
            pass

        try:
            MediaManager.deinit()
        except Exception:
            pass


if __name__ == "__main__":
    print("[WIFI] Connecting ...")
    sta = connect_wifi(WIFI_SSID, WIFI_PASSWORD)
    print("[WIFI] Connected IP:", sta.ifconfig()[0])

    print("[INIT] Starting RTSP smart agriculture server ...")
    server = SmartAgricultureRtspServer()
    server.start()
