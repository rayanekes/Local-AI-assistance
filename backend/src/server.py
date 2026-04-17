import os
import json
import asyncio
import websockets
import re
import numpy as np
import torch
import scipy.io.wavfile as wav
from collections import deque

from faster_whisper import WhisperModel
from silero_vad import load_silero_vad, get_speech_timestamps
from llama_cpp import Llama

BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# =========================
# GESTION MÉMOIRE DYNAMIQUE (RAG)
# =========================

MEMORY_FILE = os.path.join(BASE_DIR, "memory", "memoire_utilisateur.json")

def charger_memoire():
    if os.path.exists(MEMORY_FILE):
        try:
            with open(MEMORY_FILE, 'r', encoding='utf-8') as f:
                mem = json.load(f)
                return mem.get("contexte_utilisateur", "Aucun contexte particulier.")
        except Exception as e:
            pass
    return "L'utilisateur est un étudiant/ingénieur travaillant sur un ESP32."

def sauvegarder_memoire(nouveau_contexte):
    with open(MEMORY_FILE, 'w', encoding='utf-8') as f:
        json.dump({"contexte_utilisateur": nouveau_contexte},
                  f, ensure_ascii=False, indent=4)

async def synthese_memoire_background(nouvelle_info, llm_instance):
    mem_actuelle = charger_memoire()

    prompt_synthese = (
        f"Tu es un module cognitif. Voici la mémoire actuelle de l'utilisateur: '{mem_actuelle}'. "
        f"Voici la dernière information de la conversation: '{nouvelle_info}'. "
        "Mets à jour la mémoire globale en une phrase concise en français, sans aucun format JSON."
    )

    try:
        def run_llm():
            return llm_instance.create_chat_completion(
                messages=[{"role": "system", "content": prompt_synthese}],
                max_tokens=50,
                temperature=0.3
            )

        res = await asyncio.to_thread(run_llm)
        nouveau_contexte = res["choices"][0]["message"]["content"].strip()
        await asyncio.to_thread(sauvegarder_memoire, nouveau_contexte)
    except Exception as e:
        pass

# =========================
# CONFIGURATION MATÉRIEL
# =========================

# Le dossier des modèles lourds est mutualisé à l'extérieur du dépôt Git pour éviter la duplication
MODELS_DIR = os.path.expanduser("~/projet_robot/models")
INPUT_WAV = os.path.join(BASE_DIR, "input.wav")

SAMPLE_RATE_MIC = 16000
SAMPLE_RATE_TTS = 22050
CHUNK_SIZE_MIC = 1024

# Supporte à la fois le fichier unique et le premier fichier d'un modèle divisé (split)
LLM_MODEL_PATH = os.path.join(MODELS_DIR, "qwen2.5-7b-instruct-q4_k_m.gguf")
LLM_MODEL_PATH_SPLIT = os.path.join(MODELS_DIR, "qwen2.5-7b-instruct-q4_k_m-00001-of-00002.gguf")
WHISPER_MODEL = "medium" # Modèle plus grand pour une meilleure détection, tout en gérant la VRAM (6GB)
WHISPER_DEVICE = "cuda"

import shutil

PIPER_BIN = "piper" # Utilise la commande pip globale (piper-tts)
PIPER_MODEL = os.path.join(BASE_DIR, "piper", "fr_FR-siwis-medium.onnx")

memoire_dynamique = charger_memoire()

SYSTEM_PROMPT = (
    "You are an interactive engineering robot assistant. "
    f"Here is what you know about the user so far: {memoire_dynamique}\n"
    "The user's input will be provided in English (translated from Moroccan Darija and French). "
    "You must understand perfectly, BUT you MUST reply ONLY in pure and natural French. "
    "Never generate words in Arabic or English in your spoken response. "
    "You also control a hardware system with an ILI9341 TFT screen. "
    "You MUST ALWAYS respond with a strictly valid JSON object in the following format:\n"
    "{\n"
    "  \"speech\": \"Texte en français pur pour le robot.\",\n"
    "  \"emotion\": \"joie|neutre|triste\",\n"
    "  \"gpio_commands\": [{\"pin\": 4, \"state\": true}]\n"
    "}\n"
    "Return NOTHING except the valid JSON."
)

conversation_history = [{"role": "system", "content": SYSTEM_PROMPT}]

