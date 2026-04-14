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

### Prérequis
```bash
cd backend
pip install -r requirements.txt
```

### Exécution
```bash
python src/server.py
```

## Firmware
Le code est conçu pour être compilé et flashé avec [PlatformIO](https://platformio.org/).

### Prérequis
- VSCode avec l'extension PlatformIO
- Une carte ESP32

Ouvrez le dossier `firmware` dans VSCode, puis compilez et téléversez le code.
