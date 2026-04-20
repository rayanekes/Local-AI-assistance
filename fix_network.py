with open("firmware/src/network_ws.cpp", "r") as f:
    content = f.read()

content = content.replace('webSocket.begin(server_ip, server_port, "/");', 'webSocket.begin(server_ip, server_port, "/");\n    webSocket.setExtraHeaders("Origin: http://localhost\\r\\nHost: localhost");')

with open("firmware/src/network_ws.cpp", "w") as f:
    f.write(content)

with open("backend/src/server.py", "r") as f:
    server_content = f.read()

import re
server_content = re.sub(r'async with websockets.serve\(handle_esp32_connection, "0.0.0.0", 8765, family=socket.AF_INET, reuse_address=True, reuse_port=True\):', r'async def process_request(path, request_headers):\n        pass # Accept all\n    async with websockets.serve(handle_esp32_connection, "0.0.0.0", 8765, family=socket.AF_INET, reuse_address=True, reuse_port=True, process_request=process_request):', server_content)

with open("backend/src/server.py", "w") as f:
    f.write(server_content)
