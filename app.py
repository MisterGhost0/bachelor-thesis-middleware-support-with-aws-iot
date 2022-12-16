from flask import Flask, redirect, url_for
from views import views
from awsbackend import aws, aws_broker
import os, threading

app = Flask(__name__)
app.config['SECRET_KEY'] = 'batuhan'
app.register_blueprint(views, url_prefix="/")
app.register_blueprint(aws, url_prefix="/")

if __name__ == '__main__':
    threading.Thread(target=aws_broker, daemon=True).start()
    port = int(os.environ.get('PORT', 5000))
    app.run(debug=True, port=port, host='0.0.0.0')
