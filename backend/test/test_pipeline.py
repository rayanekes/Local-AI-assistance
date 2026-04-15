import os
import sys
import json
import re
import asyncio
import queue
import numpy as np
import sounddevice as sd
import scipy.io.wavfile as wav

# Ajouter le chemin parent pour pouvoir importer la configuration si besoin
BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.append(BASE_DIR)

from faster_whisper import WhisperModel
from llama_cpp import Llama

# =========================
# CONFIGURATION
# =========================
# Le dossier des modèles lourds est désormais mutualisé avec l'ancien projet
MODELS_DIR = "/home/rayane/projet_robot/models"
INPUT_WAV = os.path.join(BASE_DIR, "test_input.wav")

SAMPLE_RATE_MIC = 16000
SAMPLE_RATE_TTS = 22050

LLM_MODEL_PATH = os.path.join(MODELS_DIR, "qwen2.5-3b-instruct-q5_k_m.gguf")
WHISPER_MODEL = "small"
WHISPER_DEVICE = "cuda"

import shutil

PIPER_BIN = "piper" # Utilise la commande installée via pip (piper-tts)
PIPER_MODEL = os.path.join(BASE_DIR, "piper", "fr_FR-siwis-medium.onnx")

SYSTEM_PROMPT = (
    "You are an interactive engineering robot assistant. "
    "The user's input will be provided in English (translated from Moroccan Darija and French). "
    "You must understand perfectly, BUT you MUST reply ONLY in pure and natural French. "
    "Never generate words in Arabic or English in your spoken response. "
    "You MUST ALWAYS respond with a strictly valid JSON object in the following format:\n"
    "{\n  \"speech\": \"Texte en français pur pour le robot.\",\n  \"emotion\": \"joie|neutre|triste\"\n}\n"
    "Return NOTHING except the valid JSON."
)

# =========================
# INITIALISATION ML PARALLÈLE
# =========================
import concurrent.futures

if not os.path.exists(LLM_MODEL_PATH):
    print(f"\n❌ ERREUR CRITIQUE : Modèle IA introuvable !")
    print(f"Le fichier attendu est : {LLM_MODEL_PATH}")
    sys.exit(1)

if not shutil.which(PIPER_BIN):
    print("\n❌ ERREUR CRITIQUE : La commande système 'piper' est introuvable !")
    print("-> Exécutez : pip install piper-tts")
    sys.exit(1)

if not os.path.exists(PIPER_MODEL):
    print(f"\n❌ ERREUR CRITIQUE : Modèle de voix introuvable dans '{PIPER_MODEL}'.")
    print("-> Placez le fichier 'fr_FR-siwis-medium.onnx' (et son .json) dans le dossier 'backend/piper/'.")
    sys.exit(1)

print("⏳ Chargement des modèles IA en parallèle...")

def load_whisper():
    try:
        model = WhisperModel(WHISPER_MODEL, device=WHISPER_DEVICE, compute_type="float16")
        print("✅ Whisper chargé (GPU)")
        return model
    except Exception as e:
        model = WhisperModel(WHISPER_MODEL, device="cpu", compute_type="int8")
        print("⚠️ Whisper chargé (CPU)")
        return model

def load_llama():
    try:
        model = Llama(
            model_path=LLM_MODEL_PATH,
            n_gpu_layers=-1,
            n_ctx=4096,
            verbose=False,
            # Force la création et l'usage du cache KV pour éviter le recalcul complet du contexte (latence)
            use_mmap=True,
            use_mlock=False
        )
        print("✅ LLaMA chargé (GPU)")
        return model
    except Exception as e:
        print(f"\n❌ Erreur LLaMA : {e}")
        sys.exit(1)

# Lancement parallèle via ThreadPoolExecutor
with concurrent.futures.ThreadPoolExecutor() as executor:
    future_w = executor.submit(load_whisper)
    future_l = executor.submit(load_llama)

    whisper = future_w.result()
    llm = future_l.result()

# =========================
# FONCTIONS DU PIPELINE
# =========================

def record_audio():
    print("\n🎤 [1/4] ENREGISTREMENT (Appuyez sur 'Entrée' pour arrêter de parler)...")
    q = queue.Queue()

    def callback(indata, frames, time, status):
        if status: pass
        q.put(indata.copy())

    stream = sd.InputStream(samplerate=SAMPLE_RATE_MIC, channels=1, dtype='int16', callback=callback)

    audio_data = []
    with stream:
        input() # Attend la pression de Entrée sans bloquer le thread audio

    while not q.empty():
        audio_data.append(q.get())

    if audio_data:
        recording = np.concatenate(audio_data, axis=0)
        wav.write(INPUT_WAV, SAMPLE_RATE_MIC, recording)
        print("✅ Enregistrement terminé.")
        return True
    return False

def run_stt():
    print("🧠 [2/4] TRANSCRIPTION (Faster-Whisper)...")
    custom_vocab = "Terminale STE, ADC, ATC, PE, Transmettre, ESP32."
    prompt_darija_tech = f"Bonjour. Kidayr labas? Wach nbedaw l'installation dial le serveur? {custom_vocab}"

    segments, _ = whisper.transcribe(
        INPUT_WAV,
        task="translate",
        beam_size=2,
        initial_prompt=prompt_darija_tech
    )
    text = "".join([s.text for s in segments]).strip()
    print(f"   -> Traduction : '{text}'")
    return text

def run_llm(user_text):
    print("🤖 [3/4] GÉNÉRATION (LLaMA)...")
    messages = [
        {"role": "system", "content": SYSTEM_PROMPT},
        {"role": "user", "content": user_text}
    ]

    res = llm.create_chat_completion(
        messages=messages,
        max_tokens=200,
        temperature=0.7
    )

    full_response = res["choices"][0]["message"]["content"]
    print(f"   -> JSON brut : {full_response}")

    # Extraire la parole
    speech_match = re.search(r'"speech"\s*:\s*"([^"]+)"', full_response)
    if speech_match:
        return speech_match.group(1).replace('\\n', ' ').replace('\\"', '"')
    return "Je n'ai pas pu générer une réponse correcte."

async def run_tts(speech_text):
    print("🗣️ [4/4] SYNTHÈSE VOCALE (Piper)...")
    piper_proc = await asyncio.create_subprocess_exec(
        PIPER_BIN, "--model", PIPER_MODEL, "--output_raw",
        stdin=asyncio.subprocess.PIPE, stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE
    )

    piper_proc.stdin.write(speech_text.encode("utf-8"))
    await piper_proc.stdin.drain()
    piper_proc.stdin.close()

    stream = sd.OutputStream(samplerate=SAMPLE_RATE_TTS, channels=1, dtype='int16')
    print("   🔊 Lecture en cours...")
    with stream:
        while True:
            audio_out = await piper_proc.stdout.read(4096)
            if not audio_out:
                break
            audio_data = np.frombuffer(audio_out, dtype=np.int16)
            stream.write(audio_data)

    # Récupérer et afficher l'erreur si Piper plante silencieusement
    _, stderr_data = await piper_proc.communicate()
    if piper_proc.returncode != 0:
        print(f"❌ Erreur Piper (Code {piper_proc.returncode}):\n{stderr_data.decode('utf-8')}")

    print("✅ Cycle terminé.\n" + "="*50)

async def main():
    while True:
        if record_audio():
            text = run_stt()
            if text:
                speech = run_llm(text)
                await run_tts(speech)
        else:
            print("Erreur d'enregistrement.")

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nArrêt du testeur de pipeline.")