# =========================
# INITIALISATION ML PARALLÈLE
# =========================

import sys
import concurrent.futures

# Détermination automatique du bon fichier LLM à charger
if os.path.exists(LLM_MODEL_PATH_SPLIT):
    actual_llm_path = LLM_MODEL_PATH_SPLIT
elif os.path.exists(LLM_MODEL_PATH):
    actual_llm_path = LLM_MODEL_PATH
else:
    print(f"\n❌ ERREUR CRITIQUE : Modèle IA (GGUF) introuvable dans '{MODELS_DIR}'.")
    print(f"-> Veuillez y placer '{os.path.basename(LLM_MODEL_PATH)}' OU '{os.path.basename(LLM_MODEL_PATH_SPLIT)}'.")
    sys.exit(1)

if not shutil.which(PIPER_BIN):
    print("\n❌ ERREUR CRITIQUE : Exécutable système 'piper' introuvable.")
    print("-> Assurez-vous d'avoir exécuté : pip install piper-tts")
    sys.exit(1)

if not os.path.exists(PIPER_MODEL):
    print(f"\n❌ ERREUR CRITIQUE : Modèle de voix introuvable dans '{PIPER_MODEL}'.")
    print("-> Veuillez placer 'fr_FR-siwis-medium.onnx' dans le dossier 'backend/piper/'.")
    sys.exit(1)

if not os.path.exists(PIPER_MODEL + ".json"):
    print(f"\n❌ ERREUR CRITIQUE : Fichier de configuration phonétique Piper introuvable.")
    print(f"Le fichier attendu est : '{PIPER_MODEL}.json'.")
    sys.exit(1)

print("⏳ Chargement des modèles IA en parallèle...")

def load_whisper():
    try:
        # Utilisation de int8_float16 pour réduire considérablement la VRAM de Whisper (garde Whisper rapide sur GPU)
        model = WhisperModel(WHISPER_MODEL, device=WHISPER_DEVICE, compute_type="int8_float16")
        print("✅ Whisper chargé (GPU - Optimisé VRAM)")
        return model
    except Exception as e:
        model = WhisperModel(WHISPER_MODEL, device="cpu", compute_type="int8")
        print("⚠️ Whisper chargé (CPU)")
        return model

def load_llama():
    try:
        # Vérification interne pour savoir si CUDA est réellement activé dans llama.cpp
        import llama_cpp
        if not llama_cpp.llama_supports_gpu_offload():
            print("\n⚠️ AVERTISSEMENT : llama-cpp-python n'a pas été compilé avec le support CUDA !")
            print("Le modèle tourne actuellement sur le CPU (très lent, 100% CPU, faible utilisation VRAM).")
            print("Pour corriger : CMAKE_ARGS=\"-DGGML_CUDA=on\" pip install llama-cpp-python --force-reinstall --no-cache-dir\n")

        model = Llama(
            model_path=actual_llm_path,
            # On fixe à 20 layers pour que LLaMA laisse environ 300-500 Mo de VRAM vides (les layers restantes iront sur le CPU)
            n_gpu_layers=20,
            n_ctx=4096,
            verbose=False,
            # Charge les poids via la mémoire virtuelle du système (Memory Mapping) pour réduire la RAM système
            use_mmap=True,
            use_mlock=False
        )
        if llama_cpp.llama_supports_gpu_offload():
            print("✅ LLaMA chargé (GPU - CUDA)")
        else:
            print("⚠️ LLaMA chargé (CPU - TRES LENT)")
        return model
    except Exception as e:
        print(f"\n❌ Erreur LLaMA: {e}")
        sys.exit(1)

def load_vad():
    model = load_silero_vad()
    print("✅ Silero VAD chargé")
    return model

# Lancer le chargement dans 3 threads parallèles (Diminue la latence de démarrage (I/O))
with concurrent.futures.ThreadPoolExecutor() as executor:
    future_whisper = executor.submit(load_whisper)
    future_llama = executor.submit(load_llama)
    future_vad = executor.submit(load_vad)

    whisper = future_whisper.result()
    llm = future_llama.result()
    vad_model = future_vad.result()

# =========================
# OUTILS
# =========================

