import json
import time

pipe_path = r"\\.\pipe\fooyin_mcp_v2"

def send_request(method, params=None, request_id=1):
    req = {
        "jsonrpc": "2.0",
        "id": request_id,
        "method": method,
        "params": params or {}
    }
    
    try:
        with open(pipe_path, "r+b", buffering=0) as f:
            msg = (json.dumps(req) + "\n").encode("utf-8")
            print(f"Sending: {msg.decode().strip()}")
            f.write(msg)
            
            # Warte kurz auf Antwort
            time.sleep(0.1)
            response = f.readline().decode("utf-8")
            print(f"Received: {response.strip()}")
            return json.loads(response)
    except FileNotFoundError:
        print(f"Error: Pipe {pipe_path} not found. Is the plugin loaded?")
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    print("Testing Fooyin MCP Plugin...")
    # 1. Ping / Initialize
    send_request("initialize", {"protocolVersion": "2024-11-05"})
    # 2. List Tools
    send_request("tools/list")
    # 3. Get Now Playing
    send_request("tools/call", {"name": "getNowPlaying", "arguments": {}})
    # 4. Get State (including volume)
    send_request("tools/call", {"name": "getPlaybackState", "arguments": {}})
    # 5. Set Volume
    send_request("tools/call", {"name": "setVolume", "arguments": {"volume": 0.5}})
    # 6. Verify Volume Change
    send_request("tools/call", {"name": "getPlaybackState", "arguments": {}})
