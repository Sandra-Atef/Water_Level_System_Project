from flask import Flask, render_template, request, jsonify
from flask_sqlalchemy import SQLAlchemy
from datetime import datetime

app = Flask(__name__)
app.config['SQLALCHEMY_DATABASE_URI'] = 'sqlite:///readings.db'
db = SQLAlchemy(app)

class Readings(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    sensor1 = db.Column(db.String(10), nullable=False)
    sensor2 = db.Column(db.String(10), nullable=False)
    dateCreated = db.Column(db.DateTime, default=datetime.utcnow)

    def __repr__(self):
        return f'<Reading {self.id}: S1={self.sensor1} cm, S2={self.sensor2} cm>'

with app.app_context():
    db.create_all()
 

@app.route('/')
def home():
    return render_template("home.html")

@app.route('/submit')
def submit():
    return render_template("submit.html")

@app.route('/depth')
def depth():
    latest = Readings.query.order_by(Readings.dateCreated.desc()).first()

    if latest:
        sensor1 = latest.sensor1
        sensor2 = latest.sensor2
    else:
        sensor1 = -2
        sensor2 = -2

    return render_template("depth.html", sensor1=sensor1, sensor2=sensor2)

@app.route('/success')
def success():
    firstReading = request.args.get("firstReading", "0")
    secondReading = request.args.get("secondReading", "0")

    reading = Readings(sensor1=firstReading, sensor2=secondReading)
    db.session.add(reading)
    db.session.commit()

    return render_template("success.html", firstReading=firstReading, secondReading=secondReading)

@app.route('/readings')
def readings():
    return render_template('readings.html')


@app.route('/api/readings')
def get_readings():
    # Get date filter parameters
    start_date = request.args.get('start_date')
    end_date = request.args.get('end_date')
    
    # Base query
    query = Readings.query
    
    # Apply date filters if provided
    if start_date:
        start_date_obj = datetime.strptime(start_date, '%Y-%m-%d')
        query = query.filter(Readings.dateCreated >= start_date_obj)
    
    if end_date:
        # Add one day to include the end date fully
        end_date_obj = datetime.strptime(end_date, '%Y-%m-%d')
        end_date_obj = end_date_obj.replace(hour=23, minute=59, second=59)
        query = query.filter(Readings.dateCreated <= end_date_obj)
    
    # Order by date descending (newest first)
    query = query.order_by(Readings.dateCreated.desc())
    
    # Execute the query
    readings = query.all()
    
    # Format the results as JSON
    result = []
    for reading in readings:
        result.append({
            'id': reading.id,
            'sensor1': reading.sensor1,
            'sensor2': reading.sensor2,
            'dateCreated': reading.dateCreated.isoformat()
        })
    
    return jsonify(result)