class JSONSpeechExtractor:
    def __init__(self):
        self.state = 'WAIT'
        self.buffer = ""

    def extract_chunk(self, token):
        self.buffer += token
        if self.state == 'WAIT':
            match = re.search(r'"speech"\s*:\s*"', self.buffer)
            if match:
                self.state = 'EXTRACTING'
                self.buffer = self.buffer[match.end():]
            elif len(self.buffer) > 100:
                self.buffer = self.buffer[-50:]
            return ""

        if self.state == 'EXTRACTING':
            match = re.search(r'(?<!\\)"', self.buffer)
            if match:
                self.state = 'FINISHED'
                speech = self.buffer[:match.start()]
                self.buffer = self.buffer[match.end():]
                return speech
            if len(self.buffer) > 1:
                speech = self.buffer[:-1]
                self.buffer = self.buffer[-1:]
                return speech
        return ""

def split_tts_sentence(buffer):
    match = re.search(r'([.?!]+)', buffer)
    if match and match.end() >= 10:
        return buffer[:match.end()], buffer[match.end():]
    return None, buffer

# =========================
# SERVEUR WEBSOCKET ASYNC
# =========================

AUTH_TOKEN = os.environ.get("WS_AUTH_TOKEN", "secure_token_esp32_rayane_2024")

async def process_auth(connection, request):
    auth_header = request.headers.get("Authorization")
    if auth_header != f"Bearer {AUTH_TOKEN}":
        print(f"⚠️ [Security] Tentative de connexion WebSocket non autorisée")
        return connection.respond(403, "Unauthorized\n")

