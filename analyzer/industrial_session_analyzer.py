from influxdb_client import InfluxDBClient, Point
from influxdb_client.client.write_api import SYNCHRONOUS
from datetime import datetime, timedelta
import json
import os

class WorkSessionAnalyzer:
    def __init__(self, token, host, org, bucket):
        self.client = InfluxDBClient(url=f"http://{host}:8086", token=token, org=org)
        self.query_api = self.client.query_api()
        self.bucket = bucket
        self.mold_file = "device_molds.json"
        self.load_mold_assignments()

    def load_mold_assignments(self):
        if os.path.exists(self.mold_file):
            with open(self.mold_file, 'r') as f:
                self.device_molds = json.load(f)
        else:
            self.device_molds = {}

    def save_mold_assignments(self):
        with open(self.mold_file, 'w') as f:
            json.dump(self.device_molds, f)

    def get_mold_for_device(self, device_id):
        query = f'''
        from(bucket: "{self.bucket}")
          |> range(start: -365d)
          |> filter(fn: (r) => r._measurement == "nfc_event")
          |> filter(fn: (r) => r._field == "tag_data")
          |> filter(fn: (r) => r._value =~ /^die:/)
          |> filter(fn: (r) => r.device == "{device_id}")
          |> last()
        '''
        result = self.query_api.query(query)
        for table in result:
            for record in table.records:
                tag_data = record.get_value()
                if tag_data and tag_data.startswith("die:"):
                    mold_id = tag_data.split(":")[1]
                    self.device_molds[device_id] = mold_id
                    self.save_mold_assignments()
                    return mold_id
        return self.device_molds.get(device_id, "Unknown")

    def analyze_work_sessions(self, device_id, hours=24):
        query = f'''
        from(bucket: "{self.bucket}")
          |> range(start: -{hours}h)
          |> filter(fn: (r) => r._measurement == "nfc_event" or r._measurement == "relay")
          |> filter(fn: (r) => r.device == "{device_id}")
          |> sort(columns: ["_time"])
        '''
        result = self.query_api.query(query)
        sessions = []
        current_session = None
        nfc_relay_events = []

        # Collect all NFC and relay events
        for table in result:
            for record in table.records:
                nfc_relay_events.append({
                    'time': record.get_time(),
                    'measurement': record.get_measurement(),
                    'field': record.get_field(),
                    'value': record.get_value(),
                    'device': record['device']
                })

        press_query = f'''
        from(bucket: "{self.bucket}")
          |> range(start: -{hours}h)
          |> filter(fn: (r) => r._measurement == "press_counter")
          |> filter(fn: (r) => r.device == "{device_id}")
          |> sort(columns: ["_time"])
        '''
        press_result = self.query_api.query(press_query)
        press_events = []
        for table in press_result:
            for record in table.records:
                if record.get_field() == "count":
                    press_events.append({
                        'time': record.get_time(),
                        'count': int(record.get_value())
                    })

        for table in result:
            for record in table.records:
                time = record.get_time()
                measurement = record.get_measurement()
                field = record.get_field()
                value = record.get_value()
                
                if (measurement == "nfc_event" and field == "tag_data" and 
                    value.startswith("personnel:") and current_session is None):
                    current_session = {
                        'start_time': time,
                        'start_cause': 'nfc_personnel',  # Track start cause
                        'operator_id': value.split(":")[1],
                        'press_counts': [],
                        'end_time': None,
                        'end_cause': None,  # Initialize end cause
                        'end_reason': None
                    }
                    print(f"Session started at {time} by operator {current_session['operator_id']} via NFC personnel tag")
                
                elif (current_session and current_session['end_time'] is None and
                      ((measurement == "nfc_event" and field == "relay_state" and value == False) or
                       (measurement == "relay" and field == "state" and value == False))):
                    current_session['end_time'] = time
                    current_session['end_cause'] = 'nfc_relay' if measurement == "nfc_event" else 'relay'  # Track end cause
                    current_session['end_reason'] = 'manual' if measurement == "nfc_event" else 'timeout'
                    sessions.append(current_session)
                    print(f"Session ended at {time}, reason: {current_session['end_reason']}, cause: {current_session['end_cause']}")
                    current_session = None

        # Append ongoing session if it exists
        if current_session:
            current_session['end_reason'] = 'ongoing'
            sessions.append(current_session)
            print(f"Ongoing session started at {current_session['start_time']} by operator {current_session['operator_id']} via {current_session['start_cause']}")

        out_of_session_presses = []
        for session in sessions:
            session_start = session['start_time']
            session_end = session['end_time'] or datetime.utcnow().replace(tzinfo=session_start.tzinfo)
            for press_event in press_events:
                if session_start <= press_event['time'] <= session_end:
                    session['press_counts'].append(press_event)

        for press_event in press_events:
            in_session = False
            for session in sessions:
                session_start = session['start_time']
                session_end = session['end_time'] or datetime.utcnow().replace(tzinfo=session_start.tzinfo)
                if session_start <= press_event['time'] <= session_end:
                    in_session = True
                    break
            if not in_session:
                out_of_session_presses.append(press_event)

        out_of_session_sessions = []
        out_of_session_records = []
        current_out_session = None
        for press in out_of_session_presses:
            if current_out_session is None:
                current_out_session = {
                    'start_time': press['time'],
                    'start_cause': 'press_event',  # Out-of-session presses start due to press events
                    'operator_id': None,
                    'press_counts': [press],
                    'end_time': None,
                    'end_cause': None,
                    'end_reason': 'out_of_session'
                }
            else:
                time_diff = (press['time'] - current_out_session['press_counts'][-1]['time']).total_seconds() / 60.0
                if time_diff < 10:
                    current_out_session['press_counts'].append(press)
                else:
                    current_out_session['end_time'] = current_out_session['press_counts'][-1]['time']
                    current_out_session['end_cause'] = 'press_timeout'  # End due to timeout between presses
                    out_of_session_sessions.append(current_out_session)
                    current_out_session = {
                        'start_time': press['time'],
                        'start_cause': 'press_event',
                        'operator_id': None,
                        'press_counts': [press],
                        'end_time': None,
                        'end_cause': None,
                        'end_reason': 'out_of_session'
                    }
        if current_out_session:
            current_out_session['end_time'] = current_out_session['press_counts'][-1]['time']
            current_out_session['end_cause'] = 'press_timeout'
            out_of_session_sessions.append(current_out_session)

        # Process remaining out-of-session presses as individual records
        for i in range(len(out_of_session_presses) - 1):
            time_diff = (out_of_session_presses[i + 1]['time'] - out_of_session_presses[i]['time']).total_seconds() / 60.0
            if time_diff >= 10:
                out_of_session_records.append({
                    'begin_time': out_of_session_presses[i]['time'],
                    'end_time': out_of_session_presses[i + 1]['time'],
                    'count': out_of_session_presses[i]['count']
                })

        # Compute statistics for all sessions (regular and out-of-session)
        all_sessions = sessions + out_of_session_sessions
        for session in all_sessions:
            counts = session['press_counts']
            if counts:
                # Sort counts by time to ensure correct order
                counts.sort(key=lambda x: x['time'])
                # Get all count values
                count_values = [c['count'] for c in counts]
                # Total count is the difference between max and min counts plus 1
                session['total_count'] = max(count_values) - min(count_values) + 1 if count_values else 0
                # Find missing counts
                if count_values:
                    min_count = min(count_values)
                    max_count = max(count_values)
                    expected_counts = set(range(min_count, max_count + 1))
                    actual_counts = set(count_values)
                    session['missing_counts'] = sorted(list(expected_counts - actual_counts))
                else:
                    session['missing_counts'] = []
            else:
                session['total_count'] = 0
                session['missing_counts'] = []
            
            if len(counts) > 1:
                total_time = (counts[-1]['time'] - counts[0]['time']).total_seconds()
                session['avg_time_between_counts'] = total_time / (len(counts) - 1)
            else:
                session['avg_time_between_counts'] = 0

        return all_sessions, out_of_session_records, nfc_relay_events

    def get_session_statistics(self, device_id, hours=24):
        sessions, out_of_session_records, nfc_relay_events = self.analyze_work_sessions(device_id, hours)
        total_count = sum(session['total_count'] for session in sessions)
        
        if sessions:
            avg_session_duration = sum(
                (session['end_time'] - session['start_time']).total_seconds() 
                for session in sessions if session['end_time']
            ) / len([s for s in sessions if s['end_time']]) or 1  # Avoid division by zero
        else:
            avg_session_duration = 0
        
        return {
            'total_sessions': len(sessions),
            'total_products': total_count,
            'avg_session_duration': avg_session_duration,
            'sessions': sessions,
            'out_of_session_records': out_of_session_records,
            'nfc_relay_events': nfc_relay_events,
            'current_mold': self.get_mold_for_device(device_id)
        }

