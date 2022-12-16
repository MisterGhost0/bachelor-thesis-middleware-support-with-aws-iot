from flask import Blueprint, request, flash ,render_template, request_finished
from awscrt import io, mqtt
from awsiot import mqtt_connection_builder
import time
from datetime import datetime
from sqlalchemy import true

# Define ENDPOINT, CLIENT_ID, PATH_TO_CERTIFICATE, PATH_TO_PRIVATE_KEY, PATH_TO_AMAZON_ROOT_CA_1, MESSAGE, TOPIC, and RANGE
ENDPOINT = "your-aws-api-endpoint.amazonaws.com"
CLIENT_ID = "Raspberry4Batuhan"
PATH_TO_CERTIFICATE = "certificates/c56a28ec03c6-certificate.pem.crt"
PATH_TO_PRIVATE_KEY = "certificates/c56a28ec03c6-private.pem.key"
PATH_TO_AMAZON_ROOT_CA_1 = "certificates/AmazonRootCA1.pem"
CUSTOM_TOPIC_1 = "esp32/temperature"
CUSTOM_TOPIC_2 = "esp8266/taguid"
SHADOW_TOPIC_1 = "$aws/things/ESP32-Temperature-Click/shadow/name/temperature_click_shadow/update/accepted"
SHADOW_TOPIC_2 = "$aws/things/ESP32-Temperature-Click/shadow/name/temperature_click_shadow/update"
TAGUID_1 = "138D9380" #blue -> open
TAGUID_2 = "CC6B979B" #white -> close
RANGE = 20

aws = Blueprint(__name__, "aws")

# Callback when the subscribed topic receives a message
@aws.route('/')
def message_handler(topic, payload):
    dateTimeObject = datetime.now()
    timestamp = dateTimeObject.strftime("%m-%d-%Y--%H:%M:%S")
    logmessage = "Event " + "On: " + '{}'.format(timestamp) + " from topic: " + '{}'.format(topic) + ", message: " + '{}'.format(payload) + " was subscribed in the AWS broker!"

    with open("logs.txt", "a") as fileHandle:
        fileHandle.write("{}\n" .format(logmessage))
    
    with open("logs.txt") as fileHandle:
        data = fileHandle.readlines()
    #print(data)
    #return render_template("logs_extended.html", data=data)


# Callback when the subscribed topic receives a message
@aws.route('/')
def message_handler_shadow(topic, payload):

    logmessage = '{}'.format(payload)

    with open("shadow_logs.json", "a") as fileHandle:
        fileHandle.write("{}\n" .format(logmessage))
    
    with open("shadow_logs.json") as fileHandle:
        data = fileHandle.readlines()
    #print(data)
    #return render_template("logs_extended.html", data=data)

@aws.route('/')
def aws_broker():

    dateTimeObject = datetime.now()
    timestamp = dateTimeObject.strftime("%d-%m-%Y--%H:%M:%S")

    # Spin up resources
    event_loop_group = io.EventLoopGroup(1)
    host_resolver = io.DefaultHostResolver(event_loop_group)
    client_bootstrap = io.ClientBootstrap(event_loop_group, host_resolver)
    mqtt_connection = mqtt_connection_builder.mtls_from_path(
                endpoint=ENDPOINT,
                cert_filepath=PATH_TO_CERTIFICATE,
                pri_key_filepath=PATH_TO_PRIVATE_KEY,
                client_bootstrap=client_bootstrap,
                ca_filepath=PATH_TO_AMAZON_ROOT_CA_1,
                client_id=CLIENT_ID,
                clean_session=False,
                keep_alive_secs=10)

    #print("Connecting to {} with client ID '{}'...".format(ENDPOINT, CLIENT_ID))
    logmessage = "On: " + '{}'.format(timestamp) + " Connecting to " + '{}'.format(ENDPOINT) + " with client ID " + '{}'.format(CLIENT_ID)

    with open("logs.txt", "a") as fileHandle:
        fileHandle.write("{}\n" .format(logmessage))

    # Make the connect() call
    connect_future = mqtt_connection.connect()

    # Future.result() waits until a result is available
    connect_future.result()
    
    #print("Connected to the AWS Endpoint Rest API successfully!")
    logmessage = "On: " + '{}'.format(timestamp) + " Connected to the AWS Endpoint Rest API successfully!"

    with open("logs.txt", "a") as fileHandle:
        fileHandle.write("{}\n" .format(logmessage))

    #print("Begin to monitoring to the topics activities: {} & {} ..." .format(CUSTOM_TOPIC_1, CUSTOM_TOPIC_2))
    logmessage = "On: " + '{}'.format(timestamp) + " Starting to monitor activites on topics: " + '{}'.format(CUSTOM_TOPIC_1) + " & " + '{}'.format(CUSTOM_TOPIC_2) + " & " + '{}'.format(SHADOW_TOPIC_1)

    with open("logs.txt", "a") as fileHandle:
        fileHandle.write("{}\n" .format(logmessage))
    
    while true:

        mqtt_connection.subscribe(CUSTOM_TOPIC_1, qos=mqtt.QoS.AT_LEAST_ONCE, callback=message_handler)
        
        mqtt_connection.subscribe(CUSTOM_TOPIC_2, qos=mqtt.QoS.AT_LEAST_ONCE, callback=message_handler)

        mqtt_connection.subscribe(SHADOW_TOPIC_1, qos=mqtt.QoS.AT_LEAST_ONCE, callback=message_handler_shadow)
        
        time.sleep(1)
        
    #print('Subscribe End')
    disconnect_future = mqtt_connection.disconnect()
    disconnect_future.result()
    #print("Disconnected!")
    #return render_template('home.html')