async def handle_esp32_connection(websocket):
    is_speaking = False
    robot_is_answering = False
    interrupt_flag = False

    audio_buffer = []
    pre_roll = deque(maxlen=8)
    silence_frames = 0
    silence_threshold = 20

    async def send_json_command(key, value):
        # Envoie un JSON plat, ex: {"status": "thinking"} ou {"emotion": "joie"}
        payload = {key: value}
        await websocket.send(json.dumps(payload))

    async def read_piper_stdout(piper_proc, websocket, state_container):
        try:
            while True:
                if state_container["interrupt"]:
                    piper_proc.terminate()
                    break
                # On lit des blocs de 4096 octets (2048 samples en 16-bit)
                audio_out = await piper_proc.stdout.read(4096)
                if not audio_out:
                    break
                await websocket.send(audio_out)

                # --- Pacing Audio ---
                # 4096 octets / 2 (car 16 bits = 2 octets) = 2048 samples
                # 2048 samples à 22050 Hz représentent environ 0.092 secondes d'audio.
                # On demande à Python d'attendre un peu avant d'envoyer le paquet suivant,
                # sinon il sature la petite file d'attente (Queue) FreeRTOS de l'ESP32, ce qui cause des saccades.
                await asyncio.sleep(0.08) # Légèrement inférieur à 0.092 pour garder un petit buffer d'avance
        except Exception:
            pass

    def transcribe_audio(wav_path):
        custom_vocab = "Terminale STE, ADC, ATC, PE, Transmettre, ESP32."
        prompt_darija_tech = f"Bonjour. Kidayr labas? Wach nbedaw l'installation dial le serveur? {custom_vocab}"
        segments, _ = whisper.transcribe(
            wav_path,
            task="translate",
            beam_size=5, # Plus grand beam_size pour plus de précision
            initial_prompt=prompt_darija_tech
        )
        return "".join([s.text for s in segments]).strip()

    async def run_llm_and_tts(transcribed_text):
        nonlocal robot_is_answering, interrupt_flag
        robot_is_answering = True
        interrupt_flag = False

        conversation_history.append({"role": "user", "content": transcribed_text})
        await send_json_command("status", "thinking")

        extractor = JSONSpeechExtractor()
        tts_buffer = ""
        full_llm_response = ""
        emotion_sent = False

        piper_proc = await asyncio.create_subprocess_exec(
            PIPER_BIN, "--model", PIPER_MODEL, "--output_raw",
            stdin=asyncio.subprocess.PIPE, stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.DEVNULL
        )

        state_container = {"interrupt": False}
        stream_task = asyncio.create_task(read_piper_stdout(piper_proc, websocket, state_container))

        def generate_llm_stream():
            return llm.create_chat_completion(
                messages=conversation_history,
                max_tokens=200,
                temperature=0.7,
                stream=True
            )

        await send_json_command("status", "speaking")

        # Wrapping the standard stream generator execution in a thread pool to unblock Asyncio event loop
        stream_generator = await asyncio.to_thread(generate_llm_stream)

        while True:
            if interrupt_flag:
                state_container["interrupt"] = True
                break

            try:
                # Yield next token from generator in thread to prevent blocking
                chunk = await asyncio.to_thread(next, stream_generator)
            except StopIteration:
                break

            delta = chunk["choices"][0].get("delta", {})
            if "content" in delta:
                token = delta["content"]
                full_llm_response += token

                # N'envoyer l'émotion qu'une seule fois par réponse
                if not emotion_sent:
                    emotion_match = re.search(r'"emotion"\s*:\s*"([^"]+)"', full_llm_response)
                    if emotion_match:
                        await send_json_command("emotion", emotion_match.group(1))
                        emotion_sent = True

                speech_part = extractor.extract_chunk(token)
                if speech_part:
                    speech_part = speech_part.replace('\\n', ' ').replace('\\"', '"')
                    tts_buffer += speech_part

                    sentence, tts_buffer = split_tts_sentence(tts_buffer)
                    if sentence:
                        piper_proc.stdin.write(sentence.encode("utf-8"))
                        await piper_proc.stdin.drain()

        if tts_buffer.strip() and not interrupt_flag:
            piper_proc.stdin.write(tts_buffer.encode("utf-8"))
            await piper_proc.stdin.drain()
            piper_proc.stdin.close()

        if interrupt_flag:
            state_container["interrupt"] = True
            if not piper_proc.stdin.is_closing():
                piper_proc.stdin.close()

        await stream_task

        if piper_proc.returncode is None:
            piper_proc.terminate()

        robot_is_answering = False
        await send_json_command("status", "idle")

        if full_llm_response and not interrupt_flag:
            conversation_history.append({"role": "assistant", "content": full_llm_response})
            asyncio.create_task(synthese_memoire_background(transcribed_text, llm))

    try:
        async for message in websocket:
            if type(message) is bytes:
                chunk = np.frombuffer(message, dtype=np.int16)
                audio_tensor = torch.from_numpy(chunk.astype(np.float32) / 32768.0)

                # Abaissement du seuil de probabilité pour détecter la voix plus facilement (par défaut c'est souvent 0.5)
                timestamps = await asyncio.to_thread(get_speech_timestamps, audio_tensor, vad_model, sampling_rate=SAMPLE_RATE_MIC, threshold=0.3)
                voice_detected = len(timestamps) > 0

                # Calcul de l'énergie (RMS) pour l'AEC heuristique
                rms = np.sqrt(np.mean(audio_tensor.numpy()**2))

                if not is_speaking:
                    pre_roll.append(chunk)
                    if voice_detected:
                        # Si le robot parle, on exige un volume (RMS) beaucoup plus fort pour déclencher le Barge-in
                        # Cela évite que l'écho de son propre haut-parleur ne l'interrompe (AEC logiciel basique)
                        barge_in_threshold = 0.05  # À ajuster empiriquement
                        if robot_is_answering and rms < barge_in_threshold:
                            continue # C'est sûrement de l'écho, on ignore

                        is_speaking = True
                        audio_buffer.extend(list(pre_roll))
                        pre_roll.clear()
                        silence_frames = 0

                        # Déclencher l'interruption
                        if robot_is_answering:
                            interrupt_flag = True
                else:
                    audio_buffer.append(chunk)
                    if voice_detected:
                        silence_frames = 0
                    else:
                        silence_frames += 1
                        if silence_frames > silence_threshold:
                            is_speaking = False

                            audio_data = np.concatenate(audio_buffer)
                            await asyncio.to_thread(wav.write, INPUT_WAV, SAMPLE_RATE_MIC, audio_data)
                            text = await asyncio.to_thread(transcribe_audio, INPUT_WAV)

                            if text:
                                asyncio.create_task(run_llm_and_tts(text))

                            audio_buffer = []
                            silence_frames = 0
    except websockets.exceptions.ConnectionClosed:
        pass


async def main():
    # On utilise process_request pour une authentification au niveau du handshake HTTP
    async with websockets.serve(handle_esp32_connection, "0.0.0.0", 8765, process_request=process_auth):
        await asyncio.Future()

if __name__ == "__main__":
    asyncio.run(main())