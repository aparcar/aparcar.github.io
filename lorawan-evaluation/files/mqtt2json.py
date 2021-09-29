import json
from os import getenv
from sys import stderr

import paho.mqtt.client as mqtt

broker = getenv("MQTT_URL", "nam1.cloud.thethings.network")
username = getenv("MQTT_USER", "meshlab@ttn")
token = getenv("MQTT_TOKEN")

assert token is not None, "Please set MQTT_TOKEN env variable"


def on_connect(mqttc, obj, flags, rc):
    print("rc: " + str(rc), file=stderr)


def on_message(mqttc, obj, msg):
    decoded_message = json.loads(msg.payload.decode("utf-8"))

    # use this as node_id to have the unique dev_eui
    #node_id = decoded_message["end_device_ids"]["dev_eui"]

    # use this as node_id to have the human readable identifier
    node_id = msg.topic.split("/")[-2]

    if "decoded_payload" in decoded_message.get("uplink_message", {}):
        print(
            json.dumps(
                {
                    "from": node_id,
                    "p": decoded_message["uplink_message"]["decoded_payload"],
                }
            )
        )


def on_publish(mqttc, obj, mid):
    print("mid: " + str(mid), file=stderr)


def on_subscribe(mqttc, obj, mid, granted_qos):
    print("Subscribed: " + str(mid) + " " + str(granted_qos), file=stderr)


def on_log(mqttc, obj, level, string):
    print(string)


mqttc = mqtt.Client()
mqttc.username_pw_set(username, token)
mqttc.on_message = on_message
mqttc.on_connect = on_connect
mqttc.on_publish = on_publish
mqttc.on_subscribe = on_subscribe
mqttc.tls_set_context()

# Uncomment to enable debug messages
# mqttc.on_log = on_log

mqttc.connect(broker, 8883, 60)
mqttc.subscribe("#", 0)

mqttc.loop_forever()
