# Projet Robot Assistant (ESP32 + Serveur AI)

Ce projet divise l'architecture du robot en deux parties principales :
1. **Un Backend (Python) :** Serveur AI basé sur WebSocket avec traitement vocal (Whisper, Silero VAD), génération de texte (LLaMA) et synthèse vocale (Piper).
2. **Un Firmware (C++) :** Code embarqué pour l'ESP32 contrôlant le microphone, le haut-parleur (I2S), un écran TFT (ILI9341) et des GPIOs.

## Structure du projet

```
├── backend/                  # Serveur d'intelligence artificielle
│   ├── src/                  # Code source (ex: server.py)
│   ├── models/               # Modèles LLM (à télécharger)
│   ├── piper/                # Exécutable et modèles Piper TTS (à télécharger)
│   ├── memory/               # Sauvegarde du contexte (RAG)
│   └── requirements.txt      # Dépendances Python
│
├── firmware/                 # Code embarqué pour ESP32
│   ├── src/                  # Fichiers sources C++
│   ├── include/              # Fichiers d'en-tête (headers)
│   └── platformio.ini        # Configuration PlatformIO
│
├── .gitignore                # Fichiers à ignorer par git
└── README.md                 # Cette documentation
```

## Backend
Le serveur utilise un réseau WebSocket pour communiquer avec l'ESP32.

### Prérequis (Serveur Pop!_OS / Ubuntu)
Pour que les outils de test audio (comme le simulateur PC ou la capture micro) fonctionnent sous Linux, vous devez d'abord installer les bibliothèques système de gestion du son :
```bash
sudo apt-get update
sudo apt-get install portaudio19-dev
sudo apt-get install libavformat-dev libavcodec-dev libavdevice-dev libavutil-dev libswscale-dev libswresample-dev libavfilter-dev
```

Ensuite, installez toutes les dépendances Python :
```bash
cd backend
pip install -r requirements.txt
```

### ⚠️ IMPORTANT : Modèles IA (Non inclus sur GitHub)
Pour des raisons de taille de fichiers, les modèles d'Intelligence Artificielle (GGUF, ONNX) ne sont pas stockés sur GitHub. Vous devez les copier ou les télécharger manuellement dans les dossiers suivants :

1. **Modèle LLM (Llama.cpp) :**
   - Placez `qwen2.5-3b-instruct-q5_k_m.gguf` dans le dossier `backend/models/`.
2. **Modèle TTS (Piper ONNX) :**
   - Placez le fichier de voix `fr_FR-siwis-medium.onnx` (et son fichier `.json` associé) dans le dossier `backend/piper/`. L'exécutable lui-même est installé via pip (`piper-tts`).

### Exécution du Serveur Principal
```bash
python src/server.py
```

### Exécution des Tests Locaux (Micro PC)
Pour tester l'IA sans ESP32 :
```bash
python tools/simulateur_pc.py
# ou pour le pipeline pas à pas :
python test/test_pipeline.py
```

## Firmware
Le code est conçu pour être compilé et flashé avec [PlatformIO](https://platformio.org/).

### Prérequis
- VSCode avec l'extension PlatformIO
- Une carte ESP32

Ouvrez le dossier `firmware` dans VSCode, puis compilez et téléversez le code.