if __name__ == "__main__":
    token = "_2B1s0cJGXznc9rXE41mxOusyn159HAAsMyGlLxiFkZd0V8dJKztKYVEPNeKGQSgc0gulh-vhnhn6ONcCr3lBA=="
    analyzer = WorkSessionAnalyzer(
        token=token,
        host="192.168.1.37",
        org="tooswasher",
        bucket="manufacture"
    )
    
    stats = analyzer.get_session_statistics("device1", 24)
    print(f"Current mold: {stats['current_mold']}")
    print(f"Total sessions: {stats['total_sessions']}")
    print(f"Total products: {stats['total_products']}")
    
    for i, session in enumerate(stats['sessions']):
        print(f"\nSession {i+1}:")
        print(f"Operator: {session['operator_id'] if session['operator_id'] else 'None (Out-of-session)'}")
        print(f"Start: {session['start_time']}")
        print(f"Start Cause: {session['start_cause']}")
        print(f"End: {session['end_time']}")
        print(f"End Cause: {session['end_cause'] if session['end_cause'] else 'Still active'}")
        print(f"End Reason: {session['end_reason'] if session['end_reason'] else 'Still active'}")
        print(f"Duration: {session['end_time'] - session['start_time'] if session['end_time'] else 'Still active'}")
        print(f"Products: {session['total_count']}")
        print(f"Missing counts: {session['missing_counts']}")
        print(f"Avg time between counts: {session['avg_time_between_counts']:.2f}s")
    
    print("\nOut-of-session press records:")
    for i, record in enumerate(stats['out_of_session_records']):
        print(f"Record {i+1}:")
        print(f"Begin: {record['begin_time']}")
        print(f"End: {record['end_time']}")
        print(f"Count: {record['count']}")
    
    print("\nNFC and Relay Events:")
    for i, event in enumerate(stats['nfc_relay_events']):
        print(f"Event {i+1}:")
        print(f"Time: {event['time']}")
        print(f"Measurement: {event['measurement']}")
        print(f"Field: {event['field']}")
        print(f"Value: {event['value']}")
        print(f"Device: {event['device']}")