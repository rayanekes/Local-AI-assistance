with open("backend/src/server.py", "r") as f:
    content = f.read()

import re

# Replace parallel loading
search = r'''# Lancer le chargement dans 3 threads parallèles \(Diminue la latence de démarrage \(I/O\)\)
with concurrent\.futures\.ThreadPoolExecutor\(\) as executor:
    future_whisper = executor\.submit\(load_whisper\)
    future_llama = executor\.submit\(load_llama\)
    future_vad = executor\.submit\(load_vad\)

    whisper = future_whisper\.result\(\)
    llm = future_llama\.result\(\)
    vad_model = future_vad\.result\(\)'''

replace = r'''# Lancer le chargement de manière séquentielle pour éviter les pics de VRAM
print("Chargement de Whisper...")
whisper = load_whisper()
print("Chargement de Llama...")
llm = load_llama()
print("Chargement de VAD...")
vad_model = load_vad()'''

content = re.sub(search, replace, content)

search2 = r'''    async with websockets.serve\(handle_esp32_connection, "0.0.0.0", 8765, family=socket.AF_INET, reuse_address=True, reuse_port=True, process_request=process_request\):
        await asyncio.Future\(\)'''

replace2 = r'''    async with websockets.serve(handle_esp32_connection, "0.0.0.0", 8765, family=socket.AF_INET, reuse_address=True, reuse_port=True, process_request=process_request):
        print("SERVER_READY")
        await asyncio.Future()'''

content = content.replace(search2, replace2)

# Also fix the import of socket since process_request uses it, wait process_request doesn't use socket.
# I need to ensure process_request correctly takes path and request_headers.

with open("backend/src/server.py", "w") as f:
    f.write(content)
