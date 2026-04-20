with open("backend/src/server.py", "r") as f:
    content = f.read()

import re

search = r'''    async with websockets.serve\(handle_esp32_connection, "0.0.0.0", 8765, family=socket.AF_INET, reuse_address=True, reuse_port=True, process_request=process_request\):
        await asyncio.Future\(\)'''

replace = r'''    async with websockets.serve(handle_esp32_connection, "0.0.0.0", 8765, family=socket.AF_INET, reuse_address=True, reuse_port=True, process_request=process_request):
        print("SERVER_READY")
        await asyncio.Future()'''

content = re.sub(search, replace, content)

with open("backend/src/server.py", "w") as f:
    f.write(content)
