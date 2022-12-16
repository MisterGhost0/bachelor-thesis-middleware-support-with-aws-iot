from flask import Blueprint, render_template, redirect, url_for, jsonify
from awsbackend import control_buzzer_click, publish_payload
import json

views = Blueprint(__name__, "views")

@views.route("/", methods=['GET','POST'])
def startup():
    #return render_template("home.html")
    return redirect(url_for('views.home'))

@views.route("/home", methods=['GET','POST'])
def home():
    return render_template("home.html")

@views.route("/motor_control", methods=['GET', 'POST'])
def motor_control():
    publish_payload()
    return render_template("motor_control.html")

@views.route("/alarm_control", methods=['GET', 'POST'])
def alarm_control():
    control_buzzer_click()
    return render_template("alarm_control.html")

@views.route("/logs", methods=['GET','POST'])
def logs():
    return render_template("logs.html")

@views.route("/logs_extended", methods=['GET','POST'])
def logs_extended():
    data = ""
    with open("logs.txt") as fileHandle:
        content = fileHandle.readlines()
        ## -1 = Intervall für lesen des Contents.. (-)n = rückwärts jedes nth Element oder +1 zb vorwärts jedes nth Element 
        for i in reversed(content):
            data += i + "\n"
    return render_template("logs_extended.html", data=data)

@views.route("/shadow_logs_extended", methods=['GET','POST'])
def shadow_logs_extended():
    data = ""
    
    with open("shadow_logs.json") as fileHandle:
        content = fileHandle.readlines()
        ## -1 = Intervall für lesen des Contents.. (-)n = rückwärts jedes nth Element oder +1 zb vorwärts jedes nth Element 
        for i in reversed(content):
            data += i + "\n"
    
    return render_template("shadow_logs_extended.html", data = data)