@aws.route('/motor_control', methods=['GET', 'POST'])
def publish_payload():

    dateTimeObject = datetime.now()
    timestamp = dateTimeObject.strftime("%d-%m-%Y--%H:%M:%S")

    # Spin up resources
    event_loop_group = io.EventLoopGroup(1)
    host_resolver = io.DefaultHostResolver(event_loop_group)
    client_bootstrap = io.ClientBootstrap(event_loop_group, host_resolver)
    mqtt_connection = mqtt_connection_builder.mtls_from_path(
                endpoint=ENDPOINT,
                cert_filepath=PATH_TO_CERTIFICATE,
                pri_key_filepath=PATH_TO_PRIVATE_KEY,
                client_bootstrap=client_bootstrap,
                ca_filepath=PATH_TO_AMAZON_ROOT_CA_1,
                client_id=CLIENT_ID,
                clean_session=False,
                keep_alive_secs=10)
    
    if request.method == 'POST':
        
        formValue = request.form.get('taguid')

        if formValue == TAGUID_1:

            # Make the connect() call
            connect_future = mqtt_connection.connect()

            # Future.result() waits until a result is available
            #connect_future.result()
            dataPayload = '{}'.format(formValue)
            mqtt_connection.publish(topic=CUSTOM_TOPIC_2, payload=dataPayload, qos=mqtt.QoS.AT_LEAST_ONCE)
            
            logmessage = "Event " + "On: " + '{}'.format(timestamp) + " to topic: " + '{}'.format(CUSTOM_TOPIC_2) + ", message: " + '{}'.format(dataPayload) + " was sent from WEB API!"
            
            with open("logs.txt", "a") as fileHandle:
                fileHandle.write("{}\n" .format(logmessage))
            
            flash('Correct UID was published successfully!! Ventile is opening now!', category='success')
            
            disconnect_future = mqtt_connection.disconnect()
            #disconnect_future.result()
            
            return render_template("home.html")

        elif formValue == TAGUID_2:
            # Make the connect() call
            connect_future = mqtt_connection.connect()

            # Future.result() waits until a result is available
            #connect_future.result()
            dataPayload = '{}'.format(formValue)
            mqtt_connection.publish(topic=CUSTOM_TOPIC_2, payload=dataPayload, qos=mqtt.QoS.AT_LEAST_ONCE)
            
            logmessage = "Event " + "On: " + '{}'.format(timestamp) + " to topic: " + '{}'.format(CUSTOM_TOPIC_2) + ", message: " + '{}'.format(dataPayload) + " was sent from WEB API!"
            with open("logs.txt", "a") as fileHandle:
                fileHandle.write("{}\n" .format(logmessage))

            flash('Correct UID was published successfully! Ventile is closing now!', category='success')
            
            disconnect_future = mqtt_connection.disconnect()
            #disconnect_future.result()
            
            return render_template("home.html")

        elif formValue != TAGUID_1:
            flash('UID is not valid! In this case not able to control the motor. Try again with correct UID!', category='error')
            
        elif formValue != TAGUID_2:
            flash('UID is not valid! In this case not able to control the motor. Try again with correct UID!', category='error')

    return render_template("motor_control.html")

@aws.route('/alarm_control', methods=['GET', 'POST'])
def control_buzzer_click():

    dateTimeObject = datetime.now()
    timestamp = dateTimeObject.strftime("%d-%m-%Y--%H:%M:%S")

    resetPayload = "{\"state\":{\"reported\":{\"temperature\":00.00}}}"

    # Spin up resources
    event_loop_group = io.EventLoopGroup(1)
    host_resolver = io.DefaultHostResolver(event_loop_group)
    client_bootstrap = io.ClientBootstrap(event_loop_group, host_resolver)
    mqtt_connection = mqtt_connection_builder.mtls_from_path(
                endpoint=ENDPOINT,
                cert_filepath=PATH_TO_CERTIFICATE,
                pri_key_filepath=PATH_TO_PRIVATE_KEY,
                client_bootstrap=client_bootstrap,
                ca_filepath=PATH_TO_AMAZON_ROOT_CA_1,
                client_id=CLIENT_ID,
                clean_session=False,
                keep_alive_secs=10)

    connect_future = mqtt_connection.connect()
    #connect_future.result()

    dataPayload = '{}'.format(resetPayload)
    mqtt_connection.publish(topic=SHADOW_TOPIC_2, payload=dataPayload, qos=mqtt.QoS.AT_LEAST_ONCE)

    #flash('Alarm is turned off!', category='success')
    
    logmessage = "Event " + "On: " + '{}'.format(timestamp) + " to topic: " + '{}'.format(SHADOW_TOPIC_2) + ", message: " + '{}'.format(dataPayload) + " was sent from WEB API!"

    with open("logs.txt", "a") as fileHandle:
        fileHandle.write("{}\n" .format(logmessage))
        
    disconnect_future = mqtt_connection.disconnect()
    #disconnect_future.result()
    
    return render_template("alarm_control.html")
