from flask import Flask

app = Flask(__name__)
@app.route('/ws')
def ws_handler():
    return "WebSocket endpoint"
